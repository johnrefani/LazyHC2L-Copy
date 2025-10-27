
CXX = g++
CXXFLAGS = -std=c++20 -O2 -I/mingw64/include

OBJS_COMMON = road_network.o util.o

all: dhl_runner dhl_query dhl_update

dhl_runner: index.o $(OBJS_COMMON)
	$(CXX) -o $@ $^

dhl_query: query.o $(OBJS_COMMON)
	$(CXX) -o $@ $^

dhl_update: update.o $(OBJS_COMMON)
	$(CXX) -o $@ $^

index.o: index.cpp
	$(CXX) $(CXXFLAGS) -c $<

query.o: query.cpp
	$(CXX) $(CXXFLAGS) -c $<

update.o: update.cpp
	$(CXX) $(CXXFLAGS) -c $<

road_network.o: road_network.cpp
	$(CXX) $(CXXFLAGS) -c $<

util.o: util.cpp
	$(CXX) $(CXXFLAGS) -c $<

clean:
	rm -f *.o dhl_runner dhl_query dhl_update
