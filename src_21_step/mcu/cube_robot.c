#include <stdio.h>
#include <math.h>
#include <string.h>
#include "pico/stdlib.h"
//#include "pico/multicore.h"
#include "hardware/pio.h"
#include "hardware/clocks.h"
#include "hardware/irq.h"
#include "step_ctrl.pio.h"
#include "color_detect.h"
#include "solve.h"
#include "spi_flash.h"

// 步进电机使能信号 低有效 (20220706 由16改为2)
#define SPEPPER_EN    2  
// 步进电机控制信号
#define SPEPPER_STEP0 11
#define SPEPPER_DIR0  10
#define SPEPPER_STEP1 9
#define SPEPPER_DIR1  8
#define SPEPPER_STEP2 7
#define SPEPPER_DIR2  6
// 霍尔开关,用于寻找零点
#define HALL_0        5
#define HALL_1        3
#define HALL_2        4
// 按键(20220710 由0,1改为27,28)
#define BUTTON_0      27
#define BUTTON_1      28
#define LED_PIN PICO_DEFAULT_LED_PIN
// UART for NAND FLASH Programming
#define UART_TX_PIN 0
#define UART_RX_PIN 1
#define BAUD_RATE 1000000

// 颜色传感器信号{{SDA,SCL},{SDA,SCL}}
const uint i2c_gpio[2][2] = {{14,15}, {12,13}};

// 根据实际测试结果调整电机初始角度
#define STEPPER0_OFFSET 100
#define STEPPER1_OFFSET 110
#define STEPPER2_OFFSET 375

// 顺时针、逆时针的定义,取决与驱动芯片的类型和电机的接线顺序
#define DIR0_CCW false
#define DIR0_CW  true
#define DIR1_CCW false
#define DIR1_CW  true
#define DIR2_CCW true
#define DIR2_CW  false

#define RPM       (200.0f * 16 / 60)

typedef struct step_ctrl_t{
    int s;    // 加速过程需要的步数
    int i;    // 当前步
    int steps;// 步数
    float vi; // 当前速度
    float a;  // 加速度（脉冲/s2）
} step_ctrl;

// MCU的栈较小,因此比较大的数组,放在堆上,防止栈溢出
// cube_robot的全局变量：
uint16_t color_buffer[54*3] = {0}; 
char cube_str[55] = {0};
char solution_str[70] = {0};
step_ctrl stepper_ctrl[3] = {0};
float speed_factor = 1.0f;
float accel_factor = 1.0f;

void sample_color(uint32_t g, uint16_t *buffer);

// -------------------------------------电机控制------------------------------------------------

// 开始电机控制
// sm电机号0-2 steps：步数,v0：初始速度（脉冲/s）,v：最高速度（脉冲/s）,a：加速度（脉冲/s2）
static void stepper_move_none_block(int sm, int steps, float v0, float v, float a)
{
    step_ctrl *p_step_ctrl = &stepper_ctrl[sm];
    p_step_ctrl -> vi = v0 * speed_factor;
    p_step_ctrl -> a = a * accel_factor;
    p_step_ctrl -> s = (int)roundf((v * v - v0 * v0) / (2 * a)); // 计算加速过程需要的步数
    p_step_ctrl -> i = 0;
    p_step_ctrl -> steps = steps;
    pio_set_irq0_source_enabled(pio0, PIO_INTR_SM0_TXNFULL_LSB + sm, true);
}
// 等待电机控制指令执行完毕
static void stepper_move_wait_all(int sm)
{
    // 等待中断关闭
    while(pio0->inte0 & (1u << (sm + PIO_INTR_SM0_TXNFULL_LSB))){
        asm("nop");
    }
    // 等待fifo清空
    while(!pio_sm_is_tx_fifo_empty(pio0, sm)){
        asm("nop");
    }
}
static void stepper_move(int sm, int steps, float v0, float v, float a)
{
    stepper_move_none_block(sm, steps, v0, v, a);
    stepper_move_wait_all(sm);
}
// 等待到输出指定的步数,因为有fifo,所以会提前4个脉冲返回
static void stepper_move_wait(int sm, int step_to_wait)
{
    volatile int *index = &stepper_ctrl[sm].i;
    while(*index <= step_to_wait){
        asm("nop");
    }
}
static void stepper_move_01_with_color_cmd(int steps, const uint32_t *color_cmd, uint16_t *buffer)
{
    const float V_START_WITH_LOAD = 190.0f*RPM;// 带载起跳速度
    const float V_MAX_WITH_LOAD = 380.0f*RPM;  // 带载最大速度
    const float A_MAX_WITH_LOAD = 5.0e5f;      // 带载最大加速度
    // 采样延后量,解决速度快了采样不准确问题,在低速运行不出错的前提下,读取越晚越好
    // 120可能出错 100不出错
    const int SAMPLE_OFFSET = 70;              
    stepper_move_none_block(0, steps, V_START_WITH_LOAD, V_MAX_WITH_LOAD, A_MAX_WITH_LOAD); 
    stepper_move_none_block(1, steps, V_START_WITH_LOAD, V_MAX_WITH_LOAD, A_MAX_WITH_LOAD); 
    // 当 steps = 2400 时
    // 在0 400 800 1200 1600 2000 2400采集
    if(color_cmd != 0){
        for(int i=0; i<steps; i+=400){
            if(color_cmd[i/400] != 0){
                stepper_move_wait(0, i + SAMPLE_OFFSET);
                sample_color(color_cmd[i/400], buffer);
            }
        }
    }
    stepper_move_wait_all(0);
    sleep_us(1200);
    if(color_cmd != 0 && color_cmd[steps/400] != 0){
        sample_color(color_cmd[steps/400], buffer);
    }
}
// 计算脉冲延时
static uint32_t stepper_move_calc(step_ctrl *p_step_ctrl)
{
    const float F = 10000000.0f; // PIO频率
    float accel;
    int i = p_step_ctrl -> i;
    int s = p_step_ctrl -> s;
    int steps = p_step_ctrl -> steps;
    float vi = p_step_ctrl -> vi;
    float a = p_step_ctrl -> a;
    if (i < s && i < steps / 2){
        accel = a;
    }else if (i >= steps - s){
        accel = -a;
    }else{
        accel = 0;
    }
    vi = sqrtf(vi * vi + 2.0f * accel);
    p_step_ctrl -> vi = vi;
    p_step_ctrl -> i = i + 1;
    return (uint32_t)roundf(F / vi) - 33; // 33的来源参考step_ctrl.pio
}
// PIO FIFO非满中断,每次处理耗时5us左右
void pio_fifo_not_full_handler()
{
    //gpio_put(PICO_DEFAULT_LED_PIN, 1);
    step_ctrl *p;
    uint32_t date;
    int sm;
    // 对应通道中断功能开启,且FIFO非满,此时应当填充新的数据
    for(sm=0;sm<3;sm++){
        while((pio0->inte0 & (1u << (sm + PIO_INTR_SM0_TXNFULL_LSB))) && !pio_sm_is_tx_fifo_full(pio0, sm)){
            p = &stepper_ctrl[sm];
            date = stepper_move_calc(p);
            pio_sm_put(pio0, sm, date);
            if (p->i >= p->steps) {
                pio_set_irq0_source_enabled(pio0, sm + PIO_INTR_SM0_TXNFULL_LSB, false);
                break;
            }
        }
    }
    //gpio_put(PICO_DEFAULT_LED_PIN, 0);
}
// 初始化电机控制PIO
static void init_stepper_ctrl(void)
{
    // 初始化PIO
    uint offset = pio_add_program(pio0, &step_ctrl_program);
    step_ctrl_program_init(pio0, 0, offset, SPEPPER_STEP0);
    step_ctrl_program_init(pio0, 1, offset, SPEPPER_STEP1);
    step_ctrl_program_init(pio0, 2, offset, SPEPPER_STEP2);
    // 开中断
    irq_set_exclusive_handler(PIO0_IRQ_0, pio_fifo_not_full_handler);
    irq_set_enabled(PIO0_IRQ_0, true);
}

