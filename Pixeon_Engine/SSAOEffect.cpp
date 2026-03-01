// SSAOEffect.cpp
#include "SSAOEffect.h"
#include "System.h"
#include "ShaderManager.h"
#include "GUI.h"
#include "IMGUI/imgui.h"
#include "ImageUtils.h"
#include <random>

using namespace DirectX;
using Microsoft::WRL::ComPtr;

SSAOEffect::SSAOEffect()
{
	EnsureKernel();
	EnsureNoise();
}

PostEffectBase* SSAOEffect::Clone() const
{
	SSAOEffect* c = new SSAOEffect();
	c->radius = radius;
	c->bias = bias;
	c->power = power;
	c->aoStrength = aoStrength;
	c->enabled = enabled;
	c->priority = priority;
	return c;
}

void SSAOEffect::Apply(
	ID3D11ShaderResourceView* input,
	ID3D11RenderTargetView* output,
	int width, int height)
{
	auto* dx = DirectX11::GetInstance();
	auto* ctx = dx->GetContext();
	auto* sm = ShaderManager::GetInstance();

	if (aoGenerated_) {
		aoGenerated_ = false; depthSRV_ = nullptr; normalSRV_ = nullptr;
		if (input && output) {
			ComPtr<ID3D11RenderTargetView> r; ComPtr<ID3D11DepthStencilView> d;
			ctx->OMGetRenderTargets(1, r.GetAddressOf(), d.GetAddressOf());
			ctx->OMSetRenderTargets(1, &output, nullptr);
			D3D11_VIEWPORT v{}; v.Width = (float)width; v.Height = (float)height; v.MaxDepth = 1.0f;
			ctx->RSSetViewports(1, &v);
			ImageUtils::DrawSRV(input, 0, 0, (float)width, (float)height,
				XMFLOAT4(1, 1, 1, 1), XMFLOAT4(0, 0, 1, 1), true, 1.0f);
			ctx->OMSetRenderTargets(1, r.GetAddressOf(), d.Get());
		}
		return;
	}

	ID3D11VertexShader* fullscreenVS = sm->GetVertexShader("VS_Fullscreen");
	ID3D11PixelShader* ssaoPS = sm->GetPixelShader("PS_SSAO");
	ID3D11PixelShader* blurPS = sm->GetPixelShader("PS_SSAOBlur");
	ID3D11PixelShader* compositePS = sm->GetPixelShader("PS_SSAOComposite");

	if (!depthSRV_ || !fullscreenVS || !ssaoPS || !blurPS || !compositePS)
	{
		if (input && output)
		{
			ComPtr<ID3D11RenderTargetView> oldRTV;
			ComPtr<ID3D11DepthStencilView> oldDSV;
			ctx->OMGetRenderTargets(1, oldRTV.GetAddressOf(), oldDSV.GetAddressOf());
			ctx->OMSetRenderTargets(1, &output, nullptr);
			D3D11_VIEWPORT vp{};
			vp.Width = (float)width; vp.Height = (float)height; vp.MaxDepth = 1.0f;
			ctx->RSSetViewports(1, &vp);
			ImageUtils::DrawSRV(input, 0.0f, 0.0f, (float)width, (float)height,
				DirectX::XMFLOAT4(1, 1, 1, 1), DirectX::XMFLOAT4(0, 0, 1, 1), true, 1.0f);
			ctx->OMSetRenderTargets(1, oldRTV.GetAddressOf(), oldDSV.Get());
		}
		depthSRV_ = nullptr;
		return;
	}

	if (!EnsureRenderTargets(width, height)) { depthSRV_ = nullptr; return; }
	EnsureSamplers();
	EnsureDepthStencilState();
	EnsureConstantBuffers();

	ComPtr<ID3D11RenderTargetView>   oldRTV;
	ComPtr<ID3D11DepthStencilView>   oldDSV;
	ComPtr<ID3D11DepthStencilState>  oldDSS;
	ComPtr<ID3D11BlendState>         oldBlend;
	FLOAT oldBlendFactor[4] = {};
	UINT  oldSampleMask = 0;
	UINT  oldStencilRef = 0;
	D3D11_VIEWPORT oldVP;
	UINT numVP = 1;

	ctx->OMGetRenderTargets(1, oldRTV.GetAddressOf(), oldDSV.GetAddressOf());
	ctx->OMGetDepthStencilState(oldDSS.GetAddressOf(), &oldStencilRef);
	ctx->OMGetBlendState(oldBlend.GetAddressOf(), oldBlendFactor, &oldSampleMask);
	ctx->RSGetViewports(&numVP, &oldVP);

	ctx->OMSetDepthStencilState(dsOff_.Get(), 0);

	ctx->VSSetShader(fullscreenVS, nullptr, 0);

	ctx->IASetInputLayout(nullptr);
	ctx->IASetVertexBuffers(0, 0, nullptr, nullptr, nullptr);
	ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	{
		SSAO_Params cb{};
		cb.proj = XMMatrixTranspose(proj_);
		cb.invProj = XMMatrixTranspose(invProj_);
		cb.resolution = { (float)width, (float)height };
		cb.radius = radius;
		cb.bias = bias;
		cb.power = power;
		ctx->UpdateSubresource(ssaoCB_.Get(), 0, nullptr, &cb, 0, 0);

		KernelCB kcb{};
		for (int k = 0; k < 64; ++k) kcb.kernel[k] = kernel_[k];
		ctx->UpdateSubresource(kernelCB_.Get(), 0, nullptr, &kcb, 0, 0);

		ID3D11Buffer* cbs[] = { ssaoCB_.Get(), kernelCB_.Get() };
		ctx->PSSetConstantBuffers(1, 2, cbs);

		ID3D11ShaderResourceView* srvs[] = { nullptr, depthSRV_, noiseSRV_.Get() };
		ctx->PSSetShaderResources(0, 3, srvs);

		ID3D11SamplerState* smps[] = { pointClampSmp_.Get(), pointWrapSmp_.Get() };
		ctx->PSSetSamplers(0, 2, smps);

		DrawFullscreen(ssaoPS, nullptr, 0, 0, nullptr, 0, 0, ssaoRTV_.Get(), width, height);
	}

	ID3D11ShaderResourceView* null3[3] = {};
	ctx->PSSetShaderResources(0, 3, null3);

	{
		BlurParams bcb{};
		bcb.texelSize = { 1.0f / width, 1.0f / height };
		ctx->UpdateSubresource(blurCB_.Get(), 0, nullptr, &bcb, 0, 0);

		ID3D11Buffer* cbs[] = { blurCB_.Get() };
		ctx->PSSetConstantBuffers(1, 1, cbs);

		ID3D11ShaderResourceView* srvs[] = { ssaoSRV_.Get() };
		ctx->PSSetShaderResources(0, 1, srvs);

		ID3D11SamplerState* smps[] = { pointClampSmp_.Get() };
		ctx->PSSetSamplers(0, 1, smps);

		DrawFullscreen(blurPS, nullptr, 0, 0, nullptr, 0, 0, blurRTV_.Get(), width, height);
	}

	ID3D11ShaderResourceView* null1[1] = {};
	ctx->PSSetShaderResources(0, 1, null1);

	{
		CompositeParams ccb{};
		ccb.aoStrength = aoStrength;
		ctx->UpdateSubresource(compositeCB_.Get(), 0, nullptr, &ccb, 0, 0);

		ID3D11Buffer* cbs[] = { compositeCB_.Get() };
		ctx->PSSetConstantBuffers(1, 1, cbs);

		ID3D11ShaderResourceView* srvs[] = { input, blurSRV_.Get() };
		ctx->PSSetShaderResources(0, 2, srvs);

		ID3D11SamplerState* smps[] = { linearSmp_.Get() };
		ctx->PSSetSamplers(0, 1, smps);

		DrawFullscreen(compositePS, nullptr, 0, 0, nullptr, 0, 0, output, width, height);
	}

	ID3D11ShaderResourceView* null2[2] = {};
	ctx->PSSetShaderResources(0, 2, null2);

	ctx->OMSetDepthStencilState(oldDSS.Get(), oldStencilRef);
	ctx->OMSetBlendState(oldBlend.Get(), oldBlendFactor, oldSampleMask);
	ctx->OMSetRenderTargets(1, oldRTV.GetAddressOf(), oldDSV.Get());
	ctx->RSSetViewports(1, &oldVP);

	depthSRV_ = nullptr;
}

