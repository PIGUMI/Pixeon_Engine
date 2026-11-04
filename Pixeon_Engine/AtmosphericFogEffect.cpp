#include "AtmosphericFogEffect.h"
#include "System.h"
#include "ShaderManager.h"
#include "GUI.h"
#include "IMGUI/imgui.h"

using namespace DirectX;

AtmosphericFogEffect::AtmosphericFogEffect()
{
	priority = 10;
}

AtmosphericFogEffect::~AtmosphericFogEffect()
{
	ReleaseResources();
}

bool AtmosphericFogEffect::EnsureResources()
{
	if (m_resourcesReady) return true;
	RHI::Device* dev = GraphicsSystem::GetInstance()->GetDevice();
	if (!dev) return false;

	auto* sm = ShaderManager::GetInstance();
	m_vs = sm->GetVertexShader("VS_Fullscreen");
	m_ps = sm->GetPixelShader("PS_AtmosphericFog");
	if (!m_vs || !m_ps) return false;

	RHI::BufferDesc bd;
	bd.i_Size = sizeof(FogCB);
	bd.i_BindFlags = RHI::Bind_ConstantBuffer;
	m_cb = dev->BufferCreate(bd);

	RHI::SamplerDesc sd;
	sd.e_Filter = RHI::Filter::Point;
	sd.e_AddressU = sd.e_AddressV = sd.e_AddressW = RHI::AddressMode::Clamp;
	sd.e_Compare = RHI::CompareFunc::Always;
	m_pointClamp = dev->SamplerCreate(sd);

	m_resourcesReady = m_cb && m_pointClamp;
	return m_resourcesReady;
}

void AtmosphericFogEffect::ReleaseResources()
{
	m_cb.reset();
	m_pointClamp.reset();
	m_vs = nullptr;
	m_ps = nullptr;
	m_resourcesReady = false;
}

void AtmosphericFogEffect::Apply(
	RHI::Texture* input,
	RHI::Texture* output,
	int width, int height)
{
	if (!input || !output) return;
	if (!EnsureResources()) return;
	GraphicsSystem* gs = GraphicsSystem::GetInstance();
	RHI::Device* dev = gs->GetDevice();

	FogCB cb = {};
	cb.invProj = XMMatrixTranspose(m_invProj);
	cb.invView = XMMatrixTranspose(m_invView);
	cb.fogColor = fogColor;
	cb.fogDensity = fogDensity;
	cb.fogStart = fogStart;
	cb.fogEnd = fogEnd;
	cb.fogHeight = fogHeight;
	cb.heightFalloff = heightFalloff;
	cb.resolutionX = (float)width;
	cb.resolutionY = (float)height;
	cb.fogMode = (int)fogMode;
	cb._padFog = 0.0f;
	cb.cameraPos = m_cameraPos;
	cb._pad = 0.0f;
	dev->BufferUpdate(m_cb.get(), &cb, sizeof(cb));

	ScopedOutput scope(output, width, height);
	gs->SetDepthMode(DEPTH_OFF);
	dev->ShaderSet(RHI::ShaderStage::Vertex, m_vs);
	dev->ShaderSet(RHI::ShaderStage::Pixel, m_ps);
	dev->ConstantBufferSet(RHI::ShaderStage::Pixel, 0, m_cb.get());
	dev->TextureSet(RHI::ShaderStage::Pixel, 0, input);
	dev->TextureSet(RHI::ShaderStage::Pixel, 1, m_depth);
	dev->SamplerSet(RHI::ShaderStage::Pixel, 0, m_pointClamp.get());
	dev->InputLayoutSet(nullptr);
	dev->TopologySet(RHI::Topology::TriangleList);
	dev->Draw(3, 0);

	dev->TextureSet(RHI::ShaderStage::Pixel, 1, nullptr);
	dev->DepthStencilStateSet(nullptr);
}

void AtmosphericFogEffect::DrawInspector()
{
	auto SJ = [](const char* s) { return GUI::GetInstance()->ShiftJISToUTF8(s); };

	ImGui::ColorEdit3(SJ("フォグカラー").c_str(), (float*)&fogColor);

	const char* modeNames[] = { "Linear", "Exponential", "Exponential2" };
	int modeIdx = (int)fogMode;
	if (ImGui::Combo(SJ("フォグモード").c_str(), &modeIdx, modeNames, 3))
		fogMode = (FogMode)modeIdx;

	if (fogMode == FogMode::Linear)
	{
		ImGui::DragFloat(SJ("開始距離").c_str(), &fogStart, 1.0f, 0.0f, fogEnd - 0.1f);
		ImGui::DragFloat(SJ("終了距離").c_str(), &fogEnd, 1.0f, fogStart + 0.1f, 10000.0f);
	}
	else
	{
		ImGui::DragFloat(SJ("フォグ密度").c_str(), &fogDensity, 0.001f, 0.0001f, 1.0f, "%.4f");
	}

	ImGui::Separator();
	ImGui::Text(SJ("高さフォグ").c_str());
	ImGui::DragFloat(SJ("最大高さ (0=無効)").c_str(), &fogHeight, 1.0f, 0.0f, 2000.0f);
	ImGui::DragFloat(SJ("高さ減衰率").c_str(), &heightFalloff, 0.001f, 0.0001f, 1.0f, "%.4f");
}

void AtmosphericFogEffect::SaveToJson(nlohmann::json& j) const
{
	j["fogColor"] = { fogColor.x, fogColor.y, fogColor.z };
	j["fogDensity"] = fogDensity;
	j["fogStart"] = fogStart;
	j["fogEnd"] = fogEnd;
	j["fogHeight"] = fogHeight;
	j["heightFalloff"] = heightFalloff;
	j["fogMode"] = (int)fogMode;
}

void AtmosphericFogEffect::LoadFromJson(const nlohmann::json& j)
{
	if (j.contains("fogColor") && j["fogColor"].is_array())
	{
		fogColor.x = j["fogColor"][0];
		fogColor.y = j["fogColor"][1];
		fogColor.z = j["fogColor"][2];
	}
	fogDensity = j.value("fogDensity", 0.02f);
	fogStart = j.value("fogStart", 10.0f);
	fogEnd = j.value("fogEnd", 200.0f);
	fogHeight = j.value("fogHeight", 0.0f);
	heightFalloff = j.value("heightFalloff", 0.1f);
	fogMode = (FogMode)j.value("fogMode", 1);
}

PostEffectBase* AtmosphericFogEffect::Clone() const
{
	auto* c = new AtmosphericFogEffect();
	c->fogColor = fogColor;
	c->fogDensity = fogDensity;
	c->fogStart = fogStart;
	c->fogEnd = fogEnd;
	c->fogHeight = fogHeight;
	c->heightFalloff = heightFalloff;
	c->fogMode = fogMode;
	c->enabled = enabled;
	c->priority = priority;
	return c;
}