// 电机移动归零
void stepper_zero(void) 
{
    bool s0,s1,s2;
    int i = 0, j = 0, k = 0;
    bool s0_ok = false;
    bool s1_ok = false;
    bool s2_ok = false;
    s0 = gpio_get(HALL_0);
    s1 = gpio_get(HALL_1);
    s2 = gpio_get(HALL_2);
    while (!(s0_ok && s1_ok && s2_ok))
    {
        if(s0_ok == false){
            gpio_put(SPEPPER_STEP0, 1);
            sleep_us(4);
            i++;
            gpio_put(SPEPPER_STEP0, 0);
            if(!s0 && gpio_get(HALL_0))
                s0_ok = true;
            s0 = gpio_get(HALL_0);
        }
        if(s1_ok == false){
            gpio_put(SPEPPER_STEP1, 1);
            sleep_us(4);
            j++;
            gpio_put(SPEPPER_STEP1, 0);
            if(!s1 && gpio_get(HALL_1))
                s1_ok = true;
            s1 = gpio_get(HALL_1);
        }
        if(s2_ok == false){
            gpio_put(SPEPPER_STEP2, 1);
            sleep_us(4);
            k++;
            gpio_put(SPEPPER_STEP2, 0);
            if(!s2 && gpio_get(HALL_2))
                s2_ok = true;
            s2 = gpio_get(HALL_2);
        }
        sleep_us(400);
    }
    printf("stepper_zero: %d, %d, %d\n",i,j,k);
}
// IO初始化
void init_io(void)
{
    gpio_init(LED_PIN);
    gpio_init(SPEPPER_EN);
    gpio_init(SPEPPER_DIR0);
    gpio_init(SPEPPER_DIR1);
    gpio_init(SPEPPER_DIR2);
    gpio_init(SPEPPER_STEP0);
    gpio_init(SPEPPER_STEP1);
    gpio_init(SPEPPER_STEP2);
    gpio_init(SPEPPER_STEP2);
    gpio_init(HALL_0);
    gpio_init(HALL_1);
    gpio_init(HALL_2);
    gpio_init(BUTTON_0);
    gpio_init(BUTTON_1);

    gpio_put(SPEPPER_EN,    1);
    gpio_put(SPEPPER_DIR0,  0);
    gpio_put(SPEPPER_DIR1,  0);
    gpio_put(SPEPPER_DIR2,  0);
    gpio_put(SPEPPER_STEP0, 0);
    gpio_put(SPEPPER_STEP1, 0);
    gpio_put(SPEPPER_STEP2, 0);

    gpio_set_dir(LED_PIN, GPIO_OUT);
    gpio_set_dir(SPEPPER_EN, GPIO_OUT);
    gpio_set_dir(SPEPPER_DIR0, GPIO_OUT);
    gpio_set_dir(SPEPPER_DIR1, GPIO_OUT);
    gpio_set_dir(SPEPPER_DIR2, GPIO_OUT);
    gpio_set_dir(SPEPPER_STEP0, GPIO_OUT);
    gpio_set_dir(SPEPPER_STEP1, GPIO_OUT);
    gpio_set_dir(SPEPPER_STEP2, GPIO_OUT);

    gpio_set_dir(HALL_0, GPIO_IN);
    gpio_set_dir(HALL_1, GPIO_IN);
    gpio_set_dir(HALL_2, GPIO_IN);
    gpio_set_dir(BUTTON_0, GPIO_IN);
    gpio_set_dir(BUTTON_1, GPIO_IN);

    gpio_pull_up(HALL_0);
    gpio_pull_up(HALL_1);
    gpio_pull_up(HALL_2);
    gpio_pull_up(BUTTON_0);
    gpio_pull_up(BUTTON_1);

    for (int i=0;i<2;i++){
        for(int j=0;j<2;j++){
            gpio_init(i2c_gpio[i][j]);
            gpio_pull_up(i2c_gpio[i][j]);
            gpio_set_dir(i2c_gpio[i][j], GPIO_IN);
            gpio_put(i2c_gpio[i][j], 0);
        }
    }

    // Set the TX and RX pins by using the function select on the GPIO
    // Set datasheet for more information on function select
    gpio_set_function(UART_TX_PIN, GPIO_FUNC_UART);
    gpio_set_function(UART_RX_PIN, GPIO_FUNC_UART);
}
// --------------- 魔方控制部分 -----------------------
// 旋转魔方的上下(U D)面,注意参数只支持正数,U为逆时针,D为顺时针
// 参数取值范围0 800 1600 2400,不可以同时为0
// U 黄色 0号电机
// D 白色 1号电机
// 支持合并操作,可以同时旋转U和D
void cube_ud(int face_u, int face_d)
{
    const int gap = 24;
    const float V_START_WITH_LOAD = 150.0f*RPM; // 带载起跳速度
    const float V_MAX_WITH_LOAD = 500.0f*RPM;  // 带载最大速度
    const float A_MAX_WITH_LOAD = 4.5e5f;       // 带载最大加速度
    // const float V_START_GAP = 100.0f*RPM;    // 卡住魔方中间层时的运行速度
    gpio_put(SPEPPER_DIR0, DIR0_CCW);
    gpio_put(SPEPPER_DIR1, DIR1_CW);
    // 步骤1：卡住魔方中间层(V20版本框架更加坚固,不需要这个操作)
    //stepper_move_none_block(0, gap, V_START_GAP, V_MAX_WITH_LOAD, A_MAX_WITH_LOAD);
    //stepper_move           (1, gap, V_START_GAP, V_MAX_WITH_LOAD, A_MAX_WITH_LOAD);
    // 步骤2：旋转需要的角度
    if(face_u > 0){
        stepper_move_none_block(0, face_u, V_START_WITH_LOAD, V_MAX_WITH_LOAD, A_MAX_WITH_LOAD);
    }
    if(face_d > 0){
        stepper_move_none_block(1, face_d, V_START_WITH_LOAD, V_MAX_WITH_LOAD, A_MAX_WITH_LOAD);
    }
    if(face_u > face_d){
        stepper_move_wait_all(0);
    }else{
        stepper_move_wait_all(1);
    }
    // 步骤3：恢复初始位置(V20版本框架更加坚固,不需要这个操作)
    //sleep_ms(1);
    //gpio_put(SPEPPER_DIR0, DIR0_CW);
    //gpio_put(SPEPPER_DIR1, DIR1_CCW);
    //stepper_move_none_block(0, gap, V_START_GAP, V_MAX_WITH_LOAD, A_MAX_WITH_LOAD);
    //stepper_move           (1, gap, V_START_GAP, V_MAX_WITH_LOAD, A_MAX_WITH_LOAD);
    sleep_us(1600);
}
// 旋转魔方的左前右后面(L F R B),参数正负都可以,正数表示顺时针,负数表示逆时针
// 参数取值范围 800 1600 -800 -1600
void cube_lfrb(int steps)
{
    const float V_START_NO_LOAD = 170.0f*RPM;   // 空载起跳速度
    const float V_MAX_NO_LOAD = 1500.0f*RPM;    // 空载最大速度 1500RPM = 80KHz
    const float A_MAX_NO_LOAD = 15e5f;          // 空载最大加速度
    const float V_START_WITH_LOAD = 170.0f*RPM; // 带载起跳速度
    const float V_MAX_WITH_LOAD = 600.0f*RPM;  // 带载最大速度
    const float A_MAX_WITH_LOAD = 5.5e5f;       // 带载最大加速度
    const float V_SLOW = 120.0f*RPM; // 最后一小段要低速运转,等待魔方减速
    const int TOUCH_STEP = 256;
    const int SLOW_STEP = 100;
    bool dir;
    if(steps > 0){
        dir = DIR2_CW;
    }else{
        dir = DIR2_CCW;
        steps = -steps;
    }
    // 步骤1：旋转臂接触魔方,接触之前应当减速,否则接触后受力突变,会丢步
    gpio_put(SPEPPER_DIR2, dir);
    stepper_move(2, TOUCH_STEP, V_START_NO_LOAD, V_MAX_NO_LOAD, A_MAX_NO_LOAD);
    // 步骤2：旋转需要的角度
    stepper_move(2, steps - SLOW_STEP, V_START_WITH_LOAD, V_MAX_NO_LOAD, A_MAX_WITH_LOAD);
    // 步骤3：最后一小段要低速运转
    stepper_move(2, SLOW_STEP, V_SLOW, V_SLOW, A_MAX_WITH_LOAD);
    // 步骤4：恢复初始位置
    sleep_us(1200);
    gpio_put(SPEPPER_DIR2, !dir);
    stepper_move(2, TOUCH_STEP, V_START_NO_LOAD, V_MAX_NO_LOAD, A_MAX_NO_LOAD);
}

