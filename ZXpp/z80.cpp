//	C++ conversion of James O'Hara's ZX Spectrum 48K Emulator.
//	This represents the Z80 CPU

#include "z80.h"
#include "flags.cpp"
#include "memory.h"
#include "IioDevice.h"
#include <cmath>
#include <iostream>
#include <windows.h>

//	Constructor
Z80:: Z80(Memory* mem)
{
	memory = mem;
	memory->CPU = this;
	Reset();
}

//	Sets power-on defaults
void Z80:: Reset()
{
	PC = 0;
	//  AF and SP always set to FFFFh after a reset (other registers undefined).
	A = 0xff;
	SP = 0xffff;
	B = 0; C = 0; D = 0; E = 0; H = 0; L = 0; I = 0; R = 0; IXH = 0; IXL = 0; IYH = 0; IYL = 0;
	F = 0xff;
	F2 = 0;
	isHalted = false;
	ignorePrefix = false;
	IFF1 = false;
	IFF2 = false;
	interruptMode = 0;

	//  Sanity
	opcode = 0;
	prefix = 0;
	prefix2 = 0;
	displacement = 0;
	cycleTStates = 0;
}

//	Starts an interrupt-handling routine
void Z80:: Interrupt(bool nonMaskable)
{

    if (nonMaskable)
    {
		R++;
        memory->memory[--SP] = PC >> 8;
        memory->memory[--SP] = PC & 0xff;
        PC = 0x0066;
        return;
    }
    if (IFF1)
    {
		R++;
        switch (interruptMode)
        {
            case 0:
                //  The device supplies an instruction on the data bus : not used in ZX Spectrum
                break;
            case 1:
                //  The processor restarts at location 0x0038
                memory->memory[--SP] = PC >> 8;
                memory->memory[--SP] = PC & 0xff;
                PC = 0x0038;
                break;
            case 2:
                //  The device supplies the LSByte of the routine pointer, HSByte in register I.
                //  Not (generally used)
                break;

        }
    }
}

//	Main execution loop
void Z80:: Run(bool exitOnNOP, int maxTStates)
{
	cycleTStates = cycleTStates % 69888;
    previousTStates = cycleTStates;
	bool running = true;
	while (running)
	{

		//  Fetch
		opcode = memory->memory[PC++];
		prefix = 0;
		prefix2 = 0;
		displacement = 0;

		//  Check for prefix bytes
		if (opcode == 0xCB || opcode == 0xDD || opcode == 0xED || opcode == 0xFD)
		{
			//  This instruction has a prefix byte
			prefix = opcode;
			opcode = memory->memory[PC++];
			if (prefix == 0xDD || prefix == 0xFD)
			{
				cycleTStates += 4;
				if (opcode == 0xCB)
				{
					//  This instruction has 2 prefix bytes and displacement byte
					prefix2 = 0xCB;
					displacement = memory->memory[PC++];
					if (displacement > 127)
					{
						displacement = -256 + displacement;
					}
					opcode = memory->memory[PC++];
				}
				else if (opcode == 0xDD || opcode == 0xED || opcode == 0xFD)
				{
					//  Ignore current prefix and continue;
					NONI();
					R += 2;
					continue;
				}
				else
				{
					//  HL, H & L are replaced by IX, IXH, IXL and (HL) by (IX+d) for DD opcodes

					//  HL, H & L are replaced by IY, IYH, IYL and (HL) by (IY+d) for FD opcodes

					//  ~EXCEPTIONS~
					//  EX DE, HL is unaffected by these rules

				}
			}
		}

		//  Decode
		int x = opcode >> 6;                //  1st octal (bits 7-6)
		int y = (opcode & 0x3F) >> 3;       //  2nd octal (bits 5-3)
		int z = opcode & 0x07;              //  3rd octal (bits 2-0)
		int p = y >> 1;                     //  y right-shifted 1 (bits 5-4)
		int q = y % 2;                      //  y modulo 2 (bit 3)

		//  Execute
		switch (prefix)
		{
			case 0:
			case 0xDD:
			case 0xFD:
				//  DDCB & FDCB double-prefixed opcodes:
				if (prefix2 == 0xCB)
				{     
					if (x == 0)
					{
						//  Rotate / Shift memory location and copy result to register
						if (y == 0) { RLC(6); }
						if (y == 1) { RRC(6); }
						if (y == 2) { RL(6); }
						if (y == 3) { RR(6); }
						if (y == 4) { SLA(6); }
						if (y == 5) { SRA(6); }
						if (y == 6) { SLL(6); }
						if (y == 7) { SRL(6); }
						//  Copy result to register ?
						if (z != 6)
						{
							ignorePrefix = true;
							SetRegister(z, GetRegister(6));
							ignorePrefix = false;
							cycleTStates -= 11;
						}
						else
						{
							cycleTStates -= 4;
						}
						break;
					}
					//  Test bit at memory loc
					if (x == 1) { cycleTStates -= 8; BIT(y, 6); break; }
					//  Reset bit and copy to register
					if (x == 2)
					{ 
						RES(y, 6);
						//  Copy result to register ?
						if (z != 6)
						{
							ignorePrefix = true;
							SetRegister(z, GetRegister(6));
							ignorePrefix = false;
							cycleTStates -= 11;
						}
						else
						{
							cycleTStates -= 4;
						}
						break;
					}
					//  Set bit and copy to register
					if (x == 3)
					{
						SET(y, 6);
						//  Copy result to register ?
						if (z != 6)
						{
							ignorePrefix = true;
							SetRegister(z, GetRegister(6));
							ignorePrefix = false;
							cycleTStates -= 11;
						}
						else
						{
							cycleTStates -= 4;
						}
						break;
					}
					break;
				}
				//  Unprefixed opcodes (& DD & FD prefixed opcodes w/o CB 2nd prefix)
				if (x == 0)
				{
					//  Relative jumps & misc. operations
					if (z == 0)
					{
						if (y == 0) { NOP(); break; }
						if (y == 1) { EX_AF_AF2(); break; }
						if (y == 2) { DJNZ(); break; }
						if (y > 2 && y < 8) { JR(y - 4); break; }
					}

					//  16-bit immediate load & add
					if (z == 1)
					{  
						if (q == 0) { LD_dd_nn(p); break; }
						if (q == 1) { ADD_HL_ss(p); break; }
					}

					//  Indirect loading
					if (z == 2)
					{
						if (q == 0)
						{
							if (p == 0) { LD_BC_A(); break; }
							if (p == 1) { LD_DE_A(); break; }
							if (p == 2) { LD_nn_HL(); break; }
							if (p == 3) { LD_nn_A(); break; }
						}
						if (q == 1)
						{
							if (p == 0) { LD_A_BC(); break; }
							if (p == 1) { LD_A_DE(); break; }
							if (p == 2) { LD_HL_nn(); break; }
							if (p == 3) { LD_A_nn(); break; }
						}
					}

					//  16-bit INC, DEC
					if (z == 3)
					{
						if (q == 0) { INC_ss(p); break; }
						if (q == 1) { DEC_ss(p); break; }
					}

					//  8-bit INC, DEC
					if (z == 4) { INC_r(y); break; }
					if (z == 5) { DEC_r(y); break; }
                            
					//  8-bit Load Immediate
					if (z == 6) { LD_r_n(y); break; }

					//  Misc. Flag & Accumulator ops
					if (z == 7)
					{
						if (y == 0) { RLCA(); break; }
						if (y == 1) { RRCA(); break; }
						if (y == 2) { RLA(); break; }
						if (y == 3) { RRA(); break; }
						if (y == 4) { DAA(); break; }
						if (y == 5) { CPL(); break; }
						if (y == 6) { SCF(); break; }
						if (y == 7) { CCF(); break; }
					}
				}

				if (x == 1)
				{

					//  Halt
					if (y == 6 && z == 6) { HALT(); break; }

					//  8-bit loading
					if (y < 8 && z < 8) { LD_r_r(y, z); break; }
				}

				if (x == 2)
				{

					//  Operations on Accumulator and register/memory location
					if (y == 0) { ADD_A_r(z); break; }
					if (y == 1) { ADC_A_r(z); break; }
					if (y == 2) { SUB_r(z); break; }
					if (y == 3) { SBC_A_r(z); break; }
					if (y == 4) { AND_r(z); break; }
					if (y == 5) { XOR_r(z); break; }
					if (y == 6) { OR_r(z); break; }
					if (y == 7) { CP_r(z); break; }
				}

				if (x == 3)
				{
					//  Conditional return
					if (z == 0) { RET_cc(y); break; }

					//  POP & misc operations
					if (z == 1)
					{
						if (q == 0) { POP_qq(p); break; }
						if (q == 1)
						{
							if (p == 0) { RET(); break; }
							if (p == 1) { EXX(); break; }
							if (p == 2) { JP_HL(); break; }
							if (p == 3) { LD_SP_HL(); break; }
						}
					}

					//  Conditional jump
					if (z == 2) { JP_cc_nn(y); break; }

					//  Misc ops.
					if (z == 3)
					{
						if (y == 0) { JP_nn(); break; }
						if (y == 1) { std::cout << "CB Incorrectly handled!"; break; }
						if (y == 2) { OUT_n_A(); break; }
						if (y == 3) { IN_A_n(); break; }
						if (y == 4) { EX_SP_HL(); break; }
						if (y == 5) { EX_DE_HL(); break; }
						if (y == 6) { DI(); break; }
						if (y == 7) { EI(); break; }
					}

					//  Conditional call
					if (z == 4) { CALL_cc_nn(y); break; }

					//  Push & call
					if (z == 5)
					{
						if (q == 0) { PUSH_qq(p); break; }
						if (q == 1)
						{
							if (p == 0) { CALL_nn(); break; }
							if (p == 1) { std::cout << "DD Prefix incorrectly handled."; break; }
							if (p == 2) { std::cout << "ED Prefix incorrectly handled."; break; }
							if (p == 3) { std::cout << "FD Prefix incorrectly handled."; break; }
						}
					}

					//  Operations on Accumulator and immediate operand
					if (z == 6)
					{ 
						if (y == 0) { ADD_A_n(); break; }
						if (y == 1) { ADC_A_n(); break; }
						if (y == 2) { SUB_n(); break; }
						if (y == 3) { SBC_A_n(); break; }
						if (y == 4) { AND_n(); break; }
						if (y == 5) { XOR_n(); break; }
						if (y == 6) { OR_n(); break; }
						if (y == 7) { CP_n(); break; }
					}

					//  Restart
					if (z == 7) { RST(y * 8); break; }

				}
				std::cout << "Unknown opcode: " << opcode;
				break;
			case 0xCB:
				//  CB prefixed opcodes
				if (x == 0)
				{
					//  Rotate / Shift register/memory
					if (y == 0) { RLC(z); break; }
					if (y == 1) { RRC(z); break; }
					if (y == 2) { RL(z); break; }
					if (y == 3) { RR(z); break; }
					if (y == 4) { SLA(z); break; }
					if (y == 5) { SRA(z); break; }
					if (y == 6) { SLL(z); break; }
					if (y == 7) { SRL(z); break; }
				}

				//  Test / reset / set bit
				if (x == 1) { BIT(y, z); break; }
				if (x == 2) { RES(y, z); break; }
				if (x == 3) { SET(y, z); break; }
				std::cout << "Unknown opcode: " << opcode;
				break;
			case 0xED:
				//  ED prefixed opcodes
				if (x == 0 || x == 3) { NONI(); NOP();  break; }

				if (x == 1)
				{
					//  Port I/O
					if (z == 0) { IN_r_C(y); break; }
					if (z == 1) { OUT_C_r(y); break; }

					//  16-bit add/subtract with Carry
					if (z == 2)
					{
						if (q == 0) { SBC_HL(p); break; }
						if (q == 1) { ADC_HL(p); break; }
					}

					//  Load register pair from/to immediate address
					if (z == 3)
					{
						if (q == 0) { LD_nn_dd(p); break; }
						if (q == 1) { LD_dd_nn2(p); break; }
					}

					//  Negation
					if (z == 4) { NEG(); break; }

					//  Return from interrupt
					if (z == 5)
					{
						if (y == 1) { RETI(); break; }
						RETN(); break;
					}

					//  Set interrupt mode
					if (z == 6) { IM(y); break; }

					//  Misc. operations
					if (z == 7)
					{
						if (y == 0) { LD_I_A(); break; }
						if (y == 1) { LD_R_A(); break; }
						if (y == 2) { LD_A_I(); break; }
						if (y == 3) { LD_A_R(); break; }
						if (y == 4) { RRD(); break; }
						if (y == 5) { RLD(); break; }
						if (y == 6 || y == 7) { NOP(); break; }
					}
				}
				if (x == 2)
				{
					//  Block instructions
					if (y == 4)
					{
						if (z == 0) { LDI(); break; }
						if (z == 1) { CPI(); break; }
						if (z == 2) { INI(); break; }
						if (z == 3) { OUTI(); break; }
					}
					if (y == 5)
					{
						if (z == 0) { LDD(); break; }
						if (z == 1) { CPD(); break; }
						if (z == 2) { IND(); break; }
						if (z == 3) { OUTD(); break; }
					}
					if (y == 6)
					{
						if (z == 0) { LDIR(); break; }
						if (z == 1) { CPIR(); break; }
						if (z == 2) { INIR(); break; }
						if (z == 3) { OTIR(); break; }
					}
					if (y == 7)
					{
						if (z == 0) { LDDR(); break; }
						if (z == 1) { CPDR(); break; }
						if (z == 2) { INDR(); break; }
						if (z == 3) { OTDR(); break; }
					}
					NONI(); NOP();
				}
				break;
			default:
				//  Unknown opcode
				std::cout << "Unknown prefix: " << prefix;
				break;
		}

		//  Increment r :- twice for prefixed instructions
		R++;
		if (prefix != 0)
			R++;
		if (R >= 256)
			R = 0;

		//  If we have defined a max amount of tStates to run for then check if we should return
		if (maxTStates > 0 && cycleTStates - previousTStates >= maxTStates)
		{
			running = false;
		}
	}
}

