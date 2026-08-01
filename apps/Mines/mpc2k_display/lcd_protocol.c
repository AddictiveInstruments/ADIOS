#include <mios32.h>
#include "lcd_protocol.h"
#include "oled_ssd1322.h"


#define MODER_PA0_7_INPUT_MASK   (0xFFFF0000u)
#define MODER_PA0_7_OUTPUT_MASK  (0xFFFF0000u | 0x00005555u)
static const uint32_t moder_output_const = MODER_PA0_7_OUTPUT_MASK;
static const uint32_t moder_input_const  = MODER_PA0_7_INPUT_MASK;

/* Pas de GDRAM local : les pixels partent directement vers l'OLED.
 * Pas de compteur de dépassement non plus : la fenêtre matérielle
 * SSD1322 (0-63 colonnes / 0-59 lignes, fixée une fois pour toutes
 * à l'init OLED) borne tout débordement, quel que soit le nombre
 * d'octets envoyés par l'hôte au-delà de la largeur/hauteur logique. */
typedef struct {
    uint8_t x, y, last_cmd, inc_mode;
} lcd_left_t;

typedef struct {
    uint8_t x, y, last_cmd, inc_mode;
} lcd_right_t;

static lcd_left_t  left;
static lcd_right_t right;
static volatile uint8_t status_preload;

static volatile uint32_t cap_cb_single;
static volatile uint32_t cap_db_single;
static volatile uint32_t cb_last_known;

/* ---------------------------------------------------------------------
 * GPIO du bus LCD
 * DB (PA0-7) : bidirectionnel, open-drain, 5V via pull-ups externes déjà
 *              présents sur le circuit.
 * CB : PB0(masse/TIM3_CH3), PB4(IOWR/TIM3_CH1), PB5(IORD/TIM3_CH2),
 *      PB6(CS1), PB7(DC), PB8(RST), PB9(CS2)
 * --------------------------------------------------------------------- */
static void lcd_gpio_init(void)
{
    LL_IOP_GRP1_EnableClock(LL_IOP_GRP1_PERIPH_GPIOA);
    LL_IOP_GRP1_EnableClock(LL_IOP_GRP1_PERIPH_GPIOB);

    LL_GPIO_InitTypeDef db = {0};
    db.Pin        = LL_GPIO_PIN_0|LL_GPIO_PIN_1|LL_GPIO_PIN_2|LL_GPIO_PIN_3|
                     LL_GPIO_PIN_4|LL_GPIO_PIN_5|LL_GPIO_PIN_6|LL_GPIO_PIN_7;
    db.Mode       = LL_GPIO_MODE_INPUT;
    db.Speed      = LL_GPIO_SPEED_FREQ_VERY_HIGH;
    db.OutputType = LL_GPIO_OUTPUT_OPENDRAIN;
    db.Pull       = LL_GPIO_PULL_NO;
    LL_GPIO_Init(GPIOA, &db);

    LL_GPIO_InitTypeDef cb = {0};
    cb.Pin  = LL_GPIO_PIN_0|LL_GPIO_PIN_4|LL_GPIO_PIN_5|LL_GPIO_PIN_6|
              LL_GPIO_PIN_7|LL_GPIO_PIN_8|LL_GPIO_PIN_9;
    cb.Mode = LL_GPIO_MODE_INPUT;
    cb.Pull = LL_GPIO_PULL_NO;
    LL_GPIO_Init(GPIOB, &cb);

    LL_GPIO_SetPinMode(GPIOB, LL_GPIO_PIN_4, LL_GPIO_MODE_ALTERNATE);
    LL_GPIO_SetAFPin_0_7(GPIOB, LL_GPIO_PIN_4, LL_GPIO_AF_1);   // TIM3_CH1 = IOWR
    LL_GPIO_SetPinMode(GPIOB, LL_GPIO_PIN_5, LL_GPIO_MODE_ALTERNATE);
    LL_GPIO_SetAFPin_0_7(GPIOB, LL_GPIO_PIN_5, LL_GPIO_AF_1);   // TIM3_CH2 = IORD
    LL_GPIO_SetPinMode(GPIOB, LL_GPIO_PIN_0, LL_GPIO_MODE_ALTERNATE);
    LL_GPIO_SetAFPin_0_7(GPIOB, LL_GPIO_PIN_0, LL_GPIO_AF_1);   // TIM3_CH3 = masse
}

