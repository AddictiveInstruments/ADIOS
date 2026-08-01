#ifndef OLED_SSD1322_H
#define OLED_SSD1322_H

#include <stdint.h>

/* Initialise GPIO (CS/RST + SPI1/DMA), lance la séquence de config du
 * SSD1322 (bloquant, one-shot) et prépare les canaux DMA pour l'envoi
 * asynchrone en fonctionnement normal. A appeler une fois au démarrage. */
void oled_module_init(void);

/* Ajuste le centrage vertical (offset autour de la base neutre (64-60)/2=2),
 * adj de 0 à 3. Écriture bloquante, à appeler hors du chemin temps réel. */
void oled_set_row_offset_adj(uint8_t adj);

/* Repositionne le pointeur d'écriture du SSD1322 et reprogramme le sens
 * d'incrémentation matériel (inc_mode: 0=horizontal/X, 1=vertical/Y).
 * col_start_4px est exprimé en unités logiques 8px (coordonnée X du
 * protocole LCD) ; la conversion en unités colonne OLED (4px réels) et
 * l'offset module sont gérés en interne. Non bloquant : ignoré si un
 * envoi est déjà en cours. */
void oled_send_reposition(uint8_t col_start_4px, uint8_t row, uint8_t inc_mode);

/* Convertit et envoie un octet LCD (8 pixels 1-bit) en 4 octets OLED
 * (8 pixels 4-bit) à la position courante du pointeur SSD1322.
 * Non bloquant : ignoré si un envoi est déjà en cours. */
void oled_send_pixels(uint8_t lcd_byte);

#endif
