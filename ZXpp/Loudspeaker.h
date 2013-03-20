#ifndef LOUDSPEAKER
#define LOUDSPEAKER

#pragma comment(lib, "dsound.lib")
#pragma comment(lib, "dxguid.lib")
#pragma comment(lib, "winmm.lib")

#include <windows.h>
#include <mmsystem.h>
#include <dsound.h>
#include <stdio.h>
#include <vector>

class Loudspeaker
{
private:
	IDirectSound8* directSound;
	IDirectSoundBuffer* primaryBuffer;
	IDirectSoundBuffer8* secondaryBuffer;

public:
	Loudspeaker();
	~Loudspeaker();
	void Init(HWND hwnd);
	void Play(std::vector<unsigned char> buffer);
};
#endif