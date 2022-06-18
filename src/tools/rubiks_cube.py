#!/usr/bin/python3
#color_sample 用于规划颜色扫描方式的辅助程序
import random

# UF UR UB UL DF DR DB DL FR FL BR BL UFR URB UBL ULF DRF DFL DLB DBR
# 棱块状态+方向x16
# 与参考方向一致0，与参考方向不一致1
UF = 0
UR = 1
UB = 2
UL = 3
DF = 4
DR = 5
DB = 6
DL = 7
FR = 8
FL = 9 
BR = 10
BL = 11
FU = 0 + 16
RU = 1 + 16
BU = 2 + 16
LU = 3 + 16
FD = 4 + 16
RD = 5 + 16
BD = 6 + 16
LD = 7 + 16
RF = 8 + 16
LF = 9 + 16
RB = 10 + 16
LB = 11 + 16
# 角块状态+方向x8
# 与参考方向一致0，与参考方向不一致1，2
UFR = 0
URB = 1
UBL = 2 
ULF = 3
DRF = 4
DFL = 5
DLB = 6
DBR = 7
FRU = 0 + 8
RBU = 1 + 8
BLU = 2 + 8
LFU = 3 + 8
RFD = 4 + 8
FLD = 5 + 8
LBD = 6 + 8
BRD = 7 + 8
RUF = 0 + 16
BUR = 1 + 16
LUB = 2 + 16
FUL = 3 + 16
FDR = 4 + 16
LDF = 5 + 16
BDL = 6 + 16
RDB = 7 + 16

#                           +------------------------+
#                           | U1(0)   U2(1)   U3(2)  |
#                           |                        |
#                           | U4(3)   U5(4)   U6(5)  |
#                           |                        |
#                           | U7(6)   U8(7)   U9(8)  |
#  +------------------------|------------------------|------------------------+------------------------+
#  | L1(36)  L2(37)  L3(38) | F1(18)  F2(19)  F3(20) | R1(9)   R2(10)  R3(11) | B1(45)  B2(46)  B3(47) |
#                           |                        |                        |                        |
#  | L4(39)  L5(40)  L6(41) | F4(21)  F5(22)  F6(23) | R4(12)  R5(13)  R6(14) | B4(48)  B5(49)  B6(50) |
#                           |                        |                        |                        |
#  | L7(42)  L8(43)  L9(44) | F7(24)  F8(25)  F9(26) | R7(15)  R8(16)  R9(17) | B7(51)  B8(52)  B9(53) |
#  +------------------------+------------------------+------------------------+------------------------+
#                           | D1(27)  D2(28)  D3(29) |
#                           |                        |
#                           | D4(30)  D5(31)  D6(32) |
#                           |                        |
#                           | D7(33)  D8(34)  D9(35) |
#                           +------------------------+

# 用于转换两种表示方式 24个棱块角块 <---> 54个面
edge_to_face = (
    (UF, 'U8', 'F2'),(UR, 'U6', 'R2'),(UB, 'U2', 'B2'),(UL, 'U4', 'L2'),
    (DF, 'D2', 'F8'),(DR, 'D6', 'R8'),(DB, 'D8', 'B8'),(DL, 'D4', 'L8'),
    (FR, 'F6', 'R4'),(FL, 'F4', 'L6'),(BR, 'B4', 'R6'),(BL, 'B6', 'L4'),
)
corner_to_face = (
    (UFR,'U9','F3','R1'),(URB,'U3','R3','B1'),(UBL,'U1','B3','L1'),(ULF,'U7','L3','F1'),
    (DRF,'D3','R7','F9'),(DFL,'D1','F7','L9'),(DLB,'D7','L7','B9'),(DBR,'D9','B7','R9'),
)
facs_offset_dict = {'U':0,'R':9,'F':18,'D':27,'L':36,'B':45}
str2edge = {'UF' : UF, 'UR' : UR, 'UB' : UB, 'UL' : UL,
            'DF' : DF, 'DR' : DR, 'DB' : DB, 'DL' : DL,
            'FR' : FR, 'FL' : FL, 'BR' : BR, 'BL' : BL,
            'FU' : FU, 'RU' : RU, 'BU' : BU, 'LU' : LU,
            'FD' : FD, 'RD' : RD, 'BD' : BD, 'LD' : LD,
            'RF' : RF, 'LF' : LF, 'RB' : RB, 'LB' : LB}
