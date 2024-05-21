#include <stdio.h>
#include <inttypes.h>
#include <string.h>
#include "solve.h"
#include "spi_flash.h"

//                           2-----------2------------1
//                           | U1(0)   U2(1)   U3(2)  |
//                           |                        |
//                           3 U4(3)   U5(4)   U6(5)  1
//                           |                        |
//                           | U7(6)   U8(7)   U9(8)  |
//  2-----------3------------3-----------0------------0-----------1------------1------------2------------2
//  | L1(36)  L2(37)  L3(38) | F1(18)  F2(19)  F3(20) | R1(9)   R2(10)  R3(11) |  B1(45)  B2(46)  B3(47) |
//  |                        |                        |                        |                         |
// 11 L4(39)  L5(40)  L6(41) 9 F4(21)  F5(22)  F6(23) 8 R4(12)  R5(13)  R6(14) 10 B4(48)  B5(49)  B6(50) 11
//  |                        |                        |                        |                         |
//  | L7(42)  L8(43)  L9(44) | F7(24)  F8(25)  F9(26) | R7(15)  R8(16)  R9(17) |  B7(51)  B8(52)  B9(53) |
//  3-----------7------------5-----------4------------4-----------5------------7------------6------------3
//                           | D1(27)  D2(28)  D3(29) |
//                           |                        |
//                           7 D4(30)  D5(31)  D6(32) 5
//                           |                        |
//                           | D7(33)  D8(34)  D9(35) |
//                           6-----------6------------7

uint8_t *lookup = NULL;

// 寻找小于等于STEP_LIMIT的解
// 使用树莓派4时的性能（time ./solve > log.txt）
// STEP_LIMIT=24 P2_STEP_LIMIT_MAX=11 测试10000个随机打乱的魔方，最大24步, 平均21.68步，耗时6.824s
// STEP_LIMIT=23 P2_STEP_LIMIT_MAX=11 测试10000个随机打乱的魔方，最大23步, 平均21.68步，耗时6.836s
// STEP_LIMIT=22 P2_STEP_LIMIT_MAX=10 测试10000个随机打乱的魔方，最大22步, 平均21.21步，耗时11.405s
// STEP_LIMIT=21 P2_STEP_LIMIT_MAX=10 测试10000个随机打乱的魔方，最大21步, 平均20.73步，耗时1m17.598s
// STEP_LIMIT=20 P2_STEP_LIMIT_MAX=10 测试10000个随机打乱的魔方，最大20步, 耗时82分53秒

// 使用树莓派RP2040时的性能(2024-03-28优化前)
// 使用cube_robot.c里的debug_solve()测试
// STEP_LIMIT=23 P2_STEP_LIMIT_MAX=11 测试50个随机打乱的魔方，耗时9123ms

// 使用树莓派RP2040时的性能(2024-03-28优化后)
// 使用cube_robot.c里的debug_solve()测试
// STEP_LIMIT=23 P2_STEP_LIMIT_MAX=11 测试50个随机打乱的魔方，耗时8982ms

