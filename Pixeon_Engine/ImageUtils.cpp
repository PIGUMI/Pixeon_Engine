#include "ImageUtils.h"
#include "System.h"
#include "ShaderManager.h"
#include <vector>
#include <wrl/client.h>

using Microsoft::WRL::ComPtr;

namespace ImageUtils {
	struct V {
		float p[3];
		float uv[2];
	};

	void DrawSRV(ID3D11ShaderResourceView* srv,
		float x, float y, float width, float height,
		const DirectX::XMFLOAT4& color,
		const DirectX::XMFLOAT4& uvRect,
		bool premultipliedAlpha,
		float opacity)
	{
		if (!srv) return;
		auto dx = DirectX11::GetInstance();
		if (!dx) return;
		ID3D11Device* dev = dx->GetDevice();
		ID3D11DeviceContext* ctx = dx->GetContext();
		if (!dev || !ctx) return;

		ShaderManager* sm = ShaderManager::GetInstance();

		// 静的リソース（一度作成）
		static ComPtr<ID3D11Buffer> s_vb;
		static ComPtr<ID3D11InputLayout> s_layout;
		static ComPtr<ID3D11Buffer> s_cb;
		static ComPtr<ID3D11DepthStencilState> s_dsOff;
		static ComPtr<ID3D11BlendState> s_premultBlend;
		static ID3D11VertexShader* s_vs = nullptr;
		static ID3D11PixelShader* s_ps = nullptr;

		// VS/PS
		if (!s_vs) {
			s_vs = sm->GetVertexShader("VS_ImgQuad");
			if (!s_vs) { sm->UpdateAndCompileShaders(); s_vs = sm->GetVertexShader("VS_ImgQuad"); }
		}
		if (!s_ps) {
			s_ps = sm->GetPixelShader("PS_ImgQuad");
			if (!s_ps) { sm->UpdateAndCompileShaders(); s_ps = sm->GetPixelShader("PS_ImgQuad"); }
		}
		if (!s_vs || !s_ps) return;

		if (!s_vb) {
			D3D11_BUFFER_DESC bd{};
			bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
			bd.ByteWidth = sizeof(V) * 6;
			bd.Usage = D3D11_USAGE_DYNAMIC;
			bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
			if (FAILED(dev->CreateBuffer(&bd, nullptr, s_vb.GetAddressOf()))) return;
		}

		if (!s_layout) {
			const void* bc = nullptr; size_t bcSize = 0;
			if (!sm->GetVSBytecode("VS_ImgQuad", &bc, &bcSize)) return;
			D3D11_INPUT_ELEMENT_DESC desc[] = {
				{ "POSITION",0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,  D3D11_INPUT_PER_VERTEX_DATA,0 },
				{ "TEXCOORD",0, DXGI_FORMAT_R32G32_FLOAT,    0, 12, D3D11_INPUT_PER_VERTEX_DATA,0 },
			};
			if (FAILED(dev->CreateInputLayout(desc, _countof(desc), bc, bcSize, s_layout.GetAddressOf()))) return;
		}

		if (!s_cb) {
			struct CBVS { DirectX::XMMATRIX View; DirectX::XMMATRIX Proj; DirectX::XMFLOAT4 Color; int mode2D; float pad[3]; };
			D3D11_BUFFER_DESC cbd{};
			cbd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
			cbd.ByteWidth = (UINT)sizeof(CBVS);
			cbd.Usage = D3D11_USAGE_DEFAULT;
			if (FAILED(dev->CreateBuffer(&cbd, nullptr, s_cb.GetAddressOf()))) return;
		}

		if (!s_dsOff) {
			D3D11_DEPTH_STENCIL_DESC dsdesc{};
			dsdesc.DepthEnable = FALSE;
			dsdesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
			dsdesc.StencilEnable = FALSE;
			dev->CreateDepthStencilState(&dsdesc, s_dsOff.GetAddressOf());
		}

		if (!s_premultBlend) {
			D3D11_BLEND_DESC bdesc{};
			bdesc.RenderTarget[0].BlendEnable = TRUE;
			bdesc.RenderTarget[0].SrcBlend = D3D11_BLEND_ONE;
			bdesc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
			bdesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
			bdesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
			bdesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
			bdesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
			bdesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
			dev->CreateBlendState(&bdesc, s_premultBlend.GetAddressOf());
		}

		RenderTarget* rt = dx->GetDefaultRTV();
		if (!rt) return;
		float W = (float)rt->GetWidth();
		float H = (float)rt->GetHeight();

		auto toNDC = [&](float px, float py)->DirectX::XMFLOAT3 {
			float ndcX = (px / W) * 2.0f - 1.0f;
			float ndcY = 1.0f - (py / H) * 2.0f;
			return DirectX::XMFLOAT3(ndcX, ndcY, 0.0f);
			};

		float x0 = x;
		float y0 = y;
		float x1 = x + width;
		float y1 = y + height;

		DirectX::XMFLOAT3 tl = toNDC(x0, y0);
		DirectX::XMFLOAT3 tr = toNDC(x1, y0);
		DirectX::XMFLOAT3 br = toNDC(x1, y1);
		DirectX::XMFLOAT3 bl = toNDC(x0, y1);

		V verts[6];

		verts[0].p[0] = tl.x; verts[0].p[1] = tl.y; verts[0].p[2] = tl.z; verts[0].uv[0] = uvRect.x; verts[0].uv[1] = uvRect.y;
		verts[1].p[0] = tr.x; verts[1].p[1] = tr.y; verts[1].p[2] = tr.z; verts[1].uv[0] = uvRect.z; verts[1].uv[1] = uvRect.y;
		verts[2].p[0] = br.x; verts[2].p[1] = br.y; verts[2].p[2] = br.z; verts[2].uv[0] = uvRect.z; verts[2].uv[1] = uvRect.w;

		verts[3].p[0] = tl.x; verts[3].p[1] = tl.y; verts[3].p[2] = tl.z; verts[3].uv[0] = uvRect.x; verts[3].uv[1] = uvRect.y;
		verts[4].p[0] = br.x; verts[4].p[1] = br.y; verts[4].p[2] = br.z; verts[4].uv[0] = uvRect.z; verts[4].uv[1] = uvRect.w;
		verts[5].p[0] = bl.x; verts[5].p[1] = bl.y; verts[5].p[2] = bl.z; verts[5].uv[0] = uvRect.x; verts[5].uv[1] = uvRect.w;

		ID3D11BlendState* oldBlend = nullptr;
		FLOAT oldBlendFactor[4] = { 0,0,0,0 };
		UINT oldSampleMask = 0;
		ctx->OMGetBlendState(&oldBlend, oldBlendFactor, &oldSampleMask);

		ID3D11DepthStencilState* oldDSS = nullptr;
		UINT oldStencilRef = 0;
		ctx->OMGetDepthStencilState(&oldDSS, &oldStencilRef);

		D3D11_MAPPED_SUBRESOURCE mp{};
		if (SUCCEEDED(ctx->Map(s_vb.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mp))) {
			memcpy(mp.pData, verts, sizeof(verts));
			ctx->Unmap(s_vb.Get(), 0);
		}

		{
			struct CBVS { DirectX::XMMATRIX View; DirectX::XMMATRIX Proj; DirectX::XMFLOAT4 Color; int mode2D; float pad[3]; };
			CBVS cb{};
			cb.View = DirectX::XMMatrixIdentity();
			cb.Proj = DirectX::XMMatrixIdentity();
			cb.Color = color;
			cb.Color.w *= opacity;
			cb.mode2D = 1;
			ctx->UpdateSubresource(s_cb.Get(), 0, nullptr, &cb, 0, 0);
		}

		UINT stride = sizeof(V);
		UINT offset = 0;
		ID3D11Buffer* vbptr = s_vb.Get();
		ctx->IASetVertexBuffers(0, 1, &vbptr, &stride, &offset);
		ctx->IASetInputLayout(s_layout.Get());
		ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		ctx->VSSetShader(s_vs, nullptr, 0);
		ctx->PSSetShader(s_ps, nullptr, 0);
		ID3D11Buffer* cbs[] = { s_cb.Get() };
		ctx->VSSetConstantBuffers(0, 1, cbs);
		ctx->PSSetConstantBuffers(0, 1, cbs);

		ctx->PSSetShaderResources(0, 1, &srv);

		dx->SetSamplerState(SAMPLER_LINEAR);

		if (premultipliedAlpha && s_premultBlend) {
			FLOAT bf[4] = { 0,0,0,0 };
			ctx->OMSetBlendState(s_premultBlend.Get(), bf, 0xFFFFFFFF);
		}
		else {
			dx->SetBlendMode(BLEND_ALPHA);
		}
		ctx->OMSetDepthStencilState(s_dsOff.Get(), 0);

		ctx->Draw(6, 0);

		ID3D11ShaderResourceView* nullSRV[1] = { nullptr };
		ctx->PSSetShaderResources(0, 1, nullSRV);

		ctx->OMSetDepthStencilState(oldDSS, oldStencilRef);
		if (oldDSS) oldDSS->Release();

		ctx->OMSetBlendState(oldBlend, oldBlendFactor, oldSampleMask);
		if (oldBlend) oldBlend->Release();
	}