str2corner={'UFR': UFR, 'URB': URB, 'UBL': UBL, 'ULF': ULF,
            'DRF': DRF, 'DFL': DFL, 'DLB': DLB, 'DBR': DBR,
            'FRU': FRU, 'RBU': RBU, 'BLU': BLU, 'LFU': LFU,
            'RFD': RFD, 'FLD': FLD, 'LBD': LBD, 'BRD': BRD,
            'RUF': RUF, 'BUR': BUR, 'LUB': LUB,'FUL': FUL,
            'FDR': FDR, 'LDF': LDF,'BDL': BDL, 'RDB': RDB}


# 魔方旋转时各个块的变化
route_map = {
        "L":  ((0, 1, 2, 11, 4, 5, 6, 9, 8, 3, 10, 7), (0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0), (0, 1, 6, 2, 4, 3, 5, 7), (0, 0, 2, 1, 0, 2, 1, 0)),
        "L'": ((0, 1, 2, 9, 4, 5, 6, 11, 8, 7, 10, 3), (0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0), (0, 1, 3, 5, 4, 6, 2, 7), (0, 0, 2, 1, 0, 2, 1, 0)),
        "L2": ((0, 1, 2, 7, 4, 5, 6, 3, 8, 11, 10, 9), (0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0), (0, 1, 5, 6, 4, 2, 3, 7), (0, 0, 0, 0, 0, 0, 0, 0)),
        "R":  ((0, 8, 2, 3, 4, 10, 6, 7, 5, 9, 1, 11), (0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0), (4, 0, 2, 3, 7, 5, 6, 1), (2, 1, 0, 0, 1, 0, 0, 2)),
        "R'": ((0, 10, 2, 3, 4, 8, 6, 7, 1, 9, 5, 11), (0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0), (1, 7, 2, 3, 0, 5, 6, 4), (2, 1, 0, 0, 1, 0, 0, 2)),
        "R2": ((0, 5, 2, 3, 4, 1, 6, 7, 10, 9, 8, 11), (0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0), (7, 4, 2, 3, 1, 5, 6, 0), (0, 0, 0, 0, 0, 0, 0, 0)),
        "U":  ((1, 2, 3, 0, 4, 5, 6, 7, 8, 9, 10, 11), (0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0), (1, 2, 3, 0, 4, 5, 6, 7), (0, 0, 0, 0, 0, 0, 0, 0)),
        "U'": ((3, 0, 1, 2, 4, 5, 6, 7, 8, 9, 10, 11), (0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0), (3, 0, 1, 2, 4, 5, 6, 7), (0, 0, 0, 0, 0, 0, 0, 0)),
        "U2": ((2, 3, 0, 1, 4, 5, 6, 7, 8, 9, 10, 11), (0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0), (2, 3, 0, 1, 4, 5, 6, 7), (0, 0, 0, 0, 0, 0, 0, 0)),
        "D":  ((0, 1, 2, 3, 7, 4, 5, 6, 8, 9, 10, 11), (0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0), (0, 1, 2, 3, 5, 6, 7, 4), (0, 0, 0, 0, 0, 0, 0, 0)),
        "D'": ((0, 1, 2, 3, 5, 6, 7, 4, 8, 9, 10, 11), (0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0), (0, 1, 2, 3, 7, 4, 5, 6), (0, 0, 0, 0, 0, 0, 0, 0)),
        "D2": ((0, 1, 2, 3, 6, 7, 4, 5, 8, 9, 10, 11), (0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0), (0, 1, 2, 3, 6, 7, 4, 5), (0, 0, 0, 0, 0, 0, 0, 0)),
        "F":  ((9, 1, 2, 3, 8, 5, 6, 7, 0, 4, 10, 11), (1, 0, 0, 0, 1, 0, 0, 0, 1, 1, 0, 0), (3, 1, 2, 5, 0, 4, 6, 7), (1, 0, 0, 2, 2, 1, 0, 0)),
        "F'": ((8, 1, 2, 3, 9, 5, 6, 7, 4, 0, 10, 11), (1, 0, 0, 0, 1, 0, 0, 0, 1, 1, 0, 0), (4, 1, 2, 0, 5, 3, 6, 7), (1, 0, 0, 2, 2, 1, 0, 0)),
        "F2": ((4, 1, 2, 3, 0, 5, 6, 7, 9, 8, 10, 11), (0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0), (5, 1, 2, 4, 3, 0, 6, 7), (0, 0, 0, 0, 0, 0, 0, 0)),
        "B":  ((0, 1, 10, 3, 4, 5, 11, 7, 8, 9, 6, 2), (0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 1), (0, 7, 1, 3, 4, 5, 2, 6), (0, 2, 1, 0, 0, 0, 2, 1)),
        "B'": ((0, 1, 11, 3, 4, 5, 10, 7, 8, 9, 2, 6), (0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 1), (0, 2, 6, 3, 4, 5, 7, 1), (0, 2, 1, 0, 0, 0, 2, 1)),
        "B2": ((0, 1, 6, 3, 4, 5, 2, 7, 8, 9, 11, 10), (0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0), (0, 6, 7, 3, 4, 5, 1, 2), (0, 0, 0, 0, 0, 0, 0, 0))
    }