//	Adds a pointer to the ULA
void Z80:: AddDevice(IioDevice* ula)
{
	io = ula;
}

/*
 *	Helper methods
 */

//	Reads in the optional displacement byte required by some opcodes
void Z80:: ReadDisplacementByte()
{
	displacement = memory->memory[PC++];
    if (displacement > 127)
    {
        displacement = -256 + displacement;
    }
}

//	Gets the flag value as an int (power of 2)
int Z80:: FlagAsInt(Flag flag)
{
	switch (flag)
	{
		case CARRY:
		return 1;
		case SUBTRACT:
		return 2;
		case PARITYOVERFLOW:
		return 4;
		case F3:
		return 8;
		case HALFCARRY:
		return 16;
		case F5:
		return 32;
		case ZERO:
		return 64;
		case SIGN:
		return 128;
	}
}

//	Sets the specified flag
void Z80:: Set(Flag flag)
{
	F = F | FlagAsInt(flag);
}

//	Resets the specified flag
void Z80:: Reset(Flag flag)
{
	F = F & ~FlagAsInt(flag);
}

//	Sets the Carry Flag after an 8-bit addition if there was a carry, resets it otherwise
void Z80:: ModifyCarryFlag8(int initial, int addition)
{
	if (initial + addition > 255 || initial + addition < 0)
		Set(CARRY);
	else
		Reset(CARRY);
}

// Sets the Carry Flag after a 16-but addition if there was a carry, resets it otherwise
void Z80:: ModifyCarryFlag16(int initial, int addition)
{
	if (initial + addition > 65535 || initial + addition < 0)
        Set(CARRY);
    else
        Reset(CARRY);
}

// Sets the Half-carry/borrow flag after an addition/subtraction if bit 3 carried or bit 4 is borrowed, resets it otherwise
void Z80:: ModifyHalfCarryFlag8(int initial, int addition)
{
	if (addition >= 0)
    {
        int hc = ((initial & 0xf) + (addition & 0xf)) & 0x10;
        //  Half-carry
        if (hc == 0x10)
			Set(HALFCARRY);
        else
			Reset(HALFCARRY);
    }
    else
    {
        int subtract = -addition;
        if ((((initial & 0x0f) - (subtract & 0x0f)) & 0x10) != 0)
			Set(HALFCARRY);
        else
			Reset(HALFCARRY);
    }
}

// Sets the Half-carry/borrow flag after an addition/subtraction if bit 3 carried or bit 4 is borrowed, resets it otherwise
void Z80:: ModifyHalfCarryFlag16(int initial, int addition)
{
	int carry = 0;
    if ((initial & 0xff) + (addition & 0xff) > 0xff)
    {
        carry = 1;
    }
    initial = initial >> 8;
    addition = (addition >> 8) + carry;

    ModifyHalfCarryFlag8(initial, addition);
}

//	Sets the Sign flag if result is negative, resets it otherwise
void Z80:: ModifySignFlag8(int result)
{
	if (result > 127 && result < 256)
		Set(SIGN);
    else
		Reset(SIGN);
}

// Sets the Sign flag if result is negative, resets it otherwise
void Z80:: ModifySignFlag16(int result)
{
	result = result >> 8;
    ModifySignFlag8(result);
}

// Sets the Zero flag if result is zero, resets it otherwise
void Z80:: ModifyZeroFlag(int result)
{
	if (result == 0)
		Set(ZERO);
    else
		Reset(ZERO);
}

//	Sets the Overflow flag if there was an overflow after 8-bit addition or subtraction, resets it otherwise
void Z80:: ModifyOverflowFlag8(int initial, int addition, int result)
{
    if (addition >= 0)
    {
        //  For addition, operands with like signs may cause overflow
        if (((initial & 0x80) ^ (addition & 0x80)) == 0)
        {
            //  If the result has a different sign then overflow is caused
            if (((result & 0x80) ^ (initial & 0x80)) == 0x80)
            {
				Set(PARITYOVERFLOW);
                return;
            }
        }
        Reset(PARITYOVERFLOW);
    }
    else
    {
        int subtract = -addition;
        if (((initial ^ subtract) & (initial ^ result) & 0x80) != 0)
        {
            Set(PARITYOVERFLOW);
        }
        else
        {
            Reset(PARITYOVERFLOW);
        } 
    }
}

//	Sets the Overflow flag if there was an overflow after 16-bit addition or subtraction, resets it otherwise
void Z80:: ModifyOverflowFlag16(int initial, int addition, int result)
{
	int carry = 0;
    if ((initial & 0xff) + (addition & 0xff) > 0xff)
    {
        carry = 1;
    }
    initial = initial >> 8;
    addition = (addition >> 8) + carry;
    result = result >> 8;

    ModifyOverflowFlag8(initial, addition, result);
}

//	Set the Parity flag if even number of bits in result, resets it otherwise
void Z80:: ModifyParityFlagLogical(int result)
{
	int count = 0;
	if ((result & 1) == 1) count ++;
	if ((result & 2) == 2) count ++;
	if ((result & 4) == 4) count ++;
	if ((result & 8) == 8) count ++;
	if ((result & 16) == 16) count ++;
	if ((result & 32) == 32) count ++;
	if ((result & 64) == 64) count ++;
	if ((result & 128) == 128) count ++;

	if ((result & 256) == 256) count ++;
	if ((result & 512) == 512) count ++;
	if ((result & 1024) == 1024) count ++;
	if ((result & 2048) == 2048) count ++;
	if ((result & 4096) == 4096) count ++;
	if ((result & 8192) == 8192) count ++;
	if ((result & 16384) == 16384) count ++;
	if ((result & 32768) == 32768) count ++;

    if ((count & 1) == 1)
		Reset(PARITYOVERFLOW);
    else
		Set(PARITYOVERFLOW);
}

