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
// STEP_LIMIT=22 测试10000个随机打乱的魔方，最大22步, 平均21.21步，耗时15秒
// STEP_LIMIT=21 测试10000个随机打乱的魔方，最大21步, 平均20.77步，耗时1分19秒
// STEP_LIMIT=20 测试10000个随机打乱的魔方，最大20步, 耗时82分53秒
#define STEP_LIMIT           22
#define NEED_VERIFY_CODE

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


static int get_u8(int base, int offset)
{
    uint32_t addr = base + offset;
    uint8_t *tmp = get_from_cache(addr);
    int b = tmp[addr%BLOCK_SIZE];
    //printf("u8 %x %x %x\n",base,offset,b);
    return b;
} 
static int get_u16(int base, int offset)
{
    uint32_t addr = base + offset*2;
    uint8_t *tmp = get_from_cache(addr);
    int b = tmp[addr%BLOCK_SIZE] | tmp[(addr%BLOCK_SIZE) + 1] << 8;
    //printf("u16 %x %x %x\n",base,offset,b);
    return b;
} 
static int get_u32(int base, int offset)
{
    uint32_t addr = base + offset*4;
    uint8_t *tmp = get_from_cache(addr);
    int b = tmp[addr%BLOCK_SIZE] | tmp[(addr%BLOCK_SIZE) + 1] << 8 |
            tmp[(addr%BLOCK_SIZE) + 2] << 16 | tmp[(addr%BLOCK_SIZE) + 3] << 24;
    //printf("u32 %x %x %x\n",base,offset,b);
    return b;
} 
static int get_u2(int base, int offset)
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
static void cube_from_face_54(cube_t *c, const char *cube_str)
{
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
        c -> ep[i] = tmp % 12;
        c -> er[i] = tmp / 12;
    }
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
        c -> cp[i] = tmp % 8;
        c -> cr[i] = tmp / 8;
    }
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
    cube_from_face_54(&cube, cubr_str);
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
                if(p2_step_limit > 10){
                    p2_step_limit = 10;// 限制二阶段最多10步
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

