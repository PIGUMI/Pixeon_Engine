/*
* ファイル名　D3D12Native
* 説　　　明　D3D12 バックエンドのネイティブオブジェクト取得
*             Effekseer など D3D12 API を直接必要とする外部ライブラリ連携でのみ使う
*/
#ifndef RHI_D3D12_NATIVE_H
#define RHI_D3D12_NATIVE_H

#include "../RHI.h"
#include <d3d12.h>

namespace RHI {
	ID3D12Resource* D3D12ResourceGet(Texture* texture);
	uint32_t D3D12FramesInFlightGet(Device* device);
}

#endif // !RHI_D3D12_NATIVE_H
