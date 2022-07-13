#include <stdio.h>
#include <hardware/spi.h>
#include "spi_flash.h"
#include "pico/stdlib.h"

#define SPI0_TX  19
#define SPI0_SCK 18
#define SPI0_CSN 17
#define SPI0_RX  16

#define SPI_FLASH_FREQ (31250*1000) //125MHz/4

#define FLASH_DEVICE_RESET                              0xFF
#define FLASH_JEDEC_ID                                  0x9F
#define FLASH_READ_STATUS_REGISTER                      0x0F //或0x05
#define FLASH_WRITE_STATUS_REGISTER                     0x1F //或0x01
#define FLASH_STATUS_REG1_ADDR                          0xA0 //低四位可以为任意值
#define FLASH_STATUS_REG2_ADDR                          0xB0 //低四位可以为任意值
#define FLASH_STATUS_REG3_ADDR                          0xC0 //低四位可以为任意值
#define FLASH_WRITE_ENABLE                              0x06
#define FLASH_WRITE_DISABLE                             0x04
#define FLASH_BAD_BLOCK_MANAGEMENT                      0xA1
#define FLASH_READ_BBM_LUT                              0xA5
#define FLASH_LAST_ECC_FAILURE_PAGE_ADDR                0xA9
#define FLASH_BLOCK_ERASE                               0xD8
#define FLASH_PROGRAM_DATA_LOAD                         0x02
#define FLASH_RANDOM_PROGRAM_DATA_LOAD                  0x84
#define FLASH_QUAD_PROGRAM_DATA_LOAD                    0x32
#define FLASH_RANDOM_QUADPROGRAM_DATA_LOAD              0x34
#define FLASH_PROGRAM_ECECUTE                           0x10
#define FLASH_PAGE_DATA_READ                            0x13
#define FLASH_READ                                      0x03
#define FLASH_FAST_READ                                 0x0B
#define FLASH_FAST_READ_WITH_4BYTE_ADDR                 0x0C
#define FLASH_FAST_READ_DUAL_OUTPUT                     0x3B
#define FLASH_FAST_READ_DUAL_OUTPUT_WITH_4BYTE_ADDR     0x3C
#define FLASH_FAST_READ_QUAD_OUTPUT                     0x6B
#define FLASH_FAST_READ_QUAD_OUTPUT_WITH_4BYTE_ADDR     0x6C
#define FLASH_FAST_READ_DUAL_IO                         0xBB
#define FLASH_FAST_READ_DUAL_IO_WITH_4BYTE_ADDR         0xBC
#define FLASH_FAST_READ_QUAD_IO                         0xEB
#define FLASH_FAST_READ_QUAD_IO_WITH_4BYTE_ADDR         0xEC

// SR3状态寄存器标识
#define FLASH_SR_BUSY                                   0x01//芯片忙，正在进行写入或擦除等操作
#define FLASH_SR_WEL                                    0x02//可写标识位
#define FLASH_SR_ERASE_FAIL                             0x04//擦除失败
#define FLASH_SR_WRITE_FAIL                             0x08//编程（写入）失败
#define FLASH_SR_ECC_LESS_THAN_4BIT_ERR                 0x10//ECC校验有1-4bit的错误，已被纠正
#define FLASH_SR_ECC_MORE_THAN_4BIT_ERR                 0x20//ECC校验有多于4bit的错误，无法被纠正
#define FLASH_SR_ECC_DATA_DAMAGED                       0x30//每页都有多于4bit的错误，不建议继续使用
#define FLASH_SR_BBM_LUT_FULL                           0x40//坏块管理表已满