//	Sets the undocumented bits 3 & 5 of the flags register after an 8-bit operation
void Z80:: ModifyUndocumentedFlags8(int result)
{
	
	if ((result & 8) == 8)
        Set(F3);
    else
        Reset(F3); 
    if ((result & 32) == 32) 
	{
        Set(F5);
	}
    else
	{
        Reset(F5);
	}
}

//	Sets the undocumented bits 3 & 5 of the flags register after a 16-bit operation
void Z80:: ModifyUndocumentedFlags16(int result)
{
	result = result >> 8;
    ModifyUndocumentedFlags8(result);
}

//	Undocumented LDI / LDIR / LDD / LDIR behavior
void Z80:: ModifyUndocumentedFlagsLoadGroup()
{
	if (((memory->memory[Get16BitRegisters(2, false)] + A) & 2) == 2)
    {
        Set(F5);
    }
    else
    {
        Reset(F5);
    }
    if (((memory->memory[Get16BitRegisters(2, false)] + A) & 8) == 8)
    {
        Set(F3);
    }
    else
    {
        Reset(F3);
    }
}

//	Undocumented CPI / CPIR / CPD / CPDR behavior
void Z80:: ModifyUndocumentedFlagsCompareGroup(int n)
{
	if ((n & 2) == 2)
    {
        Set(F5);
    }
    else
    {
        Reset(F5);
    }
    if ((n & 8) == 8)
    {
        Set(F3);
    }
    else
    {
        Reset(F3);
    }
}

//	Gets the value of a register
int Z80:: GetRegister(int reg)
{
	switch (reg)
    {
        case 0: return B;
        case 1: return C;
        case 2: return D;
        case 3: return E;
        case 4:
            if (ignorePrefix)
                return H;
            switch (prefix) {
                case 0xDD:
                    return IXH;
                case 0xFD:
                    return IYH;
                default:
                    return H;
            } 
        case 5:
            if (ignorePrefix)
                return L;
            switch (prefix)
            {
                case 0xDD:
                    return IXL;
                case 0xFD:
                    return IYL;
                default:
                    return L;
            } 
        case 6:
            cycleTStates += 3;  
            switch (prefix) {
                case 0xDD:
                case 0xFD:
                    cycleTStates += 4; return (memory->memory[Get16BitRegisters(2, false) + displacement]);
                default:
                    return memory->memory[Get16BitRegisters(2, false)];
            } 
        case 7: return A;
		default: std::cout <<"Tried to get an unknown register" << std::endl; return 0;
    }
}

// Sets a register to the given 8-bit value
void Z80:: SetRegister(int reg, int value)
{
	value = value & 0xff;
    switch (reg)
    {
        case 0: B = value; return;
        case 1: C = value; return;
        case 2: D = value; return;
        case 3: E = value; return;
        case 4:
            if (ignorePrefix)
            {
                H = value; return;
            }
            switch (prefix) {
                case 0xDD:
                    IXH = value; return;
                case 0xFD:
                    IYH = value; return;
                default:
                    H = value; return;
            }
        case 5:
            if (ignorePrefix)
            {
                L = value; return;
            }
            switch (prefix)
            {
                case 0xDD:
                    IXL = value; return;
                case 0xFD:
                    IYL = value; return;
                default:
                    L = value; return;
            }
        case 6:
            cycleTStates += 3; 
            switch (prefix)
            {
                case 0xDD:
                case 0xFD:
                    cycleTStates +=4; memory->memory[Get16BitRegisters(2, false) + displacement] = value; return;
                default:
                    memory->memory[Get16BitRegisters(2, false)] = value; return;
            }
        case 7: A = value; return;
		default: std::cout << "Tried to set an unknown register" << std::endl; return;
    }
}

// Sets a register pair to the given 16-bit value
void Z80:: Set16BitRegisters(int registerPair, int value)
{
	Set16BitRegisters(registerPair, value & 0xFF, (value & 0xFFFF) >> 8);
}

// Sets a register pair to the given low and high bytes
void Z80:: Set16BitRegisters(int registerPair, int lowByte, int highByte)
{
	switch (registerPair)
    {
        case 0: B = highByte; C = lowByte; return;
        case 1: D = highByte; E = lowByte; return;
        case 2:
            switch (prefix)
            {
                case 0xDD:
                    IXH = highByte; IXL = lowByte; return;
                case 0xFD:
                    IYH = highByte; IYL = lowByte; return;
                default:
                    H = highByte; L = lowByte; return;
            }
        case 3: SP = highByte; SP = (SP << 8); SP += lowByte; return;
		default: std::cout << "Tried to set an unknown Register pair." << std::endl; return;
    }
}

// Gets the 16-bit value stored in a register pair
int Z80:: Get16BitRegisters(int registerPair, bool ignorePrefix)
{
	int result;
    switch (registerPair)
    {
        case 0: result = B; result = (result << 8); result += C; return result;
        case 1: result = D; result = (result << 8); result += E; return result;
        case 2:
            if (ignorePrefix)
            {
                result = H; result = (result << 8); result += L; return result;
            }
            switch (prefix)
            {
                case 0xDD:
                        result = IXH; result = (result << 8); result += IXL; return result;
                case 0xFD:
                    result = IYH; result = (result << 8); result += IYL; return result;
                default:
                    result = H; result = (result << 8); result += L; return result;
            }
        case 3: return SP;
		default:std::cout << "Tried to get an unknown register pair" << std::endl; return 0;
    }
}

// Checks if the given condition is true
bool Z80:: CheckCondition(int condition)
{

	switch (condition)
    {
        case -1: return true;
		case 0: if ((F & ZERO) == ZERO) return false; return true;
        case 1: if ((F & ZERO) == ZERO) return true; return false;
        case 2: if ((F & CARRY) == CARRY) return false; return true;
        case 3: if ((F & CARRY) == CARRY) return true; return false;
		case 4: if ((F & PARITYOVERFLOW) == PARITYOVERFLOW) return false; return true;
        case 5: if ((F & PARITYOVERFLOW) == PARITYOVERFLOW) return true; return false;
        case 6: if ((F & SIGN) == SIGN) return false; return true;
        case 7: if ((F & SIGN) == SIGN) return true; return false;
		default: std::cout << "Unknown condition code." << std::endl; HALT(); return false;
    }
}

/*
 *	Opcodes
 */

void Z80:: OTDR()
{
    int written = memory->memory[Get16BitRegisters(2, false)];
    io->Write(Get16BitRegisters(0, false), written);
    B = (B - 1) & 0xff;
    Set16BitRegisters(2, Get16BitRegisters(2, false) - 1);
    if (B != 0)
    {
        PC = (PC - 2) & 0xffff;
        cycleTStates += 21;
    }
    else
    {
        cycleTStates += 16;
    }
    ModifySignFlag8(B);
    ModifyZeroFlag(B);

    if ((written & 128) == 128)
    {
		Set(SUBTRACT);
    }
    else
    {
		Reset(SUBTRACT);
    }
    ModifyUndocumentedFlags8(B);

    if (written + L > 255)
    {
        Set(CARRY);
		Set(HALFCARRY);
    }
    else
    {
        Reset(CARRY);
        Reset(HALFCARRY);
    }

    int result = ((written + L) & 7) ^ B;
    ModifyParityFlagLogical(result);       
}
void Z80:: INDR()
{
	int read = io->Read(Get16BitRegisters(0, false));
    memory->memory[Get16BitRegisters(2, false)] = read;
    Set16BitRegisters(2, Get16BitRegisters(2, false) - 1);
    B = (B - 1) & 0xff;
    if (B != 0)
    {
        PC = (PC - 2) & 0xffff;
        cycleTStates += 21;
    }
    else
    {
        cycleTStates += 16;
    }
    ModifySignFlag8(B);
    ModifyZeroFlag(B);

    if ((read & 128) == 128)
    {
		Set(SUBTRACT);
    }
    else
    {
		Reset(SUBTRACT);
    }
    ModifyUndocumentedFlags8(B);

    if (read + ((C - 1) & 255) > 255)
    {
		Set(CARRY);
		Set(HALFCARRY);
    }
    else
    {
		Reset(CARRY);
		Reset(HALFCARRY);
    }

    int result = (((read + ((C - 1) & 255)) & 7) ^ B);
    ModifyParityFlagLogical(result);
}
void Z80:: CPDR()
{
	int compare = memory->memory[Get16BitRegisters(2, false)];
    if (compare == A)
		Set(ZERO);
    else
		Reset(ZERO);

    Set16BitRegisters(2, Get16BitRegisters(2, false) - 1);
    Set16BitRegisters(0, Get16BitRegisters(0, false) - 1);

    if (compare != A && Get16BitRegisters(0, false) != 0)
    {
        PC = (PC - 2) & 0xffff;
        cycleTStates += 21;
    }
    else
    {
        cycleTStates += 16;
    }

    ModifySignFlag8((A - compare) & 0xff);
    ModifyHalfCarryFlag8(A, -compare);
    int n = (A - compare) & 0xff;
    if ((F & 16) == 16)	//	halfCarry
    {
        n = (n-1) & 0xff;
    }
    ModifyUndocumentedFlagsCompareGroup(n);
    if (Get16BitRegisters(0, false) != 0)
		Set(PARITYOVERFLOW);
    else
		Reset(PARITYOVERFLOW);

	Set(SUBTRACT);
}
void Z80:: LDDR()
{
	memory->memory[Get16BitRegisters(1, false)] = memory->memory[Get16BitRegisters(2, false)];
    if (Get16BitRegisters(0, false) - 1 == 0)
    {
        cycleTStates += 16;
    }
    else
    {
        cycleTStates += 21;
        PC = (PC - 2) & 0xffff;
    }
	Reset(HALFCARRY);

	Reset(SUBTRACT);

    ModifyUndocumentedFlagsLoadGroup();

    Set16BitRegisters(0, Get16BitRegisters(0, false) - 1);
    Set16BitRegisters(1, Get16BitRegisters(1, false) - 1);
    Set16BitRegisters(2, Get16BitRegisters(2, false) - 1);

	Reset(PARITYOVERFLOW);
}