/* ---------------------------------------------------------------------
 * TIM3 : CH1 = XOR(IOWR,IORD,masse) capture différée (écriture) ;
 *        CH2 = IORD brut, front descendant seul -> MODER (lecture)
 * --------------------------------------------------------------------- */
static void lcd_tim3_init(void)
{
    LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_TIM3);

    LL_TIM_InitTypeDef t = {0};
    t.Prescaler = 0; t.CounterMode = LL_TIM_COUNTERMODE_UP; t.Autoreload = 0xFFFF;
    LL_TIM_Init(TIM3, &t);

    LL_TIM_IC_SetActiveInput(TIM3, LL_TIM_CHANNEL_CH1, LL_TIM_ACTIVEINPUT_DIRECTTI);
    LL_TIM_IC_SetFilter(TIM3, LL_TIM_CHANNEL_CH1, LL_TIM_IC_FILTER_FDIV1);   // pas de filtre
    LL_TIM_IC_SetPolarity(TIM3, LL_TIM_CHANNEL_CH1, LL_TIM_IC_POLARITY_BOTHEDGE);

    LL_TIM_IC_SetActiveInput(TIM3, LL_TIM_CHANNEL_CH2, LL_TIM_ACTIVEINPUT_DIRECTTI);
    LL_TIM_IC_SetFilter(TIM3, LL_TIM_CHANNEL_CH2, LL_TIM_IC_FILTER_FDIV1);
    LL_TIM_IC_SetPolarity(TIM3, LL_TIM_CHANNEL_CH2, LL_TIM_IC_POLARITY_FALLING);

    LL_TIM_IC_SetActiveInput(TIM3, LL_TIM_CHANNEL_CH3, LL_TIM_ACTIVEINPUT_DIRECTTI);
    LL_TIM_IC_SetFilter(TIM3, LL_TIM_CHANNEL_CH3, LL_TIM_IC_FILTER_FDIV1);

    TIM3->CR2 |= TIM_CR2_TI1S;   // TI1 = CH1 xor CH2 xor CH3

    LL_TIM_CC_EnableChannel(TIM3, LL_TIM_CHANNEL_CH1|LL_TIM_CHANNEL_CH2|LL_TIM_CHANNEL_CH3);
    LL_TIM_EnableDMAReq_CC1(TIM3);
    LL_TIM_EnableDMAReq_CC2(TIM3);
    LL_TIM_EnableCounter(TIM3);
}

/* ---------------------------------------------------------------------
 * DMA du bus LCD (canaux 1/2/3)
 * --------------------------------------------------------------------- */
