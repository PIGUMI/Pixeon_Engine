#pragma once
#include <string>
#include <unordered_map>
#include <mutex>
#include <vector>
#include <Windows.h>

class ErrorLogger {
public:
	struct Entry {
		std::string tag;
		std::string message;
		int         count = 0;
		bool        shownMessageBox = false;
	};

	static ErrorLogger& Instance() {
		static ErrorLogger inst;
		return inst;
	}

	// エラー記録
	void LogError(const std::string& tag,
		const std::string& message,
		bool fatal = false,
		int perMsgLimit = 1)
	{
		//std::lock_guard<std::mutex> lk(m_mtx);
		//std::string key = tag + "||" + message;
		//auto& e = m_entries[key];
		//if (e.count == 0) {
		//    e.tag = tag;
		//    e.message = message;
		//}
		//e.count++;

		//// 常にデバッグ出力
		//std::string dbg = "[ERROR][" + tag + "] " + message + " (count=" + std::to_string(e.count) + ")\n";
		//OutputDebugStringA(dbg.c_str());

		//// 表示条件:
		//// fatal の場合 → 常に MessageBox
		//// それ以外 → 初回 (e.count==1) か perMsgLimit 未満表示の間のみ
		//bool needBox = false;
		//if (fatal) {
		//    needBox = true;
		//}
		//else if (!e.shownMessageBox && e.count <= perMsgLimit) {
		//    needBox = true;
		//}

		//if (needBox) {
		//    std::string boxTitle = fatal ? ("FATAL: " + tag) : ("Error: " + tag);
		//    MessageBoxA(nullptr, message.c_str(), boxTitle.c_str(), MB_OK | MB_ICONERROR);
		//    if (!fatal && e.count >= perMsgLimit) {
		//        e.shownMessageBox = true;
		//    }
		//}
	}

	// 警告（必要なら拡張）
	void LogWarning(const std::string& tag, const std::string& message, int perMsgLimit = 2) {
		std::lock_guard<std::mutex> lk(m_mtx);
		std::string key = "W:" + tag + "||" + message;
		auto& e = m_entries[key];
		if (e.count == 0) {
			e.tag = tag;
			e.message = message;
		}
		e.count++;
		std::string dbg = "[WARN][" + tag + "] " + message + " (count=" + std::to_string(e.count) + ")\n";
		OutputDebugStringA(dbg.c_str());
		if (e.count <= perMsgLimit) {
			MessageBoxA(nullptr, message.c_str(), ("Warning: " + tag).c_str(), MB_OK | MB_ICONWARNING);
		}
	}

	// GUI 出力用
	void DrawDebugGUI(bool* pOpen = nullptr);

private:
	ErrorLogger() = default;
	std::unordered_map<std::string, Entry> m_entries;
	std::mutex m_mtx;
};