// 翻转魔方,参数只能为正
// 参数取值范围 800 1600 2400
// 翻转魔方时,动力来自两个电机,可以设置较高的速度
void flip_cube(int steps)
{
    const float V_START_WITH_LOAD = 190.0f*RPM;// 带载起跳速度
    const float V_MAX_WITH_LOAD = 500.0f*RPM;  // 带载最大速度
    const float A_MAX_WITH_LOAD = 5.0e5f;      // 带载最大加速度
    gpio_put(SPEPPER_DIR0, DIR0_CW);
    gpio_put(SPEPPER_DIR1, DIR1_CCW);
    stepper_move_none_block(0, steps, V_START_WITH_LOAD, V_MAX_WITH_LOAD, A_MAX_WITH_LOAD);
    stepper_move           (1, steps, V_START_WITH_LOAD, V_MAX_WITH_LOAD, A_MAX_WITH_LOAD);
    sleep_us(2000);
}
// 采集颜色
void cube_get_color(uint16_t *buffer)
{
    #define UD(x,y) ( ((x)<<8) | (y) )
    const uint32_t color_cmd_0_270_L[7] = {UD(36,42),UD(23,48),UD(38,44),UD(10,43),UD(9,15),UD(50,21),UD(11,17)};
    const uint32_t color_cmd_1_180_R[5] = {0,UD(37,16),UD(33,27),UD(19,52),UD(0,6)};
    const uint32_t color_cmd_2_270_F[7] = {0,0,UD(29,35),UD(46,25),UD(8,2),0,0};
    const uint32_t color_cmd_3_180_B[5] = {0,0,UD(18,51),UD(12,41),UD(47,26)};
    const uint32_t color_cmd_4_270_R[7] = {0,0,UD(45,24),UD(39,14),UD(20,53),0,0};
    const uint32_t color_cmd_5_180_L[5] = {0,0,0,UD(5,32),0};
    //const uint32_t color_cmd_6_90_F[3]= {0,0,0};
    const uint32_t color_cmd_7_180_B[5] = {0,UD(3,30),0,UD(7,28),0};
    const uint32_t color_cmd_8_180  [5] = {0,0,0,UD(1,34),0};

    gpio_put(SPEPPER_DIR0, DIR0_CW);
    gpio_put(SPEPPER_DIR1, DIR1_CCW);
    stepper_move_01_with_color_cmd(2400, color_cmd_0_270_L, buffer); 
    cube_lfrb(800);
    stepper_move_01_with_color_cmd(1600, color_cmd_1_180_R, buffer); 
    cube_lfrb(800);
    stepper_move_01_with_color_cmd(2400, color_cmd_2_270_F, buffer); 
    cube_lfrb(800);
    stepper_move_01_with_color_cmd(1600, color_cmd_3_180_B, buffer); 
    cube_lfrb(800);
    stepper_move_01_with_color_cmd(2400, color_cmd_4_270_R, buffer); 
    cube_lfrb(800);
    stepper_move_01_with_color_cmd(1600, color_cmd_5_180_L, buffer); 
    cube_lfrb(800);
    stepper_move_01_with_color_cmd(800 , 0                , buffer); 
    cube_lfrb(800);
    stepper_move_01_with_color_cmd(1600, color_cmd_7_180_B, buffer); 
    cube_lfrb(800);
    stepper_move_01_with_color_cmd(1600, color_cmd_8_180  , buffer);
}
// 拧魔方,支持如下参数
// "U", "R", "F", "D", "L", "B"
// "U'", "R'", "F'", "D'", "L'", "B'"
// "U2", "R2", "F2", "D2", "L2", "B2"
// 关于U、D的任意操作可以合并执行,分别传递action action2参数即可
char cube_tweak(char face_on_stepper_2, char *action, char *action2)
{
    char face = action[0];
    char face_after_tweak = face_on_stepper_2;
    int angle_u = 0, angle_d = 0;
    int angle_lfrb = 0;
    int flip_time = 0;
    assert(face=='U' || face=='R' || face=='F' || face=='D' || face=='L' || face=='B');
    if(face=='L' || face=='F' || face=='R' || face=='B'){
        if(action[1] == '\''){
            angle_lfrb = -800;
        }else if (action[1] == '2'){
            angle_lfrb = 1600;
        }else if (action[1] == '0'){
            angle_lfrb = 0;
        }else{
            angle_lfrb = 800;
        }
        face_after_tweak = face;
        // F -> R -> B -> L -> F
        // 一共16种组合
        if(face_on_stepper_2 == 'F'){
            if(face_after_tweak == 'F'){
                flip_time = 0;
            }else if(face_after_tweak == 'R'){
                flip_time = 1;
            }else if(face_after_tweak == 'B'){
                flip_time = 2;
            }else if(face_after_tweak == 'L'){
                flip_time = 3;
            }
        }else if(face_on_stepper_2 == 'R'){
            if(face_after_tweak == 'F'){
                flip_time = 3;            
            }else if(face_after_tweak == 'R'){
                flip_time = 0;
            }else if(face_after_tweak == 'B'){
                flip_time = 1;
            }else if(face_after_tweak == 'L'){
                flip_time = 2;
            }
        }else if(face_on_stepper_2 == 'B'){
            if(face_after_tweak == 'F'){
                flip_time = 2;            
            }else if(face_after_tweak == 'R'){
                flip_time = 3;
            }else if(face_after_tweak == 'B'){
                flip_time = 0;
            }else if(face_after_tweak == 'L'){
                flip_time = 1;
            }
        }else if(face_on_stepper_2 == 'L'){
            if(face_after_tweak == 'F'){
                flip_time = 1;           
            }else if(face_after_tweak == 'R'){
                flip_time = 2;
            }else if(face_after_tweak == 'B'){
                flip_time = 3;
            }else if(face_after_tweak == 'L'){
                flip_time = 0;
            }
        }
        // 执行动作
        if(flip_time != 0){
            flip_cube(flip_time * 800);
        }
        if(angle_lfrb != 0){
            cube_lfrb(angle_lfrb);
        }
        
    }else{
        // U只能逆时针
        if(face=='U'){
            if(action[1] == '\''){
                angle_u = 800;
            }else if (action[1] == '2'){
                angle_u = 1600;
            }else{
                angle_u = 2400;
            }
        // D只能顺时针
        }else if(face=='D'){
            if(action[1] == '\''){
                angle_d = 2400;
            }else if (action[1] == '2'){
                angle_d = 1600;
            }else{
                angle_d = 800;
            }
        }
        if(action2 != 0){
            // 可以同时拧2个面
            assert(action2[0] == 'U' || action2[0] == 'D');
            if(action2[0]=='U'){
                assert(angle_u == 0);
                if(action2[1] == '\''){
                    angle_u = 800;
                }else if (action2[1] == '2'){
                    angle_u = 1600;
                }else{
                    angle_u = 2400;
                }
            }else if(action2[0]=='D'){
                assert(angle_d == 0);
                if(action2[1] == '\''){
                    angle_d = 2400;
                }else if (action2[1] == '2'){
                    angle_d = 1600;
                }else{
                    angle_d = 800;
                }
            }
        }
        // 执行动作
        cube_ud(angle_u, angle_d);
    }
    
    return face_after_tweak;
}
// 按照求解结果执行动作
char cube_tweak_str(char face_on_stepper_2, char *str)
{
    char *p = str;
    char *p_dst = str;
    int count = 0;
    // 整理格式,顺便统计数量
    while(1)
    {
        if(*p == 'U' || *p == 'R' || *p == 'F' || *p == 'D' || *p == 'L' || *p == 'B'){
            p_dst[count * 2] = *p;
            p++;
            p_dst[count * 2 + 1] = *p;
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
    p_dst[count*2] = 0;
    for(int i=0;i<count;){
        if(i <= count - 2 && 
          ((p_dst[i*2] == 'U' && p_dst[i*2+2] == 'D') || 
          (p_dst[i*2] == 'D' && p_dst[i*2+2] == 'U'))){
                //printf("%c%c %c%c\n",p_dst[i*2],p_dst[i*2+1],p_dst[i*2+2],p_dst[i*2+3]);
                face_on_stepper_2 = cube_tweak(face_on_stepper_2, p_dst+i*2, p_dst+i*2+2);
                i += 2;
        }else{
            //printf("%c%c\n",p_dst[i*2],p_dst[i*2+1]);
            face_on_stepper_2 = cube_tweak(face_on_stepper_2, p_dst+i*2, 0);
            i += 1;
        }
    }
    //face_on_stepper_2 = cube_tweak(face_on_stepper_2, "F0", 0);// 还原完成后,调整魔方朝向,这个不是必须的
    return face_on_stepper_2;
}

// ----------------------- 软件模拟I2C ------------------------------------
// 不知为何硬件I2C用不了,从设备不回复ACK
#define SDA0()      gpio_set_dir(i2c_gpio[i2c][0], GPIO_OUT)
#define SDA1()      gpio_set_dir(i2c_gpio[i2c][0], GPIO_IN)
#define SDA_READ()  gpio_get(i2c_gpio[i2c][0])  
#define SCL0()      gpio_set_dir(i2c_gpio[i2c][1], GPIO_OUT)
#define SCL1()      gpio_set_dir(i2c_gpio[i2c][1], GPIO_IN)
#define I2C_DELAY() sleep_us(1)
#define I2C_ACK     0
#define I2C_NACK    1
#define I2C_READ    1
#define I2C_WRITE   0
void i2c_start(uint i2c)
{
    // SCL -----__
    // SDA ---____
    SDA1();
    I2C_DELAY();
    SCL1();
    I2C_DELAY();
    SDA0();
    I2C_DELAY();
    SCL0();
    I2C_DELAY();
}
void i2c_stop(uint i2c)
{
    // SCL _------
    // SDA ____---
    SDA0();
    I2C_DELAY();
    SCL1();
    I2C_DELAY();
    SDA1();
    I2C_DELAY();
}
bool i2c_send(uint i2c, uint8_t date)
{
    bool ack;
    int i;
    for(i=0;i<8;i++){
        if(date & 0x80)
            SDA1();
        else
            SDA0();
        I2C_DELAY();
        SCL1();
        I2C_DELAY();
        SCL0();
        date = date << 1;
    }
    SDA1();//释放总线,等待ACK
    I2C_DELAY();
    SCL1();
    I2C_DELAY();
    ack = SDA_READ();
    SCL0();
    //I2C_DELAY();
    return ack;
}

uint8_t i2c_get(uint i2c, bool ack)
{
    uint8_t get;
    int i;
    SDA1();
    I2C_DELAY();
    for(i=0;i<8;i++){
        get = get << 1;
        SCL1();
        I2C_DELAY();
        get = get | SDA_READ();
        SCL0();
        I2C_DELAY();
    }
    if(ack)
        SDA1(); // NACK
    else
        SDA0(); // ACK
    I2C_DELAY();
    SCL1();
    I2C_DELAY();
    SCL0();
    return get;
}
// ----------------------- TCS3472驱动 ------------------------------------
// 确认传感器通信
// 返回true表示ID正确
bool tcs3472_check_id(int i2c)
{
    i2c_start(i2c);
    i2c_send(i2c, (0x29<<1) | I2C_WRITE);
    i2c_send(i2c,0x80 | 0x12);// 选择ID寄存器
    i2c_start(i2c);
    i2c_send(i2c, (0x29<<1) | I2C_READ);
    uint8_t id = i2c_get(i2c, I2C_NACK);
    i2c_stop(i2c);
    return id == 0x4D;
}

void tcs3472_enable(int i2c)
{
    // ATIME = 0xFF
    i2c_start(i2c);
    i2c_send(i2c, (0x29<<1) | I2C_WRITE);
    i2c_send(i2c, 0x80 | 0x01);
    i2c_send(i2c, 0xFF); 
    i2c_stop(i2c);
    // WTIME = 0xFF
    i2c_start(i2c);
    i2c_send(i2c, (0x29<<1) | I2C_WRITE);
    i2c_send(i2c, 0x80 | 0x03);
    i2c_send(i2c, 0xFF); 
    i2c_stop(i2c);
    // AGAIN = 0x1
    i2c_start(i2c);
    i2c_send(i2c, (0x29<<1) | I2C_WRITE);
    i2c_send(i2c, 0x80 | 0x0F);
    i2c_send(i2c, 0x1); 
    i2c_stop(i2c);

    // ENABLE = PON
    i2c_start(i2c);
    i2c_send(i2c, (0x29<<1) | I2C_WRITE);
    i2c_send(i2c, 0x80 | 0x00);// 选择ENABLE寄存器
    i2c_send(i2c, 0x01); // PON
    i2c_stop(i2c);
    // delay >= 2.4ms
    sleep_ms(3);
    // ENABLE = PON + AEN
    i2c_start(i2c);
    i2c_send(i2c, (0x29<<1) | I2C_WRITE);
    i2c_send(i2c, 0x80 | 0x00);// 选择ENABLE寄存器
    i2c_send(i2c, 0x03);// PON + AEN
    i2c_stop(i2c);
}

void tcs3472_dump(int i2c, uint16_t *r, uint16_t *g, uint16_t *b)
{
    int i;
    i2c_start(i2c);
    i2c_send(i2c, (0x29<<1) | I2C_WRITE);
    i2c_send(i2c,0xA0 | 0x16);// 选择RDATAL寄存器,并且开启地址累加
    i2c_start(i2c);
    i2c_send(i2c, (0x29<<1) | I2C_READ);
    *r = i2c_get(i2c, I2C_ACK);
    *r |= i2c_get(i2c, I2C_ACK) << 8;
    *g = i2c_get(i2c, I2C_ACK);
    *g |= i2c_get(i2c, I2C_ACK) << 8;
    *b = i2c_get(i2c, I2C_ACK);
    *b |= i2c_get(i2c, I2C_NACK) << 8;
    i2c_stop(i2c);
}
void sample_color(uint32_t g, uint16_t *buffer)
{
    gpio_put(PICO_DEFAULT_LED_PIN, 1);
    if(g < 0xFFFF){
        uint16_t r0,g0,b0,r1,g1,b1;
        tcs3472_dump(0, &r0, &g0, &b0);
        tcs3472_dump(1, &r1, &g1, &b1);
        buffer[(g>>8) * 3 + 0] = r0;
        buffer[(g>>8) * 3 + 1] = g0;
        buffer[(g>>8) * 3 + 2] = b0;
        buffer[(g&0xff) * 3 + 0] = r1;
        buffer[(g&0xff) * 3 + 1] = g1;
        buffer[(g&0xff) * 3 + 2] = b1;
        //printf("ID=(%u,%u) r0=%u g0=%u b0=%u r1=%u g1=%u b1=%u\n",g>>8,g&0xFF,r0,g0,b0,r1,g1,b1);
    } 
    gpio_put(PICO_DEFAULT_LED_PIN, 0);
}
// ---------------------------------调试用的----------------------------------------------
#if 0
static void show_time(char *s, absolute_time_t t1)
{
    absolute_time_t t2 = get_absolute_time();
    int tims_cost_in_us = (int)absolute_time_diff_us(t1, t2);
    printf("%s Totel time cost %d.%03dms\n", s, tims_cost_in_us/1000, tims_cost_in_us%1000);
}
// cube_lfrb(800); cube_lfrb(1600);
// flip_cube(800); flip_cube(1600); flip_cube(2400);
// cube_ud(0,800); cube_ud(0,1600); cube_ud(0,2400);
// cube_ud(800,2400); cube_ud(1600,2400); cube_ud(800,1600);
static void debug_stepper_1(void)
{
    //cube_get_color(color_buffer);
    absolute_time_t t1;
    t1 = get_absolute_time();
    cube_lfrb(800);
    show_time("flip_cube(800)", t1);
    sleep_ms(400);

    t1 = get_absolute_time();
    cube_lfrb(1600);
    show_time("cube_ud(0,2400)", t1);
    sleep_ms(400);

    cube_lfrb(-800);
    sleep_ms(400);
    cube_lfrb(-1600);
    sleep_ms(400);
}
const char test_case[50][55]=
{
    "URBUUURBFDBDURFDFDURLBFLBDRUBFRDLFRBRFBLLLUFLLUFDBDRDL",
    "LDFLUBLLUFULFRRRRFBBLUFRBDFRBDUDBRDRDFUDLFDLUDRBUBFULB",
    "UFULUBDFLDDLLRRLRRRRBUFFLRUFUBLDDDUUBDBLLFRUDFDRBBBFBF",
    "UULDURUDBDFBURLDBRLLRLFRFDRLBFRDRLLBRFBULUDDUDBFBBFUFF",
    "BDLUURLDRUBDDRFLLRBRBLFFDUFFRULDBLFUDBUFLUBDRFBRRBUFLD",
    "RULDUFRLULDDBRFRRFDDBUFRDLBRUULDUBRUFRFBLFDBBFBULBDLFL",
    "RBDBULFFDLDLBRUBBBLRFUFLULRRFDFDDBDLDRUULLUDFBUFRBFURR",
    "BLLUULBULUUDLRDBDBDBFLFFDUUFRRRDRFFLLFRDLBRBRFDDBBFURU",
    "BFDBUBFDUFDLRRBRLLRRLLFUUDDLLBFDURDDRLDULFUUBBRURBBFFF",
    "DBFRUUUURULLLRBBRDFFBDFDFLLUFUUDUDFLFFLDLBBBRDLRRBRBDR",
    "UFRUUUFRLFLUDRDBDDRFDBFRBBDUULBDLRUBFRDBLDBLLFLLFBRRFU",
    "LBRFUDRBDLLUFRLRDRDDFLFDBBDULBUDRBBBURFULUDFLFRFFBRUUL",
    "DUDUUBFDBLRBFRBUBFURDDFLRFBUULLDULFDLLRFLLFBBRRFDBDRRU",
    "BBDFUULDFRRLRRRDURBBDLFDULFRDLUDFFDDRLURLUUBFBLUFBBBFL",
    "BBBFULRUBUURFRRRDFDFLUFDLRUUUFFDRLDLRRFLLBBLFDLUBBDDBD",
    "RURFULBFBUBBDRRFDLDLLFFRLRRFUUDDLBUFDRRLLDDBDUBFBBUUFL",
    "LBRDUDRBLDLUFRUULLBUBRFUUFRLRBDDFFUFFFDBLBDRBFDDRBLULR",
    "DULLUFLBBDRUFRBRURDRRRFLRDBFFUDDLFFFLDBULDUBUBRFLBBDUL",
    "BFFUULRBDLDURRLDBRDLFFFURDLFRBBDRUUBDBFULDBDULRRFBLUFL",
    "LLRRULRBRFBFRRDBRUUUUUFFFFDLDLBDDDFBFUBBLLBDUDFDLBRLUR",
    "LBBLULLDFUFUURURUUURRRFBRBFBRDLDFLDBDDFFLUDBDRDBLBRLFF",
    "RUUBUUDDUBLLLRRFRUFRRBFDLULDRDFDBUUFBDRFLLBDBFBDFBLRFL",
    "RRDDUFBBBUDBRRDDBRLLLFFULLRUUFRDUDFUULDFLUFDFRBBBBRFLL",
    "LFBLUDUBFLRDRRFRFRBUURFUFBURDFUDRBDDFBRLLBLLDLLDUBDBFU",
    "UDDFURBURFFFBRLRUURFURFDBLFLBDDDRDLBFLUBLBBRDLFLDBULUR",
    "LDUBURFBLDUBBRUBLDURBDFLDFURRRLDFLURFDRLLFFUBLRDBBDFFU",
    "UFDBUBRRLULLBRRDLFUUFDFDUDFLRLUDDDRDRUBLLFBFBBLFBBURFR",
    "RRLUUDUUFLRUFRLLFURLURFDRBBDDDDDRLURBBFBLUDLFBBDFBLBFF",
    "RFURULBLLFBLURUBUDUDDDFRFLUUFRBDBDDLDDLFLFBRRFRFLBUBBR",
    "BRUBUUULLURBRRULDRRDFUFFFRBDDDFDBFLBDUFFLFDDLLBRLBLUBR",
    "LRBBUFRLRUULFRRLRLBFBBFDUUFRLUDDUFDDDLDULDDRFUFBBBBFLR",
    "DLRDUDDUULFUBRRLBRLFBRFRFLBRUDBDLLDBRBFDLFFUDFFBUBLURU",
    "FBUBUFRLLDDRLRDDUBDFBDFUBUFDFLFDBFULULFDLRURRBRRBBLURL",
    "FFFBUULDBRFRLRFFRUDBULFUBUUDBLRDDFDBDLBRLDDURULLRBBLFR",
    "UUBFUDLRBURDRRLRFFUULLFFDLURBFBDDDFLBUFDLDRRBLBRUBBDLF",
    "LLDBUFUBRFDFFRUBBRFLUBFRDLURURUDUBRFDRLLLDLRBLDBFBFDDU",
    "DFDLURBLBRBRFRUFLLUDUUFRBDRDFUBDFLUBLBLRLBFDRFUFRBDDLU",
    "LBULUBULLBRFDRDFUBBDUUFBDFDRRRBDFFDDFFRULRULBRUDFBLLRL",
    "FRURULRDLUFBDRBDUUDBFUFLDLLLUBLDRBRRUBFDLFRBFLFRUBFBDD",
    "DRBFUULLUBFLRRLUBDDURBFURBLDUFDDDFFFRRBRLLRFFUBBDBDLLU",
    "BFFDURBDBDFLBRRLDFRLRUFLDUFBBULDBRDDUFURLLUFLDULBBURRF",
    "UBRUUBUFFRDURRBDULFDDDFFLUBDBRLDRUDDLFLLLRRFBFRBLBUFLB",
    "FFLLUFLDURDDRRLDRLUBFRFUULBBBRFDDDFURUFBLBLURFLDDBUBRB",
    "LLURUURFDFBLRRDUBFURRBFURRFBBRDDDLLLDDBFLLULDFFBFBUDUB",
    "BFBRUURDDFRDFRBBUUFBRDFLLRLFFUUDFFURDDUBLLULDLDRRBLBBL",
    "BDDUUDDFRFRFFRFRLRRRULFLDRFBBDUDBFBBURBULDULLLFLUBBUDL",
    "UBURUUBUURLBFRURDLDBFFFDRLBDBUBDLBFDFFRDLUDDFLRLRBRFLL",
    "BDBRULULUBUDFRUDRFLFRUFRULRRDFFDBLBUDDBBLBDUFLFRRBLLDF",
    "FUFLUFRUBUDLFRRBLBFRLBFLLRUDDRUDDUBRRBUFLRLLBDBDFBUDDF",
    "URRLULRRULFFURRRDLFDBUFLLBDFDBFDLBFBFBURLBUUDDFLUBBDDR",
};
static void debug_solve()
{
    int totel_time = 0;
    for(int i=0; i<50; i++){
        absolute_time_t t1 = get_absolute_time();
        int steps = solve(test_case[i], solution_str);
        absolute_time_t t2 = get_absolute_time();
        int tims_cost_in_ms = (int)absolute_time_diff_us(t1, t2) / 1000;
        printf("Find %d step solution in %dms: %s\n", steps, tims_cost_in_ms, solution_str);
        totel_time += tims_cost_in_ms;
    }
    printf("totel time cost %dms\n", totel_time);
}
#endif
// 使用CubeExplorer5.0生成的WCA打乱公式(最长21步)
const int scramble_count = 30;
const char scramble_string[30][21*3 + 1]=
{
    "D R2 U2 L2 B2 D F2 D L2 B2 D L2 F' U B R F2 U L D2 R2 ",
    "B2 F2 D L2 D B2 U' R2 B2 L2 U2 L B' L2 R2 U' R2 U2 R2 B' R ",
    "D2 B' R2 D2 B' U2 B' D2 B2 R2 B U' F' L2 F2 R D' B U' R B' ",
    "B2 D2 F2 D' B2 U B2 U R2 D' L2 U' R D F2 U2 R' F' R F2 R' ",
    "F2 U2 L' B2 U2 F2 L2 F2 U2 R D' B' U' L' F' D F2 D B D' ",
    "R' F2 L2 D2 U2 B2 L U2 L2 R' U' F' L2 B D2 R D2 R2 U F R2 ",
    "D L2 F2 L2 D F2 U' R2 U2 B2 F2 R' F' L' B' U B2 D' B D2 U' ",
    "D2 R2 B2 D2 B' F' R2 D2 B L2 D' L' B D F D B2 R' B U2 R' ",
    "D2 L2 F2 R2 U' R2 D2 F2 D' U2 R2 B L2 F L F R D2 U' B L ",
    "R2 D2 F' L2 R2 B' F' R2 B L' B2 D' B R' F' D B' F2 L2 D2 F' ",
    "D U2 F2 L2 B2 L2 D2 F2 U2 L F U L B2 D2 L' B' L2 D' R D ",
    "R' U2 L D2 R2 D2 F2 R' B2 R D2 U' L' D' B2 L R' B D U' F' ",
    "R2 F' R2 U2 F2 D2 R2 B D2 L2 D2 R2 D' B2 R F' D2 B2 D2 F' D2 ",
    "R2 U2 F' U2 L2 B' F' U2 F R2 F2 L' D L B' R' D' F L2 D2 U2 ",
    "D2 B D2 L2 D2 R2 U2 F' D2 F' L F U' B F L B2 L' U R2 U ",
    "R2 B2 F2 R D2 F2 D2 L U2 L B' F U L2 F D2 L' D' R' F2 U' ",
    "F2 D U2 B2 R2 U R2 D' U2 L2 U2 L D B' U2 R' D2 R2 D2 B D ",
    "L2 B2 L F2 U2 B2 F2 R2 D2 L B2 F' U' L2 D2 B' R2 U2 F D L' ",
    "D F2 U' F2 R2 U F2 R2 D2 B2 F2 L' B' R' U F' U2 L F R' F ",
    "L D2 L' U2 F2 D2 B2 R B2 L' B2 L2 D B L U2 F' U R2 F' D ",
    "U2 L2 R2 F D2 B' F2 D2 R2 B2 U' B2 F' L' D F' L2 R' U' F' ",
    "R2 U' R2 U F2 D' R2 D' R2 D2 F R B2 R2 B2 U B L2 F D2 U2 ",
    "L' F2 U2 L2 F2 L' B2 D2 R' U2 F R B2 D R U' L U B2 L2 U2 ",
    "R2 U2 B F2 D2 L2 F2 L2 F2 U2 L' F2 D F' L B' R F L' F L2 ",
    "D2 U R2 U' F2 L2 F2 U' F2 R' F2 D B2 R' B2 D B R' U B2 U2 ",
    "D B2 U2 L2 B2 D B2 L2 B2 L2 D' L2 F' R' B U' F' R D B2 D2 ",
    "L2 R D2 U2 B2 U2 R' D2 L' R' D' B F' D L U B D2 F R2 D2 ",
    "B2 D2 U2 L' F2 L R2 F2 R2 D2 U R2 B L' U2 F2 R D' U' R2 B' ",
    "U2 F2 D' B2 F2 D B2 U R2 U' B' L2 R F2 R D2 U2 F2 U' L U2 ",
    "L R2 B2 U2 L U2 F2 U2 B2 F2 R2 D' F D B2 R' U B2 F2 U R' ",
};

// ---------------------------------主程序----------------------------------------------
int main(void)
{
    stdio_init_all();
    printf("Init.\n");
    uart_init(uart0, BAUD_RATE);
    init_io();
    sleep_ms(100);
    flash_init();
    // flash自检和刷写
    if(!gpio_get(BUTTON_0)){
        //do_flash_test();
        if(flash_crc_all_table() != 0xA8093698UL){
            printf("FLASH CRC ERROR. Use prog_flash tool.\n");
            // 2020-07-10改为使用串口传输,USB连续传输数据不稳定,找不到BUG原因
            flash_program_via_stdio();
        }else{
            printf("FLASH CRC ok.\n");
        }
    }
    // multicore_launch_core1(main_core1);
    // 步进电机回零
    gpio_put(SPEPPER_EN, 0);
    stepper_zero();
    // 在此之后,使用PIO控制电机
    init_stepper_ctrl();
    stepper_move_none_block(0, STEPPER0_OFFSET, 100*RPM,300*RPM,150000);
    stepper_move_none_block(1, STEPPER1_OFFSET, 100*RPM,300*RPM,150000);
    stepper_move_none_block(2, STEPPER2_OFFSET, 100*RPM,300*RPM,150000);
    // 检查颜色传感器是否正常,并且初始化
    for(int i=0; i<2; i++){
        if(tcs3472_check_id(i)){
            tcs3472_enable(i);
            printf("tcs3472_check_id(%d) pass\n",i);
        }else{
            printf("tcs3472_check_id(%d) fail\n",i);
            while (1);            
        }
    }
    bool auto_test = false;
    int scramble_index = 0;
    char face = 'F';
    while (1) 
    {
        // 等待按键按下
        while(!auto_test){
            sleep_ms(10);
            if(!gpio_get(BUTTON_1)){
                sleep_ms(10);
                if(!gpio_get(BUTTON_1)){
                    absolute_time_t t_but_1 = get_absolute_time();
                    while(!gpio_get(BUTTON_1));
                    absolute_time_t t_but_2 = get_absolute_time();
                    int time = (int)absolute_time_diff_us(t_but_1, t_but_2);
                    if(time < 500*1000){
                        // 按下时间小于0.5s
                        speed_factor = 1.0f;
                        accel_factor = 1.0f;
                    }else{
                        // 按下时间大于等于0.5s
                        speed_factor = 0.125f;// 降速设置
                        accel_factor = 0.125f * 0.125f;
                    }
                    break;
                }
            }
            if(!gpio_get(BUTTON_0)){
                sleep_ms(10);
                if(!gpio_get(BUTTON_0)){
                    auto_test = true;
                    scramble_index = 0;
                    break;
                }
            }
        }
        //debug_solve();///
        //continue;///
        // 打乱魔方
        if(auto_test){
            face = 'F';
            printf("test case %d, ",scramble_index);
            absolute_time_t tt1 = get_absolute_time();
            strcpy(solution_str, scramble_string[scramble_index]);
            face = cube_tweak_str(face, solution_str);
            absolute_time_t tt2 = get_absolute_time();
            printf("Scramble time cost: %dms\n", (int)absolute_time_diff_us(tt1, tt2) / 1000);
            face = cube_tweak(face, "F0", 0);// 完成后,调整魔方朝向
            sleep_ms(500);
        }
        // 采集每一块的颜色
        absolute_time_t t1,t2,t3,t4;
        t1 = get_absolute_time();
        cube_get_color(color_buffer);
        // 识别颜色
        color_detect(color_buffer, cube_str);
        for(int i=0; i<54*3; i++){
            // 用完清理数据,这很重要,这BUG调了一小时
            // 魔方中间块对应的6个数据,每次重新采样是不会更新的
            color_buffer[i] = 0;
        }
        if(cube_str[0] != 0){
            printf("%s,", cube_str);
        }else{
            printf("Error,", cube_str);
        }
        t2 = get_absolute_time();
        printf("%dms,", (int)absolute_time_diff_us(t1, t2) / 1000);
        // 求解魔方
        char *solution;
        if(cube_str[0] != 0){
            int steps = solve(cube_str, solution_str);
            if(steps >= 0){
                printf("%s(%d steps),", solution_str, steps);
                solution = solution_str;
            }else{
                printf("Cube Error,");
                solution = 0;
            }
            //multicore_fifo_push_blocking((uint32_t)solution_str);
        }else{
            printf("Color Error,");
            solution = 0;
        }
        t3 = get_absolute_time();
        printf("%dms,", (int)absolute_time_diff_us(t2, t3) / 1000);
        // 计算完成后执行对应的动作
        face = 'F';
        if(solution != 0 && solution[0] != 0){
            face = cube_tweak_str(face, solution);
        }
        t4 = get_absolute_time();
        printf("%dms,", (int)absolute_time_diff_us(t3, t4) / 1000);
        printf("%dms\n", (int)absolute_time_diff_us(t1, t4) / 1000);
        if(auto_test){
            face = cube_tweak(face, "F0", 0);// 完成后,调整魔方朝向
            sleep_ms(500);
            scramble_index++;
            if(scramble_index >= scramble_count){
                scramble_index = 0;
                auto_test = false;
            }
        }
    }
    return 0;
}

