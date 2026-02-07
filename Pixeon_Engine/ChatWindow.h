#pragma once

#include "IZANAGI.h"
#include <string>
#include <vector>
#include <mutex>
#include <deque>

struct ChatMessage {
	std::string sender;
	std::string content;
	float timestamp;
	bool isError;

	ChatMessage(const std::string& s, const std::string& c, float t = 0.0f, bool err = false)
		: sender(s), content(c), timestamp(t), isError(err) {
	}
};

class ChatWindow {
public:
	static ChatWindow* GetInstance();
	static void DestroyInstance();

	void Initialize();
	void Draw();
	void Update();
	void Shutdown();

	bool IsInitialized() const { return isInitialized_; }
	bool IsVisible() const { return isVisible_; }
	void SetVisible(bool visible) { isVisible_ = visible; }
	void ToggleVisible() { isVisible_ = !isVisible_; }

	// Shift-JISÇ©ÇÁUTF-8Ç÷ÇÃïœä∑ÅiåˆäJÅj
	std::string ConvertToUTF8(const std::string& sjis);

private:
	ChatWindow();
	~ChatWindow();

	void SendAIMessage(const std::string& messageUTF8);
	void CheckAIResponse();
	void AddMessage(const std::string& sender, const std::string& contentUTF8, bool isError = false);
	void ScrollToBottom();

private:
	static ChatWindow* instance_;

	bool isInitialized_;
	bool isVisible_;
	bool autoScroll_;
	bool scrollToBottom_;

	std::deque<ChatMessage> messages_;
	std::mutex messagesMutex_;

	float lastCheckTime_;
	float checkInterval_;

	float windowWidth_;
	float windowHeight_;
	bool isWaitingForResponse_;

	std::string lastResponseContent_;
};