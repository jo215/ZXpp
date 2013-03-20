#ifndef FRAMECOUNTER
#define FRAMECOUNTER

#include "SpriteBatch.h"
#include "SpriteFont.h"
#include <windows.h>
#include "Timer.h"

using namespace DirectX;
using namespace std;

class FrameRateCounter
{
private:
	int frameRate, frameCounter;
	unique_ptr<SpriteBatch> spriteBatch;
	unique_ptr<SpriteFont>  spriteFont;
	Timer* timer;
	string output;
public:
	FrameRateCounter(ID3D11Device* device, ID3D11DeviceContext* context);
	~FrameRateCounter();
	void render();
	void setOutput(string s);
};

#endif