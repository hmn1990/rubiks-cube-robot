from cube import *
import sys
import time
from collections import deque
def load_test_case(num_test_case):
    print("load test_case.")
    test_case = [None]*num_test_case
    with open("test_case.txt",'r') as f:
        for i in range(num_test_case):
            test_case[i] = f.readline()
    return test_case


class CroodCube():
    def __init__(self):
        self.twist = 0
        self.flip = 0
        self.slice_sort = 11856 # 494*24 for solved cube
        
        self.ud_edge = 0
        self.corner = 0
        
        self.step = 0
        self.last_step = 255
        self.prun_cache = 255
        
    def __str__(self):
        return "twist=%d,flip=%d,slice_sort=%d,step=%d,last_step=%d,ud_edge=%d,corner=%d,prun_cache=%d"% \
               (self.twist,self.flip,self.slice_sort,self.step,self.last_step,self.ud_edge,self.corner,self.prun_cache)

    def move_p1(self, d):
        cube = CroodCube()
        cube.twist = twist_move[18 * self.twist + d]
        cube.flip = flip_move[18 * self.flip + d]
        cube.slice_sort = slice_sort_move[18 * self.slice_sort + d]
        cube.step = self.step + 1
        cube.last_step = d;
        return cube
        
    def move_p2(self, d):
        cube = CroodCube()
        cube.corner = corner_move[18 * self.corner + d]
        cube.ud_edge = ud_edge_move[18 * self.ud_edge + d]
        cube.slice_sort = slice_sort_move[18 * self.slice_sort + d]
        cube.step = self.step + 1
        cube.last_step = d
        return cube
    # 角块方向twist，取值范围<2187
    # 棱边方向flip，取值范围<2048
    # 中间层的4条棱边的选取状态slice_sort/24，取值范围<495
    # flip和slice组合成了一个坐标，flipslice = flip * 495 + slice_sort/24，取值范围<1013760
    # 查询分类号sym = index_to_class[flipslice]，取值范围<64430
    # 剪枝表的索引sym * 2187 + twsit，取值范围<140908410
    def encode_prun1_index(self):
        flipslice = self.flip * 495 + self.slice_sort // 24
        class_ = index_to_class[flipslice] & 0xffff
        sym = index_to_class[flipslice] >> 16
        prun1_index = class_ * 2187 + twist_sym[16 * self.twist + sym]
        return prun1_index
    
    def decode_prun1_index(self, idx):
        self.twist = idx % 2187
        class_     = idx // 2187
        flipslice  = class_to_index[class_]
        self.flip  = flipslice // 495
        self.slice_sort = flipslice % 495 * 24

    def encode_prun21_index(self):
        class_ = index_to_class_p2[self.corner] & 0xffff
        sym = index_to_class_p2[self.corner] >> 16
        prun21_index = class_ * 40320 + ud_edge_sym[16 * self.ud_edge + sym]
        return prun21_index
    
    def decode_prun21_index(self, idx):
        self.ud_edge = idx % 40320
        class_     = idx // 40320
        self.corner  = class_to_index_p2[class_]
        
    def encode_prun22_index(self):
        return self.corner * 24  + self.slice_sort % 24
    
    def decode_prun22_index(self, idx):
        self.corner = idx // 24
        self.slice_sort = 494*24 + idx % 24
        
