CXX = g++
CXXFLAGS = -Wall -Werror -Wpedantic -std=c++17 -Isrc/include
LIBS =

hush: src/main.cpp linereader.o hush.o
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LIBS)

%.o: src/%.cpp src/include/%.hpp
	$(CXX) $(CXXFLAGS) -o $@ -c $< $(LIBS)

clean:
	rm -rf *.o hush
