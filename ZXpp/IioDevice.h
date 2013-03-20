#ifndef IIO
#define IIO

class IioDevice
{
public:
	virtual void Write(int port, int dataByte) = 0;
	virtual int Read(int port) = 0;
};

#endif