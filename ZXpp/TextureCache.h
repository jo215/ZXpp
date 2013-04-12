#ifndef TEXTURECACHE
#define TEXTURECACHE

#include <d3d11.h>

class TextureCache
{
private:

	ID3D11ShaderResourceView*           g_pTextureRV1 [256];
	ID3D11ShaderResourceView*			block;
public:
	TextureCache(ID3D11Device* device, ID3D11DeviceContext* context);
	~TextureCache();
	void Cleanup();
	ID3D11ShaderResourceView* getTexture(int i);
	ID3D11ShaderResourceView* getBlock();
};

#endif