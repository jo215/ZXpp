#ifndef FUSETESTS
#define FUSETESTS

#include <vector>
#include "FrameRateCounter.h"
#include "z80.h"
#include "memory.h"
#include "MockIODevice.h"

using namespace std;

class TestZ80 {

private: 
	FrameRateCounter* frc;
	vector<string> testsIn;
	vector<string> testsExpected;
	vector<string> split(const char *str, char c);
	unsigned int hexToInt(string hex);
	
public:
	string readLine(ifstream& stream);
	TestZ80(FrameRateCounter* frc);
	~TestZ80();
	void RunTests();
};

#endif