static const uint32_t crc_table[256] = {
    0x00000000UL,0x77073096UL,0xEE0E612CUL,0x990951BAUL,0x076DC419UL,0x706AF48FUL,0xE963A535UL,0x9E6495A3UL,
    0x0EDB8832UL,0x79DCB8A4UL,0xE0D5E91EUL,0x97D2D988UL,0x09B64C2BUL,0x7EB17CBDUL,0xE7B82D07UL,0x90BF1D91UL,
    0x1DB71064UL,0x6AB020F2UL,0xF3B97148UL,0x84BE41DEUL,0x1ADAD47DUL,0x6DDDE4EBUL,0xF4D4B551UL,0x83D385C7UL,
    0x136C9856UL,0x646BA8C0UL,0xFD62F97AUL,0x8A65C9ECUL,0x14015C4FUL,0x63066CD9UL,0xFA0F3D63UL,0x8D080DF5UL,
    0x3B6E20C8UL,0x4C69105EUL,0xD56041E4UL,0xA2677172UL,0x3C03E4D1UL,0x4B04D447UL,0xD20D85FDUL,0xA50AB56BUL,
    0x35B5A8FAUL,0x42B2986CUL,0xDBBBC9D6UL,0xACBCF940UL,0x32D86CE3UL,0x45DF5C75UL,0xDCD60DCFUL,0xABD13D59UL,
    0x26D930ACUL,0x51DE003AUL,0xC8D75180UL,0xBFD06116UL,0x21B4F4B5UL,0x56B3C423UL,0xCFBA9599UL,0xB8BDA50FUL,
    0x2802B89EUL,0x5F058808UL,0xC60CD9B2UL,0xB10BE924UL,0x2F6F7C87UL,0x58684C11UL,0xC1611DABUL,0xB6662D3DUL,
    0x76DC4190UL,0x01DB7106UL,0x98D220BCUL,0xEFD5102AUL,0x71B18589UL,0x06B6B51FUL,0x9FBFE4A5UL,0xE8B8D433UL,
    0x7807C9A2UL,0x0F00F934UL,0x9609A88EUL,0xE10E9818UL,0x7F6A0DBBUL,0x086D3D2DUL,0x91646C97UL,0xE6635C01UL,
    0x6B6B51F4UL,0x1C6C6162UL,0x856530D8UL,0xF262004EUL,0x6C0695EDUL,0x1B01A57BUL,0x8208F4C1UL,0xF50FC457UL,
    0x65B0D9C6UL,0x12B7E950UL,0x8BBEB8EAUL,0xFCB9887CUL,0x62DD1DDFUL,0x15DA2D49UL,0x8CD37CF3UL,0xFBD44C65UL,
    0x4DB26158UL,0x3AB551CEUL,0xA3BC0074UL,0xD4BB30E2UL,0x4ADFA541UL,0x3DD895D7UL,0xA4D1C46DUL,0xD3D6F4FBUL,
    0x4369E96AUL,0x346ED9FCUL,0xAD678846UL,0xDA60B8D0UL,0x44042D73UL,0x33031DE5UL,0xAA0A4C5FUL,0xDD0D7CC9UL,
    0x5005713CUL,0x270241AAUL,0xBE0B1010UL,0xC90C2086UL,0x5768B525UL,0x206F85B3UL,0xB966D409UL,0xCE61E49FUL,
    0x5EDEF90EUL,0x29D9C998UL,0xB0D09822UL,0xC7D7A8B4UL,0x59B33D17UL,0x2EB40D81UL,0xB7BD5C3BUL,0xC0BA6CADUL,
    0xEDB88320UL,0x9ABFB3B6UL,0x03B6E20CUL,0x74B1D29AUL,0xEAD54739UL,0x9DD277AFUL,0x04DB2615UL,0x73DC1683UL,
    0xE3630B12UL,0x94643B84UL,0x0D6D6A3EUL,0x7A6A5AA8UL,0xE40ECF0BUL,0x9309FF9DUL,0x0A00AE27UL,0x7D079EB1UL,
    0xF00F9344UL,0x8708A3D2UL,0x1E01F268UL,0x6906C2FEUL,0xF762575DUL,0x806567CBUL,0x196C3671UL,0x6E6B06E7UL,
    0xFED41B76UL,0x89D32BE0UL,0x10DA7A5AUL,0x67DD4ACCUL,0xF9B9DF6FUL,0x8EBEEFF9UL,0x17B7BE43UL,0x60B08ED5UL,
    0xD6D6A3E8UL,0xA1D1937EUL,0x38D8C2C4UL,0x4FDFF252UL,0xD1BB67F1UL,0xA6BC5767UL,0x3FB506DDUL,0x48B2364BUL,
    0xD80D2BDAUL,0xAF0A1B4CUL,0x36034AF6UL,0x41047A60UL,0xDF60EFC3UL,0xA867DF55UL,0x316E8EEFUL,0x4669BE79UL,
    0xCB61B38CUL,0xBC66831AUL,0x256FD2A0UL,0x5268E236UL,0xCC0C7795UL,0xBB0B4703UL,0x220216B9UL,0x5505262FUL,
    0xC5BA3BBEUL,0xB2BD0B28UL,0x2BB45A92UL,0x5CB36A04UL,0xC2D7FFA7UL,0xB5D0CF31UL,0x2CD99E8BUL,0x5BDEAE1DUL,
    0x9B64C2B0UL,0xEC63F226UL,0x756AA39CUL,0x026D930AUL,0x9C0906A9UL,0xEB0E363FUL,0x72076785UL,0x05005713UL,
    0x95BF4A82UL,0xE2B87A14UL,0x7BB12BAEUL,0x0CB61B38UL,0x92D28E9BUL,0xE5D5BE0DUL,0x7CDCEFB7UL,0x0BDBDF21UL,
    0x86D3D2D4UL,0xF1D4E242UL,0x68DDB3F8UL,0x1FDA836EUL,0x81BE16CDUL,0xF6B9265BUL,0x6FB077E1UL,0x18B74777UL,
    0x88085AE6UL,0xFF0F6A70UL,0x66063BCAUL,0x11010B5CUL,0x8F659EFFUL,0xF862AE69UL,0x616BFFD3UL,0x166CCF45UL,
    0xA00AE278UL,0xD70DD2EEUL,0x4E048354UL,0x3903B3C2UL,0xA7672661UL,0xD06016F7UL,0x4969474DUL,0x3E6E77DBUL,
    0xAED16A4AUL,0xD9D65ADCUL,0x40DF0B66UL,0x37D83BF0UL,0xA9BCAE53UL,0xDEBB9EC5UL,0x47B2CF7FUL,0x30B5FFE9UL,
    0xBDBDF21CUL,0xCABAC28AUL,0x53B39330UL,0x24B4A3A6UL,0xBAD03605UL,0xCDD70693UL,0x54DE5729UL,0x23D967BFUL,
    0xB3667A2EUL,0xC4614AB8UL,0x5D681B02UL,0x2A6F2B94UL,0xB40BBE37UL,0xC30C8EA1UL,0x5A05DF1BUL,0x2D02EF8DUL,
};