/*
优化前
Find 21 step solution in 98ms: U B F2 D' R B F' R2 D2 L' F2 D' L2 D U2 F2 D U2 B2 R2 L2 
Find 21 step solution in 76ms: D B D' R' D F2 U R' B F' L2 D2 F2 D' F2 D2 F2 R2 D' B2 L2 
Find 22 step solution in 297ms: B2 L2 D2 L U R F' D2 R' L B2 F2 R2 D F2 D2 U F2 U R2 B2 U 
Find 22 step solution in 246ms: F D2 U B D' B' L' F U L' U2 R2 D' U2 B2 U2 B2 L2 U R2 B2 D 
Find 23 step solution in 837ms: F L F2 R' F2 U L D L F R2 B2 D2 U' R2 B2 U F2 L2 D' L2 D' R2 
Find 21 step solution in 189ms: R F' L2 D' U L' D' F D' B L2 D' U' R2 F2 D F2 D2 L2 F2 D 
Find 22 step solution in 137ms: B2 R2 U' B R2 D' U' B L D R' L2 F2 U' L2 D L2 U2 L2 B2 L2 F2 
Find 23 step solution in 832ms: F R L' B U2 R' L' U B R F D2 R2 B2 U2 F2 L2 D U2 R2 D' R2 F2 
Find 22 step solution in 35ms: F2 R2 D B L2 F' U' L D R D2 L2 F2 D' B2 D2 U' R2 U F2 U B2 
Find 23 step solution in 756ms: F2 U B D' B2 L F2 L D U2 L F2 L2 U F2 R2 D' U' L2 U' R2 D R2 
Find 23 step solution in 304ms: B2 L2 U F2 R' D F U' B' F L D2 F2 D' L2 F2 D U2 R2 U2 B2 R2 L2 
Find 22 step solution in 142ms: D' F L2 B2 U' R B2 F U' L' F2 D2 B2 R2 D' R2 D' R2 L2 D R2 U2 
Find 21 step solution in 98ms: R2 L F2 R B D B U' L' F2 D' B2 R2 U L2 B2 U' F2 D' F2 D 
Find 21 step solution in 58ms: F D2 U' F' R2 B' L D' B' L2 D' U2 R2 D' F2 U B2 L2 U' B2 F2 
Find 22 step solution in 187ms: F2 D R' B2 R D B' R B L' F2 D L2 U2 B2 U2 F2 L2 F2 U' R2 F2 
Find 21 step solution in 71ms: U' L2 D R2 B' D2 R2 U L D2 U' L2 U B2 D2 U F2 R2 U' B2 L2 
Find 21 step solution in 432ms: F' U' L' F L2 U F' D2 L' D' B' F2 U2 B2 L2 F2 D L2 F2 U L2 
Find 21 step solution in 192ms: B R U' F R U2 R2 F' L2 F' R' L2 D U2 R2 D2 B2 R2 B2 U' R2 
Find 23 step solution in 257ms: F2 D' F U' R B R' U F2 U' L' B2 D U F2 L2 F2 D' F2 U R2 U B2 
Find 22 step solution in 636ms: F' L B' L' D' L' F' D U L2 F' R2 U R2 U' B2 U' B2 D B2 R2 L2 
Find 22 step solution in 59ms: D2 U' R2 F' L' B' U B2 D F' R2 F2 U' L2 B2 D B2 D2 U R2 B2 D 
Find 20 step solution in 47ms: B' D2 U R' D2 B' F' R' F' R2 F2 D U F2 D B2 R2 F2 D' F2 
Find 21 step solution in 34ms: B2 L2 F U B' F L D' R L B2 F2 U2 R2 D' B2 D' R2 B2 U' F2 
Find 20 step solution in 53ms: B2 D' U' F L F' R' F U L' U2 R2 B2 D2 U' F2 D L2 U L2 
Find 22 step solution in 279ms: B' U' F' D' R F' R2 L' D R B F2 D' B2 U2 R2 F2 L2 D L2 U' F2 
Find 22 step solution in 36ms: B2 D2 R D2 U' F' D2 B' U2 L D2 F2 D R2 B2 D' B2 U' F2 D2 L2 D2 
Find 22 step solution in 72ms: B D U L B2 U' B R D2 F' U2 R2 D2 B2 U B2 L2 F2 U R2 U' L2 
Find 23 step solution in 198ms: B2 D2 L2 F' R' U' R B' F2 L2 F D2 F2 R2 U' L2 D2 U F2 U' L2 U2 R2 
Find 23 step solution in 91ms: B2 L' D' F R L2 D2 R2 F D' L' D F2 D2 U' L2 F2 D2 B2 D L2 U' F2 
Find 22 step solution in 67ms: U' L F U' B U B2 U' R F' R2 L2 F2 R2 U B2 D2 U R2 U F2 R2 
Find 21 step solution in 96ms: B2 F L' F2 R' F D2 R' F U2 R2 L2 U' L2 B2 D F2 D U2 L2 B2 
Find 20 step solution in 87ms: D F' D U' R D' F2 R2 U B D2 L2 D B2 D' R2 B2 D' B2 U 
Find 21 step solution in 292ms: B' D2 R B' L' D' R2 F' L2 F2 D2 B2 U' L2 U2 B2 U F2 D' L2 D 
Find 22 step solution in 113ms: D2 B U R' L2 U2 L' U L B' R2 D' R2 D2 U' F2 U' B2 F2 U2 L2 D' 
Find 20 step solution in 103ms: B F' L' B2 L U2 B' F' U F L D B2 D B2 F2 R2 D B2 D 
Find 23 step solution in 110ms: B' U' R U R2 F' L' F2 U2 R F D' B2 R2 B2 F2 R2 D F2 D2 F2 L2 B2 
Find 22 step solution in 175ms: B2 U B2 L' B F' U' F2 D2 U2 L' F2 D L2 F2 D' B2 U' F2 U' F2 U2 
Find 20 step solution in 53ms: U2 R' B2 U2 B' R L U F' R2 D' U2 F2 L2 D' U' F2 U R2 B2 
Find 21 step solution in 37ms: D B2 R' D2 R U' L' B' F D B2 D' U2 B2 U L2 U R2 L2 D F2 
Find 20 step solution in 25ms: U2 B' U R U B D' U2 R' B2 L2 F2 D2 U R2 D R2 D' F2 D' 
Find 22 step solution in 74ms: B2 R2 L D' R' L U' F' D2 B' U' R2 L2 U B2 D U2 F2 R2 L2 U2 F2 
Find 22 step solution in 161ms: L2 U' R' L' B' F' R2 D R' B R2 D' R2 B2 U2 F2 U2 R2 D R2 D F2 
Find 21 step solution in 77ms: B2 U L' F U2 R' F' D' L U' R' U B2 U' R2 F2 L2 F2 D2 F2 D2 
Find 21 step solution in 125ms: B D R' U' B F D2 B R F L2 D' U2 F2 U' L2 U' L2 D2 R2 L2 
Find 21 step solution in 65ms: U B' U' B U2 R D' U L F' D2 U2 F2 D' F2 L2 U F2 L2 U B2 
Find 21 step solution in 122ms: U2 R' L' B2 U L D R B L' U2 F2 L2 B2 R2 D' R2 L2 B2 U B2 
Find 23 step solution in 91ms: B' D U2 L F U L' B F' U R' L2 B2 U2 R2 D' B2 U2 F2 U' B2 D F2 
Find 22 step solution in 267ms: B R' B' L' D' B2 L2 F' D B R2 D2 F2 R2 L2 D B2 L2 D' F2 U F2 
Find 22 step solution in 88ms: B D2 L2 D R' U2 B' F' D B' D L2 D R2 L2 B2 D2 U R2 U' F2 U 
Find 22 step solution in 146ms: L2 F U L2 U' R B' D L F' L2 U L2 B2 R2 U2 F2 D B2 R2 D' B2 
totel time cost 9123ms

优化后
Find 21 step solution in 97ms: U B F2 D' R B F' R2 D2 L' F2 D' L2 D U2 F2 D U2 B2 R2 L2 
Find 21 step solution in 75ms: D B D' R' D F2 U R' B F' L2 D2 F2 D' F2 D2 F2 R2 D' B2 L2 
Find 22 step solution in 292ms: B2 L2 D2 L U R F' D2 R' L B2 F2 R2 D F2 D2 U F2 U R2 B2 U 
Find 22 step solution in 242ms: F D2 U B D' B' L' F U L' U2 R2 D' U2 B2 U2 B2 L2 U R2 B2 D 
Find 23 step solution in 823ms: F L F2 R' F2 U L D L F R2 B2 D2 U' R2 B2 U F2 L2 D' L2 D' R2 
Find 21 step solution in 187ms: R F' L2 D' U L' D' F D' B L2 D' U' R2 F2 D F2 D2 L2 F2 D 
Find 22 step solution in 135ms: B2 R2 U' B R2 D' U' B L D R' L2 F2 U' L2 D L2 U2 L2 B2 L2 F2 
Find 23 step solution in 818ms: F R L' B U2 R' L' U B R F D2 R2 B2 U2 F2 L2 D U2 R2 D' R2 F2 
Find 22 step solution in 35ms: F2 R2 D B L2 F' U' L D R D2 L2 F2 D' B2 D2 U' R2 U F2 U B2 
Find 23 step solution in 742ms: F2 U B D' B2 L F2 L D U2 L F2 L2 U F2 R2 D' U' L2 U' R2 D R2 
Find 23 step solution in 300ms: B2 L2 U F2 R' D F U' B' F L D2 F2 D' L2 F2 D U2 R2 U2 B2 R2 L2 
Find 22 step solution in 140ms: D' F L2 B2 U' R B2 F U' L' F2 D2 B2 R2 D' R2 D' R2 L2 D R2 U2 
Find 21 step solution in 97ms: R2 L F2 R B D B U' L' F2 D' B2 R2 U L2 B2 U' F2 D' F2 D 
Find 21 step solution in 57ms: F D2 U' F' R2 B' L D' B' L2 D' U2 R2 D' F2 U B2 L2 U' B2 F2 
Find 22 step solution in 184ms: F2 D R' B2 R D B' R B L' F2 D L2 U2 B2 U2 F2 L2 F2 U' R2 F2 
Find 21 step solution in 70ms: U' L2 D R2 B' D2 R2 U L D2 U' L2 U B2 D2 U F2 R2 U' B2 L2 
Find 21 step solution in 424ms: F' U' L' F L2 U F' D2 L' D' B' F2 U2 B2 L2 F2 D L2 F2 U L2 
Find 21 step solution in 189ms: B R U' F R U2 R2 F' L2 F' R' L2 D U2 R2 D2 B2 R2 B2 U' R2 
Find 23 step solution in 254ms: F2 D' F U' R B R' U F2 U' L' B2 D U F2 L2 F2 D' F2 U R2 U B2 
Find 22 step solution in 627ms: F' L B' L' D' L' F' D U L2 F' R2 U R2 U' B2 U' B2 D B2 R2 L2 
Find 22 step solution in 58ms: D2 U' R2 F' L' B' U B2 D F' R2 F2 U' L2 B2 D B2 D2 U R2 B2 D 
Find 20 step solution in 46ms: B' D2 U R' D2 B' F' R' F' R2 F2 D U F2 D B2 R2 F2 D' F2 
Find 21 step solution in 33ms: B2 L2 F U B' F L D' R L B2 F2 U2 R2 D' B2 D' R2 B2 U' F2 
Find 20 step solution in 52ms: B2 D' U' F L F' R' F U L' U2 R2 B2 D2 U' F2 D L2 U L2 
Find 22 step solution in 275ms: B' U' F' D' R F' R2 L' D R B F2 D' B2 U2 R2 F2 L2 D L2 U' F2 
Find 22 step solution in 36ms: B2 D2 R D2 U' F' D2 B' U2 L D2 F2 D R2 B2 D' B2 U' F2 D2 L2 D2 
Find 22 step solution in 72ms: B D U L B2 U' B R D2 F' U2 R2 D2 B2 U B2 L2 F2 U R2 U' L2 
Find 23 step solution in 195ms: B2 D2 L2 F' R' U' R B' F2 L2 F D2 F2 R2 U' L2 D2 U F2 U' L2 U2 R2 
Find 23 step solution in 89ms: B2 L' D' F R L2 D2 R2 F D' L' D F2 D2 U' L2 F2 D2 B2 D L2 U' F2 
Find 22 step solution in 66ms: U' L F U' B U B2 U' R F' R2 L2 F2 R2 U B2 D2 U R2 U F2 R2 
Find 21 step solution in 95ms: B2 F L' F2 R' F D2 R' F U2 R2 L2 U' L2 B2 D F2 D U2 L2 B2 
Find 20 step solution in 86ms: D F' D U' R D' F2 R2 U B D2 L2 D B2 D' R2 B2 D' B2 U 
Find 21 step solution in 285ms: B' D2 R B' L' D' R2 F' L2 F2 D2 B2 U' L2 U2 B2 U F2 D' L2 D 
Find 22 step solution in 112ms: D2 B U R' L2 U2 L' U L B' R2 D' R2 D2 U' F2 U' B2 F2 U2 L2 D' 
Find 20 step solution in 102ms: B F' L' B2 L U2 B' F' U F L D B2 D B2 F2 R2 D B2 D 
Find 23 step solution in 109ms: B' U' R U R2 F' L' F2 U2 R F D' B2 R2 B2 F2 R2 D F2 D2 F2 L2 B2 
Find 22 step solution in 172ms: B2 U B2 L' B F' U' F2 D2 U2 L' F2 D L2 F2 D' B2 U' F2 U' F2 U2 
Find 20 step solution in 52ms: U2 R' B2 U2 B' R L U F' R2 D' U2 F2 L2 D' U' F2 U R2 B2 
Find 21 step solution in 36ms: D B2 R' D2 R U' L' B' F D B2 D' U2 B2 U L2 U R2 L2 D F2 
Find 20 step solution in 25ms: U2 B' U R U B D' U2 R' B2 L2 F2 D2 U R2 D R2 D' F2 D' 
Find 22 step solution in 73ms: B2 R2 L D' R' L U' F' D2 B' U' R2 L2 U B2 D U2 F2 R2 L2 U2 F2 
Find 22 step solution in 159ms: L2 U' R' L' B' F' R2 D R' B R2 D' R2 B2 U2 F2 U2 R2 D R2 D F2 
Find 21 step solution in 76ms: B2 U L' F U2 R' F' D' L U' R' U B2 U' R2 F2 L2 F2 D2 F2 D2 
Find 21 step solution in 123ms: B D R' U' B F D2 B R F L2 D' U2 F2 U' L2 U' L2 D2 R2 L2 
Find 21 step solution in 64ms: U B' U' B U2 R D' U L F' D2 U2 F2 D' F2 L2 U F2 L2 U B2 
Find 21 step solution in 120ms: U2 R' L' B2 U L D R B L' U2 F2 L2 B2 R2 D' R2 L2 B2 U B2 
Find 23 step solution in 90ms: B' D U2 L F U L' B F' U R' L2 B2 U2 R2 D' B2 U2 F2 U' B2 D F2 
Find 22 step solution in 262ms: B R' B' L' D' B2 L2 F' D B R2 D2 F2 R2 L2 D B2 L2 D' F2 U F2 
Find 22 step solution in 87ms: B D2 L2 D R' U2 B' F' D B' D L2 D R2 L2 B2 D2 U R2 U' F2 U 
Find 22 step solution in 144ms: L2 F U L2 U' R B' D L F' L2 U L2 B2 R2 U2 F2 D B2 R2 D' B2 
totel time cost 8982ms
*/

