#include <stdio.h>
#include <assert.h>
#include <inttypes.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <termios.h>

uint8_t *lookup = NULL;

#define LOOKUP_TABLE_SIZE    74091948
#define BLOCK_SIZE           131072
#define PAGE_SIZE            2112
#define BLOCK_COUNT          566

static uint32_t table[256];

// 位逆转
static uint32_t bitrev(uint32_t input, int bw)
{
	int i;
	uint32_t var;
	var = 0;
	for(i=0;i<bw;i++)
	{
		if(input & 0x01)
		{
			var |= 1<<(bw-1-i);
		}
		input>>=1;
	}
	return var;
}

// 码表生成
// 如:X32+X26+...X1+1,poly=(1<<26)|...|(1<<1)|(1<<0)
void crc32_init(uint32_t poly)
{
	int i;
	int j;
	uint32_t c;
	
	poly=bitrev(poly,32);
	for(i=0; i<256; i++)
	{
		c = i;
		for (j=0; j<8; j++)
		{
			if(c&1)
			{
				c=poly^(c>>1);
			}
			else
			{
				c=c>>1;
			}
		}
		table[i] = c;
	}
}

uint32_t crc32(uint32_t crc, void* input, int len)
{
	int i;
	unsigned char index;
	unsigned char* pch;
	pch = (unsigned char*)input;
	for(i=0;i<len;i++)
	{
		index = (unsigned char)(crc^*pch);
		crc = (crc>>8)^table[index];
		pch++;
	}
	return crc;
}

int check_mcu(int mcu_fd)
{
	uint8_t buf[4] = {0xAA, 0x04, 0x00, 0x00};
	write(mcu_fd, buf, 4);
	uint8_t ret[1]={0};
	int len;
	len = read(mcu_fd, ret, 1);
	if(len != 1 || ret[0] != 0xAA) 
		return 0;
	len = read(mcu_fd, ret, 1);
	if(len != 1 || ret[0] != 0x0A) {
		printf("%x\n",ret[0]);
		return 0;
	}

	return 1;
}

int read_page_crc32(int mcu_fd, int page_addr, uint32_t *crc)
{
	uint8_t buf[4] = {0xAA, 0x00, page_addr>>8, page_addr&0xff};
	write(mcu_fd, buf, 4);
	
	uint8_t ret[1]={0};
	int len;
	len = read(mcu_fd, ret, 1);
	if(len != 1 || ret[0] != 0xAA)
		return 0;
	
	uint8_t tmp[1];
	len = read(mcu_fd, tmp, 1);
	*crc = tmp[0];
	*crc <<= 8;
	len += read(mcu_fd, tmp, 1);
	*crc |= tmp[0];
	*crc <<= 8;
	len += read(mcu_fd, tmp, 1);
	*crc |= tmp[0];
	*crc <<= 8;
	len += read(mcu_fd, tmp, 1);
	*crc |= tmp[0];
	if(len != 4) 
		return 0;

	return 1;
}

int write_page(int mcu_fd, int page_addr, uint8_t *in_buf)
{
	uint8_t buf[4] = {0xAA, 0x01, page_addr>>8, page_addr&0xff};
	write(mcu_fd, buf, 4);
	write(mcu_fd, in_buf, PAGE_SIZE);
	uint8_t ret[1]={0};
	int len;
	len = read(mcu_fd, ret, 1);
	if(len != 1 || ret[0] != 0xAA)
		return 0;
	len = read(mcu_fd, ret, 1);
	if(len != 1 || ret[0] != 0x01)
		return 0;
	return 1;
}
int erase_block(int mcu_fd, int page_addr)
{
	uint8_t buf[4] = {0xAA, 0x02, page_addr>>8, page_addr&0xff};
	write(mcu_fd, buf, 4);
	uint8_t ret[1]={0};
	int len;
	len = read(mcu_fd, ret, 1);
	if(len != 1 || ret[0] != 0xAA)
		return 0;
	len = read(mcu_fd, ret, 1);
	if(len != 1 || ret[0] != 0x01)
		return 0;
	return 1;
}