class Cube():
    def __init__(self,
                 ep = [UF, UR, UB, UL,DF, DR, DB, DL, FR, FL, BR, BL],
                 er = [0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0],
                 cp = [UFR, URB, UBL, ULF, DRF, DFL, DLB, DBR],
                 cr = [0,   0,   0,   0,   0,   0,   0,   0  ],
                 step = []):
        self.ep = ep
        self.er = er
        self.cp = cp
        self.cr = cr
        self.step = step
        self.scan_set = set()
        self.scan_set_2 = set()
        self.face = 'F' #靠近电机2的面
    
    def get_tuple(self):
        return tuple(self.ep + self.er + self.cp + self.cr)
    
    # 获取表示魔方状态的字符串
    str_all        = 0
    str_edge_8     = 1
    str_edge_4     = 2
    str_corner     = 3
    str_edge_dir   = 4
    str_corner_dir = 5
    str_edge       = 6
    str_corner_dir_mid_edge = 7
    def get_str(self, mode=str_all):
        base = ('0','1','2','3','4','5','6','7','8','9',
                'A','B','C','D','E','F','G','H','I','J','K','L','M','N','O','P','Q','R','S','T','U','V')
        if mode == Cube.str_all:
            out=[0]*20
            for i in range(0,12):
                out[i] = base[self.ep[i] + self.er[i] * 16]
            for i in range(0,8):
                out[i+12] = base[self.cp[i] + self.cr[i] * 8]
        elif mode == Cube.str_edge_8:
            out=[0]*8
            for i in range(0,8):
                out[i] = base[self.ep[i]]
        elif mode == Cube.str_edge_4:
            out=[0]*4
            for i in range(0,4):
                out[i] = base[self.ep[i+8]]
        elif mode == Cube.str_corner:
            out=[0]*8
            for i in range(0,8):
                out[i] = base[self.cp[i]]
        elif mode == Cube.str_edge_dir:
            out=[0]*12
            for i in range(0,12):
                out[i] = base[self.er[i]]
        elif mode == Cube.str_edge:
            out=[0]*12
            for i in range(0,12):
                out[i] = base[self.ep[i]]
        elif mode == Cube.str_corner_dir:
            out=[0]*8
            for i in range(0,8):
                out[i] = base[self.cr[i]]
        elif mode == Cube.str_corner_dir_mid_edge:
            out1  = [0]*8
            out2 = [0]*4
            for i in range(0,8):
                out1[i] = base[self.cr[i]]
            for i in range(0,4):
                out2[i] = base[self.ep[i+8]]
            out2.sort()
            out = out1+out2
        else:
            out=[]
        return ''.join(out)
    
    def from_face_54(self, cube_str):
        # 54个面的表示方式，转换为棱块角块表示方式
        # UUUUUUUUURRRRRRRRRFFFFFFFFFDDDDDDDDDLLLLLLLLLBBBBBBBBB
        for i in range(0,12):
            index_a = int(edge_to_face[i][1][1]) + facs_offset_dict[edge_to_face[i][1][0]] - 1
            index_b = int(edge_to_face[i][2][1]) + facs_offset_dict[edge_to_face[i][2][0]] - 1
            tmp = str2edge[cube_str[index_a] + cube_str[index_b]]
            self.ep[i] = tmp % 16
            self.er[i] = tmp // 16
        for i in range(0,8):
            index_a = int(corner_to_face[i][1][1]) + facs_offset_dict[corner_to_face[i][1][0]] - 1
            index_b = int(corner_to_face[i][2][1]) + facs_offset_dict[corner_to_face[i][2][0]] - 1
            index_c = int(corner_to_face[i][3][1]) + facs_offset_dict[corner_to_face[i][3][0]] - 1
            tmp = str2corner[cube_str[index_a] + cube_str[index_b] + cube_str[index_c]]
            self.cp[i] = tmp % 8
            self.cr[i] = tmp // 8
    def get_face_54(self):
        # 转换为54个面的状态
        cube_str = [' ']*54;
        for i in range(0,6):
            cube_str[4+9*i] = ('U','R','F','D','L','B')[i]
        for i in range(0,12):
            index_a = int(edge_to_face[i][1][1]) + facs_offset_dict[edge_to_face[i][1][0]] - 1
            index_b = int(edge_to_face[i][2][1]) + facs_offset_dict[edge_to_face[i][2][0]] - 1
            for j in edge_to_face:
                if   self.er[i] == 0 and self.ep[i] == j[0]:
                    cube_str[index_a] = j[1][0]
                    cube_str[index_b] = j[2][0]
                elif self.er[i] == 1 and self.ep[i] == j[0]:
                    cube_str[index_b] = j[1][0]
                    cube_str[index_a] = j[2][0]
        for i in range(0,8):
            index_a = int(corner_to_face[i][1][1]) + facs_offset_dict[corner_to_face[i][1][0]] - 1
            index_b = int(corner_to_face[i][2][1]) + facs_offset_dict[corner_to_face[i][2][0]] - 1
            index_c = int(corner_to_face[i][3][1]) + facs_offset_dict[corner_to_face[i][3][0]] - 1
            for j in corner_to_face:
                if self.cr[i] == 0 and self.cp[i] == j[0]:
                    cube_str[index_a] = j[1][0]
                    cube_str[index_b] = j[2][0]
                    cube_str[index_c] = j[3][0]
                elif self.cr[i] == 2 and self.cp[i] == j[0]:
                    cube_str[index_b] = j[1][0]
                    cube_str[index_c] = j[2][0]
                    cube_str[index_a] = j[3][0]
                elif self.cr[i] == 1 and self.cp[i] == j[0]:
                    cube_str[index_c] = j[1][0]
                    cube_str[index_a] = j[2][0]
                    cube_str[index_b] = j[3][0]
        return cube_str
    def get_ep_and_cp(self):
        # 转换为54个面的状态
        cube_str = ""
        for i in range(0,12):
            for j in edge_to_face:
                if   self.er[i] == 0 and self.ep[i] == j[0]:
                    cube_str = cube_str + j[1][0] + j[2][0] + ' '
                elif self.er[i] == 1 and self.ep[i] == j[0]:
                    cube_str = cube_str + j[2][0] + j[1][0] + ' '
        for i in range(0,8):
            for j in corner_to_face:
                if self.cr[i] == 0 and self.cp[i] == j[0]:
                    cube_str = cube_str + j[1][0] + j[2][0] + j[3][0] + ' '
                elif self.cr[i] == 2 and self.cp[i] == j[0]:
                    cube_str = cube_str + j[3][0] + j[1][0] + j[2][0] + ' '
                    
                elif self.cr[i] == 1 and self.cp[i] == j[0]:
                    cube_str = cube_str + j[2][0] + j[3][0] + j[1][0] + ' '
        return cube_str
    def get_face_54_with_address(self):
        # 转换为54个面的状态,带位置编号
        cube_str = [' ']*54;

        for i in range(0,12):
            index_a = int(edge_to_face[i][1][1]) + facs_offset_dict[edge_to_face[i][1][0]] - 1
            index_b = int(edge_to_face[i][2][1]) + facs_offset_dict[edge_to_face[i][2][0]] - 1
            for j in edge_to_face:
                if   self.er[i] == 0 and self.ep[i] == j[0]:
                    cube_str[index_a] = j[1]
                    cube_str[index_b] = j[2]
                elif self.er[i] == 1 and self.ep[i] == j[0]:
                    cube_str[index_b] = j[1]
                    cube_str[index_a] = j[2]
        for i in range(0,8):
            index_a = int(corner_to_face[i][1][1]) + facs_offset_dict[corner_to_face[i][1][0]] - 1
            index_b = int(corner_to_face[i][2][1]) + facs_offset_dict[corner_to_face[i][2][0]] - 1
            index_c = int(corner_to_face[i][3][1]) + facs_offset_dict[corner_to_face[i][3][0]] - 1
            for j in corner_to_face:
                if self.cr[i] == 0 and self.cp[i] == j[0]:
                    cube_str[index_a] = j[1]
                    cube_str[index_b] = j[2]
                    cube_str[index_c] = j[3]
                elif self.cr[i] == 2 and self.cp[i] == j[0]:
                    cube_str[index_b] = j[1]
                    cube_str[index_c] = j[2]
                    cube_str[index_a] = j[3]
                elif self.cr[i] == 1 and self.cp[i] == j[0]:
                    cube_str[index_c] = j[1]
                    cube_str[index_a] = j[2]
                    cube_str[index_b] = j[3]
        return cube_str
    def route(self, dir):
        # 旋转魔方,返回旋转过的状态
        new_ep = [0]*12
        new_er = [0]*12
        new_cp = [0]*8
        new_cr = [0]*8
        new_step = self.step + [dir]
        r = route_map[dir]
        for i in range(0,12):
            new_ep[i] = self.ep[r[0][i]]
            new_er[i] = self.er[r[0][i]] ^ r[1][i]
        for i in range(0,8):
            new_cp[i] = self.cp[r[2][i]]
            new_cr[i] = (self.cr[r[2][i]] + r[3][i]) % 3
        self.ep = new_ep
        self.er = new_er
        self.cp = new_cp
        self.cr = new_cr
        self.step = new_step
    def route_scan(self, dir):
        # 旋转魔方,返回旋转过的状态
        new_ep = [0]*12
        new_er = [0]*12
        new_cp = [0]*8
        new_cr = [0]*8
        new_step = self.step + [dir]
        r = route_map[dir]
        for i in range(0,12):
            new_ep[i] = self.ep[r[0][i]]
            new_er[i] = self.er[r[0][i]] ^ r[1][i]
        for i in range(0,8):
            new_cp[i] = self.cp[r[2][i]]
            new_cr[i] = (self.cr[r[2][i]] + r[3][i]) % 3
            
        # 记录旋转过程中可以扫描颜色的面
        s = self.get_face_54_with_address()
        print("route", dir)
        if(dir[0] == 'U' or dir[0] == 'D'):
            # UD面旋转时可记录颜色的位置
            #          U1 U2 U3 U6 U9 U8 U7 U4
            order_u = [0, 1, 2, 5, 8, 7, 6, 3,
                       0, 1, 2, 5, 8, 7, 6, 3]
            #          D7   D8   D9   D6   D3   D2   D1   D4
            order_d = [7+26,8+26,9+26,6+26,3+26,2+26,1+26,4+26,
                       7+26,8+26,9+26,6+26,3+26,2+26,1+26,4+26]
            d = {'F':0,'R':6,'B':4,'L':2}
            if(dir == "U'"):
                lst = order_u[d[self.face]: d[self.face]+3]
            elif (dir =="U2"):
                lst = order_u[d[self.face]: d[self.face]+5]
            elif (dir =="U"):
                lst = order_u[d[self.face]: d[self.face]+7]
            elif(dir == "D"):
                lst = order_d[d[self.face]: d[self.face]+3]
            elif (dir =="D2"):
                lst = order_d[d[self.face]: d[self.face]+5]
            elif (dir =="D'"):
                lst = order_d[d[self.face]: d[self.face]+7]
            
            x = 0
            for i in lst:
                if s[i] in self.scan_set:
                    print("scan\033[32m", s[i], "\033[0m")
                else:
                    print("scan", s[i],"angle",x)
                x+=1
                self.scan_set.add(s[i])
        else:
            # 整体翻转时可以扫描颜色的面
            # U1 U7 U9 U3
            
            #               U1 U4 U7 U8 U9 U6 U3 U2
            order_u_flip = [0, 3, 6, 7, 8, 5, 2, 1,
                            0, 3, 6, 7, 8, 5, 2, 1,]
            #               D7   D4   D1   D2   D3   D6   D9   D8
            order_d_flip = [7+26,4+26,1+26,2+26,3+26,6+26,9+26,8+26,
                            7+26,4+26,1+26,2+26,3+26,6+26,9+26,8+26]
            # U1 U7 U9 U3
            d_flip = {'F':0,'R':2,'B':4,'L':6}
            # 翻转次数计算
            flip_time_dict = {"FF":0, "FR":1, "FB":2, "FL":3,
                              "RF":3, "RR":0, "RB":1, "RL":2,
                              "BF":2, "BR":3, "BB":0, "BL":1,
                              "LF":1, "LR":2, "LB":3, "LL":0}
            flip_time = flip_time_dict[self.face + dir[0]];
            print("flip", flip_time*90)
            if flip_time != 0:
                lst_u = order_u_flip[d_flip[self.face]: d_flip[self.face]+1+flip_time*2]
                lst_d = order_d_flip[d_flip[self.face]: d_flip[self.face]+1+flip_time*2]
                x = 0
                for i in lst_u:
                    if s[i] in self.scan_set:
                        print("scan\033[32m", s[i], "\033[0m")
                    else:
                        print("scan", s[i],"u",x)
                    x+=1
                    self.scan_set.add(s[i])
                x = 0
                for i in lst_d:
                    if s[i] in self.scan_set:
                        print("scan\033[32m", s[i], "\033[0m")
                    else:
                        print("scan", s[i],"d",x)
                    x+=1
                    self.scan_set.add(s[i])
            self.face = dir[0]
        
        for i in range(0,9):
            self.scan_set_2.add(s[i])
        for i in range(27,36):
            self.scan_set_2.add(s[i])
        
            
        self.ep = new_ep
        self.er = new_er
        self.cp = new_cp
        self.cr = new_cr
        self.step = new_step

    def draw(self):
        cube_str = self.get_face_54_with_address()
        cube_map = [99, 99, 99, 0,  1,  2,  99, 99, 99, 99, 99, 99, 98,
                    99, 99, 99, 3,  4,  5,  99, 99, 99, 99, 99, 99, 98,
                    99, 99, 99, 6,  7,  8,  99, 99, 99, 99, 99, 99, 98,
                    36, 37, 38, 18, 19, 20, 9,  10, 11, 45, 46, 47, 98,
                    39, 40, 41, 21, 22, 23, 12, 13, 14, 48, 49, 50, 98,
                    42, 43, 44, 24, 25, 26, 15, 16, 17, 51, 52, 53, 98,
                    99, 99, 99, 27, 28, 29, 99, 99, 99, 99, 99, 99, 98,
                    99, 99, 99, 30, 31, 32, 99, 99, 99, 99, 99, 99, 98,
                    99, 99, 99, 33, 34, 35, 99, 99, 99, 99, 99, 99, 98]
        for x in cube_map:
            if x == 99:
                print("   ", end='')
            elif x == 98:
                print("")
            else:
                code = cube_str[x]
                if code in self.scan_set:
                    print("\033[32m"+code + "\033[0m ", end='')
                elif code in self.scan_set_2:
                    print("\033[33m"+code + "\033[0m ", end='')
                else:
                    print(code + " ", end='')
    def random(self):
        for i in range(0,100):
            self.route(random.choice(("L","R","U","D","F","B","L'","R'","U'","D'","F'","B'","L2","R2","U2","D2","F2","B2")))

