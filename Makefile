
CXXFLAGS+=-std=c++14 -O3
CPPFLAGS+=-MMD

test: varint
	./varint

varint: $(patsubst %.cpp,%.o,$(wildcard *.cpp))
	$(CXX) -o $@ $^

clean:
	$(RM) *.o *.d varint

format:
	clang-format -i *.cpp

-include *.d