int program_one_block(int fd, int block, uint8_t *buf_128k, int length)
{
	uint8_t page_buffer[PAGE_SIZE];
	int succ;
	printf("Erase Block %d\n", block);
	succ = erase_block(fd, block * 64);
	if(!succ){
		printf("Fail.\n");
		return 0;
	}
	printf("Program Block %d ", block);
	int prog_len = length;
	for(int i=0; i<64; i++){
		for (int j = 0; j < PAGE_SIZE; j++)
		{
			page_buffer[j] = 0xFF;
		}
		int max = 2048; // 总共2112, 后64不处理
		if( prog_len < max){
			max = prog_len;
		}
		for(int j=0; j<max; j++){
			page_buffer[j] = buf_128k[2048 * i + j];
		}
		prog_len -= max;
		assert(prog_len >= 0);
		succ = write_page(fd, block * 64 + i, page_buffer);
		if(!succ){
			printf("Write %x Fail.\n", block * 64 + i);
			return 0;
		}
		printf(".");
		fflush(stdout);
	}
	printf("\n");
	printf("Verify Block CRC32 %d", block);
	prog_len = length;
	for(int i=0; i<64; i++){
		uint32_t flash_crc, crc;
		succ = read_page_crc32(fd, block * 64 + i, &flash_crc);
		if(!succ){
			printf("Read fail on page %x.\n", block * 64 + i);
			return 0;
		}
		for (int j = 0; j < PAGE_SIZE; j++)
		{
			page_buffer[j] = 0xFF;
		}
		int max = 2048;
		if(prog_len < max){
			max = prog_len;
		}
		for(int j=0; j<max; j++){
			page_buffer[j] = buf_128k[2048 * i + j];
		}
		prog_len -= max;
		assert(prog_len >= 0);

		crc = 0xFFFFFFFF;
		crc=crc32(crc, page_buffer, 2048);
		crc ^= 0xFFFFFFFF;
		//printf("-%d-%08X-\n", i, crc);
		if(crc != flash_crc){
			printf("Verify Error on page %x.\n", block * 64 + i);
			return 0;
		}
		printf(".");
		fflush(stdout);
	}
	printf("\n");
	return 1;
}

int main(int argc ,char **argv)
{
	crc32_init(0x4C11DB7);
	
	if(argc != 2){
		printf("Usage: %s /dev/ttyS0\n",argv[0]);
		return 1;
	}
	
	// 配置USB CDC设备，和串口的配置方式是一样的。
	int mcu_fd = open(argv[1], O_RDWR|O_NOCTTY);
	if(mcu_fd < 0){
		printf("can not open %s\n", argv[1]);
		return 1;
	}
	// https://www.cnblogs.com/Suzkfly/p/11055532.html
	struct termios termios_uart;
	memset(&termios_uart, 0, sizeof(termios_uart));
	cfsetspeed(&termios_uart, B1000000); /* 波特率 */
	termios_uart.c_cflag &= ~CSIZE;
	termios_uart.c_cflag |= CS8;      /* 数据位8 */
	termios_uart.c_cflag &= ~PARENB;
	termios_uart.c_iflag &= ~INPCK;   /* 禁能输入奇偶校验 */
	termios_uart.c_cflag &= ~CSTOPB;  /* 1个停止位 */
	termios_uart.c_cflag &= ~CRTSCTS; /* 无流控 */
	termios_uart.c_cflag |= CLOCAL;   /* 忽略modem（调制解调器）控制线 */
	termios_uart.c_cflag |= CREAD;    /* 使能接收 */
	termios_uart.c_oflag &= ~OPOST;   /* 禁能执行定义（implementation-defined）输出处理 */
	termios_uart.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG); /* 设置本地模式位原始模式 */
	termios_uart.c_cc[VTIME] = 10;   /* 设置等待时间，单位1/10秒 */
	termios_uart.c_cc[VMIN]  = 1;   /* 最少读取一个字符 */
	tcflush(mcu_fd, TCIFLUSH);      /* 清空读缓冲区 */
    if (tcsetattr(mcu_fd, TCSANOW, &termios_uart) == -1) {
        printf("tcsetattr failed\n");
    }
	
	// 判断和MCU的通信是否正常
	if(!check_mcu(mcu_fd)){
		printf("mcu not in SPI FLASH programming mode\n");
		return 1;
	}else{
		printf("mcu in SPI FLASH programming mode\n");
	}
		
	// 加载并且检查待刷写的固件
    int fd = open("lookup.dat", O_RDONLY);
	if(fd < 0){
		printf("can not open lookup.dat\n");
		return 1;
	}
    lookup = mmap(NULL, LOOKUP_TABLE_SIZE, PROT_READ, MAP_PRIVATE, fd, 0);
    uint32_t crc;
    crc32_init(0x4C11DB7);
    crc = 0xFFFFFFFF;
    crc=crc32(crc, lookup, LOOKUP_TABLE_SIZE);
	crc ^= 0xFFFFFFFF;
	printf("Check lookup.dat CRC32 = %08X\n", crc);
	if(crc != 0xA8093698){
		printf("CRC32 Error.\n");
		return 1;
	}
	// 
	//int succ = program_one_block(mcu_fd, 0, lookup + BLOCK_SIZE * 0, BLOCK_SIZE);
	//printf("%d\n",succ);
	int prog_len = LOOKUP_TABLE_SIZE;
	for(int i=0; i<BLOCK_COUNT; i++){
		int max = BLOCK_SIZE;
		if( prog_len < max){
			max = prog_len;
		}
		prog_len -= max;
		assert(prog_len >= 0);
		printf("%d, size=%d\n", i, max);
		int succ = program_one_block(mcu_fd, i, lookup + BLOCK_SIZE * i, max);
		if(!succ){
			printf("-------------FAIL-------------\n");
		}
	}
	/*
   	for(int i=0;i<256;i++){
		printf("0x%08XUL,",table[i]);
		if(i%8==7)printf("\n");
	}
	*/
    munmap(lookup, LOOKUP_TABLE_SIZE);
    close(fd);
	close(mcu_fd);
    return 0;
}
