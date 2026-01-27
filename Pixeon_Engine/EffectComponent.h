#pragma once

#include "Component.h"
#include "EffectManager.h"
#include "AssetManager.h"

#include <string>
#include <vector>
#include <DirectXMath.h>

/**
 * @brief Effekseerエフェクトを制御するコンポーネント
 */
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

	// 再生制御
	void Play();
	void Stop();
	void Pause(bool pause);
	bool IsPlaying() const;
	bool IsPaused() const;

	// 設定
	void SetEffectPath(const std::string& path);
	const std::string& GetEffectPath() const { return effectPath_; }

	void SetAutoPlay(bool v) { autoPlay_ = v; }
	bool GetAutoPlay() const { return autoPlay_; }

	void SetLoop(bool v) { loop_ = v; }
	bool GetLoop() const { return loop_; }

	void SetOffset(const DirectX::XMFLOAT3& o) { offset_ = o; if (IsPlaying()) ApplyParamsToHandle(); }
	DirectX::XMFLOAT3 GetOffset() const { return offset_; }

	void SetSpeed(float s);
	float GetSpeed() const { return speed_; }

	void SetScale(const DirectX::XMFLOAT3& s);
	DirectX::XMFLOAT3 GetScale() const { return scale_; }

	void SetRotation(const DirectX::XMFLOAT3& r);
	DirectX::XMFLOAT3 GetRotation() const { return rotation_; }

	// 動的パラメータ
	void SetDynamicInput(int32_t index, float value);
	float GetDynamicInput(int32_t index) const;

	// エフェクト情報
	Effekseer::Handle GetHandle() const { return handle_; }
	int32_t GetInstanceCount() const;

private:
	void ApplyParamsToHandle();
	DirectX::XMFLOAT3 GetWorldPositionWithOffset() const;
	DirectX::XMFLOAT3 GetWorldRotationWithOffset() const;

	// Inspector用
	void RefreshEffectList();
	int FindEffectIndexByPath(const std::string& path) const;
	void DrawPlaybackControls();
	void DrawTransformSettings();
	void DrawDynamicParameters();

private:
	// 基本設定
	std::string effectPath_;
	bool autoPlay_ = true;
	bool loop_ = false;
	bool isPaused_ = false;

	// トランスフォーム
	DirectX::XMFLOAT3 offset_{ 0,0,0 };
	DirectX::XMFLOAT3 rotation_{ 0,0,0 };
	DirectX::XMFLOAT3 scale_{ 1,1,1 };

	// 再生パラメータ
	float speed_ = 1.0f;

	// 動的パラメータ (Effekseerの動的パラメータ用)
	std::array<float, 4> dynamicInputs_ = { 0,0,0,0 };
	bool useDynamicInputs_ = false;

	// 親オブジェクトの追従設定
	bool followParentRotation_ = false;
	bool followParentScale_ = false;

	// エフェクトハンドル
	Effekseer::Handle handle_ = -1;

	// Inspector用
	std::vector<std::string> effectCandidates_;
	int selectedIndex_ = 0;
};