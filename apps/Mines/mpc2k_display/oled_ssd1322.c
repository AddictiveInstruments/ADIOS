#include <mios32.h>
#include "oled_ssd1322.h"
#include "oled_lut.h"
#include "lcd_protocol.h"   /* pour LCD_YMAX */

#define OLED_COL_OFFSET        28     /* fixe, centrage horizontal 256px dans 480px natif */
#define OLED_ROW_OFFSET_BASE   2      /* (64-60)/2 : centrage vertical neutre par défaut */

extern void delay_ms(uint32_t ms);
extern void delay_us(uint32_t us);

static volatile uint8_t oled_busy;
static uint8_t oled_row_offset_adj;   /* 0..3, réglage fin au-dessus de la base */
static uint16_t oled_reposition_buf[7];
static uint16_t oled_data_buf[4];

/* ---------------------------------------------------------------------
 * GPIO + SPI1 (3 fils, mots 9 bits, DC = bit 8)
 * PA15=CS (maintenu bas en permanence), PA12=MOSI, PB3=SCLK, PA11=RST
 * DC (anciennement PA10) n'est plus piloté par le MCU en 3 fils.
 * --------------------------------------------------------------------- */
static void oled_gpio_spi_init(void)
{
    LL_IOP_GRP1_EnableClock(LL_IOP_GRP1_PERIPH_GPIOA);
    LL_IOP_GRP1_EnableClock(LL_IOP_GRP1_PERIPH_GPIOB);
    LL_APB2_GRP1_EnableClock(LL_APB2_GRP1_PERIPH_SPI1);

    LL_GPIO_InitTypeDef o = {0};
    o.Pin = LL_GPIO_PIN_15 | LL_GPIO_PIN_11;   /* CS, RST */
    o.Mode = LL_GPIO_MODE_OUTPUT;
    o.OutputType = LL_GPIO_OUTPUT_PUSHPULL;
    o.Speed = LL_GPIO_SPEED_FREQ_HIGH;
    LL_GPIO_Init(GPIOA, &o);
    LL_GPIO_SetOutputPin(GPIOA, LL_GPIO_PIN_11);   /* RST repos = haut */
    LL_GPIO_ResetOutputPin(GPIOA, LL_GPIO_PIN_15); /* CS bas en permanence */

    LL_GPIO_InitTypeDef mosi = {0};
    mosi.Pin = LL_GPIO_PIN_12;                 /* PA12, confirmé */
    mosi.Mode = LL_GPIO_MODE_ALTERNATE;
    mosi.Alternate = LL_GPIO_AF_0;             /* A VERIFIER selon boitier exact */
    mosi.Speed = LL_GPIO_SPEED_FREQ_VERY_HIGH;
    LL_GPIO_Init(GPIOA, &mosi);

    LL_GPIO_InitTypeDef sck = {0};
    sck.Pin = LL_GPIO_PIN_3;                   /* PB3, confirmé */
    sck.Mode = LL_GPIO_MODE_ALTERNATE;
    sck.Alternate = LL_GPIO_AF_0;              /* A VERIFIER selon boitier exact */
    sck.Speed = LL_GPIO_SPEED_FREQ_VERY_HIGH;
    LL_GPIO_Init(GPIOB, &sck);

    LL_SPI_InitTypeDef spi = {0};
    spi.TransferDirection = LL_SPI_HALF_DUPLEX_TX;
    spi.Mode = LL_SPI_MODE_MASTER;
    spi.DataWidth = LL_SPI_DATAWIDTH_9BIT;         /* DC = bit 8 */
    spi.ClockPolarity = LL_SPI_POLARITY_HIGH;      /* mode 3, confirmé (CPOL=1, CPHA=1) */
    spi.ClockPhase = LL_SPI_PHASE_2EDGE;
    spi.NSS = LL_SPI_NSS_SOFT;
    spi.BaudRate = LL_SPI_BAUDRATEPRESCALER_DIV8;  /* 64MHz/8 = 8 MHz */
    spi.BitOrder = LL_SPI_MSB_FIRST;
    LL_SPI_Init(SPI1, &spi);
}

/* ---------------------------------------------------------------------
 * DMA (canal 4, SPI1 TX), déclenché depuis lcd_protocol via oled_send_*
 * --------------------------------------------------------------------- */
