#include "Timer.h"

//	Constructor
Timer::Timer()
{
	QueryPerformanceFrequency(&freq);
}

void Timer::restart()
{
	QueryPerformanceCounter(&t1);
}

double Timer::getElapsedTimeMillis()
{
	LARGE_INTEGER time;
	QueryPerformanceCounter(&time);
	return (time.QuadPart - t1.QuadPart) * 1000.0 / freq.QuadPart;
}