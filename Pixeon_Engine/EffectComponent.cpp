#include "EffectComponent.h"
#include "Object.h"
#include "Scene.h"
#include "GUI.h"
#include "IMGUI/imgui.h"

#include <algorithm>
#include <cstring>

static std::string ToLowerCopy(std::string s)
{
	std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return (char)std::tolower(c); });
	return s;
}

void EffectComponent::Init(AbstractObject* Prt)
{
	_Parent = Prt;
	_ComponentName = "EffectComponent";
	_Type = ComponentManager::COMPONENT_TYPE::EFFECT;

	_LayerNumber = 0;
	handle_ = -1;

	effectPath_.clear();
	autoPlay_ = true;
	loop_ = false;
	isPaused_ = false;
	offset_ = DirectX::XMFLOAT3(0, 0, 0);
	rotation_ = DirectX::XMFLOAT3(0, 0, 0);
	scale_ = DirectX::XMFLOAT3(1, 1, 1);
	speed_ = 1.0f;

	dynamicInputs_.fill(0.0f);
	useDynamicInputs_ = false;
	followParentRotation_ = false;
	followParentScale_ = false;

	RefreshEffectList();
	selectedIndex_ = FindEffectIndexByPath(effectPath_);
	if (selectedIndex_ < 0) selectedIndex_ = 0;
}

void EffectComponent::BeginPlay()
{
	if (autoPlay_)
	{
		Play();
	}
}

void EffectComponent::EditUpdate()
{
	// エディタプレビュー用の更新があればここに
}

void EffectComponent::InGameUpdate()
{
	if (handle_ >= 0)
	{
		if (!EffectManager::Instance()->IsPlaying(handle_))
		{
			handle_ = -1;

			if (loop_)
			{
				Play();
			}
			return;
		}

		if (!isPaused_)
		{
			ApplyParamsToHandle();
		}
	}
	else
	{
		// ループONで何らかの理由で止まっている場合の復帰
		if (loop_ && autoPlay_ && !effectPath_.empty() && !isPaused_)
		{
			Play();
		}
	}
}

void EffectComponent::UInit()
{
	Stop();
}

bool EffectComponent::IsPlaying() const
{
	if (handle_ < 0) return false;
	return EffectManager::Instance()->IsPlaying(handle_);
}

bool EffectComponent::IsPaused() const
{
	return isPaused_;
}

void EffectComponent::SetEffectPath(const std::string& path)
{
	effectPath_ = path;
	selectedIndex_ = FindEffectIndexByPath(effectPath_);
	if (selectedIndex_ < 0) selectedIndex_ = 0;
}

void EffectComponent::SetSpeed(float s)
{
	speed_ = s;
	if (IsPlaying())
	{
		EffectManager::Instance()->SetEffectSpeed(handle_, speed_);
	}
}

void EffectComponent::SetScale(const DirectX::XMFLOAT3& s)
{
	scale_ = s;
	if (IsPlaying())
	{
		ApplyParamsToHandle();
	}
}

void EffectComponent::SetRotation(const DirectX::XMFLOAT3& r)
{
	rotation_ = r;
	if (IsPlaying())
	{
		ApplyParamsToHandle();
	}
}

void EffectComponent::SetDynamicInput(int32_t index, float value)
{
	if (index >= 0 && index < 4)
	{
		dynamicInputs_[index] = value;
		if (IsPlaying())
		{
			EffectManager::Instance()->GetManager()->SetDynamicInput(handle_, index, value);
		}
	}
}

float EffectComponent::GetDynamicInput(int32_t index) const
{
	if (index >= 0 && index < 4)
	{
		return dynamicInputs_[index];
	}
	return 0.0f;
}

int32_t EffectComponent::GetInstanceCount() const
{
	if (handle_ < 0) return 0;
	return EffectManager::Instance()->GetManager()->GetInstanceCount(handle_);
}

DirectX::XMFLOAT3 EffectComponent::GetWorldPositionWithOffset() const
{
	if (!_Parent)
	{
		return offset_;
	}

	auto wt = _Parent->GetWorldTransform();
	return DirectX::XMFLOAT3(
		wt.position.x + offset_.x,
		wt.position.y + offset_.y,
		wt.position.z + offset_.z
	);
}

