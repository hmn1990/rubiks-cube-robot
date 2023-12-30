use <stepper.scad>
use <rubik_cube.scad>

//rotate([90,0,0]) stepper42();

$fa=6;
$fs=0.5;
// 滚花螺母孔径，需要比滚花螺母直径略小一点
m3_nut_hole_d = 4.4;
// m3螺丝孔，直接把螺丝拧到塑料件里
m3_hole_d = 2.8;
// 魔方旋转角度
cube_rotate=0; //0 78 45


// ------------------------------------------------------------------
//夹持魔方的结构，需要根据使用的魔方调整形状
//对强度要求较高，使用FDM原理的3D打印机，需要注意打印方向，4个孔面向X或者Y方向都行
//可以微调尺寸，解决3D打印ABS小尺寸零件时变形的问题
//根据打印机情况和使用的耗材类型调整（使用PLA的话不用调，基本不变形，但是强度很低）
//claw_3d_print();
/*
claw_size_x=18.7+0.1;
claw_size_y=18.7-0.5;
claw_r=5;

module claw_3d_print()
{
    rotate([90,0,0]){
        // 固定魔方的爪子，打印2个
        claw();
        //支撑，打完割掉
        translate([-claw_size_x/2,-claw_size_y/2, 18]) cube([claw_size_x,claw_size_y,1]); 
    }
}

module claw(){
    difference(){
        union(){
            translate([-claw_size_x/2, -claw_size_y/2, 0 ]) cube([claw_size_x,claw_size_y,12]);
            translate([-claw_size_x/2, -claw_size_y/2, 12]) claw_1(0);
            translate([claw_size_x/2,  -claw_size_y/2, 12]) claw_1(1);
            translate([claw_size_x/2,  claw_size_y/2,  12]) claw_1(2);
            translate([-claw_size_x/2, claw_size_y/2,  12]) claw_1(3);
        }
        union(){
            for(i=[0:3]){
                rotate([0,0,45+90*i]) {
                    translate([8,0,0]) cylinder(h=7, d=m3_nut_hole_d);
                }
            }
        }
    }
}

module claw_1(dir)
{
    rotate([0,0,dir*90]){
        difference(){
            cube([claw_r, claw_r, 6]);
            translate([claw_r, claw_r, 0]) cylinder(h=6,r=claw_r);
        }
    }
}
*/
// ------------------------------------------------------------------
//夹持魔方的结构，需要根据使用的魔方调整形状，铣加工版本
// m3螺丝孔，需要使用丝锥攻丝
m3_hole = 2.5;
claw_size_x=18.8;
claw_size_y=18.8;
claw_r=5.2;
claw_height=5;
module claw(){
    difference(){
        union(){
            translate([-claw_size_x/2, -claw_size_y/2, 0 ]) cube([claw_size_x,claw_size_y,17]);
            translate([-claw_size_x/2, -claw_size_y/2, 17]) claw_1(0);
            translate([claw_size_x/2,  -claw_size_y/2, 17]) claw_1(1);
            translate([claw_size_x/2,  claw_size_y/2,  17]) claw_1(2);
            translate([-claw_size_x/2, claw_size_y/2,  17]) claw_1(3);
            translate([0, 0, -7]) cylinder(h=7,r=5);
        }
        // 电机轴
        translate([0, 0, -7 ]) cylinder(h=13,r=2.5);
        // 磁铁位置
        translate([8.1/sqrt(2)+0.1, 8.1/sqrt(2)+0.1, 0]) cylinder(h=3,r=3.1);
        // 顶丝孔
        translate([0,0,-7+3]) rotate([0,90,0]) cylinder(h=50,r=m3_hole/2,center=true);
    }
    
}
// 一个爪子
module claw_1(dir){
    rotate([0,0,dir*90]){
        difference(){
            cube([3.5, 3.5, claw_height]);
            translate([claw_r, claw_r, 0]) cylinder(h=claw_height,r=claw_r);
        }
    }
}
// ------------------------------------------------------------------
// 可活动步进电机支架 打印2个
module m3_hole(len1=10, len2=3)
{
    union(){
        translate([0,0,len2]) cylinder(h=len1, d=3.4);
        cylinder(h=len2, d=6);
    }
}

