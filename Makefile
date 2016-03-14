
CXXFLAGS+=-std=c++14 -O3
CPPFLAGS+=-MMD

-include *.d

test: varint
	./varint

varint: $(patsubst %.cpp,%.o,$(wildcard *.cpp))
	$(CXX) -o $@ $<

clean:
	$(RM) *.o *.d varint
