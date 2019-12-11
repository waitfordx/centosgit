SRC_C:=$(wildcard *.cpp)  

SRC_O:=$(SRC_C:.cpp=.o)

#test:
#	@echo "$(SRC_O)"

webserver : $(SRC_O)
	g++ -g -o $@ $< 

%.o : %.cpp
	g++  -std=c++11 -c $<  



.PHONY : clean
clean :
	rm -rf *.o
