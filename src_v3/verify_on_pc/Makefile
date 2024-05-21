# 定义编译器和选项
CC = gcc
#CFLAGS = -Wall -g -ffunction-sections -fdata-sections
CFLAGS = -Wall -O3 -Wincompatible-pointer-types

# 定义所有源文件和目标文件
SOURCES = main.c solve.c table.c # 根据实际替换
OBJECTS = $(SOURCES:.c=.o)


# 编译目标
EXECUTABLE = demo

# 默认规则
all: $(EXECUTABLE)

# 规则：将.c文件编译为.o文件
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# 规则：链接.o文件为可执行文件
$(EXECUTABLE): $(OBJECTS)
	$(CC)  $(OBJECTS) -o $@
# -Wl,--gc-sections,--print-gc-sections

# 清理规则
clean:
	rm -f *.o $(EXECUTABLE)

.PHONY: clean