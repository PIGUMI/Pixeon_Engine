#pragma once

#include "Component.h"
#include "EffectManager.h"
#include "AssetManager.h"

#include <string>
#include <vector>
#include <DirectXMath.h>

class EffectComponent : public AbstractComponent
{
public:
	void Init(AbstractObject* Prt) override;
	void BeginPlay() override;
	void EditUpdate() override;
	void InGameUpdate() override;
	void UInit() override;

	void DrawInspector() override;

	void SaveToFile(std::ostream& out) override;
	void LoadFromFile(std::istream& in) override;

	// çƒê∂
	void Play();
	void Stop();
	bool IsPlaying() const;

	// ê›íË
	void SetEffectPath(const std::string& path);
	const std::string& GetEffectPath() const { return effectPath_; }

	void SetAutoPlay(bool v) { autoPlay_ = v; }
	bool GetAutoPlay() const { return autoPlay_; }

	void SetLoop(bool v) { loop_ = v; }
	bool GetLoop() const { return loop_; }

	void SetOffset(const DirectX::XMFLOAT3& o) { offset_ = o; if (IsPlaying()) ApplyParamsToHandle(); }
	DirectX::XMFLOAT3 GetOffset() const { return offset_; }

	void SetSpeed(float s) { speed_ = s; if (IsPlaying()) ApplyParamsToHandle(); }
	float GetSpeed() const { return speed_; }

private:
	void ApplyParamsToHandle();
	DirectX::XMFLOAT3 GetWorldPositionWithOffset() const;

	// Inspectoróp
	void RefreshEffectList();
	int FindEffectIndexByPath(const std::string& path) const;

private:
	std::string effectPath_;
	bool autoPlay_ = true;
	bool loop_ = false;

	DirectX::XMFLOAT3 offset_{ 0,0,0 };
	float speed_ = 1.0f;

	Effekseer::Handle handle_ = -1;

	// ëIëéÆóp
	std::vector<std::string> effectCandidates_;
	int selectedIndex_ = 0;
};