	// ピクセル化エフェクト付き描画
	void DrawSRVPixelated(ID3D11ShaderResourceView* srv,
		float x, float y, float width, float height,
		float pixelSize,
		float intensity,
		const DirectX::XMFLOAT4& color,
		const DirectX::XMFLOAT4& uvRect)
	{
		if (!srv) return;
		auto dx = DirectX11::GetInstance();
		if (!dx) return;
		ID3D11Device* dev = dx->GetDevice();
		ID3D11DeviceContext* ctx = dx->GetContext();
		if (!dev || !ctx) return;

		ShaderManager* sm = ShaderManager::GetInstance();

		// 静的リソース
		static ComPtr<ID3D11Buffer> s_vb;
		static ComPtr<ID3D11InputLayout> s_layout;
		static ComPtr<ID3D11Buffer> s_cbVS;
		static ComPtr<ID3D11Buffer> s_cbPS;
		static ComPtr<ID3D11DepthStencilState> s_dsOff;
		static ComPtr<ID3D11BlendState> s_premultBlend;
		static ID3D11VertexShader* s_vs = nullptr;
		static ID3D11PixelShader* s_ps = nullptr;

		// VS/PS
		if (!s_vs) {
			s_vs = sm->GetVertexShader("VS_PixelatedEffext");
			if (!s_vs) { sm->UpdateAndCompileShaders(); s_vs = sm->GetVertexShader("VS_PixelatedEffext"); }
		}
		if (!s_ps) {
			s_ps = sm->GetPixelShader("PS_PixelatedEffext");
			if (!s_ps) { sm->UpdateAndCompileShaders(); s_ps = sm->GetPixelShader("PS_PixelatedEffext"); }
		}
		if (!s_vs || !s_ps) return;

		if (!s_vb) {
			D3D11_BUFFER_DESC bd{};
			bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
			bd.ByteWidth = sizeof(V) * 6;
			bd.Usage = D3D11_USAGE_DYNAMIC;
			bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
			if (FAILED(dev->CreateBuffer(&bd, nullptr, s_vb.GetAddressOf()))) return;
		}

		if (!s_layout) {
			const void* bc = nullptr; size_t bcSize = 0;
			if (!sm->GetVSBytecode("VS_PixelatedEffext", &bc, &bcSize)) return;
			D3D11_INPUT_ELEMENT_DESC desc[] = {
				{ "POSITION",0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,  D3D11_INPUT_PER_VERTEX_DATA,0 },
				{ "TEXCOORD",0, DXGI_FORMAT_R32G32_FLOAT,    0, 12, D3D11_INPUT_PER_VERTEX_DATA,0 },
			};
			if (FAILED(dev->CreateInputLayout(desc, _countof(desc), bc, bcSize, s_layout.GetAddressOf()))) return;
		}

		if (!s_cbVS) {
			struct CBVS { DirectX::XMMATRIX View; DirectX::XMMATRIX Proj; DirectX::XMFLOAT4 Color; int mode2D; float pad[3]; };
			D3D11_BUFFER_DESC cbd{};
			cbd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
			cbd.ByteWidth = (UINT)sizeof(CBVS);
			cbd.Usage = D3D11_USAGE_DEFAULT;
			if (FAILED(dev->CreateBuffer(&cbd, nullptr, s_cbVS.GetAddressOf()))) return;
		}

		if (!s_cbPS) {
			struct CBPS { float pixelSize; float screenWidth; float screenHeight; float intensity; };
			D3D11_BUFFER_DESC cbd{};
			cbd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
			cbd.ByteWidth = (UINT)sizeof(CBPS);
			cbd.Usage = D3D11_USAGE_DEFAULT;
			if (FAILED(dev->CreateBuffer(&cbd, nullptr, s_cbPS.GetAddressOf()))) return;
		}

		if (!s_dsOff) {
			D3D11_DEPTH_STENCIL_DESC dsdesc{};
			dsdesc.DepthEnable = FALSE;
			dsdesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
			dsdesc.StencilEnable = FALSE;
			dev->CreateDepthStencilState(&dsdesc, s_dsOff.GetAddressOf());
		}

		if (!s_premultBlend) {
			D3D11_BLEND_DESC bdesc{};
			bdesc.RenderTarget[0].BlendEnable = TRUE;
			bdesc.RenderTarget[0].SrcBlend = D3D11_BLEND_ONE;
			bdesc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
			bdesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
			bdesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
			bdesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
			bdesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
			bdesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
			dev->CreateBlendState(&bdesc, s_premultBlend.GetAddressOf());
		}

		RenderTarget* rt = dx->GetDefaultRTV();
		if (!rt) return;
		float W = (float)rt->GetWidth();
		float H = (float)rt->GetHeight();

		auto toNDC = [&](float px, float py)->DirectX::XMFLOAT3 {
			float ndcX = (px / W) * 2.0f - 1.0f;
			float ndcY = 1.0f - (py / H) * 2.0f;
			return DirectX::XMFLOAT3(ndcX, ndcY, 0.0f);
			};

		float x0 = x;
		float y0 = y;
		float x1 = x + width;
		float y1 = y + height;

		DirectX::XMFLOAT3 tl = toNDC(x0, y0);
		DirectX::XMFLOAT3 tr = toNDC(x1, y0);
		DirectX::XMFLOAT3 br = toNDC(x1, y1);
		DirectX::XMFLOAT3 bl = toNDC(x0, y1);

		V verts[6];

		verts[0].p[0] = tl.x; verts[0].p[1] = tl.y; verts[0].p[2] = tl.z; verts[0].uv[0] = uvRect.x; verts[0].uv[1] = uvRect.y;
		verts[1].p[0] = tr.x; verts[1].p[1] = tr.y; verts[1].p[2] = tr.z; verts[1].uv[0] = uvRect.z; verts[1].uv[1] = uvRect.y;
		verts[2].p[0] = br.x; verts[2].p[1] = br.y; verts[2].p[2] = br.z; verts[2].uv[0] = uvRect.z; verts[2].uv[1] = uvRect.w;

		verts[3].p[0] = tl.x; verts[3].p[1] = tl.y; verts[3].p[2] = tl.z; verts[3].uv[0] = uvRect.x; verts[3].uv[1] = uvRect.y;
		verts[4].p[0] = br.x; verts[4].p[1] = br.y; verts[4].p[2] = br.z; verts[4].uv[0] = uvRect.z; verts[4].uv[1] = uvRect.w;
		verts[5].p[0] = bl.x; verts[5].p[1] = bl.y; verts[5].p[2] = bl.z; verts[5].uv[0] = uvRect.x; verts[5].uv[1] = uvRect.w;

		ID3D11BlendState* oldBlend = nullptr;
		FLOAT oldBlendFactor[4] = { 0,0,0,0 };
		UINT oldSampleMask = 0;
		ctx->OMGetBlendState(&oldBlend, oldBlendFactor, &oldSampleMask);

		ID3D11DepthStencilState* oldDSS = nullptr;
		UINT oldStencilRef = 0;
		ctx->OMGetDepthStencilState(&oldDSS, &oldStencilRef);

		D3D11_MAPPED_SUBRESOURCE mp{};
		if (SUCCEEDED(ctx->Map(s_vb.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mp))) {
			memcpy(mp.pData, verts, sizeof(verts));
			ctx->Unmap(s_vb.Get(), 0);
		}

		// VS定数バッファ更新
		{
			struct CBVS { DirectX::XMMATRIX View; DirectX::XMMATRIX Proj; DirectX::XMFLOAT4 Color; int mode2D; float pad[3]; };
			CBVS cb{};
			cb.View = DirectX::XMMatrixIdentity();
			cb.Proj = DirectX::XMMatrixIdentity();
			cb.Color = color;
			cb.mode2D = 1;
			ctx->UpdateSubresource(s_cbVS.Get(), 0, nullptr, &cb, 0, 0);
		}

		// PS定数バッファ更新
		{
			struct CBPS { float pixelSize; float screenWidth; float screenHeight; float intensity; };
			CBPS cbps{};
			cbps.pixelSize = pixelSize;
			cbps.screenWidth = W;
			cbps.screenHeight = H;
			cbps.intensity = intensity;
			ctx->UpdateSubresource(s_cbPS.Get(), 0, nullptr, &cbps, 0, 0);
		}

		UINT stride = sizeof(V);
		UINT offset = 0;
		ID3D11Buffer* vbptr = s_vb.Get();
		ctx->IASetVertexBuffers(0, 1, &vbptr, &stride, &offset);
		ctx->IASetInputLayout(s_layout.Get());
		ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		ctx->VSSetShader(s_vs, nullptr, 0);
		ctx->PSSetShader(s_ps, nullptr, 0);

		ID3D11Buffer* cbsVS[] = { s_cbVS.Get() };
		ctx->VSSetConstantBuffers(0, 1, cbsVS);

		ID3D11Buffer* cbsPS[] = { s_cbPS.Get() };
		ctx->PSSetConstantBuffers(1, 1, cbsPS);

		ctx->PSSetShaderResources(0, 1, &srv);

		dx->SetSamplerState(SAMPLER_LINEAR);

		FLOAT bf[4] = { 0,0,0,0 };
		ctx->OMSetBlendState(s_premultBlend.Get(), bf, 0xFFFFFFFF);
		ctx->OMSetDepthStencilState(s_dsOff.Get(), 0);

		ctx->Draw(6, 0);

		ID3D11ShaderResourceView* nullSRV[1] = { nullptr };
		ctx->PSSetShaderResources(0, 1, nullSRV);

		ctx->OMSetDepthStencilState(oldDSS, oldStencilRef);
		if (oldDSS) oldDSS->Release();

		ctx->OMSetBlendState(oldBlend, oldBlendFactor, oldSampleMask);
		if (oldBlend) oldBlend->Release();
	}
}