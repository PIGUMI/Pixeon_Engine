#include "ChatWindow.h"
#include "GUI.h"
#include <chrono>

#ifdef _WIN32
#include <windows.h>
#endif

ChatWindow* ChatWindow::instance_ = nullptr;

ChatWindow* ChatWindow::GetInstance() {
	if (instance_ == nullptr) {
		instance_ = new ChatWindow();
	}
	return instance_;
}

void ChatWindow::DestroyInstance() {
	if (instance_ != nullptr) {
		delete instance_;
		instance_ = nullptr;
	}
}

ChatWindow::ChatWindow()
	: isInitialized_(false)
	, isVisible_(false)
	, autoScroll_(true)
	, scrollToBottom_(false)
	, lastCheckTime_(0.0f)
	, checkInterval_(0.1f)
	, windowWidth_(600.0f)
	, windowHeight_(700.0f)
	, isWaitingForResponse_(false)
	, lastResponseContent_("")
{
}

ChatWindow::~ChatWindow() {
	Shutdown();
}

// Shift-JISからUTF-8への変換
std::string ChatWindow::ConvertToUTF8(const std::string& input) {
#ifdef _WIN32
	if (input.empty()) return "";

	int wideSize = MultiByteToWideChar(932, 0, input.c_str(), (int)input.length(), NULL, 0);
	if (wideSize == 0) return input;

	std::vector<wchar_t> wideStr(wideSize);
	MultiByteToWideChar(932, 0, input.c_str(), (int)input.length(), &wideStr[0], wideSize);

	int utf8Size = WideCharToMultiByte(CP_UTF8, 0, &wideStr[0], wideSize, NULL, 0, NULL, NULL);
	if (utf8Size == 0) return input;

	std::vector<char> utf8Str(utf8Size);
	WideCharToMultiByte(CP_UTF8, 0, &wideStr[0], wideSize, &utf8Str[0], utf8Size, NULL, NULL);

	return std::string(&utf8Str[0], utf8Size);
#else
	return input;
#endif
}

void ChatWindow::Initialize() {
	if (isInitialized_) return;

	IZANAGI* izanagi = IZANAGI::GetInstance();

	if (!izanagi) {
		AddMessage("System", ConvertToUTF8("IZANAGIが初期化されていません"), true);
		return;
	}

	if (!izanagi->GetInitialized()) {
		AddMessage("System", ConvertToUTF8("IZANAGIエンジンが初期化されていません"), true);
		return;
	}

	AddMessage("System", ConvertToUTF8("AIチャットが準備完了しました"), false);
	AddMessage("System", ConvertToUTF8("何か話しかけてください"), false);

	isInitialized_ = true;
}

void ChatWindow::Shutdown() {
	if (!isInitialized_) return;

	{
		std::lock_guard<std::mutex> lock(messagesMutex_);
		messages_.clear();
	}

	isInitialized_ = false;
}

void ChatWindow::Update() {
	if (!isInitialized_ || !isVisible_ || !IZANAGI::GetInstance()->GetInitialized()) return;

	static auto lastTime = std::chrono::high_resolution_clock::now();
	auto currentTime = std::chrono::high_resolution_clock::now();
	float deltaTime = std::chrono::duration<float>(currentTime - lastTime).count();
	lastTime = currentTime;

	lastCheckTime_ += deltaTime;

	if (lastCheckTime_ >= checkInterval_) {
		lastCheckTime_ = 0.0f;
		CheckAIResponse();
	}
}

