INCLUDE := -I$(shell root-config --incdir) -I$(BOOST_ROOT)/include
OPFLAGS := -O2
CFLAGS := -Wall -Wextra -std=c++14 $(INCLUDE) $(OPFLAGS)
LDFLAGS := $(shell root-config --libs) -lMinuit2

PROGS := fit scanChi2


all: $(PROGS)

fit: fit.o MultijetFOM.o
	g++ $^ $(CFLAGS) $(LDFLAGS) -o $@

scanChi2: scanChi2.o MultijetFOM.o
	g++ $^ $(CFLAGS) $(LDFLAGS) -o $@

%.o: %.cpp
	g++ -c $^ $(CFLAGS) -o $@
