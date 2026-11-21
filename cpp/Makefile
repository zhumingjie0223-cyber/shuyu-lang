CXX      := clang++
CXXFLAGS := -std=c++17 -O3 -Wall -Wextra -fPIC
AR       := ar

LIB_SRCS := shuyu_lexicon.cpp shuyu_interpreter.cpp shuyu_ops.cpp shuyu_c_api.cpp
LIB_OBJS := $(LIB_SRCS:.cpp=.o)

.PHONY: all test clean

all: libshuyu.a shuyu-cpp

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

libshuyu.a: $(LIB_OBJS)
	$(AR) rcs $@ $^

shuyu-cpp: shuyu_cli.cpp libshuyu.a
	$(CXX) $(CXXFLAGS) shuyu_cli.cpp libshuyu.a -o $@

test: test_shuyu.cpp libshuyu.a
	$(CXX) $(CXXFLAGS) test_shuyu.cpp libshuyu.a -o test_shuyu
	./test_shuyu

clean:
	rm -f $(LIB_OBJS) libshuyu.a shuyu-cpp test_shuyu