module stepper_shelf(style=0,sensor=0){
    union(){
        // 固定步进电机
        translate([0,0,42/2+0.5]) rotate([90,0,0]){
            difference(){
                hull(){
                    dst = 42/2-4;
                    translate([-dst,dst,0]) cylinder(h=8,r=4);
                    translate([dst,dst,0])  cylinder(h=8,r=4);
                    translate([-dst,-dst-0.5,4]) cube(8,center=true);
                    translate([dst,-dst-0.5,4])  cube(8,center=true);
                }
                translate([31/2, 31/2, 0])  m3_hole(10,3);
                translate([-31/2, 31/2, 0]) m3_hole(10,3);
                translate([31/2, -31/2, 0]) m3_hole(10,3);
                translate([-31/2, -31/2, 0])m3_hole(10,3);
                translate([0, 0, 8-2-0.5]) cylinder(h=10,d=22+1);
                cylinder(h=20,d=7,center=true);
                // 霍尔安装位置
                if(sensor == 0){
                    translate([(42-10.2)/2, 0, 4/2])cube([10.2,10.2,4],center=true);
                }else{
                    translate([-(42-10.2)/2, 0, 4/2])cube([10.2,10.2,4],center=true);
                }
            }
        }
        // 滑块
        if(style == 0){
            size_x = 42;
            size_y = 48+8;
            height = 10;
            r1 = height/2/sin(60);
            difference(){
                union(){
                    translate([-size_x/2+r1/2,-size_y/2,-height/2]) rotate([90,0,0]) 
                        cylinder(size_y, r=r1, center=true, $fn=6);
                    translate([size_x/2-r1/2,-size_y/2,-height/2]) rotate([90,0,0]) 
                        cylinder(size_y, r=r1, center=true, $fn=6);
                    translate([0,-size_y/2,-height/2]) 
                        cube([size_x,size_y,10],center=true);
                }
                translate([ 15,0.1,-2.5]) rotate([90,0,0]) cylinder(h=5,d=m3_nut_hole_d);
                translate([-15,0.1,-2.5]) rotate([90,0,0]) cylinder(h=5,d=m3_nut_hole_d);
            }
        }
        // 固定电机，不可活动
        if(style == 1){
            size_x = 42;
            size_y = 48+8+5;
            height = 20;
            translate([0,-(size_y-40)/2+3,-height/2])
                cube([size_x,15,height],center=true);
            translate([0,-52,-height/2])
                cube([size_x,8,height],center=true);
        }
    }
}


// ------------------------------------------------------------------
// 用于旋转魔方的十字结构
// 对强度要求较高，适当提高填充和壁厚
module cube_spin()
{
    difference(){
        union(){
            translate([0,0,2.5]) cube([72,10,5],center=true);
            translate([0,0,2.5]) rotate([0,0,90]) cube([72,10,5],center=true);
            cylinder(h=5,d=27);
            for(i=[0:3]){
                rotate([0,0,90*i]) {
                    //translate([64/2+5,-5,0]) cube([5,10,23]);
                    hull(){
                       translate([38,5-1.5,0]) cylinder(h=23,r=1.5);
                       translate([38,-5+1.5,0]) cylinder(h=23,r=1.5);
                       translate([38-2,5-1.5,0]) cylinder(h=23,r=1.5);
                       translate([38-2,-5+1.5,0]) cylinder(h=23,r=1.5);
                    }
                }
            }
        }
        for(i=[0:3]){
            rotate([0,0,45+90*i]) {
                translate([8,0,-0.5]) cylinder(h=5, d=m3_nut_hole_d);
            }
        }
        translate([0,0,-0.5]) cylinder(h=5, d=5.6);
    }
}

// ------------------------------------------------------------------
// 使魔方中间层只能单向旋转的部件
module cube_block(){
    cube_block_fixed();
    cube_block_move();
}
module cube_block_fixed(){
    difference(){
        union(){
            // 主体
            difference(){
                translate([0,1,-1.5]) cube([17,32,9],center=true);
                translate([0,3,0]) cube([9,18,99],center=true);
                rotate([0,-90,0]) cylinder(h=99,d=2.4,center=true);
            }
            // 底部限位
            translate([-5,7,-6]) cube([10,9,4.5]);
        }
        hull(){
            translate([0,8,-1.5])rotate([0,-90,0]) cylinder(h=99,d=1.8,center=true);
            translate([0,8+1.6,-1.5])rotate([0,-90,0]) cylinder(h=99,d=1.8,center=true);
        }
    }
    // 弹簧固定器
    difference(){
        translate([-5,-15,0]) cube([10,5,10]);
        translate([0,0,6]) rotate([90,0,0]) cylinder(h=99,d=m3_nut_hole_d);
    }
    // 固定螺丝孔，两个M3通孔
    difference(){ 
        translate([0,-10.5,-1.5]) cube([34,9,9],center=true);
        translate([-13,30,-1.5]) rotate([90,0,0]) cylinder(h=99,d=3.4);
        translate([13,30,-1.5]) rotate([90,0,0]) cylinder(h=99,d=3.4);
    }
}
module cube_block_move(){
    translate([4,0,0]) rotate([0,-90,0]) difference(){
        union(){
            hull(){
                translate([18-6,0,0]) cylinder(h=8,d=6);
                cylinder(h=8,d=6);
            }
            hull(){
                translate([-3,5,0]) cube([6,5,8]);
                cylinder(h=8,d=6);
            }
        }
        translate([-3-0.2,6-0.2,0-0.2]) cube([2+0.4,5+0.4,8+0.4]);
        cylinder(h=99,d=2.4,center=true);
    }
}