static inline void cs_select(void) {
    asm volatile("nop \n nop \n nop");
    gpio_put(SPI0_CSN, 0);
    asm volatile("nop \n nop \n nop");
}

static inline void cs_deselect(void) {
    asm volatile("nop \n nop \n nop");
    gpio_put(SPI0_CSN, 1);
    asm volatile("nop \n nop \n nop");
}

// 验证器件的ID是否正确
static bool flash_read_device_id(void) 
{
    cs_select();
    //  Read JEDEC ID (9Fh) 
    uint8_t cmdbuf[2] = {FLASH_JEDEC_ID, 0};
    uint8_t buf[3];
    spi_write_blocking(spi0, cmdbuf, sizeof(cmdbuf));
    spi_read_blocking(spi0, 0, buf, 3);
    cs_deselect();
    // ID should 0xef 0xaa 0x21
    return buf[0] == 0xef && buf[1] == 0xaa && buf[2] == 0x21;
}


// 等待busy变为0,并且返回sr3
static uint8_t flash_busy_wait(void)
{
    uint8_t sr3[1];
    uint8_t cmdbuf[2] = {FLASH_READ_STATUS_REGISTER, FLASH_STATUS_REG3_ADDR};
    cs_select();
    spi_write_blocking(spi0, cmdbuf, sizeof(cmdbuf));
    do{
        spi_read_blocking(spi0, 0, sr3, 1);
    }while(sr3[0] & FLASH_SR_BUSY);
    cs_deselect();
    return sr3[0];
}
// block_address 0 - 1023
// 这次从X宝买的FLASH似乎是翻新的，0x000地址有数据，0x800全都是0xff，无法判断坏快。
// 实测前70MB无坏快，暂不编写坏快处理程序
/*
uint8_t flash_bad_block_scan(uint16_t block_address)
{
    block_address <<= 6;
    uint8_t cmdbuf_1[4] = {FLASH_PAGE_DATA_READ, 0, block_address>>8, block_address&0xff};
    cs_select();
    spi_write_blocking(spi0, cmdbuf_1, sizeof(cmdbuf_1));
    cs_deselect();
    flash_busy_wait(); // about 60us with ECC, 25us without ECC 

    uint8_t cmdbuf_2[4] = {FLASH_READ, 0x08, 0x00, 0};
    uint8_t block_first_byte;
    cs_select();
    spi_write_blocking(spi0, cmdbuf_2, sizeof(cmdbuf_2));
    spi_read_blocking(spi0, 0, &block_first_byte, 1);
    cs_deselect();
    //printf("%x\n",block_first_byte);
    return block_first_byte != 0xff;
}
void flash_dump_bbm(void) 
{
    cs_select();
    uint8_t cmdbuf[2] = {FLASH_READ_BBM_LUT, 0};
    uint8_t buf[80];
    spi_write_blocking(spi0, cmdbuf, sizeof(cmdbuf));
    spi_read_blocking(spi0, 0, buf, 80);
    cs_deselect();
    for(int i=0;i<80;i+=4){
        printf("%02x%02x -> %02x%02x\n",buf[i],buf[i+1],buf[i+2],buf[i+3]);
    }
}
*/
// page_address 0 - 65535

