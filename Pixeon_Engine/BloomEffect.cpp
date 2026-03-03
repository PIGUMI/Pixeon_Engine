// BloomEffect.cpp
#include "BloomEffect.h"
#include "ImageUtils.h"
#include "System.h"
#include "GUI.h"
#include "ShaderManager.h"
#include <wrl/client.h>

using Microsoft::WRL::ComPtr;

namespace {
	struct V {
		float p[3];
		float uv[2];
	};

	static ComPtr<ID3D11Buffer> s_vb;
	static ComPtr<ID3D11InputLayout> s_layout;
	static ComPtr<ID3D11Buffer> s_cbVS;
	static ComPtr<ID3D11Buffer> s_cbPS;
	static ComPtr<ID3D11DepthStencilState> s_dsOff;
	static ComPtr<ID3D11BlendState> s_blendAlpha;
	static ComPtr<ID3D11BlendState> s_blendAdd;
}

void BloomEffect::Apply(ID3D11ShaderResourceView* input,
	ID3D11RenderTargetView* output,
	int width, int height) {
	if (!input || !output) return;

	auto* dx = DirectX11::GetInstance();
	if (!dx) return;

	ID3D11DeviceContext* ctx = dx->GetContext();
	ID3D11Device* dev = dx->GetDevice();
	if (!ctx || !dev) return;

	ShaderManager* sm = ShaderManager::GetInstance();

	static ID3D11VertexShader* vs = nullptr;
	static ID3D11PixelShader* ps = nullptr;

	if (!vs) {
		vs = sm->GetVertexShader("VS_Bloom");
		if (!vs) { sm->UpdateAndCompileShaders(); vs = sm->GetVertexShader("VS_Bloom"); }
	}
	if (!ps) {
		ps = sm->GetPixelShader("PS_Bloom");
		if (!ps) { sm->UpdateAndCompileShaders(); ps = sm->GetPixelShader("PS_Bloom"); }
	}

	if (!vs || !ps) {

		ID3D11RenderTargetView* oldRTV = nullptr;
		ID3D11DepthStencilView* oldDSV = nullptr;
		ctx->OMGetRenderTargets(1, &oldRTV, &oldDSV);

		ctx->OMSetRenderTargets(1, &output, nullptr);
		ImageUtils::DrawSRV(input, 0.0f, 0.0f, (float)width, (float)height);

		ctx->OMSetRenderTargets(1, oldRTV ? &oldRTV : nullptr, oldDSV);
		if (oldRTV) oldRTV->Release();
		if (oldDSV) oldDSV->Release();
		return;
	}

	InitializeResources(dev, vs, sm);

	CreateTempBuffer(width, height);
	if (!tempRTV_ || !tempSRV_) return;

	ID3D11RenderTargetView* oldRTV = nullptr;
	ID3D11DepthStencilView* oldDSV = nullptr;
	ctx->OMGetRenderTargets(1, &oldRTV, &oldDSV);

	ID3D11BlendState* oldBlendState = nullptr;
	FLOAT oldBlendFactor[4] = { 0, 0, 0, 0 };
	UINT oldSampleMask = 0;
	ctx->OMGetBlendState(&oldBlendState, oldBlendFactor, &oldSampleMask);

	ID3D11DepthStencilState* oldDSS = nullptr;
	UINT oldStencilRef = 0;
	ctx->OMGetDepthStencilState(&oldDSS, &oldStencilRef);

	D3D11_VIEWPORT vp = {};
	vp.Width = (FLOAT)width;
	vp.Height = (FLOAT)height;
	vp.MinDepth = 0.0f;
	vp.MaxDepth = 1.0f;
	vp.TopLeftX = 0;
	vp.TopLeftY = 0;

	ctx->OMSetRenderTargets(1, &tempRTV_, nullptr);
	ctx->RSSetViewports(1, &vp);
	ctx->OMSetDepthStencilState(s_dsOff.Get(), 0);

	float clearColor[4] = { 0, 0, 0, 0 };
	ctx->ClearRenderTargetView(tempRTV_, clearColor);

	struct BloomParams {
		float threshold;
		float intensity;
		float blurRadius;
		float screenWidth;
		float screenHeight;
		int passType;
		float pad[2];
	};

	BloomParams params;
	params.threshold = threshold;
	params.intensity = intensity;
	params.blurRadius = blurSize;
	params.screenWidth = (float)width;
	params.screenHeight = (float)height;
	params.passType = 1;

	ctx->UpdateSubresource(s_cbPS.Get(), 0, nullptr, &params, 0, 0);

	DrawQuad(ctx, vs, ps, input, 0.0f, 0.0f, (float)width, (float)height, width, height);

	ID3D11ShaderResourceView* nullSRV[1] = { nullptr };
	ctx->PSSetShaderResources(0, 1, nullSRV);

	ctx->OMSetRenderTargets(1, &output, nullptr);
	ctx->RSSetViewports(1, &vp);
	ctx->OMSetDepthStencilState(s_dsOff.Get(), 0);

	ctx->OMSetBlendState(s_blendAlpha.Get(), oldBlendFactor, 0xFFFFFFFF);
	ImageUtils::DrawSRV(input, 0.0f, 0.0f, (float)width, (float)height,
		DirectX::XMFLOAT4(1, 1, 1, 1),
		DirectX::XMFLOAT4(0, 0, 1, 1),
		true, 1.0f);

	ctx->OMSetBlendState(s_blendAdd.Get(), oldBlendFactor, 0xFFFFFFFF);

	params.passType = 2;
	ctx->UpdateSubresource(s_cbPS.Get(), 0, nullptr, &params, 0, 0);

	DrawQuad(ctx, vs, ps, tempSRV_, 0.0f, 0.0f, (float)width, (float)height,
		width, height, tint, intensity);

	ctx->PSSetShaderResources(0, 1, nullSRV);

	ctx->OMSetDepthStencilState(oldDSS, oldStencilRef);
	if (oldDSS) oldDSS->Release();

	ctx->OMSetBlendState(oldBlendState, oldBlendFactor, oldSampleMask);
	if (oldBlendState) oldBlendState->Release();

	ctx->OMSetRenderTargets(1, oldRTV ? &oldRTV : nullptr, oldDSV);
	if (oldRTV) oldRTV->Release();
	if (oldDSV) oldDSV->Release();
}

