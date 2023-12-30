$fa=6;
$fs=0.5;
use <main.scad>



// 打印1个
base_board();

// 打印1个,和V1.1一样
//cube_spin();

// 打印1个,和V1.1一样
// rotate([180,0,0]) fan_holder();

// 建议使用 爪子-铣加工.scad中的铣加工版本
// 移除3D打印版本，如果需要，可将main.scad中的相关代码取消注释
//claw_3d_print();

// 打印1个
//stepper_shelf(0, 0);

// 打印1个
//stepper_shelf(0, 1);

// 打印1个
//rotate([0,90,0])cube_block_move();
// 打印1个
//cube_block_fixed();

// 垫柱，打印不同尺寸的备用
/*
for(i=[0:7]){
    translate([10*0,i*10,0])cyl(2);
    translate([10*1,i*10,0])cyl(3);
    translate([10*2,i*10,0])cyl(4);
    translate([10*3,i*10,0])cyl(5);
}
*/