#define STEP_LIMIT           23
#define P2_STEP_LIMIT_MAX    11

#define LOOKUP_TABLE_SIZE    74091948
#define TWIST_MOVE(x)        get_u16(0, x)
#define FLIP_MOVE(x)         get_u16(78732, x)
#define SLICE_SORT_MOVE(x)   get_u16(152460, x)
#define TWIST_SYM(x)         get_u16(580140, x)
#define U_EDGE_MOVE(x)       get_u16(650124, x)
#define D_EDGE_MOVE(x)       get_u16(1077804, x)
#define CORNER_MOVE(x)       get_u16(1505484, x)
#define UD_EDGE_MOVE(x)      get_u16(2957004, x)
#define UD_EDGE_SYM(x)       get_u16(4408524, x) 
#define INDEX_TO_CLASS(x)    get_u32(5698764, x) 
#define INDEX_TO_CLASS_P2(x) get_u32(9753804, x) 
#define U_D_TO_UD(x)         get_u16(9915084, x) 
#define PRUN1_2BIT(x)        get_u2(9995724, x) 
#define PRUN21_2BIT(x)       get_u2(45222828, x) 
#define PRUN22(x)            get_u8(73124268, x) 


static inline int get_u8(int base, int offset)
{
    uint32_t addr = base + offset;
    uint8_t *tmp = get_from_cache(addr);
    int b = tmp[addr%BLOCK_SIZE];
    //printf("u8 %x %x %x\n",base,offset,b);
    return b;
} 
static inline int get_u16(int base, int offset)
{
    uint32_t addr = base + offset*2;
    uint8_t *tmp = get_from_cache(addr);
    //int b = tmp[addr%BLOCK_SIZE] | tmp[(addr%BLOCK_SIZE) + 1] << 8;
    //printf("u16 %x %x %x\n",base,offset,b);
    return *((uint16_t*)(tmp + addr % BLOCK_SIZE));
} 
static inline int get_u32(int base, int offset)
{
    uint32_t addr = base + offset*4;
    uint8_t *tmp = get_from_cache(addr);
    //int b = tmp[addr%BLOCK_SIZE] | tmp[(addr%BLOCK_SIZE) + 1] << 8 |
    //        tmp[(addr%BLOCK_SIZE) + 2] << 16 | tmp[(addr%BLOCK_SIZE) + 3] << 24;
    //printf("u32 %x %x %x\n",base,offset,b);
    return *((uint32_t*)(tmp + addr % BLOCK_SIZE));
} 
static inline int get_u2(int base, int offset)
{
    uint32_t addr = base + (offset>>2);
    uint8_t *tmp = get_from_cache(addr); 
    int y = tmp[addr%BLOCK_SIZE];
    y >>= (offset & 3) * 2;
    //printf("u2 %x %x %x\n",base,offset,y);
    return y & 3;
} 


