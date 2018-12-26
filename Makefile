CXX=clang++
CXXFLAGS=-c -std=c++11 -g -O2 -Wall -Wno-unused-function -Wshadow -fno-rtti -Wno-shadow -Wno-unused-variable
LDFLAGS=-stdlib=libc++ -lpthread -g -lgd -lpng -lfreetype -liconv -lbz2 -lz
SOURCES=$(wildcard src/*.cpp)
OBJECTS=$(addprefix obj/,$(notdir $(SOURCES:.cpp=.o)))
EXECUTABLE=bin/hangman
DEPS=$(wildcard obj/*.d)

hangman: $(OBJECTS)
	$(CXX) $(LDFLAGS) $(OBJECTS) $(wildcard ../libairplay/obj/*.o) -o $(EXECUTABLE)
	dsymutil $(EXECUTABLE)
	cp $(EXECUTABLE) ./

obj/%.o: src/%.cpp
	$(CXX) $(CXXFLAGS) $< -o $@
	$(CXX) -MM -MP -MT $@ -MT obj/$*.d $(CXXFLAGS) $< > obj/$*.d

-include $(DEPS)

git:
	git commit -a

clean:
	rm obj/*.o
	rm obj/*.d
	rm -r bin/*
