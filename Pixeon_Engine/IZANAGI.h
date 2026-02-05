/*
* IZANAGI
* OfflineLLM Project
*/

#ifndef IZANAGI_H
#define IZANAGI_H

#include <string>
#include <thread>
#include <mutex>
#include <atomic>
#include <vector>
#include <queue>
#include "llama.h"

class IZANAGI
{
public:
	static IZANAGI* GetInstance();
	static void DestroyInstance();

	bool Initialize(const std::string& modelPath, int contextSize, int threads, int gpuLayers);
	bool GetInitialized() const { return isInitialized; }
	void SendAIMessage(const std::string& message);
	std::string GetAIMessage();
	bool Processing();

	void Shutdown();

    bool LoadPersona(const std::string& personaPath);
    void SetSystemPrompt(const std::string& prompt);
    std::string GetSystemPrompt() const;
    void ClearConversationHistory();
private:
    llama_model* model;
    llama_context* context;
    llama_sampler* sampler;
    const llama_vocab* vocab;

    std::thread processingThread;
    std::mutex messageMutex;
    std::atomic<bool> isProcessing;
    std::atomic<bool> shouldExit;
    bool isInitialized;

    std::queue<std::string> inputQueue;
    std::string currentResponse;
    std::string latestResponse;

    std::vector<std::string> conversationHistory;

    std::string systemPrompt;
    int gpuLayerCount;

    void ProcessingLoop();
    std::string GenerateResponse(const std::string& userMessage);
    std::vector<llama_token> Tokenize(const std::string& text, bool addBos);
    std::string FormatPrompt(const std::string& userMessage);

    std::string UTF8ToShiftJIS(const std::string& utf8str);
    std::string ShiftJISToUTF8(const std::string& sjisstr);
private:
	static IZANAGI* instance;
	IZANAGI();
	~IZANAGI();
};

#endif // IZANAGI_H