void Z80:: OTIR()
{
	int written = memory->memory[Get16BitRegisters(2, false)];
    io->Write(Get16BitRegisters(0, false), written);
    Set16BitRegisters(2, Get16BitRegisters(2, false) + 1);
    B = (B - 1) & 0xff;
    if (B != 0)
    {
        cycleTStates += 21;
        PC = (PC - 2) & 0xffff;
    }
    else
    {
        cycleTStates += 16;
    }
    ModifySignFlag8(B);
    ModifyZeroFlag(B);

    if ((written & 128) == 128)
    {
		Set(SUBTRACT);
    }
    else
    {
		Reset(SUBTRACT);
    }
    ModifyUndocumentedFlags8(B);

    if (written + L > 255)
    {
		Set(CARRY);
		Set(HALFCARRY);
    }
    else
    {
		Reset(CARRY);
		Reset(HALFCARRY);
    }

    int result = ((written + L) & 7) ^ B;
    ModifyParityFlagLogical(result);
}
void Z80:: INIR()
{
	int read = io->Read(Get16BitRegisters(0, false));
    memory->memory[Get16BitRegisters(2, false)] = read;
    Set16BitRegisters(2, Get16BitRegisters(2, false) + 1);
    B = (B - 1) & 0xff;
    if (B != 0)
    {
        PC = (PC - 2) & 0xffff;
        cycleTStates += 21;
    }
    else
    {
        cycleTStates += 16;
    }
    ModifySignFlag8(B);
    ModifyZeroFlag(B);

    if ((read & 128) == 128)
    {
		Set(SUBTRACT);
    }
    else
    {
        Reset(SUBTRACT);
    }
    ModifyUndocumentedFlags8(B);

    if (read + ((C + 1) & 255) > 255)
    {
		Set(CARRY);
		Set(HALFCARRY);
    }
    else
    {
		Reset(CARRY);
		Reset(HALFCARRY);
    }

    int result = (((read + ((C + 1) & 255)) & 7) ^ B);
    ModifyParityFlagLogical(result);
}
void Z80:: CPIR()
{
	int compare = memory->memory[Get16BitRegisters(2, false)];
    if (compare == A)
		Set(ZERO);
    else
		Reset(ZERO);

    Set16BitRegisters(2, (Get16BitRegisters(2, false) + 1) & 0xffff);
    Set16BitRegisters(0, (Get16BitRegisters(0, false) - 1) & 0xffff);

    if (compare != A && Get16BitRegisters(0, false)  != 0)
    {
        PC = (PC - 2) & 0xffff;
        cycleTStates += 21;
    }
    else
    {
        cycleTStates += 16;
    }

    ModifySignFlag8((A - compare) & 0xff);
    ModifyHalfCarryFlag8(A, -compare);
    int n = (A - compare) & 0xff;
    if ((F & 16) == 16)	//	halfCarry
    {
        n = (n-1) & 0xff;
    }
    ModifyUndocumentedFlagsCompareGroup(n);
    if (Get16BitRegisters(0, false) != 0)
		Set(PARITYOVERFLOW);
    else
		Reset(PARITYOVERFLOW);

	Set(SUBTRACT);
}
void Z80:: LDIR()
{
	memory->memory[Get16BitRegisters(1, false)] = memory->memory[Get16BitRegisters(2, false)];         
    if (Get16BitRegisters(0, false) -1 == 0)
    {
        cycleTStates += 16;
    }
    else
    {
        cycleTStates += 21;
        PC = (PC - 2) & 0xffff;
    }     
       
	Reset(HALFCARRY);
                       
	Reset(SUBTRACT);

    ModifyUndocumentedFlagsLoadGroup();

    Set16BitRegisters(1, Get16BitRegisters(1, false) + 1);
    Set16BitRegisters(2, Get16BitRegisters(2, false) + 1);
    Set16BitRegisters(0, Get16BitRegisters(0, false) - 1);

	Reset(PARITYOVERFLOW);
}

void Z80:: OUTD()
{
	cycleTStates += 16;
    int written = memory->memory[Get16BitRegisters(2, false)];
    io->Write(Get16BitRegisters(0, false), written);
    B = (B - 1) & 0xff;
    Set16BitRegisters(2, Get16BitRegisters(2, false) - 1);

    ModifySignFlag8(B);
    ModifyZeroFlag(B);

    if ((written & 128) == 128)
    {
		Set(SUBTRACT);
    }
    else
    {
		Reset(SUBTRACT);
    }
    ModifyUndocumentedFlags8(B);

    if (written + L > 255)
    {
		Set(CARRY);
		Set(HALFCARRY);
    }
    else
    {
		Reset(CARRY);
		Reset(HALFCARRY);
    }

    int result = ((written + L) & 7) ^ B;
    ModifyParityFlagLogical(result);
}
void Z80:: IND()
{
	cycleTStates += 16;
    B = (B - 1) & 0xff;
            
    int read = io->Read(Get16BitRegisters(0, false));
    memory->memory[Get16BitRegisters(2, false)] = read;
    Set16BitRegisters(2, (Get16BitRegisters(2, false) - 1) & 0xffff);

    ModifySignFlag8(B);
    ModifyZeroFlag(B);

    if ((read & 128) == 128)
    {
		Set(SUBTRACT);
    }
    else
    {
		Reset(SUBTRACT);
    }
    ModifyUndocumentedFlags8(B);

    if (read + ((C - 1) & 255) > 255)
    {
		Set(CARRY);
		Set(HALFCARRY);
    }
    else
    {
		Reset(CARRY);
		Reset(HALFCARRY);
    }

    int result = (((read + ((C - 1) & 255)) & 7) ^ B);
    ModifyParityFlagLogical(result);
}
void Z80:: CPD()
{
	cycleTStates += 16;

    int compare =memory->memory[Get16BitRegisters(2, false)];
    if (compare == A)
        Set(ZERO);
    else
		Reset(ZERO);

    Set16BitRegisters(2, Get16BitRegisters(2, false) - 1);
    Set16BitRegisters(0, Get16BitRegisters(0, false) - 1);

    ModifySignFlag8((A - compare) & 0xff);
    ModifyHalfCarryFlag8(A, -compare);
            
    int n = (A - compare) & 0xff;
    if ((F & 16) == 16)	//	halfCarry
    {
        n = (n-1) & 0xff;
    }

    ModifyUndocumentedFlagsCompareGroup(n);
    if (Get16BitRegisters(0, false) != 0)
		Set(PARITYOVERFLOW);
    else
		Reset(PARITYOVERFLOW);

	Set(SUBTRACT);
}
void Z80:: LDD()
{
	cycleTStates += 16;

    memory->memory[Get16BitRegisters(1, false)] = memory->memory[Get16BitRegisters(2, false)];

	Reset(HALFCARRY);

	Reset(SUBTRACT);

    ModifyUndocumentedFlagsLoadGroup();

    Set16BitRegisters(0, Get16BitRegisters(0, false) - 1);
    Set16BitRegisters(1, Get16BitRegisters(1, false) - 1);
    Set16BitRegisters(2, Get16BitRegisters(2, false) - 1);

    if (Get16BitRegisters(0, false) != 0)
		Set(PARITYOVERFLOW);
    else
		Reset(PARITYOVERFLOW);
}

void Z80:: OUTI()
{
	cycleTStates += 16;

    int written = memory->memory[Get16BitRegisters(2, false)];

    io->Write(Get16BitRegisters(0, false), written);
    B = (B - 1) & 0xff;
    Set16BitRegisters(2, Get16BitRegisters(2, false) + 1);

    ModifySignFlag8(B);
    ModifyZeroFlag(B);

    if ((written & 128) == 128)
    {
		Set(SUBTRACT);
    }
    else
    {
		Reset(SUBTRACT);
    }
    ModifyUndocumentedFlags8(B);

    if (written + L > 255)
    {
		Set(CARRY);
		Set(HALFCARRY);
    }
    else
    {
		Reset(CARRY);
		Reset(HALFCARRY);
    }

    int result = ((written + L)  & 7) ^ B;
    ModifyParityFlagLogical(result);
}
void Z80:: INI()
{
	cycleTStates += 16;

    int read = io->Read(Get16BitRegisters(0, false));
    memory->memory[Get16BitRegisters(2, false)] = read;
    B = (B - 1) & 0xff;
    Set16BitRegisters(2, Get16BitRegisters(2, false) + 1);

    ModifySignFlag8(B);
    ModifyZeroFlag(B);

    if ((read & 128) == 128)
    {
		Set(SUBTRACT);
    }
    else
    {
		Reset(SUBTRACT);
    }
    ModifyUndocumentedFlags8(B);

    if (read + ((C + 1) & 255) > 255)
    {
		Set(CARRY);
		Set(HALFCARRY);
    }
    else
    {
		Reset(CARRY);
		Reset(HALFCARRY);
    }

    int result = (((read + ((C + 1) & 255)) & 7) ^ B);
    ModifyParityFlagLogical(result);
}
void Z80:: CPI()
{
	cycleTStates += 16;
    int compare = memory->memory[Get16BitRegisters(2, false)];
    if (compare == A)
        Set(ZERO);
    else
        Reset(ZERO);

    Set16BitRegisters(2, Get16BitRegisters(2, false) + 1);
    Set16BitRegisters(0, Get16BitRegisters(0, false) - 1);

    ModifySignFlag8((A - compare) & 0xff);
    ModifyHalfCarryFlag8(A, -compare);
    int n = A - compare;
    if ((F & 16) == 16)	// halfCarry
    {
        n -= 1;
    }
    ModifyUndocumentedFlagsCompareGroup(n);
    if (Get16BitRegisters(0, false) != 0)
		Set(PARITYOVERFLOW);
    else
		Reset(PARITYOVERFLOW);

	Set(SUBTRACT);
}
void Z80:: LDI()
{
	cycleTStates += 16;

    memory->memory[Get16BitRegisters(1, false)] = memory->memory[Get16BitRegisters(2, false)];

	Reset(HALFCARRY);

	Reset(SUBTRACT);

    ModifyUndocumentedFlagsLoadGroup();

    Set16BitRegisters(1, Get16BitRegisters(1, false) + 1);
    Set16BitRegisters(2, Get16BitRegisters(2, false) + 1);
    Set16BitRegisters(0, Get16BitRegisters(0, false) - 1);

    if (Get16BitRegisters(0, false) != 0)
		Set(PARITYOVERFLOW);
    else
		Reset(PARITYOVERFLOW);
}

