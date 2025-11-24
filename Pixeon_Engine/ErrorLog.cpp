#include "ErrorLog.h"
#include "IMGUI/imgui.h"

void ErrorLogger::DrawDebugGUI(bool* pOpen)
{
	if (pOpen && !*pOpen) return;
	if (ImGui::Begin("ErrorLogger", pOpen)) {
		ImGui::TextUnformatted("Logged Errors / Warnings");
		ImGui::Separator();
		ImGui::BeginChild("ErrList", ImVec2(0, 300), true);
		for (auto& kv : m_entries) {
			const auto& e = kv.second;
			ImGui::Text("[%s] %s (count=%d%s)",
				e.tag.c_str(),
				e.message.c_str(),
				e.count,
				e.shownMessageBox ? ", boxed" : "");
		}
		ImGui::EndChild();
	}
	ImGui::End();
}