// 组合 n*(n-1)*(n-m+1)/m!
static unsigned int comb(unsigned int n, unsigned int m)
{
    int c = 1;
    for(int i=0; i<m; i++){
        c *= n - i;
        c /= i + 1;
    }
    return c;
}

// 编码4个数字的全排列，例如(1, 0, 2, 3) => 6
// 基于康托展开，支持不连续的数据
static unsigned int encode_4P4(int *p)
{
    int n=0;
    for(int a=0; a<3; a++){      // 3、4都一样
        n *= 4-a;              // 计算阶乘
        for(int b = a+1; b < 4; b++){// 位于右边的比自己小的
            if(p[b] < p[a]){
                n += 1;
            }
        }
    }
    return n;
}
static unsigned int encode_8P8(uint8_t *p)
{
    int n=0;
    for(int a=0; a<7; a++){
        n *= 8-a;
        for(int b = a+1; b < 8; b++){// 位于右边的比自己小的
            if(p[b] < p[a]){
                n += 1;
            }
        }
    }
    return n;
}
// https://www.jaapsch.net/puzzles/compindx.htm
// 编码12选4,输入0-11中任意选取的4个数字，输出0-495
static unsigned int encode_12C4(int *x)
{
    return comb(x[0],1) + comb(x[1],2) + comb(x[2],3) + comb(x[3],4);
}

