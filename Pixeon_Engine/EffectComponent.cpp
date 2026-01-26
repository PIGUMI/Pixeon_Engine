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
	offset_ = DirectX::XMFLOAT3(0, 0, 0);
	speed_ = 1.0f;

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
	// 必要ならエディタプレビューなどここで
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

		ApplyParamsToHandle();
	}
	else
	{
		// ループONで何らかの理由で止まっている場合の保険
		if (loop_ && autoPlay_ && !effectPath_.empty())
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

void EffectComponent::SetEffectPath(const std::string& path)
{
	effectPath_ = path;
	selectedIndex_ = FindEffectIndexByPath(effectPath_);
	if (selectedIndex_ < 0) selectedIndex_ = 0;
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

void EffectComponent::ApplyParamsToHandle()
{
	if (handle_ < 0) return;

	auto pos = GetWorldPositionWithOffset();
	EffectManager::Instance()->SetEffectPosition(handle_, pos);
	EffectManager::Instance()->SetEffectSpeed(handle_, speed_);
}

void EffectComponent::Play()
{
	Stop();
	if (effectPath_.empty()) return;

	// 拡張子チェック（任意）
	std::string lower = ToLowerCopy(effectPath_);
#if defined(__cpp_lib_ends_with) && __cpp_lib_ends_with >= 201907L
	if (!(lower.ends_with(".efk") || lower.ends_with(".efkefc")))
	{
		// 想定外でも再生を試みるならreturnしない
	}
#else
	// C++20未満のフォールバック（必要なら）
#endif

	auto pos = GetWorldPositionWithOffset();
	handle_ = EffectManager::Instance()->PlayEffect(effectPath_, pos);

	if (handle_ >= 0)
	{
		ApplyParamsToHandle();
	}
}

void EffectComponent::Stop()
{
	if (handle_ >= 0)
	{
		EffectManager::Instance()->StopEffect(handle_);
		handle_ = -1;
	}
}

//============================================================
// Inspector（選択式）
//============================================================
void EffectComponent::RefreshEffectList()
{
	// AssetManagerのキャッシュ上の .efk / .efkefc 一覧を取得
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

void EffectComponent::DrawInspector()
{
	auto SJ = [](const char* s)->std::string { return GUI::GetInstance()->ShiftJISToUTF8(s); };

	std::string header = _ComponentName + "##" + std::to_string(reinterpret_cast<uintptr_t>(this));
	if (!ImGui::CollapsingHeader(SJ(header.c_str()).c_str())) return;

	std::string tableId = "EffectComponentTable##" + std::to_string(reinterpret_cast<uintptr_t>(this));
	if (ImGui::BeginTable(SJ(tableId.c_str()).c_str(), 2, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV))
	{
		ImGui::TableNextRow();
		ImGui::TableSetColumnIndex(0); ImGui::Text(SJ("レイヤー").c_str());
		ImGui::TableSetColumnIndex(1);
		ImGui::InputInt("##EffectLayer", &_LayerNumber);

		ImGui::TableNextRow();
		ImGui::TableSetColumnIndex(0); ImGui::Text(SJ("エフェクト").c_str());
		ImGui::TableSetColumnIndex(1);
		{
			// Refreshボタン（AssetManagerのキャッシュ更新に合わせて候補を更新）
			if (ImGui::Button(SJ("更新").c_str()))
			{
				RefreshEffectList();
				selectedIndex_ = FindEffectIndexByPath(effectPath_);
				if (selectedIndex_ < 0) selectedIndex_ = 0;
			}
			ImGui::SameLine();

			// 現在の表示名
			const char* preview = "";
			if (!effectCandidates_.empty() && selectedIndex_ >= 0 && selectedIndex_ < (int)effectCandidates_.size())
			{
				preview = effectCandidates_[selectedIndex_].c_str();
			}

			// Comboの中身
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

		// （任意）手入力欄：完全に選択式にしたいならこの行ブロック削除でOK
		ImGui::TableNextRow();
		ImGui::TableSetColumnIndex(0); ImGui::Text(SJ("パス(手入力)").c_str());
		ImGui::TableSetColumnIndex(1);
		{
			char buf[256]{};
			strncpy_s(buf, effectPath_.c_str(), sizeof(buf) - 1);
			if (ImGui::InputText("##EffectPath", buf, sizeof(buf)))
			{
				effectPath_ = buf;
				selectedIndex_ = FindEffectIndexByPath(effectPath_);
				if (selectedIndex_ < 0) selectedIndex_ = 0;
			}
		}

		ImGui::TableNextRow();
		ImGui::TableSetColumnIndex(0); ImGui::Text(SJ("自動再生").c_str());
		ImGui::TableSetColumnIndex(1);
		ImGui::Checkbox("##AutoPlay", &autoPlay_);

		ImGui::TableNextRow();
		ImGui::TableSetColumnIndex(0); ImGui::Text(SJ("ループ").c_str());
		ImGui::TableSetColumnIndex(1);
		ImGui::Checkbox("##Loop", &loop_);

		ImGui::TableNextRow();
		ImGui::TableSetColumnIndex(0); ImGui::Text(SJ("位置オフセット").c_str());
		ImGui::TableSetColumnIndex(1);
		{
			DirectX::XMFLOAT3 off = offset_;
			if (ImGui::InputFloat3("##Offset", &off.x, "%.3f"))
			{
				offset_ = off;
				if (IsPlaying()) ApplyParamsToHandle();
			}
		}

		ImGui::TableNextRow();
		ImGui::TableSetColumnIndex(0); ImGui::Text(SJ("再生速度").c_str());
		ImGui::TableSetColumnIndex(1);
		if (ImGui::DragFloat("##Speed", &speed_, 0.01f, 0.0f, 10.0f, "%.2f"))
		{
			if (IsPlaying()) ApplyParamsToHandle();
		}

		ImGui::EndTable();
	}

	ImGui::Separator();

	// 操作
	if (ImGui::Button(SJ("再生").c_str()))
	{
		Play();
	}
	ImGui::SameLine();
	if (ImGui::Button(SJ("停止").c_str()))
	{
		Stop();
	}

	ImGui::Separator();
	ImGui::Text("%s", SJ(IsPlaying() ? "状態: 再生中" : "状態: 停止中").c_str());
}

//============================================================
// Save/Load
//============================================================
void EffectComponent::SaveToFile(std::ostream& out)
{
	// path autoPlay loop offset(x y z) speed layer
	out << effectPath_ << " "
		<< autoPlay_ << " "
		<< loop_ << " "
		<< offset_.x << " " << offset_.y << " " << offset_.z << " "
		<< speed_ << " "
		<< _LayerNumber
		<< "\n";
}

void EffectComponent::LoadFromFile(std::istream& in)
{
	in >> effectPath_
		>> autoPlay_
		>> loop_
		>> offset_.x >> offset_.y >> offset_.z
		>> speed_
		>> _LayerNumber;

	// ロード後に候補を更新して選択位置を合わせる
	RefreshEffectList();
	selectedIndex_ = FindEffectIndexByPath(effectPath_);
	if (selectedIndex_ < 0) selectedIndex_ = 0;
}