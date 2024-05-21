#!/usr/bin/pypy3
# 推荐使用pypy，建立查找表的过程远比cpython快
import random
import array
#from collections import deque
#from itertools import permutations
#import json
import os

# 参考kociemba算法编写
# https://www.bilibili.com/read/cv12330074  魔方还原算法(一) 概述
# https://www.bilibili.com/read/cv12331495  魔方还原算法(二) 科先巴二阶段算法

#Group  # positions      Factor
#G0=<U  D  L  R  F  B >  4.33·10^19  2,048 (2^11)
#G1=<U  D  L  R  F2 B2>  2.11·10^16  1,082,565 (12C4 ·3^7)
#G2=<U  D  L2 R2 F2 B2>  1.95·10^10  29,400 ( 8C4^2 ·2·3)
#G3=<U2 D2 L2 R2 F2 B2>  6.63·10^5   663,552 (4!^5/12)

# Phase1 2187*2048*495    = 2217093120
# Phase2 40320*40320*24/2 = 19508428800 /2是考虑不能交换两个棱块
# 43252003274489856000

# 角块、棱块、面的编码格式如下
#                           2-----------2------------1
#                           | U1(0)   U2(1)   U3(2)  |
#                           |                        |
#                           3 U4(3)   U5(4)   U6(5)  1
#                           |                        |
#                           | U7(6)   U8(7)   U9(8)  |
#  2-----------3------------3-----------0------------0-----------1------------1------------2------------2
#  | L1(36)  L2(37)  L3(38) | F1(18)  F2(19)  F3(20) | R1(9)   R2(10)  R3(11) |  B1(45)  B2(46)  B3(47) |
#  |                        |                        |                        |                         |
# 11 L4(39)  L5(40)  L6(41) 9 F4(21)  F5(22)  F6(23) 8 R4(12)  R5(13)  R6(14) 10 B4(48)  B5(49)  B6(50) 11
#  |                        |                        |                        |                         |
#  | L7(42)  L8(43)  L9(44) | F7(24)  F8(25)  F9(26) | R7(15)  R8(16)  R9(17) |  B7(51)  B8(52)  B9(53) |
#  3-----------7------------5-----------4------------4-----------5------------7------------6------------3
#                           | D1(27)  D2(28)  D3(29) |
#                           |                        |
#                           7 D4(30)  D5(31)  D6(32) 5
#                           |                        |
#                           | D7(33)  D8(34)  D9(35) |
#                           6-----------6------------7



# 用于转换两种表示方式 20个棱块角块 <---> 54个面
# [UF, UR, UB, UL,DF, DR, DB, DL, FR, FL, BR, BL] [UFR, URB, UBL, ULF, DRF, DFL, DLB, DBR]
# UUUUUUUUURRRRRRRRRFFFFFFFFFDDDDDDDDDLLLLLLLLLBBBBBBBBB
# edge_to_face和corner_to_face里的字符串，这个版本是没有实际用途的
edge_to_face =(('UF', 7, 19),  ('UR', 5, 10),  ('UB', 1, 46),  ('UL', 3, 37),
               ('DF', 28, 25), ('DR', 32, 16), ('DB', 34, 52), ('DL', 30, 43),
               ('FR', 23, 12), ('FL', 21, 41), ('BR', 48, 14), ('BL', 50, 39))
corner_to_face =(('UFR', 8, 20, 9), ('URB', 2, 11, 45), ('UBL', 0, 47, 36), ('ULF', 6, 38, 18),
                ('DRF', 29, 15, 26), ('DFL', 27, 24, 44), ('DLB', 33, 42, 53), ('DBR', 35, 51, 17))
edge_index = ('UF','UR','UB','UL','DF','DR','DB','DL','FR','FL','BR','BL',
              'FU','RU','BU','LU','FD','RD','BD','LD','RF','LF','RB','LB')
corner_index = ('UFR','URB','UBL','ULF','DRF','DFL','DLB','DBR',
                'FRU','RBU','BLU','LFU','RFD','FLD','LBD','BRD',
                'RUF','BUR','LUB','FUL','FDR','LDF','BDL','RDB')