static void flash_write_sr(uint8_t addr, uint8_t date)
{
    uint8_t cmdbuf[3] = {FLASH_WRITE_STATUS_REGISTER, addr, date};
    cs_select();
    spi_write_blocking(spi0, cmdbuf, sizeof(cmdbuf));
    cs_deselect();
}

static uint8_t flash_read_sr(uint8_t addr)
{
    uint8_t tmp[1];
    uint8_t cmdbuf[2] = {FLASH_READ_STATUS_REGISTER, addr};
    cs_select();
    spi_write_blocking(spi0, cmdbuf, sizeof(cmdbuf));
    spi_read_blocking(spi0, 0, tmp, 1);
    cs_deselect();
    return tmp[0];
}

void flash_init(void)
{
    // Enable SPI 0 at 31.25 MHz and connect to GPIOs
    spi_init(spi0, SPI_FLASH_FREQ);
    //printf("spi_frep=%d\n",spi_set_baudrate(spi0, SPI_FLASH_FREQ));
    gpio_set_function(SPI0_RX, GPIO_FUNC_SPI);
    gpio_set_function(SPI0_SCK, GPIO_FUNC_SPI);
    gpio_set_function(SPI0_TX, GPIO_FUNC_SPI);
    // Chip select is active-low, so we'll initialise it to a driven-high state
    gpio_init(SPI0_CSN);
    gpio_put(SPI0_CSN, 1);
    gpio_set_dir(SPI0_CSN, GPIO_OUT);
    // 检查ID
    if(!flash_read_device_id()){
        printf("SPI FLASH ID ERROR.\n");
        while(1);
    }
    // 禁用ECC，切换buffer模式
    flash_write_sr(FLASH_STATUS_REG2_ADDR, 0x08);
    // 解除写保护
    flash_write_sr(FLASH_STATUS_REG1_ADDR, 0x00);
}

static void flash_write_enable(void)
{
    uint8_t cmdbuf[1] = {FLASH_WRITE_ENABLE};
    cs_select();
    spi_write_blocking(spi0, cmdbuf, sizeof(cmdbuf));
    cs_deselect();
}

