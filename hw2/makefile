all : system

system : b023040001_hw2.o
	g++ -o b023040001_system b023040001_hw2.o -lpthread -lm
b023040001_hw2.o : b023040001_hw2.cpp
	g++ -c b023040001_hw2.cpp -lpthread
clean :
	rm -f *.o b023040001_system