static void lcd_dma_init(void)
{
    LL_AHB1_GRP1_EnableClock(LL_AHB1_GRP1_PERIPH_DMA1);
    /* Pas d'activation séparée pour DMAMUX1 sur G031 : cadencé avec DMA1 */

    /* Canal 1 : capture GPIOB->IDR (CS/DC/WR/RD) à chaque front WR ou RD */
    LL_DMA_ConfigAddresses(DMA1, LL_DMA_CHANNEL_1,
        (uint32_t)&GPIOB->IDR, (uint32_t)&cap_cb_single, LL_DMA_DIRECTION_PERIPH_TO_MEMORY);
    LL_DMA_SetDataLength(DMA1, LL_DMA_CHANNEL_1, 1);
    LL_DMA_SetMode(DMA1, LL_DMA_CHANNEL_1, LL_DMA_MODE_CIRCULAR);
    LL_DMA_SetMemoryIncMode(DMA1, LL_DMA_CHANNEL_1, LL_DMA_MEMORY_NOINCREMENT);
    LL_DMA_SetPeriphSize(DMA1, LL_DMA_CHANNEL_1, LL_DMA_PDATAALIGN_WORD);
    LL_DMA_SetMemorySize(DMA1, LL_DMA_CHANNEL_1, LL_DMA_MDATAALIGN_WORD);
    LL_DMA_SetPeriphRequest(DMA1, LL_DMA_CHANNEL_1, LL_DMAMUX_REQ_TIM3_CH1);
    LL_DMA_SetChannelPriorityLevel(DMA1, LL_DMA_CHANNEL_1, LL_DMA_PRIORITY_VERYHIGH);
    LL_DMA_EnableIT_TC(DMA1, LL_DMA_CHANNEL_1);
    LL_DMA_EnableChannel(DMA1, LL_DMA_CHANNEL_1);

    /* Canal 2 : capture GPIOA->IDR (DB), synchronisé sur le même trigger */
    LL_DMA_ConfigAddresses(DMA1, LL_DMA_CHANNEL_2,
        (uint32_t)&GPIOA->IDR, (uint32_t)&cap_db_single, LL_DMA_DIRECTION_PERIPH_TO_MEMORY);
    LL_DMA_SetDataLength(DMA1, LL_DMA_CHANNEL_2, 1);
    LL_DMA_SetMode(DMA1, LL_DMA_CHANNEL_2, LL_DMA_MODE_CIRCULAR);
    LL_DMA_SetMemoryIncMode(DMA1, LL_DMA_CHANNEL_2, LL_DMA_MEMORY_NOINCREMENT);
    LL_DMA_SetPeriphSize(DMA1, LL_DMA_CHANNEL_2, LL_DMA_PDATAALIGN_WORD);
    LL_DMA_SetMemorySize(DMA1, LL_DMA_CHANNEL_2, LL_DMA_MDATAALIGN_WORD);
    LL_DMA_SetPeriphRequest(DMA1, LL_DMA_CHANNEL_2, LL_DMAMUX_REQ_TIM3_CH1);
    LL_DMA_SetChannelPriorityLevel(DMA1, LL_DMA_CHANNEL_2, LL_DMA_PRIORITY_VERYHIGH);
    LL_DMA_EnableChannel(DMA1, LL_DMA_CHANNEL_2);

    /* Canal 3 : MODER = sortie sur front descendant IORD (TIM3_CH2). 100% matériel. */
    LL_DMA_ConfigAddresses(DMA1, LL_DMA_CHANNEL_3,
        (uint32_t)&moder_output_const, (uint32_t)&GPIOA->MODER, LL_DMA_DIRECTION_MEMORY_TO_PERIPH);
    LL_DMA_SetDataLength(DMA1, LL_DMA_CHANNEL_3, 1);
    LL_DMA_SetMode(DMA1, LL_DMA_CHANNEL_3, LL_DMA_MODE_CIRCULAR);
    LL_DMA_SetMemoryIncMode(DMA1, LL_DMA_CHANNEL_3, LL_DMA_MEMORY_NOINCREMENT);
    LL_DMA_SetPeriphSize(DMA1, LL_DMA_CHANNEL_3, LL_DMA_PDATAALIGN_WORD);
    LL_DMA_SetMemorySize(DMA1, LL_DMA_CHANNEL_3, LL_DMA_MDATAALIGN_WORD);
    LL_DMA_SetPeriphRequest(DMA1, LL_DMA_CHANNEL_3, LL_DMAMUX_REQ_TIM3_CH2);
    LL_DMA_SetChannelPriorityLevel(DMA1, LL_DMA_CHANNEL_3, LL_DMA_PRIORITY_VERYHIGH);
    LL_DMA_EnableChannel(DMA1, LL_DMA_CHANNEL_3);

    /* IT du canal 1 : priorité NVIC maximale, au-dessus de tout (SPI/OLED inclus) */
    NVIC_SetPriority(DMA1_Channel1_IRQn, 0);
    NVIC_EnableIRQ(DMA1_Channel1_IRQn);
}

/* ---------------------------------------------------------------------
 * Traitement des écritures LCD (appelé depuis l'IT de décodage)
 * --------------------------------------------------------------------- */
