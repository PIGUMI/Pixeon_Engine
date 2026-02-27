// PixelateEffect.cpp
#include "PixelateEffect.h"
#include "ImageUtils.h"
#include "System.h"
#include "GUI.h"

void PixelateEffect::Apply(ID3D11ShaderResourceView* input,
	ID3D11RenderTargetView* output,
	int width, int height) {
	if (!input || !output) return;

	auto* dx = DirectX11::GetInstance();
	if (!dx) return;

	ID3D11DeviceContext* ctx = dx->GetContext();
	if (!ctx) return;

	ID3D11RenderTargetView* oldRTV = nullptr;
	ID3D11DepthStencilView* oldDSV = nullptr;
	ctx->OMGetRenderTargets(1, &oldRTV, &oldDSV);

	ctx->OMSetRenderTargets(1, &output, nullptr);

	D3D11_VIEWPORT vp = {};
	vp.Width = (FLOAT)width;
	vp.Height = (FLOAT)height;
	vp.MinDepth = 0.0f;
	vp.MaxDepth = 1.0f;
	vp.TopLeftX = 0;
	vp.TopLeftY = 0;
	ctx->RSSetViewports(1, &vp);
	ImageUtils::DrawSRVPixelated(
		input,
		0.0f, 0.0f,
		(float)width, (float)height,
		pixelSize,
		intensity,
		color
	);
	ID3D11ShaderResourceView* nullSRV[1] = { nullptr };
	ctx->PSSetShaderResources(0, 1, nullSRV);

	ctx->OMSetRenderTargets(1, oldRTV ? &oldRTV : nullptr, oldDSV);

	if (oldRTV) oldRTV->Release();
	if (oldDSV) oldDSV->Release();
}

void PixelateEffect::DrawInspector() {
	ImGui::DragFloat(GUI::GetInstance()->ShiftJISToUTF8("ピクセルサイズ").c_str(),
		&pixelSize, 0.1f, 1.0f, 64.0f);
	ImGui::DragFloat(GUI::GetInstance()->ShiftJISToUTF8("強度").c_str(),
		&intensity, 0.01f, 0.0f, 1.0f);
	ImGui::ColorEdit4(GUI::GetInstance()->ShiftJISToUTF8("カラー").c_str(), &color.x);
}

void PixelateEffect::SaveToJson(nlohmann::json& j) const {
	j["pixelSize"] = pixelSize;
	j["intensity"] = intensity;
	j["color"] = { color.x, color.y, color.z, color.w };
}

void PixelateEffect::LoadFromJson(const nlohmann::json& j) {
	pixelSize = j.value("pixelSize", 8.0f);
	intensity = j.value("intensity", 1.0f);

	if (j.contains("color") && j["color"].is_array() && j["color"].size() == 4) {
		color.x = j["color"][0];
		color.y = j["color"][1];
		color.z = j["color"][2];
		color.w = j["color"][3];
	}
}

PostEffectBase* PixelateEffect::Clone() const {
	PixelateEffect* clone = new PixelateEffect();
	clone->pixelSize = pixelSize;
	clone->intensity = intensity;
	clone->color = color;
	clone->enabled = enabled;
	clone->priority = priority;
	return clone;
}