// 用于转换两种表示方式 20个棱块角块 <---> 54个面
// [UF, UR, UB, UL,DF, DR, DB, DL, FR, FL, BR, BL] [UFR, URB, UBL, ULF, DRF, DFL, DLB, DBR]
// UUUUUUUUURRRRRRRRRFFFFFFFFFDDDDDDDDDLLLLLLLLLBBBBBBBBB
static const char edge_to_face[12][2] = {
    {7, 19},  {5, 10},  {1, 46},  {3, 37},
    {28, 25}, {32, 16}, {34, 52}, {30, 43},
    {23, 12}, {21, 41}, {48, 14}, {50, 39}
};
static const char corner_to_face[8][3] = {
    {8, 20, 9}, {2, 11, 45}, {0, 47, 36}, {6, 38, 18},
    {29, 15, 26}, {27, 24, 44}, {33, 42, 53}, {35, 51, 17}
};
static const char edge_index[24][2] = {
    "UF","UR","UB","UL","DF","DR","DB","DL","FR","FL","BR","BL",
    "FU","RU","BU","LU","FD","RD","BD","LD","RF","LF","RB","LB"
};
static const char corner_index[24][3] = {
    "UFR","URB","UBL","ULF","DRF","DFL","DLB","DBR",
    "FRU","RBU","BLU","LFU","RFD","FLD","LBD","BRD",
    "RUF","BUR","LUB","FUL","FDR","LDF","BDL","RDB"
};
// 魔方旋转时各个块的变化
static const char route_index[18][3] = {
    "L","L'","L2","R","R'","R2",
    "U","U'","U2","D","D'","D2",
    "F","F'","F2","B","B'","B2"
};
typedef enum route_enum {
    L=0, L3, L2, R, R3, R2, U, U3, U2, D, D3, D2, F, F3, F2, B, B3, B2
}route_t;

typedef struct cube_struct{
    uint8_t ep[12];
    uint8_t er[12];
    uint8_t cp[8];
    uint8_t cr[8];
} cube_t;

typedef struct crood_p1_struct{
    uint16_t twist;
    uint16_t flip;
    uint16_t slice_sort;
    uint8_t step;
    uint8_t last_step;
    uint8_t prun_cache;
} crood_p1_t;

typedef struct crood_p2_struct{
    uint16_t ud_edge;
    uint16_t corner;
    uint16_t slice_sort;
    uint8_t step;
    uint8_t last_step;
    uint8_t prun_cache;
} crood_p2_t;