void Z80:: RLD()
{
	cycleTStates += 18;

    int loc = Get16BitRegisters(2, false);
    int mLow = memory->memory[loc] & 0xf;
    int mHi = (memory->memory[loc] >> 4) & 0xf;
    int aLow = A & 0xf;

    A = (A & 0xf0) + mHi;
    memory->memory[loc] = aLow + (mLow << 4);

    ModifySignFlag8(A);
    ModifyZeroFlag(A);
	Reset(HALFCARRY);
    ModifyParityFlagLogical(A);
	Reset(SUBTRACT);

    ModifyUndocumentedFlags8(A);
}
void Z80:: RRD()
{
	cycleTStates += 18;

    int loc = Get16BitRegisters(2, false);
    int mLow = memory->memory[loc] & 0xf;
    int mHi = (memory->memory[loc] >> 4) & 0xf;
    int aLow = A & 0xf;

    A = (A & 0xf0) + mLow;
    memory->memory[loc] = mHi + (aLow << 4);

    ModifySignFlag8(A);
    ModifyZeroFlag(A);
	Reset(HALFCARRY);
    ModifyParityFlagLogical(A);
	Reset(SUBTRACT);

    ModifyUndocumentedFlags8(A);
}

void Z80:: LD_A_R()
{
	cycleTStates += 9;

    A = (R + 2) & 0xff;
    ModifySignFlag8(R);
    ModifyZeroFlag(R);
	Reset(HALFCARRY);
    if (IFF2 == true)
		Set(PARITYOVERFLOW);
    else
		Reset(PARITYOVERFLOW);
	Reset(SUBTRACT);

    ModifyUndocumentedFlags8(A);
}
void Z80:: LD_A_I()
{
	cycleTStates += 9;

    A = I;

    ModifySignFlag8(I);
    ModifyZeroFlag(I);
	Reset(HALFCARRY);
    if (IFF2 == true)
		Set(PARITYOVERFLOW);
    else
		Reset(PARITYOVERFLOW);
	Reset(SUBTRACT);
	ModifyUndocumentedFlags8(A);
}
void Z80:: LD_R_A()
{
	cycleTStates += 9;
    R = (A - 2) & 0xff;
}
void Z80:: LD_I_A()
{
	cycleTStates += 9;
    I = A;
}

void Z80:: IM(int y)
{
	cycleTStates += 8;

    if (y == 2 || y == 6)
        interruptMode = 1;
    else if (y == 3 || y == 7)
        interruptMode = 2;
    else
        interruptMode = 0;
}
void Z80:: RETI()
{
	cycleTStates += 14;

    PC = memory->memory[SP];
    SP = (SP + 1) & 0xffff;
    PC += (memory->memory[SP] << 8);
    SP = (SP + 1) & 0xffff;
}
void Z80:: RETN()
{
	cycleTStates += 14;

    PC = memory->memory[SP];
    SP = (SP + 1) & 0xffff;
    PC += (memory->memory[SP] << 8);
    SP = (SP + 1) & 0xffff;
}

void Z80:: NEG()
{
	cycleTStates += 8;

    if (A == 0x80)
		Set(PARITYOVERFLOW);
    else
        Reset(PARITYOVERFLOW);

    if (A == 0)
		Reset(CARRY);
    else
		Set(CARRY);

    int addition = -A;
    A = (unsigned char)(0 + addition);
    ModifySignFlag8(A);
    ModifyZeroFlag(A);
    ModifyHalfCarryFlag8(0, addition);
	Set(SUBTRACT);

    ModifyUndocumentedFlags8(A);
}

void Z80:: LD_dd_nn2(int dd)
{
	cycleTStates += 20;

    int address = memory->memory[PC];
	PC = (PC + 1) & 0xffff;
	address += (memory->memory[PC] << 8);
	PC = (PC + 1) & 0xffff;

	int val = memory->memory[address];
	val += (memory->memory[(address + 1) & 0xffff] << 8);

    Set16BitRegisters(dd, val);
}
void Z80:: LD_nn_dd(int dd)
{
	cycleTStates += 20;

    int address = memory->memory[PC];
	PC = (PC + 1) & 0xffff;
	address += (memory->memory[PC] << 8);
	PC = (PC + 1) & 0xffff;
    memory->memory[address] = Get16BitRegisters(dd, false) & 0xff;
    memory->memory[(address + 1) & 0xffff] = Get16BitRegisters(dd, false) >> 8;
}

void Z80:: ADC_HL(int ss)
{
	cycleTStates += 15;

    int initial = Get16BitRegisters(2, false);
    int addition = Get16BitRegisters(ss, false);

    if ((F & CARRY) == CARRY) {
        addition++;
		addition = addition & 0xffff;
	}

    int result = (initial + addition) & 0xffff;
    Set16BitRegisters(2, result);

    //  Carry / Half-carry flags set dependent on high-byte

    //  16-bit Carry
    ModifyCarryFlag16(initial, addition);
    ModifyHalfCarryFlag16(initial, addition);

    ModifySignFlag16(result);
    ModifyZeroFlag(result);

    ModifyOverflowFlag16(initial, addition, result);
	Reset(SUBTRACT);

    ModifyUndocumentedFlags16(result);
}
void Z80:: SBC_HL(int ss)
{
	cycleTStates += 15;

    int initial = Get16BitRegisters(2, false);
    int addition = Get16BitRegisters(ss, false);
    if ((F & CARRY) == CARRY) {
        addition++;
		addition = addition & 0xffff;
	}
	addition = -addition;

    int result = (initial + addition) & 0xffff;
    Set16BitRegisters(2, result);

    //  Carry / Half-carry flags set dependent on high-byte

    //  16-bit Carry
    ModifyCarryFlag16(initial, addition);
    ModifyHalfCarryFlag16(initial, addition);

    ModifySignFlag16(result);
    ModifyZeroFlag(result);

    ModifyOverflowFlag16(initial, addition, result);
	Set(SUBTRACT);

    ModifyUndocumentedFlags16(result);
}

void Z80:: OUT_C_r(int r)
{
	cycleTStates += 12;

    if (r == 6)
        io->Write(Get16BitRegisters(0, false), 0);
    else
        io->Write(Get16BitRegisters(0, false), GetRegister(r));
}
void Z80:: IN_r_C(int r)
{
	cycleTStates += 12;

    int input = io->Read(Get16BitRegisters(0, false));
    if (r != 6  && opcode != 0xed70)
        SetRegister(r, input);

    ModifySignFlag8(input);
    ModifyZeroFlag(input);
	Reset(HALFCARRY);
    ModifyParityFlagLogical(input);
	Reset(SUBTRACT);

    ModifyUndocumentedFlags8(input);
}

void Z80:: NONI()
{
	NOP();
}

void Z80:: SET(int b, int r)
{
	if (r == 6)
        cycleTStates += 9;
    else
        cycleTStates += 8;

    SetRegister(r, (GetRegister(r) | (1 << b)));
}
void Z80:: RES(int b, int r)
{
	if (r == 6)
        cycleTStates += 9;
    else
        cycleTStates += 8;

    SetRegister(r, (GetRegister(r) & ~(1 << b)));
}
void Z80:: BIT(int b, int r)
{
	if (r == 6)
        cycleTStates += 9;
    else
        cycleTStates += 8;

    r = GetRegister(r);

    if ((r & (1 << b)) == (1 << b))
    {
		Reset(ZERO);
		Reset(PARITYOVERFLOW);
    }
    else
    {
		Set(ZERO);
        Set(PARITYOVERFLOW);
    }
	Set(HALFCARRY);
    if (b == 7 && (r & 128) == 128)
    {
		Set(SIGN);
    }
    else
    {
		Reset(SIGN);
    }
	Reset(SUBTRACT);

    if (prefix2 == 0)
        ModifyUndocumentedFlags8(r);
    else
    {
        cycleTStates += 8;
        ModifyUndocumentedFlags8((Get16BitRegisters(2, false) + displacement) >> 8);
    }
}

