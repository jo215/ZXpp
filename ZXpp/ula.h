#ifndef FERRANTIULA
#define FERRANTIULA

#pragma comment(lib, "dinput8.lib")
#pragma comment(lib, "dxguid.lib")

#include <windows.h>
#include <vector>
#include "TextureCache.h"
#include "SpriteBatch.h"
#include "memory.h"
#include "z80.h"
#include "Loudspeaker.h"
#define DIRECTINPUT_VERSION 0x0800
#include <dinput.h>
#include "Timer.h"
#include "IioDevice.h"
#include <iostream>
#include <fstream>
#include <string>


/*
 *	Represents the Ferranti ULA chip
 */

using namespace DirectX;
using namespace std;
class ULA : public IioDevice
{

private:
	ID3D11DeviceContext* context;
	ID3D11RenderTargetView* backbufferview;

	TextureCache* cache;
	int border;

	Z80* z80;
	Memory* memory;
	Loudspeaker* speaker;
	unique_ptr<SpriteBatch> spriteBatch;

	IDirectInput8* directInput;
	IDirectInputDevice8* keyboard;
	unsigned char keyboardState[256];

	static const int borderHeight = 56;
	static const int borderWidth = 48;
	float screenScale;

	bool earActive, micActive;

	long framesGenerated;
	bool update;

	vector<unsigned char> buffer;
	XMVECTORF32 colors[16];

	void getUserInput();
public:
	
	//	Update & render
	void updateFrame();
	void render();
	
	//	CPU I/O
	void Write(int port, int dataByte);
	int Read(int port);
	
	//	File load / save
	void LoadSNA(string fileName);
	void SaveSNA(string fileName);

	//	Constructor / destructor
	ULA(ID3D11Device* device, ID3D11DeviceContext* context, HINSTANCE hinstance, HWND hwnd, ID3D11RenderTargetView* backBuffer);
	~ULA();

};

#endif