# ================================
#   C++ Project Makefile
# ================================

CXX = g++
CXXFLAGS = -std=c++17 -Wall -O2

# curl / pthread
LDFLAGS = -lcurl -lpthread

# 你的所有源文件
SRCS = \
    main.cpp \
    StockStructure.cpp \
    StockGrouper.cpp \
    StockUtils.cpp \
    CurlUtils.cpp \
    MatrixOperator.cpp \
    ThreadUtils.cpp \
    Bootstrapper.cpp \
    StatCalculator.cpp \
    Gnuplot.cpp

# 自动生成对应的 .o
OBJS = $(SRCS:.cpp=.o)

# 最终目标
TARGET = main

# ================================
#   默认规则
# ================================
all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(OBJS) $(LDFLAGS)

# ================================
#   Compile rule
# ================================
%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

# ================================
#   Clean
# ================================
clean:
	rm -f $(OBJS) $(TARGET)

.PHONY: all clean
