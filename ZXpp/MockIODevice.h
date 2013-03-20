#ifndef MOCKIO
#define MOCKIO

#include "IioDevice.h"
#include <string>
#include <vector>

using namespace std;

class MockIODevice : public IioDevice
{
private:
	int numbers [36];
	int currentNumber;
public:
	//	Constructor / destructor
	MockIODevice();
	~MockIODevice();
	//	Port I/O
	void Write(int port, int dataByte);
	int Read(int port);
};

#endif