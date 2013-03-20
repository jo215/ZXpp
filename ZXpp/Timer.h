#ifndef TIMER
#define TIMER

#include <iostream>
#include <Windows.h>

using namespace std;

class Timer
{
private:
	LARGE_INTEGER freq;
	LARGE_INTEGER t1;

public:
	//	Constructor / destructor
	Timer();
	~Timer();
	void restart();
	double getElapsedTimeMillis();
};
#endif