def test_route():
    cube = Cube()
    cube.from_face_54("DRLUUBFBRBLURRLRUBLRDDFDLFUFUFFDBRDUBRUFLLFDDBFLUBLRBD")
    orig = cube.get_face_54()
    for x in ('L','R','U','D','F','B'):
        cube = Cube()
        cube.from_face_54("DRLUUBFBRBLURRLRUBLRDDFDLFUFUFFDBRDUBRUFLLFDDBFLUBLRBD")
        cube = cube.route(x)
        cube = cube.route(x+"'")
        if cube.get_face_54() == orig:
            print(x,"pass")
        else:
            print(x,"fail")
            draw_cube(cube.get_face_54())
    for x in ('L','R','U','D','F','B'):
        cube = Cube()
        cube.from_face_54("DRLUUBFBRBLURRLRUBLRDDFDLFUFUFFDBRDUBRUFLLFDDBFLUBLRBD")
        cube = cube.route(x)
        cube = cube.route(x)
        cube = cube.route(x+"2")
        if cube.get_face_54() == orig:
            print(x,"pass")
        else:
            print(x,"fail")
            draw_cube(cube.get_face_54())

def test_route_2():
    cube = Cube()
    cube.from_face_54("DRLUUBFBRBLURRLRUBLRDDFDLFUFUFFDBRDUBRUFLLFDDBFLUBLRBD")
    solve = "D2 R' D' F2 B D R2 D2 R' F2 D' F2 U' B2 L2 U2 D R2 U"
    draw_cube(cube.get_face_54())
    solve = solve.split(' ')
    for step in solve:
        cube = cube.route(step)
        #print(step, end=": \n")
        #print(cube.ep,cube.er,cube.cp,cube.cr)
        #draw_cube(cube.get_face_54())
    draw_cube(cube.get_face_54())
    print(cube.step)



def color_sample():
    cube=Cube()

    #cube.draw()
    cube.route_scan("L")
    cube.route_scan("R")

    cube.route_scan("F")
    cube.route_scan("B")

    cube.route_scan("R")
    cube.route_scan("L")

    cube.route_scan("F")
    cube.route_scan("B")

    # 转到F面，不对F面进行旋转操作
    cube.route_scan("F")
    cube.route_scan("F'")

    cube.draw()
    for i in range(0,54):
        print("%02d "%i,end='')
    print("")
    print(" ".join(cube.get_face_54_with_address()))


#color_sample()
#print(cube.get_ep_and_cp())
#for i in range(0,10000):
#    cube=Cube()
#    cube.random()
#    s = "".join(cube.get_face_54())
#    print(s)

test_case = {
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
}
for i in test_case:
    c = Cube()
    c.from_face_54(i)
    print(c.cp)
    print(c.cr)
    print(c.ep)
    print(c.er)