// 滑轨
module rail(){
    dx=60;
    dy=75;
    height=10;
    height_bot=9;
    gap=0.2;//滑块间隙,0.4 -> 0.2
    cut_d=42+2*5*tan(30)+gap*2;
    translate([0,-dy/2+12,-height/2]) {
        union(){
            difference(){
                translate([0,0,-height_bot/2])
                    cube([dx,dy,height+height_bot],center=true);
                rotate([90,0,0])
                    cylinder(h=99,d=cut_d,center=true,$fn=6);
            }
            translate([0,0,-height/2-height_bot/2-gap/2])
                cube([dx,dy,height_bot-gap],center=true);
            translate([0,34,-3])
                difference(){
                    cube([dx,7,25],center=true);
                    translate([0,-3, 5]) rotate([90,0,0]) {
                        cylinder(h=1.2,d=12.3,center=true);
                    }
                    translate([-13,-3, 5]) rotate([90,0,0]) {
                        cylinder(h=1.2,d=12.3,center=true);
                    }
                    translate([13,-3, 5]) rotate([90,0,0]) {
                        cylinder(h=1.2,d=12.3,center=true);
                    }
                }
        }
    }
}

// ------------------------------------------------------------------
module round_cube(x,y,z,radius){
    hull(){
        translate([radius,radius,0])cylinder(h=z,r=radius);
        translate([-radius+x,radius,0])cylinder(h=z,r=radius);
        translate([radius,-radius+y,0])cylinder(h=z,r=radius);
        translate([-radius+x,-radius+y,0])cylinder(h=z,r=radius);
    }
}
// 大底板
module base_board(){
    dx = 148;
    dy = 241;
    // 主体
    difference(){
        union(){
            translate([-30,-dy/2,-25-3])
                 cube([dx,dy,6+3]);
            translate([46,-118,-25]){
                translate([0+2.5 ,20+2.5 ,0]) cylinder(h=10,d=8);
                translate([0+2.5 ,85+2.5,0]) cylinder(h=10,d=8);
                translate([65+2.5,20+2.5 ,0]) cylinder(h=10,d=8);
                translate([65+2.5,85+2.5,0]) cylinder(h=10,d=8);
            }
        }
        translate([30,35,-30])
             round_cube(120,120,20,15);
        translate([30,-91,-30])
             round_cube(80,56,20,15);
        translate([30,-157,-30])
             cube([99,56,20]);
        // 固定棘轮
        translate([-24,0,-22-1.5]) rotate([0,0,-90]) translate([0,1,0]) 
        {
            cube([17.4,32.4,20],center=true);
            translate([-13,-7+7,0]) rotate([90,0,0]) cylinder(h=99,d=m3_nut_hole_d);
            translate([13,-7+7,0]) rotate([90,0,0]) cylinder(h=99,d=m3_nut_hole_d);
        }
        translate([46,-118,-15]){
            translate([0+2.5 ,20+2.5 ,0]) cylinder(h=50,d=3,center=true);
            translate([0+2.5 ,85+2.5,0]) cylinder(h=50,d=3,center=true);
            translate([65+2.5,20+2.5 ,0]) cylinder(h=50,d=3,center=true);
            translate([65+2.5,85+2.5,0]) cylinder(h=50,d=3,center=true);
        }
    }
    // 滑台
    translate([0,-57.5,0]) {
        rail();
    }
    translate([0,57.5,0]) rotate([0,0,180]) {
        rail();
    }
    // 步进电机支架
    translate([62,0,0]) rotate([0,0,90]){
        stepper_shelf(style=1,sensor=1);
    }
    // 颜色传感器支架
    for(i=[-56/2-10,+56/2+10]){
        translate([-16,i,5]) 
            difference(){
                hull(){
                    rotate([90,45,0]){
                        translate([-25/2,0,0])cylinder(h=4,d=8,center=true);
                        translate([25/2,0,0])cylinder(h=4,d=8,center=true);
                    }
                    translate([0,0,-25]) cube([(25)/sqrt(2)+8,4,1],center=true);
                }
                rotate([90,45,0]){
                    translate([-25/2,0,0])cylinder(h=99,d=m3_hole_d,center=true);
                    translate([25/2,0,0])cylinder(h=99,d=m3_hole_d,center=true);
                    translate([0,4,4]) cube([19, 4, 20],center=true);
                }
            }
    }
    for(i=[-56/2-10,+56/2+10]){
        translate([25,i,-16]) 
            cube([10,4,10],center=true);
    }
}
// 风扇固定架
module fan_holder()
{
    difference(){
        union(){
            translate([-23,0,5.5]) cube([40+42+4,40,3],center=true);
            translate([-64,0,-3]) cube([4,40,15],center=true);
            cube([40,40,8],center=true);
        }
        translate([-48,0,0])cube([50,24,99],center=true);
        translate([-16,-16, 0]) cylinder(h=99,d=m3_hole_d,center=true);
        translate([-16,16, 0]) cylinder(h=99,d=m3_hole_d,center=true);
        translate([16,-16, 0]) cylinder(h=99,d=m3_hole_d,center=true);
        translate([16,16, 0]) cylinder(h=99,d=m3_hole_d,center=true);
        cylinder(h=99,d=35,center=true);
    }
}
// ------------------------------------------------------------------
// 颜色传感器
module color_sensour()
    difference(){
        union(){
            hull(){
                translate([-25/2,0,0])cylinder(h=1.6,d=6,center=true);
                translate([25/2,0,0])cylinder(h=1.6,d=6,center=true);
            }
            cube([20.5,11,1.6],center=true);
            translate([0,4,4]) cube([2.54*7, 2.54, 11],center=true);
        }
        translate([-25/2,0,0])cylinder(h=99,d=3,center=true);
        translate([25/2,0,0])cylinder(h=99,d=3,center=true);
}