void SSAOEffect::DrawFullscreen(
	ID3D11PixelShader* ps,
	ID3D11ShaderResourceView* const* srvs,
	UINT                              srvCount,
	UINT                              srvSlot,
	ID3D11Buffer* const* psCBs,
	UINT                              cbCount,
	UINT                              cbSlot,
	ID3D11RenderTargetView* rtv,
	int width, int height)
{
	auto ctx = DirectX11::GetInstance()->GetContext();

	ctx->PSSetShader(ps, nullptr, 0);

	if (srvs && srvCount > 0)
		ctx->PSSetShaderResources(srvSlot, srvCount, srvs);
	if (psCBs && cbCount > 0)
		ctx->PSSetConstantBuffers(cbSlot, cbCount, psCBs);

	ctx->OMSetRenderTargets(1, &rtv, nullptr);

	D3D11_VIEWPORT vp{};
	vp.Width = (float)width; vp.Height = (float)height; vp.MaxDepth = 1.0f;
	ctx->RSSetViewports(1, &vp);

	ctx->Draw(3, 0);
}

bool SSAOEffect::EnsureRenderTargets(int width, int height)
{
	if (cachedWidth_ == width && cachedHeight_ == height && ssaoRTV_) return true;

	cachedWidth_ = width;
	cachedHeight_ = height;

	ssaoTex_.Reset(); ssaoRTV_.Reset(); ssaoSRV_.Reset();
	blurTex_.Reset(); blurRTV_.Reset(); blurSRV_.Reset();

	auto dev = DirectX11::GetInstance()->GetDevice();

	D3D11_TEXTURE2D_DESC td{};
	td.Width = width; td.Height = height;
	td.MipLevels = 1; td.ArraySize = 1;
	td.Format = DXGI_FORMAT_R8_UNORM;
	td.SampleDesc.Count = 1;
	td.Usage = D3D11_USAGE_DEFAULT;
	td.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;

	if (FAILED(dev->CreateTexture2D(&td, nullptr, ssaoTex_.GetAddressOf()))) return false;
	dev->CreateRenderTargetView(ssaoTex_.Get(), nullptr, ssaoRTV_.GetAddressOf());
	dev->CreateShaderResourceView(ssaoTex_.Get(), nullptr, ssaoSRV_.GetAddressOf());

	if (FAILED(dev->CreateTexture2D(&td, nullptr, blurTex_.GetAddressOf()))) return false;
	dev->CreateRenderTargetView(blurTex_.Get(), nullptr, blurRTV_.GetAddressOf());
	dev->CreateShaderResourceView(blurTex_.Get(), nullptr, blurSRV_.GetAddressOf());

	return ssaoRTV_ && blurRTV_;
}