# 魔方旋转时各个块的变化
# 找个魔方，贴纸标记一下每个块的方向和编号，实际旋转一下就可以编写L R U D F B的规则了
route_index=("L","L'","L2","R","R'","R2","U","U'","U2","D","D'","D2","F","F'","F2","B","B'","B2");
route_tab=(
    ((0,1,2,11,4,5,6,9,8,3,10,7),(0,0,0,0,0,0,0,0,0,0,0,0),(0,1,6,2,4,3,5,7),(0,0,2,1,0,2,1,0)),# L  0
    ((0,1,2,9,4,5,6,11,8,7,10,3),(0,0,0,0,0,0,0,0,0,0,0,0),(0,1,3,5,4,6,2,7),(0,0,2,1,0,2,1,0)),# L' 1
    ((0,1,2,7,4,5,6,3,8,11,10,9),(0,0,0,0,0,0,0,0,0,0,0,0),(0,1,5,6,4,2,3,7),(0,0,0,0,0,0,0,0)),# L2 2
    ((0,8,2,3,4,10,6,7,5,9,1,11),(0,0,0,0,0,0,0,0,0,0,0,0),(4,0,2,3,7,5,6,1),(2,1,0,0,1,0,0,2)),# R  3
    ((0,10,2,3,4,8,6,7,1,9,5,11),(0,0,0,0,0,0,0,0,0,0,0,0),(1,7,2,3,0,5,6,4),(2,1,0,0,1,0,0,2)),# R' 4
    ((0,5,2,3,4,1,6,7,10,9,8,11),(0,0,0,0,0,0,0,0,0,0,0,0),(7,4,2,3,1,5,6,0),(0,0,0,0,0,0,0,0)),# R2 5
    ((1,2,3,0,4,5,6,7,8,9,10,11),(0,0,0,0,0,0,0,0,0,0,0,0),(1,2,3,0,4,5,6,7),(0,0,0,0,0,0,0,0)),# U  6
    ((3,0,1,2,4,5,6,7,8,9,10,11),(0,0,0,0,0,0,0,0,0,0,0,0),(3,0,1,2,4,5,6,7),(0,0,0,0,0,0,0,0)),# U' 7
    ((2,3,0,1,4,5,6,7,8,9,10,11),(0,0,0,0,0,0,0,0,0,0,0,0),(2,3,0,1,4,5,6,7),(0,0,0,0,0,0,0,0)),# U2 8
    ((0,1,2,3,7,4,5,6,8,9,10,11),(0,0,0,0,0,0,0,0,0,0,0,0),(0,1,2,3,5,6,7,4),(0,0,0,0,0,0,0,0)),# D  9
    ((0,1,2,3,5,6,7,4,8,9,10,11),(0,0,0,0,0,0,0,0,0,0,0,0),(0,1,2,3,7,4,5,6),(0,0,0,0,0,0,0,0)),# D' 10
    ((0,1,2,3,6,7,4,5,8,9,10,11),(0,0,0,0,0,0,0,0,0,0,0,0),(0,1,2,3,6,7,4,5),(0,0,0,0,0,0,0,0)),# D2 11
    ((9,1,2,3,8,5,6,7,0,4,10,11),(1,0,0,0,1,0,0,0,1,1,0,0),(3,1,2,5,0,4,6,7),(1,0,0,2,2,1,0,0)),# F  12
    ((8,1,2,3,9,5,6,7,4,0,10,11),(1,0,0,0,1,0,0,0,1,1,0,0),(4,1,2,0,5,3,6,7),(1,0,0,2,2,1,0,0)),# F' 13
    ((4,1,2,3,0,5,6,7,9,8,10,11),(0,0,0,0,0,0,0,0,0,0,0,0),(5,1,2,4,3,0,6,7),(0,0,0,0,0,0,0,0)),# F2 14
    ((0,1,10,3,4,5,11,7,8,9,6,2),(0,0,1,0,0,0,1,0,0,0,1,1),(0,7,1,3,4,5,2,6),(0,2,1,0,0,0,2,1)),# B  15
    ((0,1,11,3,4,5,10,7,8,9,2,6),(0,0,1,0,0,0,1,0,0,0,1,1),(0,2,6,3,4,5,7,1),(0,2,1,0,0,0,2,1)),# B' 16
    ((0,1,6,3,4,5,2,7,8,9,11,10),(0,0,0,0,0,0,0,0,0,0,0,0),(0,6,7,3,4,5,1,2),(0,0,0,0,0,0,0,0)) # B2 17
)

