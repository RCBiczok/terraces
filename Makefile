# Makefile June 2011 by Alexandros Stamatakis

CC = clang
CXX = clang++

GOOGLE_LIB_SRC_ROOT = lib/googletest
GOOGLE_LIB_BIN_ROOT = $(GOOGLE_LIB_SRC_ROOT)/build
GOOGLE_LIB_PATH = $(GOOGLE_LIB_BIN_ROOT)/googlemock/gtest
GOOGLE_MAIN_LIB = $(GOOGLE_LIB_PATH)/libgtest.a

CFLAGS = -I. -O2 -fomit-frame-pointer -funroll-loops \
		 -Weverything -Wno-padded -Wno-float-equal -Wno-vla -std=c99

CXXFLAGS = -I. -I$(GOOGLE_LIB_SRC_ROOT)/googletest/include -O2 -fomit-frame-pointer -funroll-loops \
		   -Weverything -Wno-padded -Wno-float-equal -Wno-vla -std=c++14

LIBRARIES = -lm -lgmp

RM = rm -f

objs    = terraces.o input_parser.o

all : terraces terraces_test

GLOBAL_DEPS = axml.h globalVariables.h

terraces : $(objs) main.o
	$(CC) -o terraces $(objs) main.o $(LIBRARIES) 

terraces_test : $(objs) $(GOOGLE_MAIN_LIB) test/terraces_test.o
	$(CXX) -o terraces_test $(objs) test/terraces_test.o $(LIBRARIES) -L$(GOOGLE_LIB_PATH) -lgtest -lpthread

$(GOOGLE_MAIN_LIB) : 
	mkdir -p $(GOOGLE_LIB_BIN_ROOT); cd $(GOOGLE_LIB_BIN_ROOT); cmake ..; make

main.o : main.c

terraces.o : terraces.c

test/terraces_test.o : test/terraces_test.cpp

input_parser.o : input_parser.c input_parser.h

clean : 
	$(RM) *.o test/*.o terraces 
	$(RM) -r $(GOOGLE_LIB_BIN_ROOT)
