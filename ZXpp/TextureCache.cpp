#include <iostream>
#include "TextureCache.h"
#include "WICTextureLoader.h"
#include <string>
using namespace DirectX;
using namespace std;

//	Constructor
TextureCache::TextureCache(ID3D11Device* device, ID3D11DeviceContext* context)
{
	//	We load all our textures here

	block = 0;
	string blockName = "pixelSlices\\block.png";
	wstring wideName = wstring(blockName.begin(), blockName.end());
	CreateWICTextureFromFile( device, context, wideName.c_str(), nullptr, &block);

	for (int i = 0; i < 256; i++)
	{
		string name = "pixelSlices\\pixelSlice";
		name += to_string(i);
		name += ".png";
		wstring wideName = wstring(name.begin(), name.end());
		g_pTextureRV1[i] = 0;
		CreateWICTextureFromFile( device, context, wideName.c_str(), nullptr, &g_pTextureRV1[i] );
	}

}

//	Gets the texture representing the given 8-bit pattern
ID3D11ShaderResourceView* TextureCache::getTexture(int i)
{
	return g_pTextureRV1[i];
}

ID3D11ShaderResourceView* TextureCache::getBlock()
{
	return block;
}

void TextureCache::Cleanup()
{
	for (int i = 0; i < 256; i++)
	{
		if (g_pTextureRV1[i]) {		
			g_pTextureRV1[i]->Release();
			g_pTextureRV1[i] = NULL;
		}
	}
	if(block)
	{
		block->Release();
		block= NULL;
	}
}