void Z80:: SRL(int m)
{
	if (m == 6)
        cycleTStates += 9;
    else
        cycleTStates += 8;

    int val = GetRegister(m);
    if ((val & 1) == 1)
    {
		Set(CARRY);
    }
    else
    {
		Reset(CARRY);
    }

    val = (val >> 1) & 0xff;

    SetRegister(m, val);
	Reset(SIGN);
    ModifyZeroFlag(val);
	Reset(HALFCARRY);
    ModifyParityFlagLogical(val);
	Reset(SUBTRACT);

    ModifyUndocumentedFlags8(val);
}
void Z80:: SLL(int m)
{
	if (m == 6)
        cycleTStates += 9;
    else
        cycleTStates += 8;

    int val = GetRegister(m);

    if ((val & 128) == 128)
		Set(CARRY);
    else
		Reset(CARRY);

    //  Z80 bug - sets bit 0 to 1 instead of 0
    val = ((val << 1) & 0xff) + 1;

    SetRegister(m, val);
    ModifySignFlag8(val);
    ModifyZeroFlag(val);
	Reset(HALFCARRY);
    ModifyParityFlagLogical(val);
	Reset(SUBTRACT);

    ModifyUndocumentedFlags8(val);
}
void Z80:: SRA(int m)
{
	if (m == 6)
        cycleTStates += 9;
    else
        cycleTStates += 8;

    int val = GetRegister(m);

    if ((val & 1) == 1)
		Set(CARRY);
    else
		Reset(CARRY);

    if ((val & 128) == 128)
    {
        val = (val >> 1) & 0xff;
        val = val | 128;
    }
    else
    {
        val = (val >> 1) & 0xff;
    }

    SetRegister(m, val);
    ModifySignFlag8(val);
    ModifyZeroFlag(val);
	Reset(HALFCARRY);
    ModifyParityFlagLogical(val);
	Reset(SUBTRACT);

    ModifyUndocumentedFlags8(val);
}
void Z80:: SLA(int m)
{
	if (m == 6)
		cycleTStates += 9;
	else
		cycleTStates += 8;

	int val = GetRegister(m);
	if ((val & 128) == 128)
		Set(CARRY);
	else
		Reset(CARRY);
	val = (val << 1) & 0xff;

	SetRegister(m, val);
	ModifySignFlag8(val);
	ModifyZeroFlag(val);
	Reset(HALFCARRY);
	ModifyParityFlagLogical(val);
	Reset(SUBTRACT);

	ModifyUndocumentedFlags8(val);
}
void Z80:: RR(int m)
{
	if (m == 6)
        cycleTStates += 9;
    else
        cycleTStates += 8;

    int val = GetRegister(m);
	if ((F & 1) == 1)	// carry flag
    {
		if ((val & 1) != 1) Reset(CARRY);
        val = ((val >> 1) + 128) & 0xff;
    }
    else
    {
		if ((val & 1) == 1) Set(CARRY);
        val = (val >> 1) & 0xff;
    }

    SetRegister(m, val);
    ModifySignFlag8(val);
    ModifyZeroFlag(val);
	Reset(HALFCARRY);
    ModifyParityFlagLogical(val);
	Reset(SUBTRACT);

    ModifyUndocumentedFlags8(val);	
}
void Z80:: RL(int m)
{
	if (m == 6)
        cycleTStates += 9;
    else
        cycleTStates += 8;

    int val = GetRegister(m);
    if ((F & 1) == 1)	// carry flag
    {
		if ((val & 128) != 128) Reset(CARRY);
        val = ((val << 1) + 1) & 0xff;
    }
    else
    {
		if ((val & 128) == 128) Set(CARRY);
        val = (val << 1) & 0xff;
    }    

    SetRegister(m, val);
    ModifySignFlag8(val);
    ModifyZeroFlag(val);
	Reset(HALFCARRY);
    ModifyParityFlagLogical(val);
	Reset(SUBTRACT);

    ModifyUndocumentedFlags8(val);
}
void Z80:: RRC(int m)
{
	if (m == 6)
        cycleTStates += 9;
    else
        cycleTStates += 8;

    int val = GetRegister(m);
    if ((val & 1) == 1)
    {
		Set(CARRY);
        val = ((val >> 1) + 128) & 0xff;
    }
    else
    {
		Reset(CARRY);
        val = (val >> 1) & 0xff;
    }
    SetRegister(m, val);
    ModifySignFlag8(val);
    ModifyZeroFlag(val);
	Reset(HALFCARRY);
    ModifyParityFlagLogical(val);
	Reset(SUBTRACT);

    ModifyUndocumentedFlags8(val);
}
void Z80:: RLC(int r)
{
	if (r == 6)
        cycleTStates += 9;
    else 
        cycleTStates += 8;
            
    int val = GetRegister(r);
    if ((val & 128) == 128)
    {
		Set(CARRY);
        val = ((val << 1) + 1) & 0xff;
    }
    else
    {
		Reset(CARRY);
        val = (val << 1) & 0xff;
    }
    SetRegister(r, val);

    ModifySignFlag8(val);
    ModifyZeroFlag(val);
	Reset(HALFCARRY);
    ModifyParityFlagLogical(val);
	Reset(SUBTRACT);

    ModifyUndocumentedFlags8(val);
}

void Z80:: RST(int p)
{
	cycleTStates += 11;
    SP = (SP - 1) & 0xffff;
    memory->memory[SP] = PC >> 8;
	SP = (SP - 1) & 0xffff;
    memory->memory[SP] = PC & 0xff;
    PC = p;
}

void Z80:: CP_n()
{
	cycleTStates += 7;

    int initial = A;
    int addition = memory->memory[PC] * -1;
	PC = (PC + 1) & 0xffff;

    A = (A + addition) & 0xff;

    ModifySignFlag8(A);
    ModifyZeroFlag(A);

    ModifyHalfCarryFlag8(initial, addition);
    ModifyCarryFlag8(initial, addition);
    ModifyOverflowFlag8(initial, addition, A);
	Set(SUBTRACT);

    A = initial;

    ModifyUndocumentedFlags8(-addition);
}
void Z80:: OR_n()
{
	cycleTStates += 7;

    A = A | memory->memory[PC];
	PC = (PC + 1) & 0xffff;

    ModifySignFlag8(A);
    ModifyZeroFlag(A);
	Reset(HALFCARRY);
    ModifyParityFlagLogical(A);
	Reset(SUBTRACT);
	Reset(CARRY);

    ModifyUndocumentedFlags8(A);
}
void Z80:: XOR_n()
{
	cycleTStates += 7;

    A = A ^ memory->memory[PC];
	PC = (PC + 1) & 0xffff;

    ModifySignFlag8(A);
    ModifyZeroFlag(A);
	Reset(HALFCARRY);
    ModifyParityFlagLogical(A);
	Reset(SUBTRACT);
	Reset(CARRY);

    ModifyUndocumentedFlags8(A);
}
void Z80:: AND_n()
{
	cycleTStates += 7;

    A = A & memory->memory[PC];
	PC = (PC + 1) & 0xffff;

    ModifySignFlag8(A);
    ModifyZeroFlag(A);
	Set(HALFCARRY);
    ModifyParityFlagLogical(A);
	Reset(SUBTRACT);
	Reset(CARRY);

    ModifyUndocumentedFlags8(A);
}

void Z80:: SBC_A_n()
{
	cycleTStates += 7;

    int initial = A;
    int addition = memory->memory[PC];
	PC = (PC + 1) & 0xffff;
    if ((F & CARRY) == CARRY) {
        addition++;
		addition = addition & 0xff;
	}
	addition = -addition;

    A = (A + addition) & 0xff;

    ModifySignFlag8(A);
    ModifyZeroFlag(A);
    ModifyHalfCarryFlag8(initial, addition);
    ModifyOverflowFlag8(initial, addition, A);
	Set(SUBTRACT);
    ModifyCarryFlag8(initial, addition);

    ModifyUndocumentedFlags8(A);
}
void Z80:: SUB_n()
{
	cycleTStates += 7;

    int initial = A;
    int addition = memory->memory[PC] * -1;
	PC = (PC + 1) & 0xffff;

    A = (A + addition) & 0xff;

    ModifySignFlag8(A);
    ModifyZeroFlag(A);
    ModifyHalfCarryFlag8(initial, addition);
    ModifyOverflowFlag8(initial, addition, A);
	Set(SUBTRACT);
    ModifyCarryFlag8(initial, addition);

    ModifyUndocumentedFlags8(A);
}
void Z80:: ADC_A_n()
{
	cycleTStates += 7;

    int initial = A;
    int addition = memory->memory[PC];
	PC = (PC + 1) & 0xffff;

    if ((F & CARRY) == CARRY) {
        addition++;
		addition = addition & 0xff;
	}
    A = (A + addition) & 0xff;

    ModifySignFlag8(A);
    ModifyZeroFlag(A);
    ModifyHalfCarryFlag8(initial, addition);
    ModifyOverflowFlag8(initial, addition, A);
	Reset(SUBTRACT);
    ModifyCarryFlag8(initial, addition);

    ModifyUndocumentedFlags8(A);
}
void Z80:: ADD_A_n()
{
	cycleTStates += 7;

    int initial = A;
    int addition = memory->memory[PC];
	PC = (PC + 1) & 0xffff;

    A = (A + addition) & 0xff;

    ModifySignFlag8(A);
    ModifyZeroFlag(A);
    ModifyHalfCarryFlag8(initial, addition);
    ModifyOverflowFlag8(initial, addition, A);
	Reset(SUBTRACT);
    ModifyCarryFlag8(initial, addition);

    ModifyUndocumentedFlags8(A);
}

void Z80:: CALL_nn()
{
	cycleTStates += 17;
	int low = memory->memory[PC];
    PC = (PC + 1) & 0xffff;
    int high = memory->memory[PC];
    PC = (PC + 1) & 0xffff;

    SP = (SP - 1) & 0xffff;
    memory->memory[SP] = PC >> 8;
    SP = (SP - 1) & 0xffff;
    memory->memory[SP] = PC & 0xff;

    PC = (high << 8) + low;
}

void Z80:: PUSH_qq(int qq)
{
	cycleTStates += 11;

    switch (qq)
    {
        case 0: 
            SP = (SP - 1) & 0xffff;
            memory->memory[SP] = B;
            SP = (SP - 1) & 0xffff;
            memory->memory[SP] = C;
            break;
        case 1:
            SP = (SP - 1) & 0xffff;
            memory->memory[SP] = D;
            SP = (SP - 1) & 0xffff;
            memory->memory[SP] = E;
            break;
        case 2:
            SP = (SP - 1) & 0xffff;
            memory->memory[SP] = GetRegister(4);
            SP = (SP - 1) & 0xffff;
            memory->memory[SP] = GetRegister(5);
            break;
        case 3:
            SP = (SP - 1) & 0xffff;
            memory->memory[SP] = A;
            SP = (SP - 1) & 0xffff;
            memory->memory[SP] = F;
            break;
    }
}
void Z80:: CALL_cc_nn(int cc)
{
	int low = memory->memory[PC];
	PC = (PC + 1) & 0xffff;
    int high = memory->memory[PC];
	PC = (PC + 1) & 0xffff;

    if (CheckCondition(cc))
    {
        cycleTStates += 17;
        SP = (SP - 1) & 0xffff;
        memory->memory[SP] = PC >> 8;
        SP = (SP - 1) & 0xffff;
        memory->memory[SP] = PC & 0xff;
        PC = (high << 8) + low;
    }
    else
    {
        cycleTStates += 10;
    }
}