DirectX::XMFLOAT3 EffectComponent::GetWorldRotationWithOffset() const
{
	if (!_Parent || !followParentRotation_)
	{
		return rotation_;
	}

	auto wt = _Parent->GetWorldTransform();
	return DirectX::XMFLOAT3(
		wt.rotation.x + rotation_.x,
		wt.rotation.y + rotation_.y,
		wt.rotation.z + rotation_.z
	);
}

void EffectComponent::ApplyParamsToHandle()
{
	if (handle_ < 0) return;

	auto mgr = EffectManager::Instance();

	// 位置
	auto pos = GetWorldPositionWithOffset();
	mgr->SetEffectPosition(handle_, pos);

	// 回転
	auto rot = GetWorldRotationWithOffset();
	mgr->SetEffectRotation(handle_, rot);

	// スケール
	DirectX::XMFLOAT3 finalScale = scale_;
	if (followParentScale_ && _Parent)
	{
		auto wt = _Parent->GetWorldTransform();
		finalScale.x *= wt.scale.x;
		finalScale.y *= wt.scale.y;
		finalScale.z *= wt.scale.z;
	}
	mgr->SetEffectScale(handle_, finalScale);

	// 速度
	mgr->SetEffectSpeed(handle_, speed_);

	// 動的パラメータ
	if (useDynamicInputs_)
	{
		auto effMgr = mgr->GetManager();
		for (int i = 0; i < 4; i++)
		{
			effMgr->SetDynamicInput(handle_, i, dynamicInputs_[i]);
		}
	}
}

void EffectComponent::Play()
{
	Stop();
	if (effectPath_.empty()) return;

	// 拡張子チェック(任意)
	std::string lower = ToLowerCopy(effectPath_);
	if (!(lower.ends_with(".efk") || lower.ends_with(".efkefc")))
	{
		// 警告を出すか、returnするか
	}

	auto pos = GetWorldPositionWithOffset();
	handle_ = EffectManager::Instance()->PlayEffect(effectPath_, pos);

	if (handle_ >= 0)
	{
		isPaused_ = false;
		ApplyParamsToHandle();
	}
}

void EffectComponent::Stop()
{
	if (handle_ >= 0)
	{
		EffectManager::Instance()->StopEffect(handle_);
		handle_ = -1;
		isPaused_ = false;
	}
}

void EffectComponent::Pause(bool pause)
{
	if (handle_ >= 0)
	{
		isPaused_ = pause;
		EffectManager::Instance()->GetManager()->SetPaused(handle_, pause);
	}
}

//============================================================
// Inspector(オプション)
//============================================================
void EffectComponent::RefreshEffectList()
{
	effectCandidates_ = AssetManager::Instance()->GetCachedEffectNames();

	if (effectCandidates_.empty())
	{
		effectCandidates_.push_back("");
	}
}

int EffectComponent::FindEffectIndexByPath(const std::string& path) const
{
	if (effectCandidates_.empty()) return 0;

	for (int i = 0; i < (int)effectCandidates_.size(); i++)
	{
		if (effectCandidates_[i] == path) return i;
	}
	return -1;
}

void EffectComponent::DrawPlaybackControls()
{
	auto SJ = [](const char* s)->std::string { return GUI::GetInstance()->ShiftJISToUTF8(s); };

	if (ImGui::Button(SJ("再生").c_str(), ImVec2(60, 0)))
	{
		Play();
	}
	ImGui::SameLine();

	if (ImGui::Button(SJ("停止").c_str(), ImVec2(60, 0)))
	{
		Stop();
	}
	ImGui::SameLine();

	const char* pauseLabel = isPaused_ ? SJ("再開").c_str() : SJ("一時停止").c_str();
	if (ImGui::Button(pauseLabel, ImVec2(80, 0)))
	{
		Pause(!isPaused_);
	}

	ImGui::SameLine();
	const char* status = IsPlaying()
		? (isPaused_ ? SJ("状態: 一時停止中").c_str() : SJ("状態: 再生中").c_str())
		: SJ("状態: 停止中").c_str();
	ImGui::TextColored(IsPlaying() ? ImVec4(0, 1, 0, 1) : ImVec4(1, 0, 0, 1), "%s", status);

	if (IsPlaying())
	{
		int count = GetInstanceCount();
		ImGui::Text("%s: %d", SJ("インスタンス数").c_str(), count);
	}
}