// 54个面的表示方式，转换为棱块角块表示方式
// 54个面的表示方式，转换为棱块角块表示方式
// 正常 返回1 错误 返回0
static int cube_from_face_54(cube_t *c, const char *cube_str)
{
    int sum = 0;
    // 棱块
    for(int i=0; i<12; i++){
        int index_a = edge_to_face[i][0];
        int index_b = edge_to_face[i][1];
        int tmp;
        for(tmp=0;tmp<24;tmp++){
            if( edge_index[tmp][0] == cube_str[index_a] &&
                edge_index[tmp][1] == cube_str[index_b] ){
                break;
            }
        }
        if(tmp < 24){
            c -> ep[i] = tmp % 12;
            c -> er[i] = tmp / 12;
        }else{
            printf("Error: invalid edge %c%c.\n",
                   cube_str[index_a],cube_str[index_b]);
            return 0;
        }
    }
    for(int i=0; i<12; i++){
        for(int j=0; j<12; j++){
            if(i != j && c -> ep[i] == c -> ep[j]){
                printf("Error: repetition edge.\n");
                return 0;
            }
        }
    }
    sum = 0;
    for(int i=0; i<12; i++){
        sum += c -> er[i];
    }
    if(sum%2 != 0){
        // 单个棱块翻转，不影响后续程序运行
        // 测试用例BRBBUUFLFUFULRLDDBLBRDFULBRFRFFDBBLRUUURLRDDDRFLDBUDFL
        printf("Flipped edge, ignore.\n");
    }
    // 角块
    for(int i=0; i<8; i++){
        int index_a = corner_to_face[i][0];
        int index_b = corner_to_face[i][1];
        int index_c = corner_to_face[i][2];
        int tmp;
        for(tmp=0;tmp<24;tmp++){
            if( corner_index[tmp][0] == cube_str[index_a] &&
                corner_index[tmp][1] == cube_str[index_b] &&
                corner_index[tmp][2] == cube_str[index_c]){
                break;
            }
        }
        if(tmp < 24){
            c -> cp[i] = tmp % 8;
            c -> cr[i] = tmp / 8;
        }else{
            printf("Error: invalid corner %c%c%c.\n",
                   cube_str[index_a],cube_str[index_b],cube_str[index_c]);
            return 0;
        }
    }
    for(int i=0; i<8; i++){
        for(int j=0; j<8; j++){
            if(i != j && c -> cp[i] == c -> cp[j]){
                printf("Error: repetition corner.\n");
                return 0;
            }
        }
    }
    sum = 0;
    for(int i=0; i<8; i++){
        sum += c -> cr[i];
    }
    if(sum%3 != 0){
        // 单个角块旋转，不影响后续程序运行
        printf("Twisted corner, ignore.\n");
    }
    // 校验棱块和角块之间的位置关系
    int cornerParity = 0;
    for (int i = 7; i >= 1; i--)
        for (int j = i - 1; j >= 0; j--)
            if (c->cp[j] > c->cp[i])
                cornerParity++;
    cornerParity = cornerParity % 2;
    int edgeParity = 0;
    for (int i = 11; i >= 1; i--)
        for (int j = i - 1; j >= 0; j--)
            if (c->ep[j] > c->ep[i])
                edgeParity++;
    edgeParity = edgeParity % 2;
    if ((edgeParity ^ cornerParity) != 0) {
        printf("ERROR: parity error.\n");
        return 0;
    }
    return 1;
}

// 角块方向，取值范围[0,2186] 用于阶段1    
// 编码所有角块方向 3^7 = 2187
static int cube_encode_twist(cube_t *c)
{
    int x = 0;
    for(int i=0; i<7; i++){
        x = x*3 + c->cr[i];
    }
    return x;
}
// 棱边方向，取值范围[0,2047] 用于阶段1
// 编码所有棱块方向 2^11 = 2048
static int cube_encode_flip(cube_t *c)
{
    int x = 0;
    for(int i=0; i<11; i++){
        x = x*2 + c->er[i];
    }
    return x;
}
// 中间层的 4 条棱边的选取状态，取值范围[0,494]
// 中层的棱边排列，4个，取值范围[0,23]
// 编码8-11号棱块的所在位置12C4 * 4! = 12P4 = 11880
// x // 24后用于一阶段 ， x % 24后用于二阶段
static int cube_encode_slice_sort(cube_t *c)
{
    int ep4_index[4] = {0,0,0,0};
    int ep4_val[4] = {0,0,0,0};
    int j = 0;
    for(int i=0; i<12; i++){
        if(c->ep[i] >= 8){
            ep4_val[j] = c->ep[i];
            ep4_index[j] = i;
            j += 1;
        }
    }
    return encode_12C4(ep4_index) * 24 + encode_4P4(ep4_val);
}
// 角块排列，8个，取值范围[0,40319]
static int cube_encode_corner(cube_t *c)
{
    return encode_8P8(c->cp);
}
// U层的棱边排列，4个，取值范围[0,11879]，对于阶段2,取值小于1680
static int cube_encode_u_edge(cube_t *c)
{
    int ep4_index[4] = {0,0,0,0};
    int ep4_val[4] = {0,0,0,0};
    int j = 0;
    for(int i=0; i<12; i++){
        if(c->ep[i] <= 3){
            ep4_val[j] = c->ep[i];
            ep4_index[j] = i;
            j += 1;
        }
    }
    return encode_12C4(ep4_index) * 24 + encode_4P4(ep4_val);
}
// D层的棱边排列，4个，取值范围[0,11879]，对于阶段2,取值小于1680
static int cube_encode_d_edge(cube_t *c)
{
    int ep4_index[4] = {0,0,0,0};
    int ep4_val[4] = {0,0,0,0};
    int j = 0;
    for(int i=0; i<12; i++){
        if(c->ep[i] >= 4 && c->ep[i] <= 7){
            ep4_val[j] = c->ep[i];
            ep4_index[j] = i;
            j += 1;
        }
    }
    return encode_12C4(ep4_index) * 24 + encode_4P4(ep4_val);
}