# ((0,1,2,3,4,5,6,7,8,9,10,11),(0,0,0,0,0,0,0,0,0,0,0,0),(0,1,2,3,4,5,6,7),(0,0,0,0,0,0,0,0))
# 围绕 UD 这个轴转动 90°，4 种
S_U4_BASIC  = ((1,2,3,0,5,6,7,4,10,8,11,9),(0,0,0,0,0,0,0,0,1,1,1,1),(1,2,3,0,7,4,5,6),(0,0,0,0,0,0,0,0))
# S_F2：围绕 FD 这个轴转动 180°，2 种
S_F2_BASIC  = ((4,7,6,5,0,3,2,1,9,8,11,10),(0,0,0,0,0,0,0,0,0,0,0,0),(5,6,7,4,3,0,1,2),(0,0,0,0,0,0,0,0))
# S_LR2：关于 LR 层的镜像，2 种
S_LR2_BASIC = ((0,3,2,1,4,7,6,5,9,8,11,10),(0,0,0,0,0,0,0,0,0,0,0,0),(3,2,1,0,5,4,7,6),(3,3,3,3,3,3,3,3))
# S_URF3：围绕 URF 这个角块转动120°，3 种
# 暂时不需要，不编写
sym_tab = (
    ((0,1,2,3,4,5,6,7,8,9,10,11),(0,0,0,0,0,0,0,0,0,0,0,0),(0,1,2,3,4,5,6,7),(0,0,0,0,0,0,0,0)),#
    ((0,3,2,1,4,7,6,5,9,8,11,10),(0,0,0,0,0,0,0,0,0,0,0,0),(3,2,1,0,5,4,7,6),(3,3,3,3,3,3,3,3)),#LR2-
    ((1,2,3,0,5,6,7,4,10,8,11,9),(0,0,0,0,0,0,0,0,1,1,1,1),(1,2,3,0,7,4,5,6),(0,0,0,0,0,0,0,0)),#U4-
    ((1,0,3,2,5,4,7,6,8,10,9,11),(0,0,0,0,0,0,0,0,1,1,1,1),(0,3,2,1,4,7,6,5),(3,3,3,3,3,3,3,3)),#U4-LR2-
    ((2,3,0,1,6,7,4,5,11,10,9,8),(0,0,0,0,0,0,0,0,0,0,0,0),(2,3,0,1,6,7,4,5),(0,0,0,0,0,0,0,0)),#U4-U4-
    ((2,1,0,3,6,5,4,7,10,11,8,9),(0,0,0,0,0,0,0,0,0,0,0,0),(1,0,3,2,7,6,5,4),(3,3,3,3,3,3,3,3)),#U4-U4-LR2-
    ((3,0,1,2,7,4,5,6,9,11,8,10),(0,0,0,0,0,0,0,0,1,1,1,1),(3,0,1,2,5,6,7,4),(0,0,0,0,0,0,0,0)),#U4-U4-U4-
    ((3,2,1,0,7,6,5,4,11,9,10,8),(0,0,0,0,0,0,0,0,1,1,1,1),(2,1,0,3,6,5,4,7),(3,3,3,3,3,3,3,3)),#U4-U4-U4-LR2-
    ((4,7,6,5,0,3,2,1,9,8,11,10),(0,0,0,0,0,0,0,0,0,0,0,0),(5,6,7,4,3,0,1,2),(0,0,0,0,0,0,0,0)),#F2-
    ((4,5,6,7,0,1,2,3,8,9,10,11),(0,0,0,0,0,0,0,0,0,0,0,0),(4,7,6,5,0,3,2,1),(3,3,3,3,3,3,3,3)),#F2-LR2-
    ((7,6,5,4,3,2,1,0,11,9,10,8),(0,0,0,0,0,0,0,0,1,1,1,1),(6,7,4,5,2,3,0,1),(0,0,0,0,0,0,0,0)),#F2-U4-
    ((7,4,5,6,3,0,1,2,9,11,8,10),(0,0,0,0,0,0,0,0,1,1,1,1),(5,4,7,6,3,2,1,0),(3,3,3,3,3,3,3,3)),#F2-U4-LR2-
    ((6,5,4,7,2,1,0,3,10,11,8,9),(0,0,0,0,0,0,0,0,0,0,0,0),(7,4,5,6,1,2,3,0),(0,0,0,0,0,0,0,0)),#F2-U4-U4-
    ((6,7,4,5,2,3,0,1,11,10,9,8),(0,0,0,0,0,0,0,0,0,0,0,0),(6,5,4,7,2,1,0,3),(3,3,3,3,3,3,3,3)),#F2-U4-U4-LR2-
    ((5,4,7,6,1,0,3,2,8,10,9,11),(0,0,0,0,0,0,0,0,1,1,1,1),(4,5,6,7,0,1,2,3),(0,0,0,0,0,0,0,0)),#F2-U4-U4-U4-
    ((5,6,7,4,1,2,3,0,10,8,11,9),(0,0,0,0,0,0,0,0,1,1,1,1),(7,6,5,4,1,0,3,2),(3,3,3,3,3,3,3,3))#F2-U4-U4-U4-LR2-
)
#0-15对称的逆变换
sym_inv_index=(0,1,6,3,4,5,2,7,8,9,10,15,12,13,14,11)


