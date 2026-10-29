/*
* ファイル名　DXGICommon
* 説　　　明　DXGI フォーマット変換と DXBC 反射の実装
*/
#include "DXGICommon.h"
#include <d3d11shader.h>
#include <d3dcompiler.h>
#include <wrl/client.h>

#if defined(_MSC_VER)
#pragma comment(lib, "d3dcompiler.lib")
#endif

namespace RHI {

	DXGI_FORMAT DxgiFormatGet(Format format)
	{
		switch (format) {
		case Format::R8_UNorm:              return DXGI_FORMAT_R8_UNORM;
		case Format::R8G8B8A8_UNorm:        return DXGI_FORMAT_R8G8B8A8_UNORM;
		case Format::R8G8B8A8_UNorm_sRGB:   return DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
		case Format::B8G8R8A8_UNorm:        return DXGI_FORMAT_B8G8R8A8_UNORM;
		case Format::B8G8R8A8_UNorm_sRGB:   return DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;
		case Format::R16_Float:             return DXGI_FORMAT_R16_FLOAT;
		case Format::R16G16_Float:          return DXGI_FORMAT_R16G16_FLOAT;
		case Format::R16G16B16A16_Float:    return DXGI_FORMAT_R16G16B16A16_FLOAT;
		case Format::R32_Float:             return DXGI_FORMAT_R32_FLOAT;
		case Format::R32G32_Float:          return DXGI_FORMAT_R32G32_FLOAT;
		case Format::R32G32B32_Float:       return DXGI_FORMAT_R32G32B32_FLOAT;
		case Format::R32G32B32A32_Float:    return DXGI_FORMAT_R32G32B32A32_FLOAT;
		case Format::R16_UInt:              return DXGI_FORMAT_R16_UINT;
		case Format::R32_UInt:              return DXGI_FORMAT_R32_UINT;
		case Format::R32G32B32A32_UInt:     return DXGI_FORMAT_R32G32B32A32_UINT;
		case Format::D24_UNorm_S8_UInt:     return DXGI_FORMAT_D24_UNORM_S8_UINT;
		case Format::D32_Float:             return DXGI_FORMAT_D32_FLOAT;
		case Format::BC1_UNorm:             return DXGI_FORMAT_BC1_UNORM;
		case Format::BC1_UNorm_sRGB:        return DXGI_FORMAT_BC1_UNORM_SRGB;
		case Format::BC2_UNorm:             return DXGI_FORMAT_BC2_UNORM;
		case Format::BC2_UNorm_sRGB:        return DXGI_FORMAT_BC2_UNORM_SRGB;
		case Format::BC3_UNorm:             return DXGI_FORMAT_BC3_UNORM;
		case Format::BC3_UNorm_sRGB:        return DXGI_FORMAT_BC3_UNORM_SRGB;
		case Format::BC4_UNorm:             return DXGI_FORMAT_BC4_UNORM;
		case Format::BC5_UNorm:             return DXGI_FORMAT_BC5_UNORM;
		case Format::BC6H_UF16:             return DXGI_FORMAT_BC6H_UF16;
		case Format::BC7_UNorm:             return DXGI_FORMAT_BC7_UNORM;
		case Format::BC7_UNorm_sRGB:        return DXGI_FORMAT_BC7_UNORM_SRGB;
		default:                            return DXGI_FORMAT_UNKNOWN;
		}
	}

	DXGI_FORMAT DxgiTypelessFormatGet(Format format)
	{
		switch (format) {
		case Format::D24_UNorm_S8_UInt: return DXGI_FORMAT_R24G8_TYPELESS;
		case Format::D32_Float:         return DXGI_FORMAT_R32_TYPELESS;
		default:                        return DxgiFormatGet(format);
		}
	}

	DXGI_FORMAT DxgiDepthSrvFormatGet(Format format)
	{
		switch (format) {
		case Format::D24_UNorm_S8_UInt: return DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
		case Format::D32_Float:         return DXGI_FORMAT_R32_FLOAT;
		default:                        return DxgiFormatGet(format);
		}
	}

	/*
	* 関数名　FormatFromDxgi
	* 引　数　format：DXGI フォーマット
	* 戻り値　RHI フォーマット
	* 説　明　DirectXTex で読み込んだ画像のフォーマットを RHI に変換する
	*/
	Format FormatFromDxgi(DXGI_FORMAT format)
	{
		for (uint32_t i = 1; i < (uint32_t)Format::Count; ++i) {
			if (DxgiFormatGet((Format)i) == format) return (Format)i;
		}
		return Format::Unknown;
	}

	/*
	* 関数名　DxbcReflect
	* 引　数　bytecode / size：DXBC　out_reflection：出力
	* 戻り値　成功なら true
	* 説　明　D3DReflect で定数バッファ（変数のオフセット込み）とバインド位置を取得する
	*/
	bool DxbcReflect(const void* bytecode, size_t size, ShaderReflection& out_reflection)
	{
		Microsoft::WRL::ComPtr<ID3D11ShaderReflection> refl;
		if (FAILED(D3DReflect(bytecode, size, IID_PPV_ARGS(refl.GetAddressOf())))) return false;

		D3D11_SHADER_DESC sdesc = {};
		refl->GetDesc(&sdesc);
		out_reflection = ShaderReflection{};

		for (UINT r = 0; r < sdesc.BoundResources; ++r) {
			D3D11_SHADER_INPUT_BIND_DESC bind = {};
			refl->GetResourceBindingDesc(r, &bind);
			const std::string name = bind.Name ? bind.Name : "";
			if (bind.Type == D3D_SIT_TEXTURE) out_reflection.textures.push_back({ name, bind.BindPoint });
			else if (bind.Type == D3D_SIT_SAMPLER) out_reflection.samplers.push_back({ name, bind.BindPoint });
			else if (bind.Type == D3D_SIT_CBUFFER) {
				ShaderConstantBuffer cb;
				cb.name = name;
				cb.i_Slot = bind.BindPoint;
				ID3D11ShaderReflectionConstantBuffer* cbr = refl->GetConstantBufferByName(bind.Name);
				D3D11_SHADER_BUFFER_DESC cbdesc = {};
				if (cbr && SUCCEEDED(cbr->GetDesc(&cbdesc))) {
					cb.i_Size = (cbdesc.Size + 15) & ~15u;
					for (UINT v = 0; v < cbdesc.Variables; ++v) {
						D3D11_SHADER_VARIABLE_DESC vdesc = {};
						cbr->GetVariableByIndex(v)->GetDesc(&vdesc);
						cb.variables.push_back({ vdesc.Name ? vdesc.Name : "", vdesc.StartOffset, vdesc.Size });
					}
				}
				out_reflection.constantBuffers.push_back(std::move(cb));
			}
		}

		for (UINT i = 0; i < sdesc.InputParameters; ++i) {
			D3D11_SIGNATURE_PARAMETER_DESC p = {};
			refl->GetInputParameterDesc(i, &p);
			out_reflection.inputs.push_back({ p.SemanticName ? p.SemanticName : "", p.SemanticIndex, p.Register });
		}
		for (UINT i = 0; i < sdesc.OutputParameters; ++i) {
			D3D11_SIGNATURE_PARAMETER_DESC p = {};
			refl->GetOutputParameterDesc(i, &p);
			out_reflection.outputs.push_back({ p.SemanticName ? p.SemanticName : "", p.SemanticIndex, p.Register });
		}
		return true;
	}
}