static void oled_dma_init(void)
{
    LL_AHB1_GRP1_EnableClock(LL_AHB1_GRP1_PERIPH_DMA1);
    /* Pas d'activation séparée pour DMAMUX1 sur G031 : cadencé avec DMA1 */

    LL_DMA_ConfigAddresses(DMA1, LL_DMA_CHANNEL_4,
        (uint32_t)oled_reposition_buf, (uint32_t)&SPI1->DR, LL_DMA_DIRECTION_MEMORY_TO_PERIPH);
    LL_DMA_SetMemoryIncMode(DMA1, LL_DMA_CHANNEL_4, LL_DMA_MEMORY_INCREMENT);
    LL_DMA_SetPeriphSize(DMA1, LL_DMA_CHANNEL_4, LL_DMA_PDATAALIGN_HALFWORD);
    LL_DMA_SetMemorySize(DMA1, LL_DMA_CHANNEL_4, LL_DMA_MDATAALIGN_HALFWORD);
    LL_DMA_SetPeriphRequest(DMA1, LL_DMA_CHANNEL_4, LL_DMAMUX_REQ_SPI1_TX);
    LL_DMA_SetChannelPriorityLevel(DMA1, LL_DMA_CHANNEL_4, LL_DMA_PRIORITY_LOW);
    LL_DMA_EnableIT_TC(DMA1, LL_DMA_CHANNEL_4);

    /* Priorité NVIC sous celle du décodage LCD (DMA1_Channel1_IRQn = 0) */
    NVIC_SetPriority(DMA1_Ch4_7_DMAMUX1_OVR_IRQn, 2);
    NVIC_EnableIRQ(DMA1_Ch4_7_DMAMUX1_OVR_IRQn);
}

/* ---------------------------------------------------------------------
 * Séquence d'init SSD1322 - bloquante, one-shot au démarrage
 * --------------------------------------------------------------------- */
static void oled_write_cmd(uint8_t cmd)
{
    while (!LL_SPI_IsActiveFlag_TXE(SPI1));
    LL_SPI_TransmitData16(SPI1, (0u << 8) | cmd);
    while (LL_SPI_IsActiveFlag_BSY(SPI1));
}
static void oled_write_data(uint8_t data)
{
    while (!LL_SPI_IsActiveFlag_TXE(SPI1));
    LL_SPI_TransmitData16(SPI1, (1u << 8) | data);
    while (LL_SPI_IsActiveFlag_BSY(SPI1));
}
static void oled_cmd(uint8_t cmd)  { oled_write_cmd(cmd); }
static void oled_dat(uint8_t data) { oled_write_data(data); }

static void oled_reset(void)
{
	int ms;
    LL_GPIO_ResetOutputPin(GPIOA, LL_GPIO_PIN_11);
    MIOS32_DELAY_Wait_uS(1000);
    LL_GPIO_SetOutputPin(GPIOA, LL_GPIO_PIN_11);
    for (ms=0; ms<50; ++ms)MIOS32_DELAY_Wait_uS(1000);
}

static void oled_hw_init_sequence(void)
{
    oled_reset();
    LL_SPI_Enable(SPI1);

    oled_cmd(0xFD); oled_dat(0x12);   /* Command Lock : déverrouille */

    oled_cmd(0xAE);                   /* Display OFF pendant la config */

    oled_cmd(0xB3); oled_dat(0x91);
    oled_cmd(0xCA); oled_dat(0x3F);   /* MUX ratio 64 (dual COM, 256x64) */
    oled_cmd(0xA2); oled_dat(OLED_ROW_OFFSET_BASE);   /* centrage vertical par défaut (adj=0) */
    oled_cmd(0xA1); oled_dat(0x00);

    oled_cmd(0xA0); oled_dat(0x14); oled_dat(0x11);   /* bit0=0 -> increment horizontal par défaut */

    oled_cmd(0xB5); oled_dat(0x00);
    oled_cmd(0xAB); oled_dat(0x01);   /* régulateur VDD interne activé */

    oled_cmd(0xB4); oled_dat(0xA0); oled_dat(0xFD);   /* VSL externe - réseau R56ohm+1 diode présent */

    oled_cmd(0xC1); oled_dat(0x9F);   /* contraste - à ajuster selon rendu */
    oled_cmd(0xC7); oled_dat(0x0F);

    oled_cmd(0xB9);                   /* Gray Scale Table linéaire par défaut */

    oled_cmd(0xB1); oled_dat(0xE2);
    oled_cmd(0xD1); oled_dat(0x82); oled_dat(0x20);

    oled_cmd(0xBB); oled_dat(0x1F);
    oled_cmd(0xB6); oled_dat(0x08);
    oled_cmd(0xBE); oled_dat(0x07);

    oled_cmd(0xA6);                   /* Normal display */
    oled_cmd(0xA9);                   /* Exit partial display */

    oled_cmd(0x15); oled_dat(OLED_COL_OFFSET); oled_dat(OLED_COL_OFFSET + 63);
    oled_cmd(0x75); oled_dat(0x00); oled_dat(LCD_YMAX);   /* fenêtre bornée aux 60 lignes réelles */

    oled_cmd(0x5C);
    for (uint32_t i = 0; i < 64u * 128u; i++)
        oled_dat(0x0f);   /* effacement RAM visible avant allumage */

    oled_cmd(0xAF);                   /* Display ON */
}