void ChatWindow::Draw() {
	ImGui::SetNextWindowSize(ImVec2(windowWidth_, windowHeight_), ImGuiCond_FirstUseEver);

	ImGuiWindowFlags flags = ImGuiWindowFlags_NoCollapse;

	std::string windowTitle = ConvertToUTF8("AIチャット[IZANAGI]");
	if (ImGui::Begin(windowTitle.c_str(), &isVisible_, flags)) {
		// ヘッダー情報
		std::string statusLabel = ConvertToUTF8("状態: ");
		ImGui::Text("%s", statusLabel.c_str());
		ImGui::SameLine();

		if (!isInitialized_) {
			std::string uninitText = ConvertToUTF8("未初期化");
			ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "%s", uninitText.c_str());
		}
		else if (isWaitingForResponse_) {
			std::string waitingText = ConvertToUTF8("応答待機中...");
			ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "%s", waitingText.c_str());

			ImGui::SameLine();
			const char* spinnerFrames[] = { "|", "/", "-", "\\" };
			static int spinnerFrame = 0;
			static float spinnerTime = 0.0f;
			spinnerTime += ImGui::GetIO().DeltaTime;
			if (spinnerTime > 0.1f) {
				spinnerFrame = (spinnerFrame + 1) % 4;
				spinnerTime = 0.0f;
			}
			ImGui::Text("%s", spinnerFrames[spinnerFrame]);
		}
		else {
			std::string readyText = ConvertToUTF8("待機中");
			ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "%s", readyText.c_str());
		}

		ImGui::SameLine();
		std::string autoScrollLabel = ConvertToUTF8("自動スクロール");
		ImGui::Checkbox(autoScrollLabel.c_str(), &autoScroll_);

		ImGui::Separator();

		// チャット履歴表示エリア
		ImGui::BeginChild("ChatHistory", ImVec2(0, -90), true, ImGuiWindowFlags_HorizontalScrollbar);

		{
			std::lock_guard<std::mutex> lock(messagesMutex_);

			for (const auto& msg : messages_) {
				ImVec4 color;
				std::string prefix;

				if (msg.sender == "User") {
					color = ImVec4(0.6f, 0.8f, 1.0f, 1.0f);
					prefix = ConvertToUTF8("あなた: ");
				}
				else if (msg.sender == "AI") {
					color = ImVec4(0.8f, 1.0f, 0.6f, 1.0f);
					prefix = ConvertToUTF8("イザナギAI: ");
				}
				else if (msg.sender == "System") {
					color = msg.isError ? ImVec4(1.0f, 0.4f, 0.4f, 1.0f) : ImVec4(0.7f, 0.7f, 0.7f, 1.0f);
					prefix = ConvertToUTF8("[システム] ");
				}

				ImGui::PushStyleColor(ImGuiCol_Text, color);
				ImGui::PushTextWrapPos(ImGui::GetContentRegionAvail().x);

				std::string displayText = prefix + msg.content;
				ImGui::TextWrapped("%s", displayText.c_str());

				ImGui::PopTextWrapPos();
				ImGui::PopStyleColor();
				ImGui::Spacing();
			}
		}

		if (scrollToBottom_ || (autoScroll_ && ImGui::GetScrollY() >= ImGui::GetScrollMaxY())) {
			ImGui::SetScrollHereY(1.0f);
			scrollToBottom_ = false;
		}

		ImGui::EndChild();

		ImGui::Separator();

		// 入力エリア（固定バッファ版、UTF-8）
		static char inputBuffer[4096] = "";

		ImGuiInputTextFlags inputFlags = ImGuiInputTextFlags_AllowTabInput;
		if (isWaitingForResponse_) {
			inputFlags |= ImGuiInputTextFlags_ReadOnly;
		}

		ImGui::PushItemWidth(-1);

		// 通常のchar配列を使用（ImGuiが内部でUTF-8を処理）
		ImGui::InputTextMultiline(
			"##Input",
			inputBuffer,
			IM_ARRAYSIZE(inputBuffer),
			ImVec2(-1, 60),
			inputFlags
		);

		// Ctrl+Enterで送信
		bool ctrlEnterPressed = false;
		if (ImGui::IsItemFocused()) {
			ImGuiIO& io = ImGui::GetIO();
			if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Enter)) {
				ctrlEnterPressed = true;
			}
		}

		ImGui::PopItemWidth();

		// 送信ボタン
		std::string sendButtonLabel = ConvertToUTF8("送信 (Ctrl+Enter)");
		if (ImGui::Button(sendButtonLabel.c_str(), ImVec2(-1, 0))) {
			ctrlEnterPressed = true;
		}

		// 送信処理
		if (ctrlEnterPressed && strlen(inputBuffer) > 0 && !isWaitingForResponse_) {
			// inputBufferは既にUTF-8（ImGuiとStartUp.cppのIME処理のおかげ）
			std::string messageUTF8(inputBuffer);

			SendAIMessage(messageUTF8);

			// バッファクリア
			memset(inputBuffer, 0, sizeof(inputBuffer));
			ImGui::SetKeyboardFocusHere(-2);
		}

		// ショートカットヘルプ
		ImGui::Spacing();
		std::string helpText = ConvertToUTF8("Ctrl+Enterで送信 | Enterで改行 | Ctrl+Shift+Cで開閉");
		ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "%s", helpText.c_str());
	}
	ImGui::End();
}

