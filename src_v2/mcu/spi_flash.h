#ifndef SPI_FLASH_H
#define SPI_FLASH_H

#define BLOCK_SIZE 32

void flash_init(void);
void flash_program_via_stdio(void);
uint32_t flash_crc_all_table(void);
// 读取缓存数据，如果没有，从FLASH加载，addr必须是BLOCK_SIZE的整数倍
uint8_t *get_from_cache(uint32_t addr);
void do_flash_test(void);
#endif