// 32 Byte Read
static void flash_read_32_byte(uint32_t address, uint8_t *buf)
{
    
    uint16_t page_address = address >> 11;
    uint16_t data_address = address & 0x7FF;
    // 如果和上次读取的页一样，不需要重新读。上电自动加载page 0
    static uint16_t old_page_addrdress = 0;
    if(old_page_addrdress != page_address){
        //printf("FLASH_PAGE_DATA_READ.\n");
        old_page_addrdress = page_address;
        uint8_t cmdbuf_1[4] = {FLASH_PAGE_DATA_READ, 0, page_address>>8, page_address&0xff};
        cs_select();
        spi_write_blocking(spi0, cmdbuf_1, sizeof(cmdbuf_1));
        cs_deselect();
        flash_busy_wait(); // <60us with ECC, <25us without ECC 
    }
    // Read 32 Byte
    uint8_t cmdbuf_2[4] = {FLASH_READ, data_address>>8, data_address&0xff, 0};
    cs_select();    
    spi_write_blocking(spi0, cmdbuf_2, sizeof(cmdbuf_2));
    spi_read_blocking(spi0, 0, buf, 32);
    cs_deselect();
}

//int _read(int handle, char *buffer, int length); 

// FLASH刷写
// PC 控制执行 擦除->查空->写入->校验
void flash_program_via_stdio(void) 
{
    // 使用stdio时，配置二进制模式，禁用CRLF转换，否则0x0A会变成0x0D
    //stdio_set_translate_crlf(&stdio_usb, false);
    while(1)
    {
        uint8_t start = uart_getc(uart0);
        if(start != 0xAA){
            continue;
        }
        uint8_t cmd = uart_getc(uart0);
        uint8_t addr_h = uart_getc(uart0);
        uint8_t addr_l = uart_getc(uart0);
        if(cmd == 0){
            // read:  PC [0xAA 0x00 地址高 地址低] MCU [0xAA + 32bit CRC]
            // 13h
            uint8_t cmdbuf_1[4] = {FLASH_PAGE_DATA_READ, 0, addr_h, addr_l};
            cs_select();
            spi_write_blocking(spi0, cmdbuf_1, sizeof(cmdbuf_1));
            cs_deselect();
            // wait <60us with ECC, <25us without ECC 
            flash_busy_wait(); 
            // read data 03h
            uint8_t cmdbuf_2[4] = {FLASH_READ, 0x00, 0x00, 0};
            cs_select();
            spi_write_blocking(spi0, cmdbuf_2, sizeof(cmdbuf_2));
            uint32_t crc = 0xFFFFFFFFUL;
            for(int i = 0; i < 2048; i ++){
                uint8_t tmp[1];
                spi_read_blocking(spi0, 0, tmp, 1);
		        unsigned char index = (unsigned char)(crc^tmp[0]);
		        crc = (crc>>8)^crc_table[index];
            }
            cs_deselect();
            crc ^= 0xFFFFFFFFUL;
            uart_putc_raw(uart0, 0xAA);
            uart_putc_raw(uart0, crc>>24);
            uart_putc_raw(uart0, (crc>>16) & 0xff);
            uart_putc_raw(uart0, (crc>>8) & 0xff);
            uart_putc_raw(uart0, crc & 0xff);
        }else if(cmd == 1){
            // write: PC [0xAA 0x01 地址高 地址低 + 2112 byte date] MCU [0xAA 0x01]
            // 第1步，使能FLASH写入，写入完成后会自动禁用
            flash_write_enable();
            // 第2步，写缓冲区 02h
            uint8_t cmdbuf[3] = {FLASH_PROGRAM_DATA_LOAD, 0, 0};
            cs_select();
            spi_write_blocking(spi0, cmdbuf, sizeof(cmdbuf));
            for(int i = 0; i < 2112; i++)
            {
                uint8_t tmp[1];
                tmp[0] = uart_getc(uart0);
                spi_write_blocking(spi0, tmp, 1);
            }
            cs_deselect();
            // 第3步，写入FLASH 10h
            uint8_t cmdbuf2[4] = {FLASH_PROGRAM_ECECUTE, 0, addr_h, addr_l};
            cs_select();
            spi_write_blocking(spi0, cmdbuf2, sizeof(cmdbuf2));
            cs_deselect();
            int flag = flash_busy_wait();
            uint8_t succ = !(flag & FLASH_SR_WRITE_FAIL);
            uart_putc_raw(uart0, 0xAA);
            uart_putc_raw(uart0, succ);
        }else if(cmd == 2){
            // Erase: PC [0xAA 0x02 地址高 地址低] MCU [0xAA 0x01]
            flash_write_enable();
            // D8h
            uint8_t cmdbuf[4] = {FLASH_BLOCK_ERASE, 0, addr_h, addr_l};
            cs_select();
            spi_write_blocking(spi0, cmdbuf, sizeof(cmdbuf));
            cs_deselect();
            int flag = flash_busy_wait();
            uint8_t succ = !(flag & FLASH_SR_ERASE_FAIL);
            uart_putc_raw(uart0, 0xAA);
            uart_putc_raw(uart0, succ);
        }else if(cmd == 3){
            // Erase: PC [0xAA 0x02 地址高 地址低] MCU [0xAA 0x01]
            flash_write_enable();
            // D8h
            uint8_t cmdbuf[4] = {FLASH_BLOCK_ERASE, 0, addr_h, addr_l};
            cs_select();
            spi_write_blocking(spi0, cmdbuf, sizeof(cmdbuf));
            cs_deselect();
            int flag = flash_busy_wait();
            uint8_t succ = !(flag & FLASH_SR_ERASE_FAIL);
            uart_putc_raw(uart0, 0xAA);
            uart_putc_raw(uart0, succ);
        }else if(cmd == 4){
            // TEST : PC [0xAA 0x04 地址高 地址低] MCU [0xAA 0x11]
            uart_putc_raw(uart0, 0xAA);
            uart_putc_raw(uart0, 0x0A);
        }
    }
}