void EffectComponent::DrawTransformSettings()
{
	auto SJ = [](const char* s)->std::string { return GUI::GetInstance()->ShiftJISToUTF8(s); };

	ImGui::TableNextRow();
	ImGui::TableSetColumnIndex(0); ImGui::Text(SJ("位置オフセット").c_str());
	ImGui::TableSetColumnIndex(1);
	{
		DirectX::XMFLOAT3 off = offset_;
		if (ImGui::DragFloat3("##Offset", &off.x, 0.1f))
		{
			SetOffset(off);
		}
	}

	ImGui::TableNextRow();
	ImGui::TableSetColumnIndex(0); ImGui::Text(SJ("回転").c_str());
	ImGui::TableSetColumnIndex(1);
	{
		DirectX::XMFLOAT3 rot = rotation_;
		if (ImGui::DragFloat3("##Rotation", &rot.x, 1.0f))
		{
			SetRotation(rot);
		}
	}

	ImGui::TableNextRow();
	ImGui::TableSetColumnIndex(0); ImGui::Text(SJ("スケール").c_str());
	ImGui::TableSetColumnIndex(1);
	{
		DirectX::XMFLOAT3 scl = scale_;
		if (ImGui::DragFloat3("##Scale", &scl.x, 0.01f, 0.001f, 100.0f))
		{
			SetScale(scl);
		}
	}

	ImGui::TableNextRow();
	ImGui::TableSetColumnIndex(0); ImGui::Text(SJ("親の回転追従").c_str());
	ImGui::TableSetColumnIndex(1);
	ImGui::Checkbox("##FollowRotation", &followParentRotation_);

	ImGui::TableNextRow();
	ImGui::TableSetColumnIndex(0); ImGui::Text(SJ("親のスケール追従").c_str());
	ImGui::TableSetColumnIndex(1);
	ImGui::Checkbox("##FollowScale", &followParentScale_);
}

void EffectComponent::DrawDynamicParameters()
{
	auto SJ = [](const char* s)->std::string { return GUI::GetInstance()->ShiftJISToUTF8(s); };

	ImGui::TableNextRow();
	ImGui::TableSetColumnIndex(0); ImGui::Text(SJ("動的パラメータ使用").c_str());
	ImGui::TableSetColumnIndex(1);
	ImGui::Checkbox("##UseDynamic", &useDynamicInputs_);

	if (useDynamicInputs_)
	{
		for (int i = 0; i < 4; i++)
		{
			ImGui::TableNextRow();
			ImGui::TableSetColumnIndex(0);
			ImGui::Text("%s %d", SJ("動的パラメータ").c_str(), i);
			ImGui::TableSetColumnIndex(1);

			std::string label = "##DynInput" + std::to_string(i);
			float val = dynamicInputs_[i];
			if (ImGui::DragFloat(label.c_str(), &val, 0.01f))
			{
				SetDynamicInput(i, val);
			}
		}
	}
}