void ChatWindow::SendAIMessage(const std::string& messageUTF8) {
	if (!isInitialized_ || messageUTF8.empty()) return;

	IZANAGI* izanagi = IZANAGI::GetInstance();
	if (!izanagi || !izanagi->GetInitialized()) {
		AddMessage("System", ConvertToUTF8("IZANAGIが利用できません"), true);
		return;
	}

	// 末尾の改行を除去
	std::string cleanMessage = messageUTF8;
	while (!cleanMessage.empty() && (cleanMessage.back() == '\n' || cleanMessage.back() == '\r')) {
		cleanMessage.pop_back();
	}

	if (cleanMessage.empty()) return;

	// ユーザーメッセージを追加（UTF-8）
	AddMessage("User", cleanMessage, false);

	// 前回の応答をクリア（重要！）
	lastResponseContent_.clear();

	// IZANAGIに送信（UTF-8のまま）
	izanagi->SendAIMessage(cleanMessage);

	// 応答待機フラグをセット
	isWaitingForResponse_ = true;

	ScrollToBottom();
}

void ChatWindow::CheckAIResponse() {
	if (!isWaitingForResponse_) return;

	IZANAGI* izanagi = IZANAGI::GetInstance();
	if (!izanagi || !izanagi->GetInitialized()) return;

	// AI処理中かチェック
	if (!izanagi->Processing()) {
		// 応答を取得（UTF-8）
		std::string response = izanagi->GetAIMessage();

		// 応答が空でなく、かつ前回と異なる場合のみ表示
		if (!response.empty() && (lastResponseContent_.empty() || response != lastResponseContent_)) {
			AddMessage("AI", response, false);
			lastResponseContent_ = response;
			ScrollToBottom();
			isWaitingForResponse_ = false;
		}
		else if (response.empty()) {
			// 空の応答が続く場合のタイムアウト処理
			static int emptyResponseCount = 0;
			emptyResponseCount++;

			if (emptyResponseCount > 20) {  // 10から20に増やす
				AddMessage("System", ConvertToUTF8("AIからの応答がありませんでした"), true);
				isWaitingForResponse_ = false;
				emptyResponseCount = 0;
			}
		}
	}
}

void ChatWindow::AddMessage(const std::string& sender, const std::string& contentUTF8, bool isError) {
	std::lock_guard<std::mutex> lock(messagesMutex_);

	auto now = std::chrono::high_resolution_clock::now();
	auto duration = now.time_since_epoch();
	float timestamp = std::chrono::duration<float>(duration).count();

	messages_.emplace_back(sender, contentUTF8, timestamp, isError);

	// メッセージ数を制限（最大100件）
	if (messages_.size() > 100) {
		messages_.pop_front();
	}
}

void ChatWindow::ScrollToBottom() {
	scrollToBottom_ = true;
}