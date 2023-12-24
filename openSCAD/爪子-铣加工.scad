
//fa最小角度 fs最小尺寸
$fa=0.5;
$fs=0.2;
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
claw();