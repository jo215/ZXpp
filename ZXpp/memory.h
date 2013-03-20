#ifndef MEMORY
#define MEMORY

//	Forward Declarations
#include <string>
class Z80;

/*
	Represents the ROM & RAM available to the ZX Spectrum
*/
class Memory
{
private:
	friend class TestZ80;
	static const int romStart = 0, romEnd = 16383, contentionStart = 0x4000, contentionEnd = 0x7fff;
	void Reset();
public:
	int memory[65536];
	Z80 *CPU;
	//	Constructor / destructor
	Memory(Z80 *z80);
	~Memory();
	//	Public methods
	int Read(int index, bool ulaAccess);
	void CorrectForContention(int index);
	void Write(int index, int value, bool ulaAccess);
	void LoadROM(std::string romFileName);
};

#endif