#define LOOKUP_TABLE_SIZE    74091948
uint32_t flash_crc_all_table(void)
{
    uint32_t crc = 0xFFFFFFFFUL;
    int prog_len = LOOKUP_TABLE_SIZE;
	for(unsigned int i=0; i<LOOKUP_TABLE_SIZE/2048+1; i++)
    {
		int max = 2048;
		if(prog_len < max){
			max = prog_len;
		}
		prog_len -= max;
        //
        uint8_t cmdbuf_1[4] = {FLASH_PAGE_DATA_READ, 0, i >> 8, i & 0xff};
        cs_select();
        spi_write_blocking(spi0, cmdbuf_1, sizeof(cmdbuf_1));
        cs_deselect();
        // wait <60us with ECC, <25us without ECC 
        flash_busy_wait(); 
        // read data 03h
        uint8_t cmdbuf_2[4] = {FLASH_READ, 0x00, 0x00, 0};
        cs_select();
        spi_write_blocking(spi0, cmdbuf_2, sizeof(cmdbuf_2));
        for(int i = 0; i < max; i ++){
            uint8_t tmp[1];
            spi_read_blocking(spi0, 0, tmp, 1);
            unsigned char index = (unsigned char)(crc^tmp[0]);
            crc = (crc>>8)^crc_table[index];
        }
        cs_deselect();
        if(i%1024==0){
            printf("\n%d%%", i*100/(LOOKUP_TABLE_SIZE/2048+1));
        }
        if(i%64==0){
            printf(".");
            fflush(stdout);
        }
        
    }
    crc ^= 0xFFFFFFFFUL;
    printf("\nCRC32 = %08X\n", crc);
    return crc;
}

// ------------------------------ 32 Byte × 4096 缓存 ---------------------------------- //
//#define BLOCK_SIZE 32
#define BLOCK_NUM  4096 // 4096
#define HASH_TABLE_SIZE  8192 // 乘法除法，用2^n可以提高效率，编译器会用移位操作优化
#define HASH(x) ( (x) % HASH_TABLE_SIZE )
typedef struct node_struct node_t;
struct node_struct{
    uint8_t data[BLOCK_SIZE];
    uint32_t key;
    node_t  *p;
    node_t  *n;
    node_t  *hash_next;    
};

// Use 224KB RAM
typedef struct lru_struct{
    node_t pool[BLOCK_NUM];
    node_t *head;
    node_t *end;
    node_t *hash_table[HASH_TABLE_SIZE];
    uint32_t count;
} lru_t;