void EffectComponent::DrawInspector()
{
	auto SJ = [](const char* s)->std::string { return GUI::GetInstance()->ShiftJISToUTF8(s); };

	std::string header = _ComponentName + "##" + std::to_string(reinterpret_cast<uintptr_t>(this));
	if (!ImGui::CollapsingHeader(SJ(header.c_str()).c_str())) return;

	// 再生コントロール
	DrawPlaybackControls();

	ImGui::Separator();

	std::string tableId = "EffectComponentTable##" + std::to_string(reinterpret_cast<uintptr_t>(this));
	if (ImGui::BeginTable(SJ(tableId.c_str()).c_str(), 2, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV))
	{
		ImGui::TableNextRow();
		ImGui::TableSetColumnIndex(0); ImGui::Text(SJ("レイヤー").c_str());
		ImGui::TableSetColumnIndex(1);
		ImGui::InputInt("##EffectLayer", &_LayerNumber);

		// エフェクト選択
		ImGui::TableNextRow();
		ImGui::TableSetColumnIndex(0); ImGui::Text(SJ("エフェクト").c_str());
		ImGui::TableSetColumnIndex(1);
		{
			if (ImGui::Button(SJ("更新").c_str()))
			{
				RefreshEffectList();
				selectedIndex_ = FindEffectIndexByPath(effectPath_);
				if (selectedIndex_ < 0) selectedIndex_ = 0;
			}
			ImGui::SameLine();

			const char* preview = "";
			if (!effectCandidates_.empty() && selectedIndex_ >= 0 && selectedIndex_ < (int)effectCandidates_.size())
			{
				preview = effectCandidates_[selectedIndex_].c_str();
			}

			std::string comboId = "##EffectSelect" + std::to_string(reinterpret_cast<uintptr_t>(this));
			if (ImGui::BeginCombo(comboId.c_str(), preview))
			{
				for (int i = 0; i < (int)effectCandidates_.size(); i++)
				{
					bool isSelected = (i == selectedIndex_);
					if (ImGui::Selectable(effectCandidates_[i].c_str(), isSelected))
					{
						selectedIndex_ = i;
						effectPath_ = effectCandidates_[i];
					}
					if (isSelected) ImGui::SetItemDefaultFocus();
				}
				ImGui::EndCombo();
			}
		}

		// 基本設定
		ImGui::TableNextRow();
		ImGui::TableSetColumnIndex(0); ImGui::Text(SJ("自動再生").c_str());
		ImGui::TableSetColumnIndex(1);
		ImGui::Checkbox("##AutoPlay", &autoPlay_);

		ImGui::TableNextRow();
		ImGui::TableSetColumnIndex(0); ImGui::Text(SJ("ループ").c_str());
		ImGui::TableSetColumnIndex(1);
		ImGui::Checkbox("##Loop", &loop_);

		ImGui::TableNextRow();
		ImGui::TableSetColumnIndex(0); ImGui::Text(SJ("再生速度").c_str());
		ImGui::TableSetColumnIndex(1);
		{
			float spd = speed_;
			if (ImGui::DragFloat("##Speed", &spd, 0.01f, 0.0f, 10.0f, "%.2f"))
			{
				SetSpeed(spd);
			}
		}

		// トランスフォーム設定
		DrawTransformSettings();

		// 動的パラメータ
		DrawDynamicParameters();

		ImGui::EndTable();
	}
}

//============================================================
// Save/Load
//============================================================
void EffectComponent::SaveToFile(std::ostream& out)
{
	out << effectPath_ << " "
		<< autoPlay_ << " "
		<< loop_ << " "
		<< offset_.x << " " << offset_.y << " " << offset_.z << " "
		<< rotation_.x << " " << rotation_.y << " " << rotation_.z << " "
		<< scale_.x << " " << scale_.y << " " << scale_.z << " "
		<< speed_ << " "
		<< followParentRotation_ << " "
		<< followParentScale_ << " "
		<< useDynamicInputs_ << " "
		<< dynamicInputs_[0] << " "
		<< dynamicInputs_[1] << " "
		<< dynamicInputs_[2] << " "
		<< dynamicInputs_[3] << " "
		<< _LayerNumber
		<< "\n";
}

void EffectComponent::LoadFromFile(std::istream& in)
{
	in >> effectPath_
		>> autoPlay_
		>> loop_
		>> offset_.x >> offset_.y >> offset_.z
		>> rotation_.x >> rotation_.y >> rotation_.z
		>> scale_.x >> scale_.y >> scale_.z
		>> speed_
		>> followParentRotation_
		>> followParentScale_
		>> useDynamicInputs_
		>> dynamicInputs_[0]
		>> dynamicInputs_[1]
		>> dynamicInputs_[2]
		>> dynamicInputs_[3]
		>> _LayerNumber;

	RefreshEffectList();
	selectedIndex_ = FindEffectIndexByPath(effectPath_);
	if (selectedIndex_ < 0) selectedIndex_ = 0;
}