# deep = 0 count =  1
# deep = 1 count =  1
# deep = 2 count =  5
# deep = 3 count =  44
# deep = 4 count =  487 
# deep = 5 count =  5841
# deep = 6 count =  68364
# deep = 7 count =  776568 1%
# deep = 8 count =  7950748 6%
# deep = 9 count =  52098876 37%
# deep = 10 count = 76236234 54%
# deep = 11 count = 3771112 3%
# deep = 12 count = 129
def creat_prun_table_1():
    # 准备工作，计算每个等价类，有多少flipslice相同的项目
    ## ------------------------------------------------------ ##
    time_last_log = time.time()
    print("creat_prun_table_1()")
    print("creat fs_sym")
    N_CLASS = 64430
    fs_sym = array.array('H', (0 for i in range(N_CLASS)))
    cc = Cube()
    for i in range(N_CLASS):
        if( time.time() - time_last_log > 1.0):
            time_last_log = time.time()
            print("\r%d%%"%(i*100//N_CLASS),end='')
        slice_ = class_to_index[i] % 495
        flip   = class_to_index[i] // 495
        cc.decode_flip(flip)
        cc.decode_slice_sort(slice_ * 24)
        for j in range(16):
            cube_b = Cube(sym_tab[j])
            cube_b.route(cc)
            cube_b.route(sym_tab[sym_inv_index[j]])
            if cube_b.encode_slice_sort() // 24 == slice_ and cube_b.encode_flip() == flip:
                fs_sym[i] |= 1 << j
        assert(fs_sym[i]>=1)
    print("")
    ## ------------------------------------------------------ ##
    N_INDEX = 140908410
    print("creat array, size =", N_INDEX)
    prun_table = array.array("B", (255 for i in range(N_INDEX)))
    deep = 0
    # 填写还原状态
    cube = CroodCube()
    index = cube.encode_prun1_index()
    prun_table[index] = deep
    count = 1
    print("\ndeep =", deep, "count =", count)                                
    count_old = 1
    while count < N_INDEX:
        for i in range(N_INDEX):
            if prun_table[i] == deep:
                cube.decode_prun1_index(i)
                for m in range(18):
                    cube_new = cube.move_p1(m)
                    #index = cube_new.encode_prun1_index()
                    flipslice = cube_new.flip * 495 + cube_new.slice_sort // 24
                    classidx = index_to_class[flipslice] & 0xffff
                    sym = index_to_class[flipslice] >> 16
                    twist = twist_sym[16 * cube_new.twist + sym]
                    index = classidx * 2187 + twist
                    # 如果还没有填表，填写，twist等价的也需要处理
                    if prun_table[index] == 255:
                        prun_table[index] = deep + 1
                        count += 1
                        sym = fs_sym[classidx]
                        if sym != 1:
                            for k in range(1, 16):
                                if sym & (1<<k):
                                    twist2 = twist_sym[16 * twist + k]
                                    index2 = 2187 * classidx + twist2
                                    if prun_table[index2] == 255:
                                        prun_table[index2] = deep + 1
                                        count += 1
                                    #else:
                                    #    assert(prun_table[index2] >= deep -1)
                        if( time.time() - time_last_log > 1.0):
                            time_last_log = time.time()
                            print("\r%f%%"%(count*100/N_INDEX),end='')
                    #else:
                    #    assert(prun_table[index] >= deep -1)
        deep += 1 # 填完一层结点，深度加1
        print("\ndeep =", deep, "count =", count - count_old)
        count_old = count
#     with open("prun1",'wb') as f:
#         prun_table.tofile(f)
    print("compress_and_save")
#     prun_table_4bit = array.array("B",(0 for i in range(N_INDEX // 2)))
#     for i in range(N_INDEX):
#         if i & 1:
#             prun_table_4bit[i >> 1] |= prun_table[i] << 4
#         else:
#             prun_table_4bit[i >> 1] |= prun_table[i]
#         
#     with open("prun1_4bit",'wb') as f:
#         prun_table_4bit.tofile(f)
#     print("ok")
    prun_table_2bit = array.array("B",(0 for i in range(N_INDEX // 4+1)))
    for i in range(N_INDEX):
        deep = prun_table[i]
        deep %= 3
        prun_table_2bit[i >> 2] |= deep << (2 * (i & 3))

    with open("prun1_2bit",'wb') as f:
        prun_table_2bit.tofile(f)
    print("ok")
    
# deep = 0 count =  1
# deep = 1 count =  3
# deep = 2 count =  10
# deep = 3 count =  52
# deep = 4 count =  285
# deep = 5 count =  1318
# deep = 6 count =  5671
# deep = 7 count =  26502
# deep = 8 count =  115467
# deep = 9 count =  470846
# deep = 10 count = 1853056 2%
# deep = 11 count = 6535823 6%
# deep = 12 count = 18349792 16%
# deep = 13 count = 32843350 29%
# deep = 14 count = 34118883 31%
# deep = 15 count = 15974563 14%
# deep = 16 count = 1290346 1%
# deep = 17 count = 19777
# deep = 18 count = 15

# corner ud_edges
def creat_prun_table_21(max_deep = 10):
    # 准备工作，计算每个等价类，有多少conner相同的项目
    ## ------------------------------------------------------ ##
    time_last_log = time.time()
    print("creat_prun_table_1()")
    print("creat corner_sym")
    N_CLASS = 2768
    fs_sym = array.array('H', (0 for i in range(N_CLASS)))
    cc = Cube()
    for i in range(N_CLASS):
        if( time.time() - time_last_log > 1.0):
            time_last_log = time.time()
            print("\r%d%%"%(i*100//N_CLASS),end='')
        corner = class_to_index_p2[i]
        cc.decode_corner(corner)
        for j in range(16):
            cube_b = Cube(sym_tab[j])
            cube_b.route(cc)
            cube_b.route(sym_tab[sym_inv_index[j]])
            if cube_b.encode_corner() == corner:
                fs_sym[i] |= 1 << j
        assert(fs_sym[i]>=1)
    print("")
    ## ------------------------------------------------------ ##
    N_INDEX = 40320 * 2768
    print("creat array, size =", N_INDEX)
    prun_table = array.array("B", (255 for i in range(N_INDEX)))
    deep = 0
    # 填写还原状态
    cube = CroodCube()
    index = cube.encode_prun21_index()
    prun_table[index] = deep
    count = 1
    print("\ndeep =", deep, "count =", count)                                
    count_old = 1
    while count < N_INDEX:
        for i in range(N_INDEX):
            if prun_table[i] == deep:
                cube.decode_prun21_index(i)
                for m in (2,5,6,7,8,9,10,11,14,17):
                    cube_new = cube.move_p2(m)
                    #index = cube_new.encode_prun21_index()
                    classidx = index_to_class_p2[cube_new.corner] & 0xffff
                    sym = index_to_class_p2[cube_new.corner] >> 16
                    ud_edge = ud_edge_sym[16 * cube_new.ud_edge + sym]
                    index = classidx * 40320 + ud_edge
                    # 如果还没有填表，填写，ud_edge等价的也需要处理
                    if prun_table[index] == 255:
                        prun_table[index] = deep + 1
                        count += 1
                        sym = fs_sym[classidx]
                        if sym != 1:
                            for k in range(1, 16):
                                if sym & (1<<k):
                                    ud_edge2 = ud_edge_sym[16 * ud_edge + k]
                                    index2 = 40320 * classidx + ud_edge2
                                    if prun_table[index2] == 255:
                                        prun_table[index2] = deep + 1
                                        count += 1
                                    else:
                                        assert(prun_table[index2] >= deep -1)
                        if( time.time() - time_last_log > 1.0):
                            time_last_log = time.time()
                            print("\r%f%%"%(count*100/N_INDEX),end='')
                    else:
                        assert(prun_table[index] >= deep -1)
        deep += 1 # 填完一层结点，深度加1
        print("\ndeep =", deep, "count =",count - count_old)
        count_old = count
        if deep >= max_deep:
            break
        
#     with open("prun21",'wb') as f:
#         prun_table.tofile(f)
    print("to 2bit table")
    prun_table_2bit = array.array("B",(0 for i in range(N_INDEX // 4)))

    for i in range(N_INDEX):
        if prun_table[i] == 255: # >max_deep的情况，用3标记
            deep = 3
        else:
            deep = prun_table[i] % 3
        prun_table_2bit[i >> 2] |= deep << (2 * (i & 3))

    with open("prun21_2bit",'wb') as f:
        prun_table_2bit.tofile(f)
    print("ok")




# corner sort 
def creat_prun_table_22():
    time_last_log = time.time()
    ## ------------------------------------------------------ ##
    N_INDEX = 40320 * 24
    print("creat array, size =", N_INDEX)
    prun_table = array.array("B", (255 for i in range(N_INDEX)))
    deep = 0
    # 填写还原状态
    cube = CroodCube()
    index = cube.encode_prun22_index()
    prun_table[index] = deep
    count = 1
    print("\ndeep =", deep, "count =", count)                                
    count_old = 1
    while count < N_INDEX:
        for i in range(N_INDEX):
            if prun_table[i] == deep:
                cube.decode_prun22_index(i)
                for m in (2,5,6,7,8,9,10,11,14,17):
                    cube_new = cube.move_p2(m)
                    index = cube_new.encode_prun22_index()
                    # 如果还没有填表，填写，ud_edge等价的也需要处理
                    if prun_table[index] == 255:
                        prun_table[index] = deep + 1
                        count += 1
                        if( time.time() - time_last_log > 1.0):
                            time_last_log = time.time()
                            print("\r%f%%"%(count*100/N_INDEX),end='')
                    else:
                        assert(prun_table[index] >= deep -1)
        deep += 1 # 填完一层结点，深度加1
        print("\ndeep =", deep, "count =",count - count_old)
        count_old = count
    with open("prun22",'wb') as f:
        prun_table.tofile(f)
        
def get_deep_prun1_mod3(x):
    y = prun1_2bit[x >> 2]
    y >>= (x & 3) * 2
    return y & 3
def get_deep_prun21_mod3(x):
    y = prun21_2bit[x >> 2]
    y >>= (x & 3) * 2
    return y & 3

def get_deep_prun1(cc):
    twist = cc.twist
    flip = cc.flip
    slice_sort = cc.slice_sort
    
    flipslice = flip * 495 + slice_sort // 24
    classidx = index_to_class[flipslice] & 0xffff
    sym = index_to_class[flipslice] >> 16
    prun1_index = classidx * 2187 + twist_sym[16 * twist + sym]
    deep_mod3 = get_deep_prun1_mod3(prun1_index)
    deep = 0
    while prun1_index != 174960: #若没到目标状态
        for d in range(18):
            new_twist = twist_move[18 * twist + d]
            new_flip = flip_move[18 * flip + d]
            new_slice_sort = slice_sort_move[18 * slice_sort + d]
            
            flipslice = new_flip * 495 + new_slice_sort // 24
            classidx = index_to_class[flipslice] & 0xffff
            sym = index_to_class[flipslice] >> 16
            prun1_index = classidx * 2187 + twist_sym[16 * new_twist + sym]
            new_deep_mod3 = get_deep_prun1_mod3(prun1_index)
            #print(flipslice,classidx,sym)
            #print(prun1_index,new_twist,new_flip,new_slice_sort,deep,new_deep_mod3,deep_mod3) 
            diff = new_deep_mod3 - deep_mod3
            if diff == 2 or diff == -1: # 如果转动后距离更近
                deep += 1
                twist = new_twist
                flip = new_flip
                slice_sort = new_slice_sort
                deep_mod3 = new_deep_mod3
                break
        assert(deep <= 12)
    cc.prun_cache = deep
    return deep


def get_deep_prun21(cc):
    #deep1 = prun21[cc.encode_prun21_index()]
    corner = cc.corner
    ud_edge = cc.ud_edge
    classidx = index_to_class_p2[corner] & 0xffff
    sym = index_to_class_p2[corner] >> 16
    prun21_index = classidx * 40320 + ud_edge_sym[16 * ud_edge + sym]
    
    deep_mod3 = get_deep_prun21_mod3(prun21_index)
    if deep_mod3 == 3: #还原步骤 > 10
        return 255
    deep = 0
    while prun21_index != 0: #若没到目标状态
        #print(corner,ud_edge)
        for d in (2,5,6,7,8,9,10,11,14,17):
            new_corner = corner_move[18 * corner + d]
            new_ud_edge = ud_edge_move[18 * ud_edge + d]
            classidx = index_to_class_p2[new_corner] & 0xffff
            sym = index_to_class_p2[new_corner] >> 16
            prun21_index = classidx * 40320 + ud_edge_sym[16 * new_ud_edge + sym]
            new_deep_mod3 = get_deep_prun21_mod3(prun21_index)
            #print(d,new_corner,new_ud_edge,"deep=",deep,\
            #      "new_deep_mod3=",new_deep_mod3,"deep_mod3=",deep_mod3,prun21[prun21_index])
            if new_deep_mod3 != 3: # 步骤数未记录的，一定是距离还原状态更远的
                diff = new_deep_mod3 - deep_mod3
                if diff == 2 or diff == -1: # 如果转动后距离更近
                    deep += 1
                    corner = new_corner
                    ud_edge = new_ud_edge
                    deep_mod3 = new_deep_mod3
                    break
        assert(deep <= 20)
    cc.prun_cache = deep
    #print("deep=",deep)
    # prun22较小，不压缩 ------------------------------------
    return deep

max_stack_p1 = 0
max_stack_p2 = 0

def search_p2(cc2, max_deep):
    print(cc2)
    cc2.step = 0
    cc2.last_step = -99
    q = deque()
    q.append(cc2)
    steps_record = [-99]*18
    while len(q) != 0:
        global max_stack_p2
        max_stack_p2 = max(len(q),max_stack_p2)
        c = q.pop()
        steps_record[c.step - 1] = c.last_step
        assert(c.prun_cache < 20)
        deep_prun21 = get_deep_prun21_mod3(c.encode_prun21_index())
        if deep_prun21 == 3: # 跳过步骤数量>10的
            continue
        diff = deep_prun21 - c.prun_cache % 3
        if diff == -1 or diff == 2:
            c.prun_cache -= 1
        elif diff == 1 or diff == -2:
            c.prun_cache += 1
        estimate = max(c.prun_cache, prun22[c.encode_prun22_index()] )
        if estimate == 0:
            return steps_record[0 : c.step]
        # 舍弃max_dedep步骤内无解的
        if estimate + c.step <= max_deep:
            for i in (2,5,6,7,8,9,10,11,14,17):
                # 如果旋转的面和上次旋转的一致，舍弃，可减少1/6的情况
                if c.last_step // 3 != i // 3:
                    c_new = c.move_p2(i)
                    c_new.prun_cache = c.prun_cache
                    q.append(c_new)
    return None

def solve(x, step_limit = 22):
    c = Cube()
    c.from_face_54(x)
    cc1 = CroodCube()
    cc2 = CroodCube()
    
    class_ = index_to_class_p2[cc1.corner] & 0xffff
    cc1.flip = c.encode_flip()
    cc1.twist = c.encode_twist()
    cc1.slice_sort = c.encode_slice_sort()

    u_edge_start = c.encode_u_edge()
    d_edge_start = c.encode_d_edge()
    corner_start = c.encode_corner()
    init_deep = get_deep_prun1(cc1)
    for p1_step_limit in range(init_deep, 999):
        print("p1_step_limit=",p1_step_limit)
        cc1.step = 0
        cc1.last_step = -99
        q = deque()
        q.append(cc1)
        steps_record = [-99]*18
        
        while len(q) != 0:
            global max_stack_p1
            max_stack_p1 = max(len(q),max_stack_p1)
            c = q.pop()
            steps_record[c.step - 1] = c.last_step
            diff = get_deep_prun1_mod3(c.encode_prun1_index()) - c.prun_cache % 3
            if diff == -1 or diff == 2:
                c.prun_cache -= 1
            elif diff == 1 or diff == -2:
                c.prun_cache += 1
            if c.prun_cache == 0:# 如果找到阶段1的解
                s1_len = c.step

                # 生成阶段2的初始数据
                slice_sort = c.slice_sort
                p2_step_limit = step_limit - s1_len#### p2允许步骤数量
                p2_step_limit = min(10, p2_step_limit) # 限制二阶段最多10步
                u_edge = u_edge_start
                d_edge = d_edge_start
                corner = corner_start
                for i in range(s1_len):
                    u_edge = u_edge_move[18 * u_edge + steps_record[i]]
                    d_edge = d_edge_move[18 * d_edge + steps_record[i]]
                    corner = corner_move[18 * corner + steps_record[i]]
                cc2.ud_edge = u_d_to_ud[u_edge*24 + d_edge%24]
                cc2.slice_sort = slice_sort
                cc2.corner = corner
                p22 = prun22[cc2.encode_prun22_index()]
                if p22 <= p2_step_limit:
                    p21 = get_deep_prun21(cc2)
                    if p21 <= p2_step_limit:
                        for max_deep in range(max(p21,p22), p2_step_limit+1):
                            s2 = search_p2(cc2, max_deep)
                            if s2 != None:
                                assert(len(s2) <= p2_step_limit)
                                s1 = steps_record[0 : s1_len]
                                for i in range(len(s1)):
                                    s1[i] = route_index[s1[i]]
                                print(s1,"LENGTH1",len(s1))
                                for i in range(len(s2)):
                                    s2[i] = route_index[s2[i]]
                                print(s2,"LENGTH2",len(s2))
                                return s1 + s2
                
            # 舍弃p1_step_limit步骤内无解的
            if c.prun_cache + c.step <= p1_step_limit:
                for i in range(18):
                    # 如果旋转的面和上次旋转的一致，舍弃，可减少1/6的情况
                    if c.last_step // 3 != i // 3:
                        c_new = c.move_p1(i)
                        c_new.prun_cache = c.prun_cache
                        q.append(c_new)
    
    return s1 + s2

def test_solve():
    #test_case = load_test_case(1)
    test_case = ["BRUBUUULLURBRRULDRRDFUFFFRBDDDFDBFLBDUFFLFDDLLBRLBLUBR\n"]
    step_log=[0]*25
    for s in test_case:
        print(s,end='')
        s1 = solve(s,20)
        c = Cube()
        c.from_face_54(s)
        
        for i in range(len(s1)):
            c.route(s1[i])
        #print("slice_sort",c.encode_slice_sort(),"corner",c.encode_corner(),"ud_edge",c.encode_ud_edge())
        #c.dump()
        assert(c.is_solved())
        step_log[len(s1)] += 1
    print(step_log, max_stack_p1, max_stack_p2)
#date_size [78732, 73728, 427680, 69984, 427680, 427680, 1451520, 1451520, 1290240, 4055040, 257720, 161280, 5536, 80640, 35227104, 27901440, 967680]
#date_start_addr [0, 78732, 152460, 580140, 650124, 1077804, 1505484, 2957004, 4408524, 5698764, 9753804, 10011524, 10172804, 10178340, 10258980, 45486084, 73387524]
def pkg_table():
    date = (twist_move, flip_move, slice_sort_move,\
            twist_sym, u_edge_move, d_edge_move,\
            corner_move, ud_edge_move, ud_edge_sym,\
            index_to_class,\
            index_to_class_p2,
            u_d_to_ud, prun1_2bit, prun21_2bit, prun22)
    date_size = [0]*15
    date_start_addr = [0]*15
    addr = 0
    for i in range(15):
        date_start_addr[i] = addr
        ds = len(date[i])
        if date[i].typecode == 'L':
            ds *= 4
        elif date[i].typecode == 'H':
            ds *= 2
        if ds % 4 != 0:
            ds = ds // 4 * 4 + 4
        date_size[i] = ds
        addr += ds
    print("date_size", date_size)
    print("date_start_addr", date_start_addr)
    print("alloc memory done, about 70M")
    out = array.array("B", (0 for i in range(addr)))
    for x in range(15):
        if date[x].typecode == 'L':
            for i in range(len(date[x])):
                out[ date_start_addr[x] + 4*i + 0] = date[x][i] & 0xff
                out[ date_start_addr[x] + 4*i + 1] = ( date[x][i] >> 8 ) & 0xff
                out[ date_start_addr[x] + 4*i + 2] = ( date[x][i] >> 16 ) & 0xff
                out[ date_start_addr[x] + 4*i + 3] = date[x][i] >> 24
        elif date[x].typecode == 'H':
            for i in range(len(date[x])):
                out[ date_start_addr[x] + 2*i + 0] = date[x][i] & 0xff
                out[ date_start_addr[x] + 2*i + 1] = date[x][i] >> 8
        elif date[x].typecode == 'B':
            for i in range(len(date[x])):
                out[ date_start_addr[x] + i ] = date[x][i]
        else:
            assert(0)
        print(".",end='')
    with open("lookup.dat",'wb') as f:
        out.tofile(f)
    print("done")
    

print("load move tables.")
succ = False
while not succ:
    twist_move = load_table("twist_move",2187*18)
    flip_move = load_table("flip_move",2048*18)
    slice_sort_move = load_table("slice_sort_move", 11880*18)
    twist_sym = load_table("twist_sym", 2187*16)
    u_edge_move = load_table("u_edge_move", 11880*18)
    d_edge_move = load_table("d_edge_move", 11880*18)
    corner_move = load_table("corner_move", 40320*18)
    ud_edge_move = load_table("ud_edge_move", 40320*18)
    ud_edge_sym = load_table("ud_edge_sym", 40320*16)
    if twist_move != None and flip_move != None and slice_sort_move != None and \
       twist_sym != None and u_edge_move != None and d_edge_move != None and \
       corner_move != None and ud_edge_move != None and ud_edge_sym != None:
        succ = True
    else:
        creat_crood_move_table()
succ = False
while not succ:
    index_to_class = load_table("index_to_class_p1", 2048 * 495, type_code = "L")
    class_to_index = load_table("class_to_index_p1", 64430, type_code = "L")
    if index_to_class != None and class_to_index != None:
        succ = True
    else:
        creat_index_to_class_table_p1()
succ = False
while not succ:
    index_to_class_p2 = load_table("index_to_class_p2", 40320, type_code = "L")
    class_to_index_p2 = load_table("class_to_index_p2", 2768, type_code = "H")
    if index_to_class != None and class_to_index != None:
        succ = True
    else:
        creat_index_to_class_table_p2()
succ = False
while not succ:
    u_d_to_ud = load_table("u_d_to_ud",1680*24)
    if u_d_to_ud != None:
        succ = True
    else:
        creat_u_d_to_ud_table()
print("load prun")
succ = False
while not succ:
    prun1_2bit = load_table("prun1_2bit", 35227103, type_code = "B")
    if prun1_2bit != None:
        succ = True
    else:
        creat_prun_table_1()
succ = False
while not succ:
    prun21_2bit = load_table("prun21_2bit", 27901440, type_code = "B")
    if prun21_2bit != None:
        succ = True
    else:
        creat_prun_table_21()
succ = False
while not succ:
    prun22 = load_table("prun22", 40320 * 24, type_code = "B")
    if prun22 != None:
        succ = True
    else:
        creat_prun_table_22()

    


#test_solve()
pkg_table()