void SSAOEffect::EnsureKernel()
{
	if (!kernel_.empty()) return;

	std::uniform_real_distribution<float> dist(0.0f, 1.0f);
	std::default_random_engine rng(42);

	kernel_.resize(64);
	for (int i = 0; i < 64; ++i)
	{
		XMFLOAT3 s(dist(rng) * 2.0f - 1.0f,
			dist(rng) * 2.0f - 1.0f,
			dist(rng));
		XMVECTOR v = XMVector3Normalize(XMLoadFloat3(&s));
		v = XMVectorScale(v, dist(rng));

		float scale = (float)i / 64.0f;
		scale = 0.1f + scale * scale * 0.9f;
		v = XMVectorScale(v, scale);

		XMFLOAT3 r;
		XMStoreFloat3(&r, v);
		kernel_[i] = { r.x, r.y, r.z, 0.0f };
	}
}

void SSAOEffect::EnsureNoise()
{
	if (noiseSRV_) return;

	std::uniform_real_distribution<float> dist(-1.0f, 1.0f);
	std::default_random_engine rng(123);

	uint8_t data[4 * 4 * 4];
	for (int i = 0; i < 16; ++i)
	{
		float x = dist(rng), y = dist(rng);
		float len = sqrtf(x * x + y * y);
		if (len > 0.0001f) { x /= len; y /= len; }
		data[i * 4 + 0] = (uint8_t)((x * 0.5f + 0.5f) * 255.0f);
		data[i * 4 + 1] = (uint8_t)((y * 0.5f + 0.5f) * 255.0f);
		data[i * 4 + 2] = 0;
		data[i * 4 + 3] = 255;
	}

	auto dev = DirectX11::GetInstance()->GetDevice();
	D3D11_TEXTURE2D_DESC td{};
	td.Width = 4; td.Height = 4; td.MipLevels = 1; td.ArraySize = 1;
	td.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	td.SampleDesc.Count = 1;
	td.Usage = D3D11_USAGE_IMMUTABLE;
	td.BindFlags = D3D11_BIND_SHADER_RESOURCE;

	D3D11_SUBRESOURCE_DATA init{ data, 4 * 4, 0 };
	dev->CreateTexture2D(&td, &init, noiseTex_.GetAddressOf());
	dev->CreateShaderResourceView(noiseTex_.Get(), nullptr, noiseSRV_.GetAddressOf());
}

