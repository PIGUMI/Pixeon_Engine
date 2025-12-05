#include "EditrGUI.h"
#include "IMGUI/imgui.h"
#include "IMGUI/imgui_impl_dx11.h"
#include "IMGUI/imgui_impl_win32.h"
#include "IMGUI/imgui_internal.h"
#include "SceneManger.h"
#include "Scene.h"
#include "Object.h"
#include <vector>
#include <string>

void EditrGUI::ShowPrefab()
{
    ImGui::Begin(ShiftJISToUTF8("Prefab").c_str());

    std::vector<Object*> prefabList = EngineManager::GetInstance()->GetPrefabs();

    float iconSize = 48.0f;
    float itemWidth = 96.0f;
    float itemHeight = 80.0f;
    float availWidth = ImGui::GetContentRegionAvail().x;
    int columns = static_cast<int>(availWidth / itemWidth);
    if (columns < 1) columns = 1;
    ImGui::Columns(columns, nullptr, false);

    static Object* selectedPrefabObj = nullptr;

    int index = 0;
    for (auto* prefab : prefabList)
    {
        ImGui::BeginGroup();

        std::string name = prefab ? prefab->GetObjectName() : "(null)";
        std::string displayName = AbbreviateName(name, 12);
        std::string idName = name + "##prefab_" + std::to_string(index++);

        float groupX = ImGui::GetCursorPosX();
        float cursorX = groupX + (itemWidth - iconSize) * 0.5f;
        ImGui::SetCursorPosX(cursorX);

        bool isSelected = (prefab == selectedPrefabObj);

        if (!isSelected) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0, 0, 0, 0));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0, 0, 0, 0));
        }
        else {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.4f, 0.6f, 1.0f, 0.5f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.4f, 0.7f, 1.0f, 0.7f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.4f, 0.5f, 1.0f, 1.0f));
        }

        bool iconClicked = false;
        if (ObjectIcon) {
            iconClicked = ImGui::ImageButton(
                (std::string("icon_") + idName).c_str(),
                ObjectIcon,
                ImVec2(iconSize, iconSize)
            );
        }
        else {
            iconClicked = ImGui::Button(
                (std::string("btn_") + idName).c_str(),
                ImVec2(iconSize, iconSize)
            );
        }

        ImGui::PopStyleColor(3);

        std::string textUTF8 = ShiftJISToUTF8(displayName);
        float textWidth = ImGui::CalcTextSize(textUTF8.c_str()).x;
        ImGui::SetCursorPosX(groupX + (itemWidth - textWidth) * 0.5f);

        ImGui::PushID(idName.c_str());
        bool nameClicked = ImGui::Selectable(
            textUTF8.c_str(),
            isSelected, 0, ImVec2(itemWidth, 0)
        );

        // 右クリックでPopup
        if (ImGui::BeginPopupContextItem("context"))
        {
            //// 必要な項目を追加例：
            //if (ImGui::MenuItem(ShiftJISToUTF8("リネーム").c_str())) {
            //    // リネーム処理
            //    // 例：ShowPrefabRenameWindow = true; RenameTargetObject = prefab;
            //    // 実装は好みに応じ EditrGUI に追加してください
            //}
            if (ImGui::MenuItem(ShiftJISToUTF8("インスタンス化").c_str())) {
				SceneManger::GetInstance()->GetCurrentScene()->AddObjectLocal(prefab->Clone());
            }
            if (ImGui::MenuItem(ShiftJISToUTF8("削除").c_str())) {
				EngineManager::GetInstance()->RemovePrefab(prefab);
            }
            ImGui::EndPopup();
        }

        ImGui::PopID();

        if (iconClicked || nameClicked) {
            selectedPrefabObj = prefab;
            SelectedObject = prefab;
        }

        ImGui::EndGroup();
        ImGui::NextColumn();
    }

    ImGui::Columns(1);
    ImGui::End();
}