static int get_deep_prun1(int twist, int flip, int slice_sort)
{
    int flipslice = flip * 495 + slice_sort / 24;
    int classidx_sym = INDEX_TO_CLASS(flipslice);
    int classidx = classidx_sym & 0xffff;
    int sym = classidx_sym >> 16;
    int prun1_index = classidx * 2187 + TWIST_SYM(16 * twist + sym);
    int deep_mod3 = PRUN1_2BIT(prun1_index);
    int deep = 0;
    // 若没到目标状态
    while(prun1_index != 174960){ 
        for(int d=0; d<18; d++){
            int new_twist = TWIST_MOVE(18 * twist + d);
            int new_flip = FLIP_MOVE(18 * flip + d);
            int new_slice_sort = SLICE_SORT_MOVE(18 * slice_sort + d);
            
            flipslice = new_flip * 495 + new_slice_sort / 24;
            classidx_sym = INDEX_TO_CLASS(flipslice);
            classidx = classidx_sym & 0xffff;
            sym = classidx_sym >> 16;
            
            prun1_index = classidx * 2187 + TWIST_SYM(16 * new_twist + sym);
            int new_deep_mod3 = PRUN1_2BIT(prun1_index);
            int diff = new_deep_mod3 - deep_mod3;
            
            if( diff == 2 || diff == -1 ){// 如果转动后距离更近
                deep += 1;
                twist = new_twist;
                flip = new_flip;
                slice_sort = new_slice_sort;
                deep_mod3 = new_deep_mod3;
                break;
            }
        }
    }
    return deep;
}
static int get_deep_prun21(int corner, int ud_edge)
{
    int classidx_sym = INDEX_TO_CLASS_P2(corner);
    int classidx = classidx_sym & 0xffff;
    int sym = classidx_sym >> 16;
    int prun21_index = classidx * 40320 + UD_EDGE_SYM(16 * ud_edge + sym);
    int deep_mod3 = PRUN21_2BIT(prun21_index);
    if (deep_mod3 == 3){ //还原步骤 > 10
        return 255;
    }
    int deep = 0;
    while(prun21_index != 0){ //若没到目标状态
        const char allow[10] = {2,5,6,7,8,9,10,11,14,17};
        for(int i=0; i < 10; i++) {
            int d = allow[i];
            int new_corner = CORNER_MOVE(18 * corner + d);
            int new_ud_edge = UD_EDGE_MOVE(18 * ud_edge + d);
            classidx_sym = INDEX_TO_CLASS_P2(new_corner);
            classidx = classidx_sym & 0xffff;
            sym = classidx_sym >> 16;
            prun21_index = classidx * 40320 + UD_EDGE_SYM(16 * new_ud_edge + sym);
            int new_deep_mod3 = PRUN21_2BIT(prun21_index);
            if( new_deep_mod3 != 3 ){ // 步骤数未记录的，一定是距离还原状态更远的
                int diff = new_deep_mod3 - deep_mod3;
                if( diff == 2 || diff == -1 ){// 如果转动后距离更近
                    deep += 1;
                    corner = new_corner;
                    ud_edge = new_ud_edge;
                    deep_mod3 = new_deep_mod3;
                    break;
                }
            }
        }
    }
    return deep;
}

static char *format_solution_string(char *p, uint8_t *steps_record, int len)
{
    for(int x=0; x<len; x++){
        const char *p_src = route_index[ steps_record[x] ];
        while(*p_src != 0){
            *p++ = *p_src++;
        }
        *p++ = ' ';
    }
    return p;
}