void Z80:: EI()
{
	cycleTStates +=4;
	IFF1 = true;
	IFF2 = true;
}
void Z80:: DI()
{
	cycleTStates += 4;
	IFF1 = false;
	IFF2 = false;
}

void Z80:: EX_DE_HL()
{
	cycleTStates += 4;

    D = D ^ H;
    H = D ^ H;
    D = D ^ H;

    E = E ^ L;
    L = E ^ L;
    E = E ^ L;
}
void Z80:: EX_SP_HL()
{
	cycleTStates += 19;

    int low = memory->memory[SP];
    int high = memory->memory[(SP + 1) & 0xffff];
    int hl = Get16BitRegisters(2, false);
    memory->memory[SP] = hl & 0xff;
    memory->memory[(SP + 1) & 0xffff] = hl >> 8;
    SetRegister(5, low);
    SetRegister(4, high);
}

void Z80:: IN_A_n()
{
	cycleTStates += 11;
    int port = memory->memory[PC];
	PC = (PC + 1) & 0xffff;
    A = io->Read((A << 8) + port);
}
void Z80:: OUT_n_A()
{
	cycleTStates += 11;
    int port = memory->memory[PC];
	PC = (PC + 1) & 0xffff;
    io->Write((A << 8) + port, A);
}

void Z80:: JP_nn()
{
	cycleTStates += 10;
	PC = memory->memory[PC] + (memory->memory[(PC + 1) & 0xffff] << 8);
}
void Z80:: JP_cc_nn(int cc)
{
	cycleTStates += 10;

    if (CheckCondition(cc)) {
        PC = memory->memory[PC] + (memory->memory[(PC + 1) & 0xffff] << 8);
	} else {
		PC = (PC + 2) & 0xffff;
	}
}

void Z80:: LD_SP_HL()
{
	cycleTStates += 6;
    SP = Get16BitRegisters(2, false);
}

void Z80:: JP_HL()
{
	cycleTStates += 4;
    PC = Get16BitRegisters(2, false);
}

void Z80:: EXX()
{
	cycleTStates += 4;

    B = B ^ B2;
    B2 = B ^ B2;
    B = B ^ B2;

    C = C ^ C2;
    C2 = C ^ C2;
    C = C ^ C2;

    D = D ^ D2;
    D2 = D ^ D2;
    D = D ^ D2;

    E = E ^ E2;
    E2 = E ^ E2;
    E = E ^ E2;

    H = H ^ H2;
    H2 = H ^ H2;
    H = H ^ H2;

    L = L ^ L2;
    L2 = L ^ L2;
    L = L ^ L2;
}

void Z80:: RET()
{
	cycleTStates += 10;
    PC = memory->memory[SP];
    SP = (SP + 1) & 0xffff;
    PC += (memory->memory[SP] << 8);
    SP = (SP + 1) & 0xffff;
}
void Z80:: POP_qq(int qq)
{
	cycleTStates += 10;
    if (qq == 3)
    {
        //  Pop to AF
        F = memory->memory[SP];
		SP = (SP + 1) & 0xffff;
        A = memory->memory[SP];
		SP = (SP + 1) & 0xffff;
    }
    else
    {
        //  Pop to standard register pair
		int low = memory->memory[SP];
		SP = (SP + 1) & 0xffff;
		int high = memory->memory[SP];
		SP = (SP + 1) & 0xffff;
        Set16BitRegisters(qq, low, high);
    }
}
void Z80:: RET_cc(int cc)
{
	if (CheckCondition(cc))
    {
        cycleTStates += 11;
        PC = memory->memory[SP];
		SP = (SP + 1) & 0xffff;
		PC += (memory->memory[SP] << 8);
		SP = (SP + 1) & 0xffff;
    }
    else
    {
        cycleTStates += 5;
    }
}

void Z80:: CP_r(int r)
{
	cycleTStates += 4;

    int initial = A;
    if (r == 6 && prefix != 0)
    {
        cycleTStates += 4;
        ReadDisplacementByte();
    }
    int reg = GetRegister(r);
    int addition = -reg;

    A = (A + addition) & 0xff;

    ModifySignFlag8(A);
    ModifyZeroFlag(A);

    ModifyHalfCarryFlag8(initial, addition);
    ModifyCarryFlag8(initial, addition);
    ModifyOverflowFlag8(initial, addition, A);
	Set(SUBTRACT);

    A = initial;

    ModifyUndocumentedFlags8(reg);
}
void Z80:: OR_r(int r)
{
	cycleTStates += 4;

    if (r == 6 && prefix != 0)
    {
        cycleTStates += 4;
        ReadDisplacementByte();
    }

    A = A | GetRegister(r);

    ModifySignFlag8(A);
    ModifyZeroFlag(A);
	Reset(HALFCARRY);
    ModifyParityFlagLogical(A);
	Reset(SUBTRACT);
	Reset(CARRY);

    ModifyUndocumentedFlags8(A);
}
void Z80:: XOR_r(int r)
{
	cycleTStates += 4;

    if (r == 6 && prefix != 0)
    {
        cycleTStates += 4;
        ReadDisplacementByte();
    }

    A = A ^ GetRegister(r);

    ModifySignFlag8(A);
    ModifyZeroFlag(A);
	Reset(HALFCARRY);
    ModifyParityFlagLogical(A);
	Reset(SUBTRACT);
	Reset(CARRY);

    ModifyUndocumentedFlags8(A);
}
void Z80:: AND_r(int r)
{
	cycleTStates += 4;

    if (r == 6 && prefix != 0)
    {
        cycleTStates += 4;
        ReadDisplacementByte();
    }

    A = A & GetRegister(r);

    ModifySignFlag8(A);
    ModifyZeroFlag(A);
	Set(HALFCARRY);
    ModifyParityFlagLogical(A);
	Reset(SUBTRACT);
	Reset(CARRY);

    ModifyUndocumentedFlags8(A);
}

void Z80:: SBC_A_r(int r)
{
	cycleTStates += 4;

    int initial = A;

    if (r == 6 && prefix != 0)
    {
        cycleTStates += 4;
        ReadDisplacementByte();
    }

    int addition = GetRegister(r);

    if ((F & CARRY) == CARRY) {
        addition++;
		addition = addition & 0xff;
	}
	addition = -addition;

    A = (A + addition) & 0xff;

    ModifySignFlag8(A);
    ModifyZeroFlag(A);

    ModifyHalfCarryFlag8(initial, addition);
    ModifyCarryFlag8(initial, addition);
    ModifyOverflowFlag8(initial, addition, A);
	Set(SUBTRACT);

    ModifyUndocumentedFlags8(A);
}
void Z80:: SUB_r(int r)
{
	cycleTStates += 4;

    int initial = A;

    if (r == 6 && prefix != 0)
    {
        cycleTStates += 4;
        ReadDisplacementByte();
    }

    int addition = -GetRegister(r);

    A = (A + addition) & 0xff;

    ModifySignFlag8(A);
    ModifyZeroFlag(A);

    ModifyHalfCarryFlag8(initial, addition);
    ModifyCarryFlag8(initial, addition);
    ModifyOverflowFlag8(initial, addition, A);
	Set(SUBTRACT);

    ModifyUndocumentedFlags8(A);
}
void Z80:: ADC_A_r(int r)
{
	cycleTStates += 4;

    int initial = A;

    if (r == 6 && prefix != 0)
    {
        cycleTStates += 4;
        ReadDisplacementByte();
    }

    int addition = GetRegister(r);

    if ((F & CARRY) == CARRY) {
        addition++;
		addition = addition & 0xff;
	}

    A = (A + addition) & 0xff;

    ModifySignFlag8(A);
    ModifyZeroFlag(A);

    ModifyHalfCarryFlag8(initial, addition);
    ModifyCarryFlag8(initial, addition);
    ModifyOverflowFlag8(initial, addition, A);
	Reset(SUBTRACT);

    ModifyUndocumentedFlags8(A);
}
void Z80:: ADD_A_r(int r)
{
	cycleTStates += 4;

    int initial = A;

    if (r == 6 && prefix != 0)
    {
        cycleTStates += 4;
        ReadDisplacementByte();
    }

    int addition = GetRegister(r);

    A = (A + addition) & 0xff;

    ModifySignFlag8(A);
    ModifyZeroFlag(A);
            
    ModifyHalfCarryFlag8(initial, addition);
    ModifyCarryFlag8(initial, addition);
    ModifyOverflowFlag8(initial, addition, A);
	Reset(SUBTRACT);
    ModifyUndocumentedFlags8(A);
}

void Z80:: HALT()
{
	cycleTStates += 4;
    isHalted = true;
    PC = (PC - 1) & 0xffff;
}

void Z80:: LD_r_r(int r, int r2)
{
	cycleTStates += 4;

    if ((r == 6 || r2 == 6) && prefix != 0)
    {
        cycleTStates += 4;
        ReadDisplacementByte();
        if (r == 6)
        {
            ignorePrefix = true;
            int t = GetRegister(r2);
            ignorePrefix = false;
            SetRegister(r, t);
        }
        else
        {
            int t = GetRegister(r2);
            ignorePrefix = true;
            SetRegister(r, t);
            ignorePrefix = false;
        }
    }
    else
    {
        SetRegister(r, GetRegister(r2));
    }
}

