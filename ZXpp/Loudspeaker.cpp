#include "Loudspeaker.h"

//	Constructor
Loudspeaker::Loudspeaker()
{
	directSound = 0;
	primaryBuffer = 0;
	secondaryBuffer = 0;
}

//	Destructor
Loudspeaker::~Loudspeaker()
{
	if (primaryBuffer)
	{
		primaryBuffer->Release();
		primaryBuffer = 0;
	}
	if (directSound)
	{
		directSound->Release();
		directSound = 0;
	}
}

//	Initialise DirectSound and buffers
void Loudspeaker::Init(HWND hwnd)
{
	//	Direct sound
	DirectSoundCreate8(NULL, &directSound, NULL);
	directSound->SetCooperativeLevel(hwnd, DSSCL_PRIORITY);

	//	Primary (soundcard) buffer
	DSBUFFERDESC bufferDesc;
	bufferDesc.dwSize = sizeof(DSBUFFERDESC);
	bufferDesc.dwFlags = DSBCAPS_PRIMARYBUFFER | DSBCAPS_CTRLVOLUME;
	bufferDesc.dwBufferBytes = 0;
	bufferDesc.dwReserved = 0;
	bufferDesc.lpwfxFormat = NULL;
	bufferDesc.guid3DAlgorithm = GUID_NULL;
	directSound->CreateSoundBuffer(&bufferDesc, &primaryBuffer, NULL);

	WAVEFORMATEX waveFormat;
	waveFormat.wFormatTag = WAVE_FORMAT_PCM;
	waveFormat.nSamplesPerSec = 44100;
	waveFormat.wBitsPerSample = 16;
	waveFormat.nChannels = 1;
	waveFormat.nBlockAlign = (waveFormat.wBitsPerSample / 8) * waveFormat.nChannels;
	waveFormat.nAvgBytesPerSec = waveFormat.nSamplesPerSec * waveFormat.nBlockAlign;
	waveFormat.cbSize = 0;
	primaryBuffer->SetFormat(&waveFormat);

	//	Secondary (CPU) buffer
	bufferDesc.dwSize = sizeof(DSBUFFERDESC);
	bufferDesc.dwFlags = DSBCAPS_CTRLVOLUME;
	bufferDesc.dwBufferBytes = 1764;
	bufferDesc.dwReserved = 0;
	bufferDesc.lpwfxFormat = &waveFormat;
	bufferDesc.guid3DAlgorithm = GUID_NULL;

	IDirectSoundBuffer* temp;
	directSound->CreateSoundBuffer(&bufferDesc, &temp, NULL);
	temp->QueryInterface(IID_IDirectSoundBuffer8, (void**)&secondaryBuffer);
	temp->Release();
	temp = 0;
}

void Loudspeaker::Play(std::vector<unsigned char> buffer)
{
	unsigned char data[1764];
	copy(buffer.begin(), buffer.end(), data);
	//	Lock secondary buffer
	unsigned char *bufferPtr;
	unsigned long bufferSize;
	secondaryBuffer->Lock(0, 1764, (void**)&bufferPtr, (DWORD*)&bufferSize, NULL, 0, 0);
	memcpy(bufferPtr, data, 1764);
	secondaryBuffer->Unlock((void**)bufferPtr, bufferSize, NULL, 0);

	secondaryBuffer->SetCurrentPosition(0);
	secondaryBuffer->SetVolume(DSBVOLUME_MAX);
	secondaryBuffer->Play(0,0,0);
}