static crood_p2_t cube_stack_p2[128];
static int search_p2(crood_p2_t *cc2, int max_deep, uint8_t *steps_record_p2)
{
    //printf("search_p2 max_deep=%d corner=%d ud_edge=%d slice_sort=%d prun_cache=%d\n",
    //        max_deep,cc2->corner,cc2->ud_edge,cc2->slice_sort,cc2->prun_cache);
    cc2 -> step = 0;
    crood_p2_t *sp = cube_stack_p2;
    memcpy(sp, cc2, sizeof(crood_p1_t));// stack push
    sp += 1;
    while(sp > cube_stack_p2){
        //assert(sp - cube_stack_p2 < 100*sizeof(crood_p2_t));
        sp -= 1;
        crood_p2_t c;
        memcpy(&c, sp, sizeof(crood_p2_t)); // 取栈顶部的元素
        if( c.step - 1 >= 0 ){
            steps_record_p2[c.step - 1] = c.last_step;
        }
        int class_sym = INDEX_TO_CLASS_P2(c.corner);
        int class_ = class_sym & 0xffff;
        int sym = class_sym >> 16;
        int prun21_index = class_ * 40320 + UD_EDGE_SYM(16 * c.ud_edge + sym);
        int deep_prun21 = PRUN21_2BIT(prun21_index);
        if( deep_prun21 == 3 ){// 跳过步骤数量>10的
            continue;
        }
        int diff = deep_prun21 - c.prun_cache % 3;
        if(diff == -1 || diff == 2){
            c.prun_cache -= 1;
        }else if(diff == 1 || diff == -2){
            c.prun_cache += 1;
        }
        int prun22_index = c.corner * 24  + c.slice_sort % 24;
        int estimate = PRUN22(prun22_index);
        if( c.prun_cache > estimate ){
            estimate = c.prun_cache;
        }
        if(estimate == 0){
            return c.step;
        }
        // 舍弃max_dedep步骤内无解的
        if(estimate + c.step <= max_deep){
            const char allow[10] = {2,5,6,7,8,9,10,11,14,17};
            for(int i=0; i < 10; i++) {
                int d = allow[i];
                // 规则1：如果旋转的面和上次旋转的一致，舍弃，可减少1/6的情况
                // 规则2：L后不允许R，U后不允许D，F后不允许D
                int old = c.last_step / 3;
                int new = d / 3;
                if(old != new && !(old == 0 && new == 1)  &&
                 !(old == 2 && new == 3)  && !(old == 4 && new == 5) ){
                    sp -> corner = CORNER_MOVE(18 * c.corner + d);
                    sp -> ud_edge = UD_EDGE_MOVE(18 * c.ud_edge + d);
                    sp -> slice_sort = SLICE_SORT_MOVE(18 * c.slice_sort + d);
                    sp -> prun_cache = c.prun_cache;
                    sp -> step = c.step + 1;
                    sp -> last_step = d;
                    sp += 1;
                }
            }
        }
    }
    return -1;// 没有找到符合要求的
}
static crood_p1_t cube_stack_p1[256];
int solve(const char *cubr_str, char *p)
{
    cube_t cube;
    crood_p1_t cc1;
    // TODO 两阶段可以合并的
    uint8_t steps_record_p1[STEP_LIMIT];
    if(!cube_from_face_54(&cube, cubr_str)){
        p[0] = 0;
        return -1;
    }
    cc1.twist = cube_encode_twist(&cube);
    cc1.flip = cube_encode_flip(&cube);
    cc1.slice_sort = cube_encode_slice_sort(&cube);
    cc1.step = 0;
    cc1.last_step = 99;
    int u_edge_start = cube_encode_u_edge(&cube);
    int d_edge_start = cube_encode_d_edge(&cube);
    int corner_start = cube_encode_corner(&cube); 
    
    int init_deep = get_deep_prun1(cc1.twist, cc1.flip, cc1.slice_sort);
    cc1.prun_cache = init_deep;
    for(int p1_step_limit=init_deep; ; p1_step_limit++){
        //printf("search_p1 max_deep=%d\n", p1_step_limit);
        crood_p1_t *sp = cube_stack_p1;
        memcpy(sp, &cc1, sizeof(crood_p1_t));// stack push
        sp += 1;
        while(sp > cube_stack_p1){
            //assert(sp - cube_stack_p1 < 200*sizeof(crood_p1_t));
            sp -= 1;
            crood_p1_t c;
            memcpy(&c, sp, sizeof(crood_p1_t)); // 取栈顶部的元素
            if( c.step - 1 >= 0 ){
                steps_record_p1[c.step - 1] = c.last_step;
            }
            int flipslice = c.flip * 495 + c.slice_sort / 24;
            int classidx_sym = INDEX_TO_CLASS(flipslice);
            int class_ = classidx_sym & 0xffff;
            int sym = classidx_sym >> 16;
            int prun1_index = class_ * 2187 + TWIST_SYM(16 * c.twist + sym);
            int diff = PRUN1_2BIT(prun1_index) - c.prun_cache % 3;
            if(diff == -1 || diff == 2){
                c.prun_cache -= 1;
            }else if( diff == 1 || diff == -2){
                c.prun_cache += 1;
            }
            if(c.prun_cache == 0){// 如果找到阶段1的解
                int s1_len = c.step;
                // 生成阶段2的初始数据
                int slice_sort = c.slice_sort;
                int p2_step_limit = STEP_LIMIT - s1_len; //p2允许步骤数量
                if(p2_step_limit > P2_STEP_LIMIT_MAX){
                    p2_step_limit = P2_STEP_LIMIT_MAX;// 限制二阶段最多10步
                }
                int u_edge = u_edge_start;
                int d_edge = d_edge_start;
                int corner = corner_start;
                for(int i = 0; i<s1_len; i++){
                    u_edge = U_EDGE_MOVE(18 * u_edge + steps_record_p1[i]);
                    d_edge = D_EDGE_MOVE(18 * d_edge + steps_record_p1[i]);
                    corner = CORNER_MOVE(18 * corner + steps_record_p1[i]);
                }
                // 二阶段搜索
                crood_p2_t cc2;
                cc2.ud_edge = U_D_TO_UD(u_edge*24 + d_edge%24);
                cc2.slice_sort = slice_sort;
                cc2.corner = corner;
                cc2.last_step = c.last_step;
                int prun22_index = cc2.corner * 24  + cc2.slice_sort % 24;
                int p22 = PRUN22(prun22_index);
                if(p22 <= p2_step_limit){
                    int p21 = get_deep_prun21(cc2.corner, cc2.ud_edge);
                    cc2.prun_cache = p21;
                    if(p21 <= p2_step_limit){
                        int max_p21_p22 = p21 > p22 ? p21 : p22;
                        for(int max_deep=max_p21_p22; max_deep<=p2_step_limit; max_deep++){
                            int s2_len = search_p2(&cc2, max_deep, steps_record_p1 + s1_len);
                            if(s2_len >= 0){
                                // 转换结果的表示方式
                                p = format_solution_string(p, steps_record_p1, s1_len + s2_len);
                                *p++ = '\0';
                                return s1_len + s2_len;
                            }
                        }
                    }
                }
            }    
            // 舍弃p1_step_limit步骤内无解的
            if( c.prun_cache + c.step <= p1_step_limit){
                for(int d=0; d<18; d++){
                    // 规则1：如果旋转的面和上次旋转的一致，舍弃，可减少1/6的情况
                    // 规则2：L后不允许R，U后不允许D，F后不允许D
                    int old = c.last_step / 3;
                    int new = d / 3;
                    if(old != new && !(old == 0 && new == 1)  &&
                     !(old == 2 && new == 3)  && !(old == 4 && new == 5) ){
                        sp -> twist = TWIST_MOVE(18 * c.twist + d);
                        sp -> flip = FLIP_MOVE(18 * c.flip + d);
                        sp -> slice_sort = SLICE_SORT_MOVE(18 * c.slice_sort + d);
                        sp -> step = c.step + 1;
                        sp -> last_step = d;
                        sp -> prun_cache = c.prun_cache;
                        sp += 1;
                    }
                }
            }
        }
    }
}

