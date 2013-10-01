CPP=clang++
CPPFLAGS=-std=c++11

all:irread


irread:1.irread.cpp
	${CPP} ${CPPFLAGS} $< -o $@ `llvm-config --cppflags --ldflags --libs core irreader`

clean:
	rm irread