lru_t lru = {0};

static void hash_add(node_t *new)
{
    uint32_t hash_val = HASH(new -> key);
    if(lru.hash_table[hash_val] == 0){
        lru.hash_table[hash_val] = new;
        new -> hash_next = 0;
    }else{
        // 如果存在hash重复
        node_t *ptr = lru.hash_table[hash_val];
        while(ptr -> hash_next){
            ptr = ptr -> hash_next;
        }
        ptr -> hash_next = new;
    }
    new -> hash_next = 0;
}

static void hash_remove(int key)
{
    node_t *remove = 0;
    node_t *before_remove = 0;
    uint32_t hash_value = HASH(key);
    node_t *ptr = lru.hash_table[hash_value];
    while(ptr){
        if(ptr->key == key){
            remove = ptr;
            break;
        }
        before_remove = ptr;
        ptr = ptr->hash_next;
    }
    if(before_remove){
        before_remove -> hash_next = remove -> hash_next;
    }else{
        lru.hash_table[hash_value] = remove -> hash_next;
    }
    remove -> hash_next = 0;
}

static uint8_t *lru_insert(uint32_t key, uint32_t data_src_addr)
{
    node_t *new;
    if(lru.count < BLOCK_NUM){
        // 当Cache未满时，新的数据项只需插到双链表头部即可。
        new = &lru.pool[lru.count++];
    }else{
        // 当Cache已满时，将新的数据项插到双链表头部，并删除双链表的尾结点即可。
        // 从hash表移除
        hash_remove(lru.end -> key);
        // 修改删除的尾结点的数据，作为新的数据项使用
        new = lru.end;
        lru.end = new -> p;
        lru.end -> n = 0;
    }
    // 将新的数据项插到双链表头部
    new -> key = key;
    flash_read_32_byte(data_src_addr, new -> data);// 从FLASH加载
    new -> n = lru.head;
    new -> p = 0;
    if(lru.head){
        lru.head -> p = new;
    }else{
        lru.end = new;
    }
    lru.head = new;
    // 更新hash
    hash_add(new);
    return new -> data;
}

static uint8_t *lru_lookup(uint32_t key)
{
    node_t *new = 0;
    node_t *ptr = lru.hash_table[HASH(key)];
    while(ptr){
        if(ptr->key == key){
            new = ptr;
            break;
        }
        ptr = ptr->hash_next;
    }
    if(new){
        // 每次数据项被查询到时，都将此数据项移动到链表头部。
        // 删除数据,重新链接前后元素
        node_t *node_p = new -> p;
        node_t *node_n = new -> n;
        if(node_p){
            node_p -> n = node_n;
        }else{
            lru.head = node_n;
        }
        if(node_n){
            node_n -> p = node_p;
        }else{
            lru.end = node_p;
        }
        // 将数据项插到双链表头部,data不变，只修改位置
        new -> n = lru.head;
        new -> p = 0;
        if(lru.head){
            lru.head -> p = new;
        }else{
            lru.end = new;
        }
        lru.head = new;
        return new -> data;
    }else{
        return 0;
    }
}

//int debug_find = 0,debug_not_find=0;

// 读取缓存数据，如果没有，从FLASH加载，addr必须是BLOCK_SIZE的整数倍
uint8_t *get_from_cache(uint32_t addr)
{
    int key = addr / BLOCK_SIZE;
    uint8_t *ret = lru_lookup(key);
    if(!ret){
        ret = lru_insert(key, key * BLOCK_SIZE);
    }
    return ret;
}

/*
void test_case(uint32_t x)
{
    uint8_t *buf;
    absolute_time_t t1 = get_absolute_time();
    buf=get_from_cache(x);
    absolute_time_t t2 = get_absolute_time();
    printf("%x(%dus): ", x, (int)absolute_time_diff_us(t1, t2));
    for(int i = 0; i < 32; i++){
        printf("%02x ",buf[i]);
    }
    printf("\n");
}
void do_flash_test(void){
    test_case(0);
    test_case(0x6f34dc/32*32);
    test_case(0x9e07c/32*32);
}
*/