void Z80:: CCF()
{
	cycleTStates += 4;

    //  Previous carry copied to half-carry?
	//  confirmed yes 24/03/2012
    if ((F & CARRY) == CARRY)	// carry flag
    {
		Set(HALFCARRY);
		Reset(CARRY);
    }
    else
    {
        Reset(HALFCARRY);
		Set(CARRY);
    }

	Reset(SUBTRACT);
    ModifyUndocumentedFlags8(A);
}
void Z80:: SCF()
{
	cycleTStates += 4;

	Set(CARRY);
	Reset(HALFCARRY);
	Reset(SUBTRACT);
    ModifyUndocumentedFlags8(A);
}
void Z80:: CPL()
{
	cycleTStates += 4;

    A = (~A) & 0xff;
	Set(HALFCARRY);
	Set(SUBTRACT);
    ModifyUndocumentedFlags8(A);
}
void Z80:: DAA()
{
	cycleTStates += 4;

	int CF = ((F & CARRY) == CARRY) ? 1 : 0;
	int HF = ((F & HALFCARRY) == HALFCARRY) ? 1 : 0;
    int NF = ((F & SUBTRACT) == SUBTRACT) ? 1 : 0;

    int hiNibble = (A >> 4) & 0xf;
    int loNibble = A & 0xf;
            
    int diff = 0;

    if (CF == 0 && hiNibble < 10 && HF == 0 && loNibble < 10) diff = 0;
    else if (CF == 0 && hiNibble < 10 && HF == 1 && loNibble < 10) diff = 6;
    else if (CF == 0 && hiNibble < 9 && loNibble > 9) diff = 6;
    else if (CF == 0 && hiNibble > 9 && HF == 0 && loNibble < 10) diff = 0x60;
    else if (CF == 1 && HF == 0 && loNibble < 10) diff = 0x60;
    else if (CF == 1 && HF == 1 && loNibble < 10) diff = 0x66;
    else if (CF == 1 && loNibble > 9) diff = 0x66;
    else if (CF == 0 && hiNibble > 8 && loNibble > 9) diff = 0x66;
    else if (CF == 0 && hiNibble > 9 && HF == 1 && loNibble < 10) diff = 0x66;

    if (CF == 0 && hiNibble < 10 && loNibble < 10) Reset(CARRY);
    else if (CF == 0 && hiNibble < 9 && loNibble > 9) Reset(CARRY);
    else if (CF == 0 && hiNibble > 8 && loNibble > 9) Set(CARRY);
    else if (CF == 0 && hiNibble > 9 && loNibble < 10) Set(CARRY);
    else if (CF == 1) Set(CARRY);

    if (NF == 0 && loNibble < 10) Reset(HALFCARRY);
    else if (NF == 0 && loNibble > 9) Set(HALFCARRY);
    else if (NF == 1 && HF == 0) Reset(HALFCARRY);
    else if (NF == 1 && HF == 1 && loNibble > 5) Reset(HALFCARRY);
    else if (NF == 1 && HF == 1 && loNibble < 6) Set(HALFCARRY);

    if (NF == 0)
        A = (A + diff) & 0xff;
    else
        A = (A - diff) & 0xff;

    ModifySignFlag8(A);
    ModifyParityFlagLogical(A);
    ModifyUndocumentedFlags8(A);
    ModifyZeroFlag(A);
}
void Z80:: RRA()
{
	cycleTStates += 4;

    bool newCarry;
    if (A % 2 == 1)
        newCarry = true;
    else
        newCarry = false;

    A = (A >> 1) & 0xff;

    if ((F & 1) == 1)	//	cary flag
        A += 128;

    if (newCarry)
		Set(CARRY);
    else
		Reset(CARRY);

	Reset(HALFCARRY);
	Reset(SUBTRACT);

    ModifyUndocumentedFlags8(A);
}
void Z80:: RLA()
{
	cycleTStates += 4;

    bool newCarry;
    if (A > 127)
        newCarry = true;
    else
        newCarry = false;

    A = (A << 1) & 0xff;

    if ((F & 1) == 1)	//	caryy flag
        A++;

    if (newCarry)
		Set(CARRY);
    else
		Reset(CARRY);

	Reset(HALFCARRY);
	Reset(SUBTRACT);

    ModifyUndocumentedFlags8(A);
}
void Z80:: RRCA()
{
	cycleTStates += 4;

    if (A % 2 == 1)
		Set(CARRY);
    else
		Reset(CARRY);

	Reset(HALFCARRY);
	Reset(SUBTRACT);

    A = (A >> 1) & 0xff;

    if ((F & 1) == 1)	//	carry flag
        A += 128;

    ModifyUndocumentedFlags8(A);
}
void Z80:: RLCA()
{
	cycleTStates += 4;           

    if (A > 127)
		Set(CARRY);
    else
		Reset(CARRY);
            
	Reset(HALFCARRY);
	Reset(SUBTRACT);

    A = (A << 1) & 0xff;

    if ((F & 1) == 1)	//	carry flag
        A = (A + 1) & 0xff;

    ModifyUndocumentedFlags8(A);
}

void Z80:: LD_r_n(int r)
{
	cycleTStates += 7;

    if (r == 6 && prefix != 0) {
        ReadDisplacementByte();
        cycleTStates++;          
    }
    SetRegister(r, memory->memory[PC]);
	PC = (PC + 1) & 0xffff;
}
void Z80:: DEC_r(int r)
{
	cycleTStates += 4;

    if (r == 6)
    {
        cycleTStates++;
        if (prefix != 0)
            ReadDisplacementByte();
    }

    int initial = GetRegister(r);
    int result = (initial - 1) & 0xff;

    SetRegister(r, result);

    ModifySignFlag8(result);
    ModifyZeroFlag(result);

    ModifyHalfCarryFlag8(initial, -1);
    ModifyOverflowFlag8(initial, -1, result);
	Set(SUBTRACT);
    ModifyUndocumentedFlags8(result);
}
void Z80:: INC_r(int r)
{
	cycleTStates += 4;

    if (r == 6)
    {
        cycleTStates++;
        if (prefix !=0)
            ReadDisplacementByte();
    }

    int initial = GetRegister(r);
    int result = (initial + 1) & 0xff;

    SetRegister(r, result);

    ModifySignFlag8(result);
    ModifyZeroFlag(result);

    ModifyHalfCarryFlag8(initial, 1);
    ModifyOverflowFlag8(initial, 1, result);
	Reset(SUBTRACT);
    ModifyUndocumentedFlags8(result);
}
void Z80:: DEC_ss(int ss)
{
	cycleTStates += 6;
    Set16BitRegisters(ss, (Get16BitRegisters(ss, false) - 1) & 0xFFFF);
}
void Z80:: INC_ss(int ss)
{
	cycleTStates += 6;
    Set16BitRegisters(ss, (Get16BitRegisters(ss, false) + 1) & 0xFFFF);
}

void Z80:: LD_A_nn()
{
	cycleTStates += 13;
	int address = memory->memory[PC] + (memory->memory[(PC + 1) & 0xffff] << 8);
    PC = (PC + 2) & 0xffff;
    A = memory->memory[address];
}
void Z80:: LD_HL_nn()
{
	cycleTStates += 16;
    int address = memory->memory[PC] + (memory->memory[(PC + 1) & 0xffff] << 8);
    PC = (PC + 2) & 0xffff;
    SetRegister(5, memory->memory[address]);
    SetRegister(4, memory->memory[(address + 1) & 0xffff]);
}
void Z80:: LD_A_BC()
{
	cycleTStates += 7;
    A = memory->memory[Get16BitRegisters(0, false)];
}
void Z80:: LD_A_DE()
{
	cycleTStates += 7;
    A = memory->memory[Get16BitRegisters(1, false)];
}
void Z80:: LD_nn_A()
{
	cycleTStates += 13;
	int address = memory->memory[PC] + (memory->memory[(PC + 1) & 0xffff] << 8);
    PC = (PC + 2) & 0xffff;
    memory->memory[address] = A;
}
void Z80:: LD_nn_HL()
{
	cycleTStates += 16;

    int address = memory->memory[PC] + (memory->memory[(PC + 1) & 0xffff] << 8);
    PC = (PC + 2) & 0xffff;
    memory->memory[address] = GetRegister(5);
    memory->memory[(address + 1) & 0xffff] = GetRegister(4);
}
void Z80:: LD_DE_A()
{
	cycleTStates += 7;
    memory->memory[Get16BitRegisters(1, false)] = A;
}
void Z80:: LD_BC_A()
{
	cycleTStates += 7;
    memory->memory[Get16BitRegisters(0, false)] = A;
}

void Z80:: ADD_HL_ss(int ss)
{
	cycleTStates += 11;

    int HL = Get16BitRegisters(2, false);
    int SS = Get16BitRegisters(ss, false);

    //  Bitmask to force max 16 bits
    Set16BitRegisters(2, (HL + SS) & 0xffff);
    //  16-bit Carry
    ModifyCarryFlag16(HL, SS);
    //  Half-carry flags set dependent on high-bit
    ModifyHalfCarryFlag16(HL, SS);
    //  N is reset
	Reset(SUBTRACT);

    ModifyUndocumentedFlags16(Get16BitRegisters(2, false));
}
void Z80:: LD_dd_nn(int dd)
{
	int low = memory->memory[PC];
    PC = (PC + 1) & 0xffff;
    int high = memory->memory[PC];
    PC = (PC + 1) & 0xffff;
    Set16BitRegisters(dd, low, high);
    cycleTStates += 10;
}

void Z80:: JR(int condition)
{
	if (CheckCondition(condition))
    {
        cycleTStates += 12;
        PC += ((signed char)memory->memory[PC] + 1);
    }
    else
    {
        PC++;
        cycleTStates += 7;
    }
    PC = PC & 0xffff;
}
void Z80:: DJNZ()
{
	B = (B - 1) & 0xff;
    if (B != 0)
    {
        cycleTStates += 13;
        PC += ((signed char)memory->memory[PC]) + 1;
    }
    else
    {
        PC++;
        cycleTStates += 8;
    }
    PC = PC & 0xffff;
}
void Z80:: EX_AF_AF2()
{
	cycleTStates += 4;

    A = A ^ A2;
    A2 = A ^ A2;
    A = A ^ A2;

    F = F ^ F2;
    F2 = F ^ F2;
    F = F ^ F2;
}
void Z80:: NOP()
{
	cycleTStates += 4;
}
