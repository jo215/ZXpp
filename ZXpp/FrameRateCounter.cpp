#include "FrameRateCounter.h"
#include <string>

//	Constructor.
FrameRateCounter::FrameRateCounter(ID3D11Device* device, ID3D11DeviceContext* context)
{
	frameRate = 0;
	frameCounter = 0;
	spriteBatch.reset( new SpriteBatch( context) );
    spriteFont.reset( new SpriteFont( device, L"Arial12.spritefont" ) );
	timer = new Timer();
	timer->restart();
}

void FrameRateCounter::setOutput(string s)
{
	output = s;
}

void FrameRateCounter::render()
{
	//double elapsedMillis = timer->getElapsedTimeMillis();
	//while (elapsedMillis < 16.6667)
	//{
	//	elapsedMillis = timer->getElapsedTimeMillis();
	//}
	//timer->restart();
	//frameCounter++;

	//string str = "Frames Rendered: ";
	//str += to_string(frameCounter);
	//wstring wideStr = wstring(str.begin(), str.end());

	//string s2 = "Time: ";
	//s2 += to_string(elapsedMillis) + " ms";
	//wstring ws2 = wstring(s2.begin(), s2.end());

	spriteBatch->Begin( SpriteSortMode_Deferred );
	//spriteFont->DrawString( spriteBatch.get(), wideStr.c_str(), XMFLOAT2( 0, 5 ), Colors::Yellow );
	spriteFont->DrawString( spriteBatch.get(), wstring(output.begin(), output.end()).c_str(), XMFLOAT2( 0, 50 ), Colors::Yellow );
	spriteBatch->End();
	
}