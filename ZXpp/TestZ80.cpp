#include "TestZ80.h"
#include <fstream>
#include <sstream>
#include <string>

//	Constructor.
TestZ80::TestZ80(FrameRateCounter* fr)
{
	frc = fr;
	RunTests();
}

//	Run the fuse tests
void TestZ80::RunTests()
{
	Memory* memory = new Memory(nullptr);
	Z80* target = new Z80(memory);
	MockIODevice* io = new MockIODevice();
	target->AddDevice(io);

	ifstream in("tests.in");
	ifstream expect("tests.expected");

	string line;
	while (in.good())
	{
		memory->Reset();
		target->Reset();
		//	Opcode being tested
		string testName = readLine(in);
		OutputDebugString(("Test: #" + testName + "\n").c_str());
		unsigned int opcode;
		if (testName.find("_") != string::npos)
		{
			opcode = hexToInt(testName.substr(0, testName.find("_")));
		}
		else
		{
			opcode = hexToInt(testName);
		}
		//	Standard registers
		string inputRegisters = readLine(in);
		vector<string> regtemp = split(inputRegisters.c_str(), ' ');

		target->A = hexToInt(regtemp[0].substr(0, 2));
		target->F = hexToInt(regtemp[0].substr(2, 2));
		target->B = hexToInt(regtemp[1].substr(0, 2));
		target->C = hexToInt(regtemp[1].substr(2, 2));
		target->D = hexToInt(regtemp[2].substr(0, 2));
		target->E = hexToInt(regtemp[2].substr(2, 2));
		target->H = hexToInt(regtemp[3].substr(0, 2));
		target->L = hexToInt(regtemp[3].substr(2, 2));

		target->A2 = hexToInt(regtemp[4].substr(0, 2));
		target->F2 = hexToInt(regtemp[4].substr(2, 2));
		target->B2 = hexToInt(regtemp[5].substr(0, 2));
		target->C2 = hexToInt(regtemp[5].substr(2, 2));
		target->D2 = hexToInt(regtemp[6].substr(0, 2));
		target->E2 = hexToInt(regtemp[6].substr(2, 2));
		target->H2 = hexToInt(regtemp[7].substr(0, 2));
		target->L2 = hexToInt(regtemp[7].substr(2, 2));

		target->IXH = hexToInt(regtemp[8].substr(0, 2));
		target->IXL = hexToInt(regtemp[8].substr(2, 2));
		target->IYH = hexToInt(regtemp[9].substr(0, 2));
		target->IYL = hexToInt(regtemp[9].substr(2, 2));
		target->SP = hexToInt(regtemp[10]);
		target->PC = hexToInt(regtemp[11]);

		//	Special registers
		regtemp = split(readLine(in).c_str(), ' ');
		target->I = hexToInt(regtemp[0]);
		target->R = hexToInt(regtemp[1]);
		if (regtemp[2] == "0")
		{
			target->IFF1 = 0;
		}
		else if (regtemp[2] == "1")
		{
			target->IFF1 = 1;
		}
		else 
		{
			exit(0);
		}
		if (regtemp[3] == "0")
		{
			target->IFF2 = 0;
		}
		else if (regtemp[3] == "1")
		{
			target->IFF2 = 1;
		}
		else 
		{
			exit(0);
		}
		if (regtemp[5] == "0")
		{
			target->isHalted = false;
		}
		else if (regtemp[5] == "1")
		{
			target->isHalted = true;
		}
		else 
		{
			exit(0);
		}
		int numStates;
		stringstream(regtemp[regtemp.size()-1]) >> numStates;

		//	Memory
		string testInput = readLine(in);
		while (testInput != "-1")
		{
			vector<string> memDef = split(testInput.c_str(), ' ');
			assert(memDef[memDef.size() - 1] == "-1");
			for (int i = 1; i < memDef.size() - 1; i++)
			{
				memory->memory[hexToInt(memDef[0]) + (i-1)] = hexToInt(memDef[i]);
			}
			testInput = readLine(in);
		}
		//	Blank line
		readLine(in);

		//	Run z80 for 1 instruction
		target->Run(false, numStates);

		//	Read expected results
		assert(readLine(expect) == testName);

		//	ignore precise timing
		testInput = readLine(expect);
		while(testInput.find(" ") == 0)
		{
			testInput = readLine(expect);
		}

		//	Output to debug window
		OutputDebugString(("A:" + to_string(target->A) + "\n").c_str());
		OutputDebugString(("F:" + to_string(target->F) + "\n").c_str());
		OutputDebugString(("B:" + to_string(target->B) + "\n").c_str());
		OutputDebugString(("C:" + to_string(target->C) + "\n").c_str());
		OutputDebugString(("D:" + to_string(target->D) + "\n").c_str());
		OutputDebugString(("E:" + to_string(target->E) + "\n").c_str());
		OutputDebugString(("H:" + to_string(target->H) + "\n").c_str());
		OutputDebugString(("L:" + to_string(target->L) + "\n").c_str());

		OutputDebugString(("PC:" + to_string(target->PC) + "\n").c_str());
		
		//	Check registers
		regtemp = split(testInput.c_str(), ' ');
		OutputDebugString(("Expected F:" + to_string(hexToInt(regtemp[0].substr(2,2))) + "\n").c_str());
		OutputDebugString(("Expected PC:" + to_string(hexToInt(regtemp[11])) + "\n").c_str());

		assert(target->A == hexToInt(regtemp[0].substr(0,2)));
		assert(target->F == hexToInt(regtemp[0].substr(2,2)));
		assert(target->B == hexToInt(regtemp[1].substr(0,2)));
		assert(target->C == hexToInt(regtemp[1].substr(2,2)));
		assert(target->D == hexToInt(regtemp[2].substr(0,2)));
		assert(target->E == hexToInt(regtemp[2].substr(2,2)));
		assert(target->H == hexToInt(regtemp[3].substr(0,2)));
		assert(target->L == hexToInt(regtemp[3].substr(2,2)));

		assert(target->A2 == hexToInt(regtemp[4].substr(0,2)));
		assert(target->F2 == hexToInt(regtemp[4].substr(2,2)));
		assert(target->B2 == hexToInt(regtemp[5].substr(0,2)));
		assert(target->C2 == hexToInt(regtemp[5].substr(2,2)));
		assert(target->D2 == hexToInt(regtemp[6].substr(0,2)));
		assert(target->E2 == hexToInt(regtemp[6].substr(2,2)));
		assert(target->H2 == hexToInt(regtemp[7].substr(0,2)));
		assert(target->L2 == hexToInt(regtemp[7].substr(2,2)));

		assert(target->IXH == hexToInt(regtemp[8].substr(0,2)));
		assert(target->IXL == hexToInt(regtemp[8].substr(2,2)));
		assert(target->IYH == hexToInt(regtemp[9].substr(0,2)));
		assert(target->IYL == hexToInt(regtemp[9].substr(2,2)));

		assert(target->SP == hexToInt(regtemp[10]));
		assert(opcode == 0 || target->PC == hexToInt(regtemp[11]));

		//	Special reg
		regtemp = split(readLine(expect).c_str(), ' ');
		
		assert(target->I == hexToInt(regtemp[0]));
		assert(opcode == 0 || target->R == hexToInt(regtemp[1]));
		assert((target->IFF1 == 1 && regtemp[2] == "1") || (target->IFF1 == 0 && regtemp[2] == "0"));
		assert((target->IFF2 == 1 && regtemp[3] == "1") || (target->IFF2 == 0 && regtemp[3] == "0"));
		assert((target->isHalted == true && regtemp[5] == "1") || (target->isHalted == false && regtemp[5] == "0"));

		stringstream(regtemp[6]) >> numStates;
		assert(opcode == 0 || target->cycleTStates == numStates);

		//	Memory
		testInput = readLine(expect);
		while (testInput.size() > 2)
		{
			vector<string> memDef = split(testInput.c_str(), ' ');
			assert(memDef[memDef.size() - 1] == "-1");
			for (int i = 1; i < memDef.size() - 1; i++)
			{
				OutputDebugString("Location ");
				OutputDebugString(to_string(hexToInt(memDef[0]) + (i - 1)).c_str());
				int val = hexToInt(memDef[i].c_str());
				OutputDebugString(", Should be: ");
				OutputDebugString(to_string(val).c_str());
				OutputDebugString(", is: ");
				OutputDebugString(to_string(memory->memory[hexToInt(memDef[0]) + (i - 1)]).c_str());

				assert(memory->memory[hexToInt(memDef[0]) + (i - 1)] == val);
			}
			testInput = readLine(expect);
		}

		frc->setOutput("Complete");
	}

}

//	Reads the next line of text from the give stream.
string TestZ80::readLine(ifstream& stream)
{
	string line;
	getline(stream, line);
	return line;
}

//	Converts a hex string to an int
unsigned int TestZ80::hexToInt(string s)
{
	unsigned int i;
	stringstream ss;
	ss << hex << s;
	ss >> i;
	return i;
}

//	Splits a string according to the given separator.
//	Credit: stack overflow
vector<string> TestZ80::split(const char *str, char c)
{
    vector<string> result;

    do
    {
        const char *begin = str;

        while(*str != c && *str)
            str++;

        result.push_back(string(begin, str));
    } while (0 != *str++);

    return result;
}