#ifndef SPI_FLASH_H
#define SPI_FLASH_H

void flash_init(void);
void flash_program_via_stdio(void);
uint32_t flash_crc_all_table(void);

#endif