module PCBA(){
    //translate([60,-110,-13])
    //    rotate([0,0,90])text("PCBA");
    translate([46,-98,-15]) difference(){
        cube([70,70,1.6]);
        translate([0+2.5 ,0+2.5 ,0]) cylinder(h=50,d=m3_hole_d,center=true);
        translate([0+2.5 ,65+2.5,0]) cylinder(h=50,d=m3_hole_d,center=true);
        translate([65+2.5,0+2.5 ,0]) cylinder(h=50,d=m3_hole_d,center=true);
        translate([65+2.5,65+2.5,0]) cylinder(h=50,d=m3_hole_d,center=true);
    }
}


// ------------------------------------------------------------------
// 
module module_a1(cube_rotate,sensor=0)
{
    translate([0,-8,42/2+0.5]){
        rotate([-90,0,0]){
            //  步进电机
            color("grey") rotate([0,0,180]) stepper42();
            // 法兰联轴器
            //color("white") translate([0,0,25]) rotate([180,0,45+cube_rotate]) flange();
            // 魔方爪
            translate([0,0,20]) 
                color("white") rotate([0,0,cube_rotate]) claw();
        } 
        translate([0,10.6,-24])color("grey")megnet();
    }
    // 步进电机支架
    stepper_shelf(0,sensor);
}

module module_a2(cube_rotate)
{
    translate([0,-8,42/2+0.5]){
        rotate([-90,0,0]){
            //  步进电机
            color("grey") rotate([0,0,180]) stepper42();
            // 法兰联轴器
            color("white") translate([0,0,25]) rotate([180,0,45]) flange();
            // 拧魔方装置
            translate([0,0,25]) 
                cube_spin();
        }
    }
}

module cyl(x){
    difference(){
        cylinder(h=x, d=6);
        cylinder(h=x, d=3.4);
    }
}
module megnet(){
    difference(){
        cube([40,5.3,14],center=true);
        translate([-30/2,0,0])rotate([90,0,0])cylinder(h=99,d=3.5,center=true);
        translate([30/2,0,0])rotate([90,0,0])cylinder(h=99,d=3.5,center=true);
    }
}
// top
translate([0,0,21.5]) rotate([0,cube_rotate,0]) rotate([90,-90,0])rubik_cube();
translate([0,-57.5,0]) {
    module_a1(cube_rotate,sensor=0);
}
translate([0,57.5,0]) rotate([0,0,180]) {
    module_a1(-cube_rotate,sensor=1);
}
translate([62,0,0]){
    rotate([0,0,90]) module_a2(-cube_rotate);
}
translate([85,-41,39]){
    rotate([0,0,-90])fan_holder();
}

base_board();
color("green") 
    PCBA();
translate([-24,0,-22]) rotate([0,0,-90]) 
    cube_block();
color("green") translate([-16,56/2+4,5]) rotate([180+90,180+45,0]) 
    color_sensour();
color("green") translate([-16,-56/2-4,5]) rotate([90,45,0]) {
    color_sensour();
}


