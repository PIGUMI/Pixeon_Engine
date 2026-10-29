/*
* ファイル名　DXGICommon
* 説　　　明　D3D11 / D3D12 バックエンドで共有する DXGI フォーマット変換と DXBC 反射
*/
#ifndef RHI_DXGI_COMMON_H
#define RHI_DXGI_COMMON_H

#include "../RHITypes.h"
#include <dxgiformat.h>

namespace RHI {
	// 通常のフォーマット変換（深度は DSV 用の D24S8 / D32 を返す）
	DXGI_FORMAT DxgiFormatGet(Format format);
	// 深度を SRV でも読む場合の TYPELESS リソースフォーマット
	DXGI_FORMAT DxgiTypelessFormatGet(Format format);
	// 深度テクスチャをシェーダーで読むときの SRV フォーマット
	DXGI_FORMAT DxgiDepthSrvFormatGet(Format format);
	// DXGI → RHI（対応していなければ Unknown）
	Format FormatFromDxgi(DXGI_FORMAT format);

	// DXBC から定数バッファ・テクスチャ・サンプラーの反射情報を取り出す
	bool DxbcReflect(const void* bytecode, size_t size, ShaderReflection& out_reflection);
}

#endif // !RHI_DXGI_COMMON_H
