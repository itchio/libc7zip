
CXX=g++
CFLAGS=-I./lib7zip
LDFLAGS=./lib7zip/lib7zip.a -lole32 -loleaut32 -luuid

all:
	$(CXX) $(CFLAGS) main.cpp -o main $(LDFLAGS)
