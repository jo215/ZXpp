#include "memory.h"
#include "z80.h"
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <windows.h>

//	Constructor
Memory::Memory(Z80 *z80)
{
	CPU = z80;
	Reset();
	LoadROM("48.rom");
}

void Memory::Reset()
{
	for (int i = 0; i < 65536; i++)
	{
		memory[i] = 0;
	}
}

using namespace std;
//	Loads a ROM file into memory
void Memory::LoadROM(string romFileName)
{
	ifstream myfile (romFileName, ios::binary);
	if (myfile.is_open())
	{
		myfile.seekg(0, ios::end);
		size_t fileSize = myfile.tellg();
		myfile.seekg(0, ios::beg);
		vector<byte> data(fileSize, 0);
		myfile.read(reinterpret_cast<char*>(&data[0]), fileSize);
		for (int i = 0; i < data.size(); i++)
		{
			memory[i] = data[i];
		}
		myfile.close();
	} else {
		cout << "Couldn't open ROM file." << endl;
	}
}

//	Returns the contents of a memory location
int Memory::Read(int index, bool ulaAccess = false)
{
	if (!ulaAccess) CorrectForContention(index);
	return memory[index];
}

void Memory::Write(int index, int value, bool ulaAccess = false)
{
	//  Ignore attempts to write to ROM

    if (!ulaAccess) CorrectForContention(index);
    memory[index] = value;
}

//	Performs any required timing adjustments due to memory contention
void Memory::CorrectForContention(int index)
{
	if (CPU == NULL) return;
	if (CPU->cycleTStates >= 14335 && CPU->cycleTStates < 57343)
	{
		int line = (CPU->cycleTStates - 14335) / 224;
		if (line < 192)
		{
			int pos = (CPU->cycleTStates - 14335) % 224;
			int delay = 6 - (pos % 8);
			if (delay < 0) delay = 0;
			CPU->cycleTStates += delay;
		}
	}
}
