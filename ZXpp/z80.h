#ifndef Z80CPU
#define Z80CPU

//	Forward declarations
class Memory;
class IioDevice;
enum Flag;

/*
	Represents a Z80 CPU.
*/
class Z80
{

private:

	friend class TestZ80;
	friend class ULA;
	//	Standard registers
	int A, B, C, D, E, F, H, L;
	//	Shadow registers
	int A2, B2, C2, D2, E2, F2, H2, L2;
	//	Stack Pointer / Program Counter
	int SP, PC;
	//	Index registers
	int IXH, IXL, IYH, IYL;
	//	Memory refresh / interrupt
	int R, I, interruptMode;
	bool IFF1, IFF2;

	//	Current instruction information
	int opcode, prefix, prefix2, displacement;

	//	Other bits
	bool isHalted, ignorePrefix, fastLoad;

	//	The memory!
	Memory* memory;

	//	Ferranti ULA chip
	IioDevice* io;

	//	Reads optional displacement byte when required for some opcodes
	void ReadDisplacementByte();
	
	//	Flag register manipulation methods
	int FlagAsInt(Flag flag);
	void Set(Flag flag);
	void Reset(Flag flag);
	void ModifyCarryFlag8(int initial, int addition);
	void ModifyCarryFlag16(int initial, int addition);
	void ModifyHalfCarryFlag8(int initial, int addition);
	void ModifyHalfCarryFlag16(int initial, int addition);
	void ModifySignFlag8(int result);
	void ModifySignFlag16(int result);
	void ModifyZeroFlag(int result);
	void ModifyOverflowFlag8(int initial, int addition, int result);
	void ModifyOverflowFlag16(int initial, int addition, int result);
	void ModifyParityFlagLogical(int result);
	void ModifyUndocumentedFlags8(int result);
	void ModifyUndocumentedFlags16(int result);
	void ModifyUndocumentedFlagsLoadGroup();
	void ModifyUndocumentedFlagsCompareGroup(int n);

	//	Get / set register(s) by number code
	int GetRegister(int reg);
	void SetRegister(int reg, int value);
	void Set16BitRegisters(int registerPair, int value);
	void Set16BitRegisters(int registerPair, int lowByte, int highByte);
	int Get16BitRegisters(int registerPair, bool ignorePrefix);
	bool CheckCondition(int condition);

	//	Z80 Opcodes
	void OTDR();
	void INDR();
	void CPDR();
	void LDDR();

	void OTIR();
	void INIR();
	void CPIR();
	void LDIR();

	void OUTD();
	void IND();
	void CPD();
	void LDD();

	void OUTI();
	void INI();
	void CPI();
	void LDI();

	void RLD();
	void RRD();

	void LD_A_R();
	void LD_A_I();
	void LD_R_A();
	void LD_I_A();

	void IM(int y);
	void RETI();
	void RETN();

	void NEG();

	void LD_dd_nn2(int dd);
	void LD_nn_dd(int dd);

	void ADC_HL(int ss);
	void SBC_HL(int ss);

	void OUT_C_r(int r);
	void IN_r_C(int r);
	
	void NONI();

	void SET(int b, int r);
	void RES(int b, int r);
	void BIT(int b, int r);

	void SRL(int m);
	void SLL(int m);
	void SRA(int m);
	void SLA(int m);
	void RR(int m);
	void RL(int m);
	void RRC(int m);
	void RLC(int r);

	void RST(int p);

	void CP_n();
	void OR_n();
	void XOR_n();
	void AND_n();

	void SBC_A_n();
	void SUB_n();
	void ADC_A_n();
	void ADD_A_n();

	void CALL_nn();
	void PUSH_qq(int qq);
	void CALL_cc_nn(int cc);
	
	void EI();
	void DI();

	void EX_DE_HL();
	void EX_SP_HL();

	void IN_A_n();
	void OUT_n_A();

	void JP_nn();
	void JP_cc_nn(int cc);

	void LD_SP_HL();

	void JP_HL();

	void EXX();

	void RET();
	void POP_qq(int qq);
	void RET_cc(int cc);

	void CP_r(int r);
	void OR_r(int r);
	void XOR_r(int r);
	void AND_r(int r);

	void SBC_A_r(int r);
	void SUB_r(int r);
	void ADC_A_r(int r);
	void ADD_A_r(int r);

	void HALT();

	void LD_r_r(int r, int r2);

	void CCF();
	void SCF();
	void CPL();
	void DAA();
	void RRA();
	void RLA();
	void RRCA();
	void RLCA();

	void LD_r_n(int r);
	void DEC_r(int r);
	void INC_r(int r);
	void DEC_ss(int ss);
	void INC_ss(int ss);

	void LD_A_nn();
	void LD_HL_nn();
	void LD_A_BC();
	void LD_A_DE();
	void LD_nn_A();
	void LD_nn_HL();
	void LD_DE_A();
	void LD_BC_A();

	void ADD_HL_ss(int ss);
	void LD_dd_nn(int dd);

	void JR(int condition);
	void DJNZ();
	void EX_AF_AF2();
	void NOP();
public:
	//	Constructor & destructor
	Z80(Memory* memory);
	~Z80();
	//	Public methods & fields
	int cycleTStates, previousTStates;
	void Reset();
	void SetFlags(int flagsByte);
	void Interrupt(bool nonMaskable);
	void Run(int maxTStates);
	//	Sets the ula pointer
	void AddDevice(IioDevice* ula);
};

#endif