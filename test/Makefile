CPP=clang++
CPPFLAGS=-g -std=c++11

all:irread jit


irread:1.irread.cpp
	${CPP} ${CPPFLAGS} $< -o $@ `llvm-config --cppflags --ldflags --libs core irreader`

jit:2.jit.cpp
	${CPP} ${CPPFLAGS} $< -o $@ `llvm-config --cppflags --ldflags --libs core irreader jit x86`

clean:
	rm irread jit
