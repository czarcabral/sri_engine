cppfiles	= project.cpp #prob3_lib.cpp
hfile		= project_lib.h
exefile		= project

$(exefile) : $(cppfiles) $(hfiles)
	g++ -std=c++11 -pthread $(cppfiles) -o $(exefile)

clean :
	rm -f $(exefile)

check:
	valgrind --leak-check=full $(exefile)