void BloomEffect::InitializeResources(ID3D11Device* dev, ID3D11VertexShader* vs, ShaderManager* sm) {
	if (!s_vb) {
		D3D11_BUFFER_DESC bd{};
		bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
		bd.ByteWidth = sizeof(V) * 6;
		bd.Usage = D3D11_USAGE_DYNAMIC;
		bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
		dev->CreateBuffer(&bd, nullptr, s_vb.GetAddressOf());
	}

	if (!s_layout) {
		const void* bc = nullptr;
		size_t bcSize = 0;
		if (sm->GetVSBytecode("VS_Bloom", &bc, &bcSize)) {
			D3D11_INPUT_ELEMENT_DESC desc[] = {
				{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
				{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
			};
			dev->CreateInputLayout(desc, _countof(desc), bc, bcSize, s_layout.GetAddressOf());
		}
	}

	if (!s_cbVS) {
		struct CBVS { DirectX::XMMATRIX View; DirectX::XMMATRIX Proj; DirectX::XMFLOAT4 Color; int mode2D; float pad[3]; };
		D3D11_BUFFER_DESC cbd{};
		cbd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
		cbd.ByteWidth = sizeof(CBVS);
		cbd.Usage = D3D11_USAGE_DEFAULT;
		dev->CreateBuffer(&cbd, nullptr, s_cbVS.GetAddressOf());
	}

	if (!s_cbPS) {
		struct CBPS { float threshold; float intensity; float blurRadius; float screenWidth; float screenHeight; int passType; float pad[2]; };
		D3D11_BUFFER_DESC cbd{};
		cbd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
		cbd.ByteWidth = sizeof(CBPS);
		cbd.Usage = D3D11_USAGE_DEFAULT;
		dev->CreateBuffer(&cbd, nullptr, s_cbPS.GetAddressOf());
	}

	if (!s_dsOff) {
		D3D11_DEPTH_STENCIL_DESC dsdesc{};
		dsdesc.DepthEnable = FALSE;
		dsdesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
		dsdesc.StencilEnable = FALSE;
		dev->CreateDepthStencilState(&dsdesc, s_dsOff.GetAddressOf());
	}

	if (!s_blendAlpha) {
		D3D11_BLEND_DESC bdesc{};
		bdesc.RenderTarget[0].BlendEnable = TRUE;
		bdesc.RenderTarget[0].SrcBlend = D3D11_BLEND_ONE;
		bdesc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
		bdesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
		bdesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
		bdesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
		bdesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
		bdesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
		dev->CreateBlendState(&bdesc, s_blendAlpha.GetAddressOf());
	}

	if (!s_blendAdd) {
		D3D11_BLEND_DESC bdesc{};
		bdesc.RenderTarget[0].BlendEnable = TRUE;
		bdesc.RenderTarget[0].SrcBlend = D3D11_BLEND_ONE;
		bdesc.RenderTarget[0].DestBlend = D3D11_BLEND_ONE;
		bdesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
		bdesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
		bdesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ONE;
		bdesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
		bdesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
		dev->CreateBlendState(&bdesc, s_blendAdd.GetAddressOf());
	}
}

void BloomEffect::DrawQuad(ID3D11DeviceContext* ctx, ID3D11VertexShader* vs, ID3D11PixelShader* ps,
	ID3D11ShaderResourceView* srv, float x, float y, float width, float height,
	int screenWidth, int screenHeight,
	const DirectX::XMFLOAT4& color, float opacity) {
	auto toNDC = [&](float px, float py)->DirectX::XMFLOAT3 {
		float ndcX = (px / screenWidth) * 2.0f - 1.0f;
		float ndcY = 1.0f - (py / screenHeight) * 2.0f;
		return DirectX::XMFLOAT3(ndcX, ndcY, 0.0f);
		};

	DirectX::XMFLOAT3 tl = toNDC(x, y);
	DirectX::XMFLOAT3 tr = toNDC(x + width, y);
	DirectX::XMFLOAT3 br = toNDC(x + width, y + height);
	DirectX::XMFLOAT3 bl = toNDC(x, y + height);

	V verts[6];
	verts[0] = { {tl.x, tl.y, tl.z}, {0, 0} };
	verts[1] = { {tr.x, tr.y, tr.z}, {1, 0} };
	verts[2] = { {br.x, br.y, br.z}, {1, 1} };
	verts[3] = { {tl.x, tl.y, tl.z}, {0, 0} };
	verts[4] = { {br.x, br.y, br.z}, {1, 1} };
	verts[5] = { {bl.x, bl.y, bl.z}, {0, 1} };

	D3D11_MAPPED_SUBRESOURCE mp{};
	if (SUCCEEDED(ctx->Map(s_vb.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mp))) {
		memcpy(mp.pData, verts, sizeof(verts));
		ctx->Unmap(s_vb.Get(), 0);
	}

	struct CBVS { DirectX::XMMATRIX View; DirectX::XMMATRIX Proj; DirectX::XMFLOAT4 Color; int mode2D; float pad[3]; };
	CBVS cb{};
	cb.View = DirectX::XMMatrixIdentity();
	cb.Proj = DirectX::XMMatrixIdentity();
	cb.Color = color;
	cb.Color.w *= opacity;
	cb.mode2D = 1;
	ctx->UpdateSubresource(s_cbVS.Get(), 0, nullptr, &cb, 0, 0);

	UINT stride = sizeof(V);
	UINT offset = 0;
	ID3D11Buffer* vbptr = s_vb.Get();
	ctx->IASetVertexBuffers(0, 1, &vbptr, &stride, &offset);
	ctx->IASetInputLayout(s_layout.Get());
	ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	ctx->VSSetShader(vs, nullptr, 0);
	ctx->PSSetShader(ps, nullptr, 0);

	ID3D11Buffer* cbs[] = { s_cbVS.Get() };
	ctx->VSSetConstantBuffers(0, 1, cbs);

	ID3D11Buffer* cbsPS[] = { s_cbPS.Get() };
	ctx->PSSetConstantBuffers(1, 1, cbsPS);

	ctx->PSSetShaderResources(0, 1, &srv);

	auto* dx = DirectX11::GetInstance();
	dx->SetSamplerState(SAMPLER_LINEAR);

	ctx->Draw(6, 0);
}

void BloomEffect::DrawInspector() {
	ImGui::DragFloat(GUI::GetInstance()->ShiftJISToUTF8("輝度閾値").c_str(),
		&threshold, 0.01f, 0.0f, 2.0f);
	ImGui::DragFloat(GUI::GetInstance()->ShiftJISToUTF8("強度").c_str(),
		&intensity, 0.01f, 0.0f, 5.0f);
	ImGui::DragFloat(GUI::GetInstance()->ShiftJISToUTF8("ブラーサイズ").c_str(),
		&blurSize, 0.1f, 0.5f, 10.0f);
	ImGui::ColorEdit4(GUI::GetInstance()->ShiftJISToUTF8("色調整").c_str(), &tint.x);
}

void BloomEffect::SaveToJson(nlohmann::json& j) const {
	j["threshold"] = threshold;
	j["intensity"] = intensity;
	j["blurSize"] = blurSize;
	j["tint"] = { tint.x, tint.y, tint.z, tint.w };
}

void BloomEffect::LoadFromJson(const nlohmann::json& j) {
	threshold = j.value("threshold", 0.8f);
	intensity = j.value("intensity", 0.5f);
	blurSize = j.value("blurSize", 2.0f);

	if (j.contains("tint") && j["tint"].is_array() && j["tint"].size() == 4) {
		tint.x = j["tint"][0];
		tint.y = j["tint"][1];
		tint.z = j["tint"][2];
		tint.w = j["tint"][3];
	}
}

PostEffectBase* BloomEffect::Clone() const {
	BloomEffect* clone = new BloomEffect();
	clone->threshold = threshold;
	clone->intensity = intensity;
	clone->blurSize = blurSize;
	clone->tint = tint;
	clone->enabled = enabled;
	clone->priority = priority;
	return clone;
}

void BloomEffect::CreateTempBuffer(int width, int height) {
	auto* dx = DirectX11::GetInstance();
	if (!dx) return;

	ID3D11Device* device = dx->GetDevice();
	if (!device) return;

	if (tempTexture_ && bufferWidth_ == width && bufferHeight_ == height) {
		return;
	}

	ReleaseTempBuffer();

	bufferWidth_ = width;
	bufferHeight_ = height;

	D3D11_TEXTURE2D_DESC texDesc = {};
	texDesc.Width = width;
	texDesc.Height = height;
	texDesc.MipLevels = 1;
	texDesc.ArraySize = 1;
	texDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
	texDesc.SampleDesc.Count = 1;
	texDesc.SampleDesc.Quality = 0;
	texDesc.Usage = D3D11_USAGE_DEFAULT;
	texDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
	texDesc.CPUAccessFlags = 0;

	HRESULT hr = device->CreateTexture2D(&texDesc, nullptr, &tempTexture_);
	if (SUCCEEDED(hr)) {
		device->CreateRenderTargetView(tempTexture_, nullptr, &tempRTV_);
		device->CreateShaderResourceView(tempTexture_, nullptr, &tempSRV_);
	}
}

void BloomEffect::ReleaseTempBuffer() {
	if (tempSRV_) { tempSRV_->Release(); tempSRV_ = nullptr; }
	if (tempRTV_) { tempRTV_->Release(); tempRTV_ = nullptr; }
	if (tempTexture_) { tempTexture_->Release(); tempTexture_ = nullptr; }

	bufferWidth_ = 0;
	bufferHeight_ = 0;
}