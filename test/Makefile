CPP=clang++

all:irread


irread:1.irread.cpp
	${CPP} $< -o $@ `llvm-config --cppflags --ldflags --libs core irreader`

clean:
	rm irread