/* ---------------------------------------------------------------------
 * Réglage de centrage vertical - bloquant, hors chemin temps réel
 * --------------------------------------------------------------------- */
void oled_set_row_offset_adj(uint8_t adj)
{
    if (adj > 3) adj = 3;
    oled_row_offset_adj = adj;
    oled_cmd(0xA2);
    oled_dat(OLED_ROW_OFFSET_BASE + oled_row_offset_adj);
}

/* ---------------------------------------------------------------------
 * Envoi asynchrone en fonctionnement normal (DMA, non bloquant)
 * --------------------------------------------------------------------- */
void oled_send_reposition(uint8_t col_start_4px, uint8_t row, uint8_t inc_mode)
{
    if (oled_busy) return;
    oled_busy = 1;

    oled_reposition_buf[0] = (0u<<8) | 0x15;
    oled_reposition_buf[1] = (1u<<8) | (uint8_t)(OLED_COL_OFFSET + col_start_4px);
    oled_reposition_buf[2] = (0u<<8) | 0x75;
    oled_reposition_buf[3] = (1u<<8) | row;
    oled_reposition_buf[4] = (0u<<8) | 0xA0;
    oled_reposition_buf[5] = (1u<<8) | (inc_mode ? 0x15 : 0x14);
    oled_reposition_buf[6] = (0u<<8) | 0x5C;

    LL_DMA_SetDataLength(DMA1, LL_DMA_CHANNEL_4, 7);
    LL_DMA_SetMemoryAddress(DMA1, LL_DMA_CHANNEL_4, (uint32_t)oled_reposition_buf);
    LL_SPI_EnableDMAReq_TX(SPI1);
    LL_DMA_EnableChannel(DMA1, LL_DMA_CHANNEL_4);
}

void oled_send_pixels(uint8_t lcd_byte)
{
    if (oled_busy) return;
    oled_busy = 1;
    const uint8_t *lut_entry = oled_lut[lcd_byte];

    oled_data_buf[0] = (1u<<8) | lut_entry[0];
    oled_data_buf[1] = (1u<<8) | lut_entry[1];
    oled_data_buf[2] = (1u<<8) | lut_entry[2];
    oled_data_buf[3] = (1u<<8) | lut_entry[3];

    LL_DMA_SetDataLength(DMA1, LL_DMA_CHANNEL_4, 4);
    LL_DMA_SetMemoryAddress(DMA1, LL_DMA_CHANNEL_4, (uint32_t)oled_data_buf);
    LL_SPI_EnableDMAReq_TX(SPI1);
    LL_DMA_EnableChannel(DMA1, LL_DMA_CHANNEL_4);
}

void DMA1_Ch4_5_DMAMUX1_OVR_IRQHandler(void)
{
    if (LL_DMA_IsActiveFlag_TC4(DMA1)) {
        LL_DMA_ClearFlag_TC4(DMA1);
        LL_DMA_DisableChannel(DMA1, LL_DMA_CHANNEL_4);
        oled_busy = 0;
    }
}

/* ---------------------------------------------------------------------
 * API publique
 * --------------------------------------------------------------------- */
void oled_module_init(void)
{
#if OLED_LUT_IN_RAM
    oled_lut_init();
#endif
    oled_gpio_spi_init();
    oled_hw_init_sequence();
    oled_dma_init();
}