class Cube():
    def __init__(self,x=None):
        if x == None:
            self.ep = [0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11] # 楞块位置
            self.er = [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  0]  # 楞块方向
            self.cp = [0, 1, 2, 3, 4, 5, 6, 7] # 角块位置 
            self.cr = [0, 0, 0, 0, 0, 0, 0, 0] # 角块方向
        else:
            self.ep = x[0] # 楞块位置
            self.er = x[1] # 楞块方向
            self.cp = x[2] # 角块位置 
            self.cr = x[3] # 角块方向
        self.step = 0
        self.last_step = -99
        #self.root = None
    def copy(self):
        c = Cube()
        c.ep = self.ep.copy()
        c.er = self.er.copy()
        c.cp = self.cp.copy()
        c.cr = self.cr.copy()
        return c
    def is_solved(self):
        r = True
        for i in range(12):
            if self.ep[i] != i:
                r = False
        for i in range(12):
            if self.er[i] != 0:
                r = False
        for i in range(8):
            if self.cp[i] != i:
                r = False
        for i in range(8):
            if self.cr[i] != 0:
                r = False
        return r
    def dump(self):
        s = []
        for i in range(0,12):
            s.append(edge_index[self.ep[i] + 12*self.er[i]])
        for i in range(0,8):
            s.append(corner_index[self.cp[i] + 8*self.cr[i]])
        print(s)
        print(self.ep)
        print(self.er)
        print(self.cp)
        print(self.cr)
    def from_face_54(self, cube_str):
        # 54个面的表示方式，转换为棱块角块表示方式
        for i in range(0,12):
            index_a = edge_to_face[i][1]
            index_b = edge_to_face[i][2]
            tmp = edge_index.index(cube_str[index_a] + cube_str[index_b])
            self.ep[i] = tmp % 12
            self.er[i] = tmp // 12
        for i in range(0,8):
            index_a = corner_to_face[i][1]
            index_b = corner_to_face[i][2]
            index_c = corner_to_face[i][3]
            tmp = corner_index.index(cube_str[index_a] + cube_str[index_b] + cube_str[index_c])
            self.cp[i] = tmp % 8
            self.cr[i] = tmp // 8
    def to_face_54(self):
        # 转换为54个面的状态
        cube_str = [' ']*54;
        cube_str[4]  = 'U'
        cube_str[13] = 'R'
        cube_str[22] = 'F'
        cube_str[31] = 'D'
        cube_str[40] = 'L'
        cube_str[49] = 'B'
        for i in range(0,12):
            index_a = edge_to_face[i][1]
            index_b = edge_to_face[i][2]
            s = edge_index[self.ep[i] + self.er[i]*12]
            cube_str[index_a] = s[0]
            cube_str[index_b] = s[1]
        for i in range(0,8):
            index_a = corner_to_face[i][1]
            index_b = corner_to_face[i][2]
            index_c = corner_to_face[i][3]
            s = corner_index[self.cp[i] + self.cr[i]*8]
            cube_str[index_a] = s[0]
            cube_str[index_b] = s[1]
            cube_str[index_c] = s[2]
        return ''.join(cube_str)
    # 拧魔方d=0-17，或者字母表示，还支持相似变换
    # 改来改去添加功能，软件架构越来越乱了
    def route(self, d, new = False):
        new_ep = [0]*12
        new_er = [0]*12
        new_cp = [0]*8
        new_cr = [0]*8
        if(type(d) == int):
            # 数字表示 0-17
            r = route_tab[d]
        elif(type(d) == tuple):
            # tuple表示
            r = d
        elif(type(d) == str):
            # 字符串表示 F2
            r = route_tab[route_index.index(d)]
        elif(type(d) == Cube):
            # 魔方状态表示
            r = (tuple(d.ep), tuple(d.er), tuple(d.cp), tuple(d.cr))
        for i in range(0,12):
            new_ep[i] = self.ep[r[0][i]]
            new_er[i] = self.er[r[0][i]] ^ r[1][i]
        for i in range(0,8):
            new_cp[i] = self.cp[r[2][i]]
            a = self.cr[r[2][i]]
            b = r[3][i]
            # 处理镜像情况
            if a<3 and b<3: # 正常操作
                tmp = a + b
                if tmp >= 3:
                    tmp -= 3
            elif a<3 and b>=3: # LR镜像
                tmp = a + b
                if tmp >= 6:
                    tmp -= 3
            elif a>=3 and b<3:# 镜像模式下旋转方向相反
                tmp = a - b
                if tmp < 3:
                    tmp += 3
            elif a>=3 and b>=3:# LR镜像还原
                tmp = a - b
                if tmp < 0:
                    tmp += 3
            new_cr[i] = tmp
        if new:
            c = Cube()
            c.ep = new_ep
            c.er = new_er
            c.cp = new_cp
            c.cr = new_cr
            c.step = self.step + 1
            c.last_step = d
            return c
        else:
            self.ep = new_ep
            self.er = new_er
            self.cp = new_cp
            self.cr = new_cr
            return None
        
    # 绘制魔方
    def draw(self):
        cube_str = self.to_face_54()
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
                print("  ", end='')
            elif x == 98:
                print("")
            else:
                print(cube_str[x] + " ", end='')
    # 随机生成魔方
    def random(self):
        for i in range(0,100):
            self.route(random.choice(("L","R","U","D","F","B","L'","R'","U'","D'","F'","B'","L2","R2","U2","D2","F2","B2")))
    def random_p2(self):
        for i in range(0,100):
            self.route(random.choice(("U","D","U'","D'","L2","R2","U2","D2","F2","B2")))    
    
    # 角块方向，取值范围[0,2186] 用于阶段1    
    # 编码所有角块方向 3^7 = 2187
    def encode_twist(self):
        x = 0
        for i in range(7):
            x = x*3 + self.cr[i]
        return x
    def decode_twise(self, twist):
        twist_sum = 0
        for i in range(7):
            tmp = twist % 3
            twist //= 3
            self.cr[6 - i] = tmp
            twist_sum += tmp
        # c和python，负数%运算规则不一样，此处避免产生负数
        self.cr[7] = (15 - twist_sum) % 3
    
    # 棱边方向，取值范围[0,2047] 用于阶段1
    # 编码所有棱块方向 2^11 = 2048
    def encode_flip(self):
        x = 0
        for i in range(11):
            x = x*2 + self.er[i]
        return x
    def decode_flip(self, flip):
        flip_sum = 0
        for i in range(11):
            tmp = flip % 2
            flip //= 2
            self.er[10 - i] = tmp
            flip_sum += tmp
        self.er[11] = (12 - flip_sum) % 2
        
    # 中间层的 4 条棱边的选取状态，取值范围[0,494]
    # 中层的棱边排列，4个，取值范围[0,23]
    # 编码8-11号棱块的所在位置12C4 * 4! = 12P4 = 11880
    # x // 24后用于一阶段 ， x % 24后用于二阶段
    def encode_slice_sort(self):
        ep4_index = [0]*4
        ep4_val = [0]*4
        j = 0
        for i in range(12):
            if self.ep[i] >= 8:
                ep4_val[j] = self.ep[i]
                ep4_index[j] = i
                j += 1
        #print(ep4_index,ep4_val,encode_12C4(ep4_index),encode_4P4(ep4_val))
        return encode_12C4(ep4_index) * 24 + encode_4P4(ep4_val)
    def decode_slice_sort(self, x):
        ep4_index = decode_12C4(x // 24)
        ep4_val = decode_4P4((8,9,10,11), x % 24)
        #print(ep4_index,ep4_val,x % 24,x // 24)
        for i in range(12):# 只填写关注的部分
            self.ep[i] = 0
        for i in range(4):
            self.ep[ep4_index[i]] = ep4_val[i]
    
    # 角块排列，8个，取值范围[0,40319]
    def encode_corner(self):
        return encode_8P8(self.cp)
    def decode_corner(self,x):
        self.cp = decode_8P8((0,1,2,3,4,5,6,7), x)
    # U层D层的棱边排列，8个，取值范围[0,40319]
    def encode_ud_edge(self):
        return encode_8P8(self.ep)
    def decode_ud_edge(self,x):
        self.ep = decode_8P8((0,1,2,3,4,5,6,7), x) + self.ep[8:12]
    # U层的棱边排列，4个，取值范围[0,11879]，对于阶段2,取值小于1680
    def encode_u_edge(self):
        ep4_index = [0]*4
        ep4_val = [0]*4
        j = 0
        for i in range(12):
            if self.ep[i] in (0,1,2,3):
                ep4_val[j] = self.ep[i]
                ep4_index[j] = i
                j += 1
        return encode_12C4(ep4_index) * 24 + encode_4P4(ep4_val)
    def decode_u_edge(self, x):
        ep4_index = decode_12C4(x // 24)
        ep4_val = decode_4P4((0,1,2,3), x % 24)
        for i in range(12):# 只填写关注的部分
            self.ep[i] = 11
        for i in range(4):
            self.ep[ep4_index[i]] = ep4_val[i]
    # D层的棱边排列，4个，取值范围[0,11879]，对于阶段2,取值小于1680
    def encode_d_edge(self):
        ep4_index = [0]*4
        ep4_val = [0]*4
        j = 0
        for i in range(12):
            if self.ep[i] in (4,5,6,7):
                ep4_val[j] = self.ep[i]
                ep4_index[j] = i
                j += 1
        return encode_12C4(ep4_index) * 24 + encode_4P4(ep4_val)
    def decode_d_edge(self, x):
        ep4_index = decode_12C4(x // 24)
        ep4_val = decode_4P4((4,5,6,7), x % 24)
        for i in range(12):# 只填写关注的部分
            self.ep[i] = 11
        for i in range(4):
            self.ep[ep4_index[i]] = ep4_val[i]
    
# 组合 n*(n-1)*(n-m+1)/m!
def comb(n,m):
    c = 1
    for i in range(m):
        c *=  n - i
        c //= i + 1
    return c

# 编码12选4,输入0-11中任意选取的4个数字，输出0-495
def encode_12C4(x):
    return comb(x[0],1) + comb(x[1],2) + comb(x[2],3) + comb(x[3],4)
# 解码码12选4,输入0-495，输出0-11中选取的4个数字
def decode_12C4(c):
    ret = [0]*4
    j = 4
    for i in range(12-1,-1,-1):# 11,10,...,2,1,0
        tmp = comb(i,j);
        if c >= tmp:
            c -= tmp
            j -= 1
            ret[j] = i
    return ret

# t = 495*[0]
# p = combinations(range(12),4)
# for x in p:
#     a = encode_12C4(x)
#     b = decode_12C4(a)
#     assert(b == list(x))
                
# 编码4个数字的全排列，例如(1, 0, 2, 3) => 6
# 基于康托展开，支持不连续的数据
# 例如 (3, 9, 5, 11) => 2
def encode_4P4(p):
    n=0;
    for a in range(3):
        n *= 4-a;
        for b in range(a+1, 4):
            if p[b] < p[a]:
                n += 1;
    return n


def decode_4P4(num, x):
    #[比首位小的数字个数,比第二位小的数字个数,比第三位小的数字个数,0]
    tmp = [0]*4
    for i in range(2,4+1):
        tmp[4-i] = x % i
        x //= i
    # 选择需要的数字，填到返回结果
    num_to_select = list(num)
    num_to_select.sort()
    ret = [0] * 4
    for i in range(4):
        ret[i] = num_to_select[tmp[i]]
        num_to_select.pop(tmp[i])
    return ret

def encode_8P8(p):
    n=0;
    for a in range(7):
        n *= 8-a;
        for b in range(a+1, 8):
            if p[b] < p[a]:
                n += 1;
    return n

def decode_8P8(num, x):
    tmp = [0]*8
    for i in range(2,8+1):
        tmp[8-i] = x % i
        x //= i
    # 选择需要的数字，填到返回结果
    num_to_select = list(num)
    num_to_select.sort()
    ret = [0] * 8
    for i in range(8):
        ret[i] = num_to_select[tmp[i]]
        num_to_select.pop(tmp[i])
    return ret


# p = permutations(range(8),8)
# for x in p:
#     e = encode_8P8(x)
#     d = decode_8P8(range(8), e)
#     assert(list(x) == d)
# print("test done")


def load_table(file,n,type_code = 'H'):
    if not os.path.isfile(file):
        print("can not find", file)
        return None
    with open(file,'rb') as f:
        load = array.array(type_code)
        load.fromfile(f,n)
    return load

def creat_crood_move_table():
    print("creat_crood_move_table.")
    cube_a = Cube()
    print("twist_move")
    tmp = array.array("H", (0 for i in range(2187*18)))
    for i in range(2187):
        cube_a.decode_twise(i)
        for j in range(18):
            cube_b = cube_a.route(j, new=True)
            tmp[i*18 + j] = cube_b.encode_twist()
    with open("twist_move",'wb') as f:
        tmp.tofile(f)
    
    print("flip_move")
    tmp  = array.array("H", (0 for i in range(2048*18)))
    for i in range(2048):
        cube_a.decode_flip(i)
        for j in range(18):
            cube_b = cube_a.route(j, new=True)
            tmp[i*18 + j] = cube_b.encode_flip()
    with open("flip_move",'wb') as f:
        tmp.tofile(f)
    
    print("slice_sort_move")
    tmp = array.array("H", (0 for i in range(11880*18)))
    for i in range(11880):
        cube_a.decode_slice_sort(i)
        for j in range(18):
            cube_b = cube_a.route(j, new=True)
            tmp[i*18 + j] = cube_b.encode_slice_sort()
    with open("slice_sort_move",'wb') as f:
        tmp.tofile(f)
        
    print("twist_sym")
    tmp  = array.array("H", (0 for i in range(2187*16)))
    for i in range(2187):
        cube_a.decode_twise(i)
        for j in range(16):
            # 对角块方向twist做 S[i]*twist*S[i]^-1
            cube_b = Cube(sym_tab[j])
            cube_b.route(cube_a)
            cube_b.route(sym_tab[sym_inv_index[j]])
            tmp[i*16 + j] = cube_b.encode_twist()
    with open("twist_sym",'wb') as f:
        tmp.tofile(f)        

    print("u_edge_move")
    tmp = array.array("H", (0 for i in range(11880*18)))
    for i in range(11880):
        cube_a.decode_u_edge(i)
        for j in range(18):
            cube_b = cube_a.route(j, new=True)
            tmp[i*18 + j] = cube_b.encode_u_edge()
    with open("u_edge_move",'wb') as f:
        tmp.tofile(f)

    print("d_edge_move")
    tmp = array.array("H", (0 for i in range(11880*18)))
    for i in range(11880):
        cube_a.decode_d_edge(i)
        for j in range(18):
            cube_b = cube_a.route(j, new=True)
            tmp[i*18 + j] = cube_b.encode_d_edge()
    with open("d_edge_move",'wb') as f:
        tmp.tofile(f)
    
    print("corner_move")
    tmp = array.array("H", (0 for i in range(40320*18)))
    for i in range(40320):
        cube_a.decode_corner(i)
        for j in range(18):
            cube_b = cube_a.route(j, new=True)
            tmp[i*18 + j] = cube_b.encode_corner()
    with open("corner_move",'wb') as f:
        tmp.tofile(f)
    
    print("ud_edge_move, only for stage 2")
    tmp = array.array("H", (0 for i in range(40320*18)))
    for i in range(40320):
        cube_a.decode_ud_edge(i)
        for j in (2,5,6,7,8,9,10,11,14,17):
            cube_b = cube_a.route(j, new=True)
            tmp[i*18 + j] = cube_b.encode_ud_edge()
    with open("ud_edge_move",'wb') as f:
        tmp.tofile(f)
    
    print("ud_edge_sym")
    tmp  = array.array("H", (0 for i in range(40320*16)))
    for i in range(40320):
        cube_a.decode_ud_edge(i)
        for j in range(16):
            # 对角块方向twist做 S[i]*twist*S[i]^-1
            cube_b = Cube(sym_tab[j])
            cube_b.route(cube_a)
            cube_b.route(sym_tab[sym_inv_index[j]])
            tmp[i*16 + j] = cube_b.encode_ud_edge()
    with open("ud_edge_sym",'wb') as f:
        tmp.tofile(f)
    
    print("done.")

# 2048 * 495 =  1013760  =>  64430
def creat_index_to_class_table_p1():
    class_id = 0
    flip_slice_to_class = array.array("L", (65535 for i in range(2048 * 495)))
    class_to_flip_slice = array.array("L", (2000000 for i in range(64430)))
    cube = Cube()
    for i in range(2048 * 495):
        if flip_slice_to_class[i] == 65535:
            flip_slice_to_class[i] = class_id
            # 反查表
            class_to_flip_slice[class_id] = i
            flip_id = i // 495
            slice_sort_id = i % 495 * 24
            cube.decode_flip(flip_id)
            cube.decode_slice_sort(slice_sort_id)
            for j in range(1,16):
                c = Cube(sym_tab[sym_inv_index[j]]) # s^-1*cc*s
                c.route(cube)
                c.route(sym_tab[j])
                flip_id = c.encode_flip()
                slice_sort_id = c.encode_slice_sort()
                sym_index = flip_id * 495 + slice_sort_id // 24
                if flip_slice_to_class[sym_index] == 65535:
                    flip_slice_to_class[sym_index] = class_id | (j<<16)
            class_id += 1
        if class_id % 1000 == 0:
            print("\rcreat_index_to_class_table (phase 1) %d%%"%(class_id*100//64430),end='')
    print("")
    assert class_id == 64430
    # 长度1013760，用于查找属于哪种等价类,高16位存储变换类型，低16位存储等价类
    with open("index_to_class_p1",'wb') as f:
        flip_slice_to_class.tofile(f)
    # 长度65535，用于反向查找flip_id * 495 + slice_sort_id // 24
    with open("class_to_index_p1",'wb') as f:
        class_to_flip_slice.tofile(f)
    print("done.")

# 40320 ==> 2768
def creat_index_to_class_table_p2():
    class_id = 0
    coner_to_class = array.array("L", (65535 for i in range(40320)))
    class_to_coner = array.array("H", (65535 for i in range(2768)))
    cube = Cube()
    for i in range(40320):
        if coner_to_class[i] == 65535:
            coner_to_class[i] = class_id
            # 反查表
            class_to_coner[class_id] = i
            cube.decode_corner(i)
            for j in range(1,16):
                c = Cube(sym_tab[sym_inv_index[j]]) # s^-1*cc*s
                c.route(cube)
                c.route(sym_tab[j])
                sym_index = c.encode_corner()
                if coner_to_class[sym_index] == 65535:
                    coner_to_class[sym_index] = class_id | (j<<16)
            class_id += 1
        if class_id % 1000 == 0:
            print("\rcreat_index_to_class_table (phase 2) %d%%"%(class_id*100//2768),end='')
    print("")
    assert class_id == 2768
    # 长度40320，用于查找属于哪种等价类,高16位存储变换类型，低16位存储等价类
    with open("index_to_class_p2",'wb') as f:
        coner_to_class.tofile(f)
    # 长度2768，用于反向查找flip_id * 495 + slice_sort_id // 24
    with open("class_to_index_p2",'wb') as f:
        class_to_coner.tofile(f)
    print("done.")

def creat_u_d_to_ud_table():
    print("creat_u_d_to_ud_table.")
    c = Cube()
    tmp = array.array("H", (65535 for i in range(24*1680)))
    for i in range(40320):
        c.decode_ud_edge(i)
        u = c.encode_u_edge()
        d = c.encode_d_edge()
        #print(c.ep,u,d)
        assert(u < 1680 and d < 1680)
        assert(tmp[u*24 + d%24] == 65535)
        tmp[u*24 + d%24] = i
        
    with open("u_d_to_ud",'wb') as f:
        tmp.tofile(f)

#S_U4_BASIC  = ((1,2,3,0,5,6,7,4,10,8,11,9),(0,0,0,0,0,0,0,0,1,1,1,1),(1,2,3,0,7,4,5,6),(0,0,0,0,0,0,0,0))
# S_F2：围绕 FD 这个轴转动 180°，2 种
#S_F2_BASIC  = ((4,7,6,5,0,3,2,1,9,8,11,10),(0,0,0,0,0,0,0,0,0,0,0,0),(5,6,7,4,3,0,1,2),(0,0,0,0,0,0,0,0))
# S_LR2：关于 LR 层的镜像，2 种
#S_LR2_BASIC = ((0,3,2,1,4,7,6,5,9,8,11,10),(0,0,0,0,0,0,0,0,0,0,0,0),(3,2,1,0,5,4,7,6),(3,3,3,3,3,3,3,3))

def print_sym_table_16():
    for a in range(2):
        for b in range(4):
            for c in range(2):
                cube = Cube()
                for i in range(a):
                    cube.route(S_F2_BASIC)
                for i in range(b):
                    cube.route(S_U4_BASIC)
                for i in range(c):
                    cube.route(S_LR2_BASIC)
                s = "F2-"*a + "U4-"*b + "LR2-"*c
                print("(",tuple(cube.ep),",",tuple(cube.er),",",tuple(cube.cp),",",tuple(cube.cr),"), #",s)

def print_sym_inv():
    for i in range(16):
        for j in range(16):
            c = Cube()
            c.route(sym_tab[i])
            c.route(sym_tab[j])
            if c.is_solved():
                print(j,end=',')
        
            
### 编码18种转动后的状态变化
# flip_move（阶段1+2） twist_move（阶段1+2） slice_sort_move（阶段1+2）
### flip × slice 到index的变换
# class_to_index_p1（阶段1，输出index，sym） index_to_class_p1（阶段1）
### 对应上面的变换，对twist进行变换
# twist_sym（阶段1）
### 阶段1查找表，约67.2MB
# prun1_4bit

### 编码18种转动后的状态变化
# corner_move（阶段1+2） d_edge_move（阶段1+2） u_edge_move（阶段1+2） ud_edge_move(阶段2)
### corner 到index的变换
# class_to_index_p2（阶段2，输出index，sym） index_to_class_p2（阶段2）
### 对应上面的变换，对ud_edge进行变换
# twist_sym（阶段1）