static inline void lcd_process_write_left(uint8_t dc, uint8_t data)
{
    if (!dc) {
        left.last_cmd = data;
        /* Optimisation protocole : pour 0x20 (flux de pixels), on force le
         * statut renvoyé à 0x00 pour que l'hôte saute le test du bit busy
         * entre chaque octet d'affichage (condition hôte : (DB&0x7F)!=0). */
        status_preload = (data == 0x20) ? 0x00 : data;
        GPIOA->ODR = (GPIOA->ODR & ~0xFFu) | status_preload;
        return;
    }
    switch (left.last_cmd) {
    case 0x23:
        left.inc_mode = (data & 0x80) ? 1 : 0;
        break;
    case 0x22:
        if (data <= LCD_YMAX) left.y = data;
        oled_send_reposition(left.x * 2, left.y, left.inc_mode);
        break;
    case 0x21:
        if (data <= 19) left.x = data;
        oled_send_reposition(left.x * 2, left.y, left.inc_mode);
        break;
    case 0x20:
        oled_send_pixels(data);
        if (left.inc_mode) { if (left.x < 19) left.x++; }
        else                { if (left.y < LCD_YMAX) left.y++; }
        break;
    }
}

static inline void lcd_process_write_right(uint8_t dc, uint8_t data)
{
    if (!dc) {
        right.last_cmd = data;
        status_preload = (data == 0x20) ? 0x00 : data;
        GPIOA->ODR = (GPIOA->ODR & ~0xFFu) | status_preload;
        return;
    }
    switch (right.last_cmd) {
    case 0x23:
        right.inc_mode = (data & 0x80) ? 1 : 0;
        break;
    case 0x22:
        if (data <= LCD_YMAX) right.y = data;
        oled_send_reposition(40 + right.x * 2, right.y, right.inc_mode);
        break;
    case 0x21:
        if (data <= 11) right.x = data;
        oled_send_reposition(40 + right.x * 2, right.y, right.inc_mode);
        break;
    case 0x20:
        oled_send_pixels(data);
        if (right.inc_mode) { if (right.x < 11) right.x++; }
        else                 { if (right.y < LCD_YMAX) right.y++; }
        break;
    }
}

/* ---------------------------------------------------------------------
 * IT de décodage du bus LCD (priorité NVIC maximale)
 * Note : "un READ/COMMAND se fait toujours sur le même CS que le
 * Write/COMMAND précédent" -> status_preload est global, pas besoin de
 * sélectionner entre left/right au moment du READ (déjà géré par le
 * préchargement direct de GPIOA->ODR à chaque écriture COMMAND).
 * --------------------------------------------------------------------- */
void DMA1_Channel1_IRQHandler(void)
{
    if (LL_DMA_IsActiveFlag_TC1(DMA1)) {
        LL_DMA_ClearFlag_TC1(DMA1);

        uint32_t cb   = cap_cb_single;
        uint32_t db   = cap_db_single;
        uint32_t diff = cb ^ cb_last_known;

        uint8_t cs1 = !(cb & (1u<<6));
        uint8_t cs2 = !(cb & (1u<<9));
        uint8_t dc  =  (cb & (1u<<7)) != 0;

        if ((diff & (1u<<4)) && (cb & (1u<<4))) {          // IOWR montant -> donnée valide
            uint8_t data = (uint8_t)db;
            if      (cs1) lcd_process_write_left (dc, data);
            else if (cs2) lcd_process_write_right(dc, data);
        }
        if ((diff & (1u<<5)) && (cb & (1u<<5))) {          // IORD montant -> relâche le bus
            GPIOA->MODER = moder_input_const;
        }
        cb_last_known = cb;
    }
}

/* ---------------------------------------------------------------------
 * API publique
 * --------------------------------------------------------------------- */
void lcd_protocol_init(void)
{
    lcd_gpio_init();
    lcd_tim3_init();
    lcd_dma_init();

    GPIOA->MODER = moder_input_const;
    cb_last_known = GPIOB->IDR;
}
