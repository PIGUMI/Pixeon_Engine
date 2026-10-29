/*
* ファイル名　D3D11Native
* 説　　　明　D3D11 バックエンドのネイティブオブジェクト取得
*             Effekseer など D3D11 API を直接必要とする外部ライブラリ連携でのみ使う
*/
#ifndef RHI_D3D11_NATIVE_H
#define RHI_D3D11_NATIVE_H

#include "../RHI.h"
#include <d3d11.h>

namespace RHI {
	ID3D11ShaderResourceView* D3D11SrvGet(Texture* texture);
	ID3D11Resource* D3D11ResourceGet(Texture* texture);
	// 既存の SRV をテクスチャとして包む（参照カウントを 1 つ持つ）
	TextureRef D3D11TextureWrap(Device* device, ID3D11ShaderResourceView* srv);
}

#endif // !RHI_D3D11_NATIVE_H
