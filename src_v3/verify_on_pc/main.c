#include <stdio.h>
#include <assert.h>
#include <inttypes.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <time.h>  
#include "solve.h"

typedef enum route_enum {
    L=0, L3, L2, R, R3, R2, U, U3, U2, D, D3, D2, F, F3, F2, B, B3, B2
}route_t;

typedef struct cube_struct{
    uint8_t ep[12];
    uint8_t er[12];
    uint8_t cp[8];
    uint8_t cr[8];
} cube_t;

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
// 用于验证正确性的测试代码
static const uint8_t route_tab_ep[18][12]={
    {0,1,2,11,4,5,6,9,8,3,10,7},// L  0
    {0,1,2,9,4,5,6,11,8,7,10,3},// L' 1
    {0,1,2,7,4,5,6,3,8,11,10,9},// L2 2
    {0,8,2,3,4,10,6,7,5,9,1,11},// R  3
    {0,10,2,3,4,8,6,7,1,9,5,11},// R' 4
    {0,5,2,3,4,1,6,7,10,9,8,11},// R2 5
    {1,2,3,0,4,5,6,7,8,9,10,11},// U  6
    {3,0,1,2,4,5,6,7,8,9,10,11},// U' 7
    {2,3,0,1,4,5,6,7,8,9,10,11},// U2 8
    {0,1,2,3,7,4,5,6,8,9,10,11},// D  9
    {0,1,2,3,5,6,7,4,8,9,10,11},// D' 10
    {0,1,2,3,6,7,4,5,8,9,10,11},// D2 11
    {9,1,2,3,8,5,6,7,0,4,10,11},// F  12
    {8,1,2,3,9,5,6,7,4,0,10,11},// F' 13
    {4,1,2,3,0,5,6,7,9,8,10,11},// F2 14
    {0,1,10,3,4,5,11,7,8,9,6,2},// B  15
    {0,1,11,3,4,5,10,7,8,9,2,6},// B' 16
    {0,1,6,3,4,5,2,7,8,9,11,10} // B2 17
};
static const uint8_t route_tab_er[18][12]={
    {0,0,0,0,0,0,0,0,0,0,0,0},// L  0
    {0,0,0,0,0,0,0,0,0,0,0,0},// L' 1
    {0,0,0,0,0,0,0,0,0,0,0,0},// L2 2
    {0,0,0,0,0,0,0,0,0,0,0,0},// R  3
    {0,0,0,0,0,0,0,0,0,0,0,0},// R' 4
    {0,0,0,0,0,0,0,0,0,0,0,0},// R2 5
    {0,0,0,0,0,0,0,0,0,0,0,0},// U  6
    {0,0,0,0,0,0,0,0,0,0,0,0},// U' 7
    {0,0,0,0,0,0,0,0,0,0,0,0},// U2 8
    {0,0,0,0,0,0,0,0,0,0,0,0},// D  9
    {0,0,0,0,0,0,0,0,0,0,0,0},// D' 10
    {0,0,0,0,0,0,0,0,0,0,0,0},// D2 11
    {1,0,0,0,1,0,0,0,1,1,0,0},// F  12
    {1,0,0,0,1,0,0,0,1,1,0,0},// F' 13
    {0,0,0,0,0,0,0,0,0,0,0,0},// F2 14
    {0,0,1,0,0,0,1,0,0,0,1,1},// B  15
    {0,0,1,0,0,0,1,0,0,0,1,1},// B' 16
    {0,0,0,0,0,0,0,0,0,0,0,0} // B2 17
};
static const uint8_t route_tab_cp[18][8]={
    {0,1,6,2,4,3,5,7},// L  0
    {0,1,3,5,4,6,2,7},// L' 1
    {0,1,5,6,4,2,3,7},// L2 2
    {4,0,2,3,7,5,6,1},// R  3
    {1,7,2,3,0,5,6,4},// R' 4
    {7,4,2,3,1,5,6,0},// R2 5
    {1,2,3,0,4,5,6,7},// U  6
    {3,0,1,2,4,5,6,7},// U' 7
    {2,3,0,1,4,5,6,7},// U2 8
    {0,1,2,3,5,6,7,4},// D  9
    {0,1,2,3,7,4,5,6},// D' 10
    {0,1,2,3,6,7,4,5},// D2 11
    {3,1,2,5,0,4,6,7},// F  12
    {4,1,2,0,5,3,6,7},// F' 13
    {5,1,2,4,3,0,6,7},// F2 14
    {0,7,1,3,4,5,2,6},// B  15
    {0,2,6,3,4,5,7,1},// B' 16
    {0,6,7,3,4,5,1,2} // B2 17
};
static const uint8_t route_tab_cr[18][8]={
    {0,0,2,1,0,2,1,0},// L  0
    {0,0,2,1,0,2,1,0},// L' 1
    {0,0,0,0,0,0,0,0},// L2 2
    {2,1,0,0,1,0,0,2},// R  3
    {2,1,0,0,1,0,0,2},// R' 4
    {0,0,0,0,0,0,0,0},// R2 5
    {0,0,0,0,0,0,0,0},// U  6
    {0,0,0,0,0,0,0,0},// U' 7
    {0,0,0,0,0,0,0,0},// U2 8
    {0,0,0,0,0,0,0,0},// D  9
    {0,0,0,0,0,0,0,0},// D' 10
    {0,0,0,0,0,0,0,0},// D2 11
    {1,0,0,2,2,1,0,0},// F  12
    {1,0,0,2,2,1,0,0},// F' 13
    {0,0,0,0,0,0,0,0},// F2 14
    {0,2,1,0,0,0,2,1},// B  15
    {0,2,1,0,0,0,2,1},// B' 16
    {0,0,0,0,0,0,0,0} // B2 17
};
static void cube_route(cube_t *c1, route_t d)
{
    uint8_t ep_tmp[12], er_tmp[12], cp_tmp[12], cr_tmp[12];
    for(int i=0; i<12; i++){
        ep_tmp[i] = c1 -> ep[route_tab_ep[d][i]];
        er_tmp[i] = c1 -> er[route_tab_ep[d][i]] ^ route_tab_er[d][i];
    }
    for(int i=0; i<8; i++){
        cp_tmp[i] = c1 -> cp[route_tab_cp[d][i]];
        cr_tmp[i] = (c1 -> cr[route_tab_cp[d][i]] + route_tab_cr[d][i] ) % 3;
    }
    for(int i=0; i<12; i++){
        c1 -> ep[i] = ep_tmp[i];
        c1 -> er[i] = er_tmp[i];
    }
    for(int i=0; i<8; i++){
        c1 -> cp[i] = cp_tmp[i];
        c1 -> cr[i] = cr_tmp[i];
    }
}
// 输出54个面的状态
static void cube_to_face_54(cube_t *c, char *cube_str)
{
    cube_str[4]  = 'U';
    cube_str[13] = 'R';
    cube_str[22] = 'F';
    cube_str[31] = 'D';
    cube_str[40] = 'L';
    cube_str[49] = 'B';
    cube_str[54] = '\0';
    for(int i=0; i<12; i++){
        int index_a = edge_to_face[i][0];
        int index_b = edge_to_face[i][1];
        const char *s = edge_index[c->ep[i] + c->er[i] * 12];
        cube_str[index_a] = s[0];
        cube_str[index_b] = s[1];
    }
    for(int i=0; i<8; i++){
        int index_a = corner_to_face[i][0];
        int index_b = corner_to_face[i][1];
        int index_c = corner_to_face[i][2];
        const char *s = corner_index[c->cp[i] + c->cr[i] * 8];
        cube_str[index_a] = s[0];
        cube_str[index_b] = s[1];
        cube_str[index_c] = s[2];
    }
}
static int cube_route_str(cube_t *c1, const char *str)
{
    const char *p = str;
    int count = 0;
    // 整理格式，顺便统计数量
    while(1)
    {
        if(*p == 'U' || *p == 'R' || *p == 'F' || *p == 'D' || *p == 'L' || *p == 'B'){
            route_t r;
            switch(*p){
                case 'L':
                    r = L;
                    break;
                case 'R':
                    r = R;
                    break;
                case 'U':
                    r = U;
                    break;
                case 'D':
                    r = D;
                    break;
                case 'F':
                    r = F;
                    break;
                case 'B':
                    r = B;
                    break;
                default:
                    assert(0);
            }
            p++;
            switch(*p){
                case '\'':
                    r += 1;
                    break;
                case '2':
                    r += 2;
                    break;
            }
            cube_route(c1, r);
            count++;
            if(*p == 0){
                break;
            }
        }
        p++;
        if(*p == 0){
            break;
        }
    }
    return count;
}
int verify(const char *str, const char *solution)
{
    cube_t cube; 
    // 测试结果的正确性
    cube_from_face_54(&cube, str);
    int count = cube_route_str(&cube, solution);
    printf("totel steps = %d ",count);
    char cube_str[55];
    cube_to_face_54(&cube, cube_str);
    if(strcmp(cube_str,"UUUUUUUUURRRRRRRRRFFFFFFFFFDDDDDDDDDLLLLLLLLLBBBBBBBBB") == 0){
        printf("PASS\n");
        return 1;
    }else{
        printf("FAIL %s\n", cube_str);
        return 0;
    }
}

