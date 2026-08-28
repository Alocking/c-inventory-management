# ============================================================
# 进销存管理系统 - Makefile
# 跨平台支持：Ubuntu / Windows (MinGW)
# ============================================================

CC       = gcc
CFLAGS   = -std=c99 -Wall -Wextra -O2
SRCDIR   = src

# 主程序源文件（不含 test_main.c）
MAIN_SRCS = $(SRCDIR)/main.c $(SRCDIR)/product.c $(SRCDIR)/inventory.c \
            $(SRCDIR)/order.c $(SRCDIR)/storage.c $(SRCDIR)/avl.c

# 测试程序源文件（test_main.c 已 #include "main.c"，需 -Dmain 重命名）
TEST_SRCS = $(SRCDIR)/test_main.c $(SRCDIR)/product.c $(SRCDIR)/inventory.c \
            $(SRCDIR)/order.c $(SRCDIR)/storage.c $(SRCDIR)/avl.c

# Windows 下 .exe 后缀
ifeq ($(OS),Windows_NT)
    TARGET  = inventory.exe
    TEST_BIN = test.exe
    RM      = del /Q
else
    TARGET  = inventory
    TEST_BIN = test
    RM      = rm -f
endif

# 默认目标：编译主程序
all: $(TARGET)

# 主程序编译
$(TARGET): $(MAIN_SRCS) $(SRCDIR)/all.h
	$(CC) $(CFLAGS) -o $@ $(MAIN_SRCS)

# 测试程序编译（屏蔽 main.c 中的 main 函数）
test: $(TEST_SRCS) $(SRCDIR)/all.h
	$(CC) $(CFLAGS) -Dmain=_disabled_main -o $(TEST_BIN) $(TEST_SRCS)

# 运行测试
test-run: test
	./$(TEST_BIN)

# 清理
clean:
ifeq ($(OS),Windows_NT)
	-del /Q *.o $(TARGET) $(TEST_BIN) product.dat stock_order.dat 2>nul
else
	rm -f *.o $(TARGET) $(TEST_BIN) product.dat stock_order.dat
endif

# 运行主程序
run: $(TARGET)
	./$(TARGET)

.PHONY: all clean test test-run run