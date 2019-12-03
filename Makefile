PYLON_ROOT=/opt/pylon5
PYLON_INCLUDE=/opt/pylon5/include/
PYLON_LIB=$(shell $(PYLON_ROOT)/bin/pylon-config --libs)
PYLON_LDFLAGS=$(shell $(PYLON_ROOT)/bin/pylon-config --libs-rpath)

timePollTest.o: timePollTest.cpp
	g++ -c timePollTest.cpp

libraryTest.o: libraryTest.cpp
	g++ -c -I$(PYLON_INCLUDE) libraryTest.cpp

bareBonesBasler.o: bareBonesBasler.cpp
	g++ -c -I$(PYLON_INCLUDE) bareBonesBasler.cpp

time_test: timePollTest.o
	g++ timePollTest.o -o timePoll

tests: libraryTest.o bareBonesBasler.o
	g++ libraryTest.o bareBonesBasler.o $(PYLON_LIB) $(PYLON_LDFLAG) -o test

clean: 
	rm *.o

all: time_test tests
