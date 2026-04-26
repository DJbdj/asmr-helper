# ASMR Helper Makefile

CC = gcc
CXX = g++
CXXFLAGS = -std=c++17 -Wall -O2
CFLAGS = -Wall -O2
LDFLAGS =

# 获取 CURL 的编译选项
CURL_CFLAGS = $(shell pkg-config --cflags libcurl 2>/dev/null)
CURL_LIBS = $(shell pkg-config --libs libcurl 2>/dev/null)

# 源文件和目标文件
SRCDIR = src
BUILDDIR = build
TARGET = asmr-helper

CPP_SRCS = $(wildcard $(SRCDIR)/*.cpp)
C_SRCS = $(SRCDIR)/linenoise.c
SRCS = $(CPP_SRCS) $(C_SRCS)
OBJECTS = $(patsubst $(SRCDIR)/%.cpp,$(BUILDDIR)/%.o,$(CPP_SRCS)) \
          $(BUILDDIR)/linenoise.o

# 默认目标
all: $(BUILDDIR) $(TARGET)

# 创建构建目录
$(BUILDDIR):
	mkdir -p $(BUILDDIR)

# 编译 C++ 目标文件
$(BUILDDIR)/%.o: $(SRCDIR)/%.cpp | $(BUILDDIR)
	$(CXX) $(CXXFLAGS) $(CURL_CFLAGS) -c $< -o $@

# 编译 linenoise C 文件
$(BUILDDIR)/linenoise.o: $(SRCDIR)/linenoise.c | $(BUILDDIR)
	$(CC) $(CFLAGS) -c $< -o $@

# 链接可执行文件
$(TARGET): $(OBJECTS)
	$(CXX) $(CXXFLAGS) $(OBJECTS) -o $@ $(CURL_LIBS)

# 清理
clean:
	rm -rf $(BUILDDIR) $(TARGET)

# 安装
install: $(TARGET)
	install -m 755 $(TARGET) /usr/local/bin/

# 卸载
uninstall:
	rm -f /usr/local/bin/$(TARGET)

.PHONY: all clean install uninstall
