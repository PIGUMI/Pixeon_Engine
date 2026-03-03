#include "ColorGradingEffect.h"
#include "ImageUtils.h"
#include "System.h"
#include "GUI.h"

void ColorGradingEffect::Apply(ID3D11ShaderResourceView* input,
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

	ImageUtils::DrawSRVColorGrading(
		input,
		0.0f, 0.0f,
		(float)width, (float)height,
		brightness,
		contrast,
		saturation,
		hueShift,
		temperature,
		tint,
		gamma
	);

	ID3D11ShaderResourceView* nullSRV[1] = { nullptr };
	ctx->PSSetShaderResources(0, 1, nullSRV);

	ctx->OMSetRenderTargets(1, oldRTV ? &oldRTV : nullptr, oldDSV);

	if (oldRTV) oldRTV->Release();
	if (oldDSV) oldDSV->Release();
}

void ColorGradingEffect::DrawInspector() {
	ImGui::Text(GUI::GetInstance()->ShiftJISToUTF8("基本調整").c_str());
	ImGui::Separator();

	ImGui::DragFloat(GUI::GetInstance()->ShiftJISToUTF8("明度").c_str(),
		&brightness, 0.01f, -1.0f, 1.0f);

	ImGui::DragFloat(GUI::GetInstance()->ShiftJISToUTF8("コントラスト").c_str(),
		&contrast, 0.01f, 0.0f, 2.0f);

	ImGui::DragFloat(GUI::GetInstance()->ShiftJISToUTF8("彩度").c_str(),
		&saturation, 0.01f, 0.0f, 2.0f);

	ImGui::Spacing();
	ImGui::Text(GUI::GetInstance()->ShiftJISToUTF8("色調整").c_str());
	ImGui::Separator();

	ImGui::DragFloat(GUI::GetInstance()->ShiftJISToUTF8("色相シフト").c_str(),
		&hueShift, 1.0f, 0.0f, 360.0f);

	ImGui::DragFloat(GUI::GetInstance()->ShiftJISToUTF8("色温度").c_str(),
		&temperature, 0.01f, -1.0f, 1.0f);

	ImGui::DragFloat(GUI::GetInstance()->ShiftJISToUTF8("色合い").c_str(),
		&tint, 0.01f, -1.0f, 1.0f);

	ImGui::Spacing();
	ImGui::Text(GUI::GetInstance()->ShiftJISToUTF8("その他").c_str());
	ImGui::Separator();

	ImGui::DragFloat(GUI::GetInstance()->ShiftJISToUTF8("ガンマ補正").c_str(),
		&gamma, 0.01f, 0.1f, 3.0f);

	ImGui::Spacing();
	if (ImGui::Button(GUI::GetInstance()->ShiftJISToUTF8("リセット").c_str(), ImVec2(100, 0))) {
		brightness = 0.0f;
		contrast = 1.0f;
		saturation = 1.0f;
		hueShift = 0.0f;
		temperature = 0.0f;
		tint = 0.0f;
		gamma = 1.0f;
	}
}

void ColorGradingEffect::SaveToJson(nlohmann::json& j) const {
	j["brightness"] = brightness;
	j["contrast"] = contrast;
	j["saturation"] = saturation;
	j["hueShift"] = hueShift;
	j["temperature"] = temperature;
	j["tint"] = tint;
	j["gamma"] = gamma;
}

void ColorGradingEffect::LoadFromJson(const nlohmann::json& j) {
	brightness = j.value("brightness", 0.0f);
	contrast = j.value("contrast", 1.0f);
	saturation = j.value("saturation", 1.0f);
	hueShift = j.value("hueShift", 0.0f);
	temperature = j.value("temperature", 0.0f);
	tint = j.value("tint", 0.0f);
	gamma = j.value("gamma", 1.0f);
}

PostEffectBase* ColorGradingEffect::Clone() const {
	ColorGradingEffect* clone = new ColorGradingEffect();
	clone->brightness = brightness;
	clone->contrast = contrast;
	clone->saturation = saturation;
	clone->hueShift = hueShift;
	clone->temperature = temperature;
	clone->tint = tint;
	clone->gamma = gamma;
	clone->enabled = enabled;
	clone->priority = priority;
	return clone;
}