void SSAOEffect::EnsureSamplers()
{
	if (pointClampSmp_) return;

	auto dev = DirectX11::GetInstance()->GetDevice();
	D3D11_SAMPLER_DESC sd{};
	sd.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
	sd.ComparisonFunc = D3D11_COMPARISON_ALWAYS;
	sd.MaxLOD = D3D11_FLOAT32_MAX;

	sd.AddressU = sd.AddressV = sd.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
	dev->CreateSamplerState(&sd, pointClampSmp_.GetAddressOf());

	sd.AddressU = sd.AddressV = sd.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
	dev->CreateSamplerState(&sd, pointWrapSmp_.GetAddressOf());

	sd.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
	sd.AddressU = sd.AddressV = sd.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
	dev->CreateSamplerState(&sd, linearSmp_.GetAddressOf());
}

void SSAOEffect::EnsureDepthStencilState()
{
	if (dsOff_) return;

	D3D11_DEPTH_STENCIL_DESC dsd{};
	dsd.DepthEnable = FALSE;
	dsd.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
	dsd.StencilEnable = FALSE;
	DirectX11::GetInstance()->GetDevice()->CreateDepthStencilState(&dsd, dsOff_.GetAddressOf());
}

void SSAOEffect::EnsureConstantBuffers()
{
	if (ssaoCB_) return;

	auto dev = DirectX11::GetInstance()->GetDevice();
	D3D11_BUFFER_DESC bd{};
	bd.Usage = D3D11_USAGE_DEFAULT;
	bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;

	bd.ByteWidth = sizeof(SSAO_Params);     dev->CreateBuffer(&bd, nullptr, ssaoCB_.GetAddressOf());
	bd.ByteWidth = sizeof(KernelCB);        dev->CreateBuffer(&bd, nullptr, kernelCB_.GetAddressOf());
	bd.ByteWidth = sizeof(BlurParams);      dev->CreateBuffer(&bd, nullptr, blurCB_.GetAddressOf());
	bd.ByteWidth = sizeof(CompositeParams); dev->CreateBuffer(&bd, nullptr, compositeCB_.GetAddressOf());
}

void SSAOEffect::DrawInspector()
{
	ImGui::DragFloat(GUI::GetInstance()->ShiftJISToUTF8("半径").c_str(), &radius, 0.01f, 0.05f, 2.0f, "%.3f");
	ImGui::DragFloat(GUI::GetInstance()->ShiftJISToUTF8("バイアス").c_str(), &bias, 0.001f, 0.001f, 0.1f, "%.4f");
	ImGui::DragFloat(GUI::GetInstance()->ShiftJISToUTF8("強度").c_str(), &power, 0.05f, 0.5f, 5.0f, "%.2f");
	ImGui::DragFloat(GUI::GetInstance()->ShiftJISToUTF8("適用強度").c_str(), &aoStrength, 0.01f, 0.0f, 1.0f, "%.2f");
}

void SSAOEffect::SaveToJson(nlohmann::json& j) const
{
	j["radius"] = radius;
	j["bias"] = bias;
	j["power"] = power;
	j["aoStrength"] = aoStrength;
}

void SSAOEffect::LoadFromJson(const nlohmann::json& j)
{
	radius = j.value("radius", 0.5f);
	bias = j.value("bias", 0.025f);
	power = j.value("power", 2.0f);
	aoStrength = j.value("aoStrength", 0.8f);
}

