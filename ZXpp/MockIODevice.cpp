#include "MockIODevice.h"

//	Constructor.
MockIODevice::MockIODevice()
{
	currentNumber = 0;
	int arr[] = {0xc1, 0xc1, 0xc1, 0xc1, 0x29, 0x7d, 0xbb, 0x40, 0x0d, 0x62, 0xf7, 0xf2, 
                            0x9a, 0x02, 0x56, 0xab, 0xd7, 0x01, 0x56, 0xab, 0x0a, 0x09, 0x08, 0x07, 0x06, 0x05, 0x04, 0x03, 0x02, 
                            0x01, 0x06, 0x05, 0x04, 0x03, 0x02, 0x01};
	for (int i = 0; i < 36; i++)
	{
		numbers[i] = arr[i];
	}
}

//	Simulate an IO write
void MockIODevice::Write(int port, int dataByte)
{
	//	do nothing
}

//	Simulate an IO read
int MockIODevice::Read(int port)
{
	return numbers[currentNumber++];
}