int main(int argc ,char **argv)
{
    char solution[128] = {0}; 
    //printf("run demo.\n");
    //demo();
    //printf("demo finish.\n");

    if(argc == 2){
        printf("argv[1]=%s\n", argv[1]);
        int len = solve(argv[1], solution);
        printf("%s LENGTH=%d\n",solution,len);
        if(len >= 0){
            verify(argv[1], solution);
        }
    }else{
        printf("no input.\n");
        printf("Usage: %s BBBFULRUBUURFRRRDFDFLUFDLRUUUFFDRLDLRRFLLBBLFDLUBBDDBD\n", argv[0]);
        printf("Run auto test.\n");
        FILE *f = fopen("test_case.txt", "r");
        char line_buffer[80];
        if(f != NULL){
            int min=99,max=0,avg=0;
            for(int i=0;i<10000;i++){ /// 10000
                fgets(line_buffer,79,f);
                printf("i=%d %s",i, line_buffer);
                
                struct timespec start, end;  
                double diff;  
                clock_gettime(CLOCK_REALTIME, &start); // 获取开始时间  
                int len = solve(line_buffer, solution);
                assert(len >= 0);
                clock_gettime(CLOCK_REALTIME, &end); // 获取结束时间  
                diff = (end.tv_sec - start.tv_sec) * 1e6 + (end.tv_nsec - start.tv_nsec) / 1e3; // 计算时间差（微秒）  
                printf("Took %.2fus to run.\n", diff);  
                if(len > max){
                    max = len;
                }
                if(len < min){
                    min = len;
                }
                avg += len;
                printf("%s\n",solution);
                if(verify(line_buffer, solution)==0){
                    printf("verify Error\n");
                    return 1;
                }
                //assert(diff < 200000);
            }
            fclose(f);
            printf("min=%d, max=%d, avg=%.2f\n",min,max,avg / 10000.0);
        }
    }    

    return 0;
}