void SSAOEffect::GenerateAO(int width, int height)
{
	if (!depthSRV_) return;

	auto* ctx = DirectX11::GetInstance()->GetContext();
	auto* sm = ShaderManager::GetInstance();

	ID3D11VertexShader* fullscreenVS = sm->GetVertexShader("VS_Fullscreen");
	ID3D11PixelShader* ssaoPS = sm->GetPixelShader("PS_SSAO");
	ID3D11PixelShader* blurPS = sm->GetPixelShader("PS_SSAOBlur");

	if (!fullscreenVS || !ssaoPS || !blurPS)
	{
		depthSRV_ = nullptr; normalSRV_ = nullptr; return;
	}
	if (!EnsureRenderTargets(width, height))
	{
		depthSRV_ = nullptr; normalSRV_ = nullptr; return;
	}
	EnsureSamplers();
	EnsureDepthStencilState();
	EnsureConstantBuffers();

	Microsoft::WRL::ComPtr<ID3D11RenderTargetView>  oldRTV;
	Microsoft::WRL::ComPtr<ID3D11DepthStencilView>  oldDSV;
	Microsoft::WRL::ComPtr<ID3D11DepthStencilState> oldDSS;
	UINT oldStencilRef = 0;
	D3D11_VIEWPORT oldVP; UINT numVP = 1;
	ctx->OMGetRenderTargets(1, oldRTV.GetAddressOf(), oldDSV.GetAddressOf());
	ctx->OMGetDepthStencilState(oldDSS.GetAddressOf(), &oldStencilRef);
	ctx->RSGetViewports(&numVP, &oldVP);

	ctx->OMSetDepthStencilState(dsOff_.Get(), 0);
	ctx->VSSetShader(fullscreenVS, nullptr, 0);
	ctx->IASetInputLayout(nullptr);
	ctx->IASetVertexBuffers(0, 0, nullptr, nullptr, nullptr);
	ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	{
		SSAO_Params cb{};
		cb.proj = DirectX::XMMatrixTranspose(proj_);
		cb.invProj = DirectX::XMMatrixTranspose(invProj_);
		cb.resolution = { (float)width, (float)height };
		cb.radius = radius; cb.bias = bias; cb.power = power;
		ctx->UpdateSubresource(ssaoCB_.Get(), 0, nullptr, &cb, 0, 0);
		KernelCB kcb{};
		for (int k = 0; k < 64; ++k) kcb.kernel[k] = kernel_[k];
		ctx->UpdateSubresource(kernelCB_.Get(), 0, nullptr, &kcb, 0, 0);
		ID3D11Buffer* cbs[] = { ssaoCB_.Get(), kernelCB_.Get() };
		ctx->PSSetConstantBuffers(1, 2, cbs);
		ID3D11SamplerState* smps[] = { pointClampSmp_.Get(), pointWrapSmp_.Get() };
		ctx->PSSetSamplers(0, 2, smps);

		ID3D11PixelShader* activePS = ssaoPS;
		if (normalSRV_)
		{
			ID3D11PixelShader* deferredPS = sm->GetPixelShader("PS_SSAO_Deferred");
			if (deferredPS) activePS = deferredPS;
			ID3D11ShaderResourceView* srvs[] = { normalSRV_, depthSRV_, noiseSRV_.Get() };
			ctx->PSSetShaderResources(0, 3, srvs);
		}
		else
		{
			ID3D11ShaderResourceView* srvs[] = { nullptr, depthSRV_, noiseSRV_.Get() };
			ctx->PSSetShaderResources(0, 3, srvs);
		}
		DrawFullscreen(activePS, nullptr, 0, 0, nullptr, 0, 0, ssaoRTV_.Get(), width, height);
		ID3D11ShaderResourceView* nulls3[3] = {};
		ctx->PSSetShaderResources(0, 3, nulls3);
	}

	{
		BlurParams bcb{}; bcb.texelSize = { 1.0f / width, 1.0f / height };
		ctx->UpdateSubresource(blurCB_.Get(), 0, nullptr, &bcb, 0, 0);
		ID3D11Buffer* cbs[] = { blurCB_.Get() };
		ctx->PSSetConstantBuffers(1, 1, cbs);
		ID3D11ShaderResourceView* srvs[] = { ssaoSRV_.Get() };
		ctx->PSSetShaderResources(0, 1, srvs);
		ID3D11SamplerState* smps[] = { pointClampSmp_.Get() };
		ctx->PSSetSamplers(0, 1, smps);
		DrawFullscreen(blurPS, nullptr, 0, 0, nullptr, 0, 0, blurRTV_.Get(), width, height);
		ID3D11ShaderResourceView* nulls1[1] = {};
		ctx->PSSetShaderResources(0, 1, nulls1);
	}

	ctx->OMSetDepthStencilState(oldDSS.Get(), oldStencilRef);
	ctx->OMSetRenderTargets(1, oldRTV.GetAddressOf(), oldDSV.Get());
	ctx->RSSetViewports(1, &oldVP);
	depthSRV_ = nullptr;
	normalSRV_ = nullptr;
	aoGenerated_ = true;
}