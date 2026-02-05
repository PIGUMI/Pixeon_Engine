#define NOMINMAX
#include "IZANAGI.h"
#include <iostream>
#include <sstream>
#include <fstream>
#include <algorithm>
#include <chrono>

#ifdef _WIN32
#include <windows.h>
#endif

IZANAGI* IZANAGI::instance = nullptr;

/*
* 関数名　GetInstance
* 引　数　なし
* 戻り値　IZANAGI*：IZANAGIのインスタンスへのポインタ
* 説　明　IZANAGIのシングルトンインスタンスを取得する関数
*/
IZANAGI* IZANAGI::GetInstance() {
	if (instance == nullptr) {
		instance = new IZANAGI();
	}
	return instance;
}

/*
* 関数名　DestroyInstance
* 引　数　なし
* 戻り値　なし
* 説　明　IZANAGIのシングルトンインスタンスを破棄する関数
*/
void IZANAGI::DestroyInstance() {
	if (instance != nullptr) {
		delete instance;
		instance = nullptr;
	}
}

/*
* 関数名　IZANAGI（コンストラクタ）
* 引　数　なし
* 戻り値　なし
* 説　明　IZANAGIクラスのコンストラクタ
*/
IZANAGI::IZANAGI()
	: model(nullptr)
	, context(nullptr)
	, sampler(nullptr)
	, vocab(nullptr)
	, isProcessing(false)
	, shouldExit(false)
	, isInitialized(false)
	, systemPrompt("You are a helpful assistant.")
	, gpuLayerCount(0) {
}

/*
* 関数名　~IZANAGI（デストラクタ）
* 引　数　なし
* 戻り値　なし
* 説　明　IZANAGIクラスのデストラクタ
*/
IZANAGI::~IZANAGI() {
	if(isInitialized)Shutdown();
}

/*
* 関数名　UTF8ToShiftJIS
* 引　数　const std::string& utf8str：UTF-8エンコードされた文字列
* 戻り値　std::string：Shift_JISエンコードされた文字列
* 説　明　UTF-8エンコードされた文字列をShift_JISエンコードに変換する関数
*/
std::string IZANAGI::UTF8ToShiftJIS(const std::string& utf8str) {
#ifdef _WIN32
    if (utf8str.empty()) return "";

    int wideSize = MultiByteToWideChar(CP_UTF8, 0, utf8str.c_str(), -1, NULL, 0);
    if (wideSize == 0) return utf8str;

    std::vector<wchar_t> wideStr(wideSize);
    MultiByteToWideChar(CP_UTF8, 0, utf8str.c_str(), -1, &wideStr[0], wideSize);

    int sjisSize = WideCharToMultiByte(CP_ACP, 0, &wideStr[0], -1, NULL, 0, NULL, NULL);
    if (sjisSize == 0) return utf8str;

    std::vector<char> sjisStr(sjisSize);
    WideCharToMultiByte(CP_ACP, 0, &wideStr[0], -1, &sjisStr[0], sjisSize, NULL, NULL);

    return std::string(&sjisStr[0]);
#else
    return utf8str;
#endif
}

/*
* 関数名　ShiftJISToUTF8
* 引　数　const std::string& sjisstr：Shift_JISエンコードされた文字列
* 戻り値　std::string：UTF-8エンコードされた文字列
* 説　明　Shift_JISエンコードされた文字列をUTF-8エンコードに変換する関数
*/
std::string IZANAGI::ShiftJISToUTF8(const std::string& sjisstr) {
#ifdef _WIN32
    if (sjisstr.empty()) return "";

    int wideSize = MultiByteToWideChar(CP_ACP, 0, sjisstr.c_str(), -1, NULL, 0);
    if (wideSize == 0) return sjisstr;

    std::vector<wchar_t> wideStr(wideSize);
    MultiByteToWideChar(CP_ACP, 0, sjisstr.c_str(), -1, &wideStr[0], wideSize);

    int utf8Size = WideCharToMultiByte(CP_UTF8, 0, &wideStr[0], -1, NULL, 0, NULL, NULL);
    if (utf8Size == 0) return sjisstr;

    std::vector<char> utf8Str(utf8Size);
    WideCharToMultiByte(CP_UTF8, 0, &wideStr[0], -1, &utf8Str[0], utf8Size, NULL, NULL);

    return std::string(&utf8Str[0]);
#else
    return sjisstr;
#endif
}

/*
* 関数名　LoadPersona
* 引　数　const std::string& personaPath：ペルソナファイルのパス
* 戻り値　bool：読み込み成功ならtrue、失敗ならfalse
* 説　明　指定されたペルソナファイルを読み込み、システムプロンプトとして設定する関数
*/
bool IZANAGI::LoadPersona(const std::string& personaPath) {
    std::ifstream file(personaPath, std::ios::binary);
    if (!file.is_open()) {
        return false;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    file.close();

    std::string fileContent = buffer.str();

    if (fileContent.length() >= 3 &&
        (unsigned char)fileContent[0] == 0xEF &&
        (unsigned char)fileContent[1] == 0xBB &&
        (unsigned char)fileContent[2] == 0xBF) {
        fileContent = fileContent.substr(3);
    }

    systemPrompt = fileContent;

    ClearConversationHistory();
    return true;
}

/*
* 関数名　SetSystemPrompt
* 引　数　const std::string& prompt：新しいシステムプロンプト
* 戻り値　なし
* 説　明　システムプロンプトを設定し、会話履歴をクリアする関数
*/
void IZANAGI::SetSystemPrompt(const std::string& prompt) {
    systemPrompt = prompt;
    ClearConversationHistory();
}

/*
* 関数名　GetSystemPrompt
* 引　数　なし
* 戻り値　std::string：現在のシステムプロンプト
* 説　明　現在のシステムプロンプトを取得する関数
*/
std::string IZANAGI::GetSystemPrompt() const {
    return systemPrompt;
}

/*
* 関数名　ClearConversationHistory
* 引　数　なし
* 戻り値　なし
* 説　明　会話履歴をクリアする関数
*/
void IZANAGI::ClearConversationHistory() {
    std::lock_guard<std::mutex> lock(messageMutex);
    conversationHistory.clear();
}

/*
* 関数名　Initialize
* 引　数　const std::string& modelPath：モデルファイルのパス
* 	   int contextSize：コンテキストサイズ
* 	   int threads：使用するスレッド数
* 	   int gpuLayers：GPUに割り当てるレイヤー数
* 戻り値　bool：初期化成功ならtrue、失敗ならfalse
* 説　明　IZANAGIエンジンを初期化する関数
*/
bool IZANAGI::Initialize(const std::string& modelPath, int contextSize, int threads, int gpuLayers) {

    if (threads <= 0) {
        threads = std::thread::hardware_concurrency() / 2; // 物理コア数の推定
        if (threads < 1) threads = 4;
    }

    llama_backend_init();

    llama_model_params model_params = llama_model_default_params();
    model_params.n_gpu_layers = 0;

    model_params.use_mmap = true;
    model_params.use_mlock = false;

    model = llama_model_load_from_file(modelPath.c_str(), model_params);
    if (!model) {
        return false;
    }

    vocab = llama_model_get_vocab(model);
    if (!vocab) {
        llama_model_free(model);
        model = nullptr;
        return false;
    }

    llama_context_params ctx_params = llama_context_default_params();
    ctx_params.n_ctx = contextSize;
    ctx_params.n_threads = threads;
    ctx_params.n_threads_batch = threads * 2;

    ctx_params.no_perf = false;

    context = llama_init_from_model(model, ctx_params);
    if (!context) {
        llama_model_free(model);
        model = nullptr;
        vocab = nullptr;
        return false;
    }

    sampler = llama_sampler_chain_init(llama_sampler_chain_default_params());
    llama_sampler_chain_add(sampler, llama_sampler_init_temp(0.8f));
    llama_sampler_chain_add(sampler, llama_sampler_init_top_k(40));
    llama_sampler_chain_add(sampler, llama_sampler_init_top_p(0.9f, 1));
    llama_sampler_chain_add(sampler, llama_sampler_init_dist(LLAMA_DEFAULT_SEED));

    shouldExit.store(false);

    processingThread = std::thread(&IZANAGI::ProcessingLoop, this);

    isInitialized = true;

    return true;
}

/*
* 関数名　Shutdown
* 引　数　なし
* 戻り値　なし
* 説　明　IZANAGIエンジンをシャットダウンする関数
*/
void IZANAGI::SendAIMessage(const std::string& messageUTF8) {
    std::lock_guard<std::mutex> lock(messageMutex);
    inputQueue.push(messageUTF8);
    latestResponse.clear();
}

/*
* 関数名　GetAIMessage
* 引　数　なし
* 戻り値　std::string：最新のAI応答メッセージ（Shift_JISエンコード）
* 説　明　最新のAI応答メッセージを取得する関数
*/
std::string IZANAGI::GetAIMessage() {
    std::lock_guard<std::mutex> lock(messageMutex);
    return latestResponse;
}

/*
* 関数名　Processing
* 引　数　なし
* 戻り値　bool：処理中ならtrue、そうでなければfalse
* 説　明　IZANAGIエンジンが現在処理中かどうかを確認する関数
*/
bool IZANAGI::Processing() {
    return isProcessing.load();
}

/*
* 関数名　Shutdown
* 引　数　なし
* 戻り値　なし
* 説　明　IZANAGIエンジンのメイン処理ループ
*/
void IZANAGI::ProcessingLoop() {
    while (!shouldExit.load()) {
        std::string messageToProcess;

        {
            std::lock_guard<std::mutex> lock(messageMutex);
            if (!inputQueue.empty()) {
                messageToProcess = inputQueue.front();
                inputQueue.pop();
            }
        }

        if (!messageToProcess.empty()) {
            isProcessing.store(true);

            std::string response = GenerateResponse(messageToProcess);

            {
                std::lock_guard<std::mutex> lock(messageMutex);
                latestResponse = response;
            }

            isProcessing.store(false);
        }
        else {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }
}

/*
* 関数名　GenerateResponse
* 引　数　const std::string& userMessage：ユーザーメッセージ
* 戻り値　std::string：生成された応答メッセージ
* 説　明　ユーザーメッセージに基づいて応答メッセージを生成する関数
*/
std::string IZANAGI::GenerateResponse(const std::string& userMessage) {
    auto start_time = std::chrono::high_resolution_clock::now();

    conversationHistory.push_back("User: " + userMessage);

    std::string prompt = FormatPrompt(userMessage);

    std::vector<llama_token> tokens = Tokenize(prompt, false);

    if (tokens.empty()) {
        return "エラー: トークン化に失敗しました";
    }

    uint32_t n_ctx = llama_n_ctx(context);
    if (tokens.size() >= n_ctx) {

        if (conversationHistory.size() > 2) {
            std::vector<std::string> temp(conversationHistory.end() - 2, conversationHistory.end());
            conversationHistory = temp;

            prompt = FormatPrompt(userMessage);
            tokens = Tokenize(prompt, false);

            if (tokens.size() >= n_ctx) {
                return "エラー: プロンプトが長すぎます（会話履歴を削減しても収まりません）";
            }
        }
    }

    static bool first_message = true;
    if (first_message || conversationHistory.size() <= 1) {
        llama_memory_t memory = llama_get_memory(context);
        llama_memory_clear(memory, false);
        first_message = false;
    }

    llama_batch batch = llama_batch_get_one(tokens.data(), tokens.size());

    auto decode_start = std::chrono::high_resolution_clock::now();
    int decode_result = llama_decode(context, batch);
    auto decode_end = std::chrono::high_resolution_clock::now();

    auto decode_ms = std::chrono::duration_cast<std::chrono::milliseconds>(decode_end - decode_start).count();

    if (decode_result != 0) {
        return "エラー: 推論に失敗しました";
    }

    std::string response;
    const int maxTokens = 256;

    auto gen_start = std::chrono::high_resolution_clock::now();
    int token_count = 0;

    for (int i = 0; i < maxTokens; ++i) {
        if (shouldExit.load()) {
            break;
        }

        llama_token newToken = llama_sampler_sample(sampler, context, -1);

        if (llama_vocab_is_eog(vocab, newToken)) {
            token_count = i;
            break;
        }

        char buf[256];
        std::memset(buf, 0, sizeof(buf));
        int n = llama_token_to_piece(vocab, newToken, buf, sizeof(buf) - 1, 0, true);

        if (n > 0) {
            std::string piece(buf, n);
            response.append(piece);

            if (i % 10 == 0 && i > 0) {
                std::cout << "." << std::flush;
            }
        }

        llama_batch nextBatch = llama_batch_get_one(&newToken, 1);
        if (llama_decode(context, nextBatch) != 0) {
            break;
        }

        token_count = i + 1;
    }

    auto gen_end = std::chrono::high_resolution_clock::now();
    auto gen_ms = std::chrono::duration_cast<std::chrono::milliseconds>(gen_end - gen_start).count();

    if (gen_ms > 0 && token_count > 0) {
        double tokens_per_sec = (token_count * 1000.0) / gen_ms;
    }

    size_t end_pos = response.find("<|im_end|>");
    if (end_pos != std::string::npos) {
        response = response.substr(0, end_pos);
    }

    size_t start = response.find_first_not_of(" \t\n\r");
    size_t end = response.find_last_not_of(" \t\n\r");
    if (start != std::string::npos && end != std::string::npos) {
        response = response.substr(start, end - start + 1);
    }

    conversationHistory.push_back("Assistant: " + response);

    auto end_time = std::chrono::high_resolution_clock::now();
    auto total_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();

    return response;
}

/*
* 関数名　Tokenize
* 引　数　const std::string& text：トークン化するテキスト
* 	   bool addBos：先頭にBOSトークンを追加するかどうか
* 戻り値　std::vector<llama_token>：トークンのベクター
* 説　明　指定されたテキストをトークン化する関数
*/
std::vector<llama_token> IZANAGI::Tokenize(const std::string& text, bool addBos) {
    int nTokens = text.length() + (addBos ? 1 : 0) + 1;
    std::vector<llama_token> tokens(nTokens);

    int n = llama_tokenize(vocab, text.c_str(), text.length(),
        tokens.data(), tokens.size(), addBos, false);

    if (n < 0) {
        tokens.resize(-n);
        n = llama_tokenize(vocab, text.c_str(), text.length(),
            tokens.data(), tokens.size(), addBos, false);
    }

    tokens.resize(n);
    return tokens;
}

/*
* 関数名　FormatPrompt
* 引　数　const std::string& userMessage：ユーザーメッセージ
* 戻り値　std::string：フォーマットされたプロンプト文字列
* 説　明　ユーザーメッセージと会話履歴を基にプロンプトをフォーマットする関数
*/
std::string IZANAGI::FormatPrompt(const std::string& userMessage) {
    std::stringstream ss;

    ss << "<|im_start|>system\n";
    ss << systemPrompt << "<|im_end|>\n";

    // 【最適化】会話履歴を最新5件のみに制限（10→5）
    int historyStart = std::max(0, (int)conversationHistory.size() - 5);
    for (int i = historyStart; i < conversationHistory.size(); ++i) {
        if (conversationHistory[i].find("User: ") == 0) {
            ss << "<|im_start|>user\n";
            ss << conversationHistory[i].substr(6) << "<|im_end|>\n";
        }
        else if (conversationHistory[i].find("Assistant: ") == 0) {
            ss << "<|im_start|>assistant\n";
            ss << conversationHistory[i].substr(11) << "<|im_end|>\n";
        }
    }

    ss << "<|im_start|>user\n";
    ss << userMessage << "<|im_end|>\n";
    ss << "<|im_start|>assistant\n";

    return ss.str();
}

/*
* 関数名　Shutdown
* 引　数　なし
* 戻り値　なし
* 説　明　IZANAGIエンジンをシャットダウンする関数
*/
void IZANAGI::Shutdown() {
    if (!isInitialized) {
        return;
    }

    shouldExit.store(true);

    if (processingThread.joinable()) {
        processingThread.join();
    }

    if (sampler) {
        llama_sampler_free(sampler);
        sampler = nullptr;
    }

    if (context) {
        llama_free(context);
        context = nullptr;
    }

    if (model) {
        llama_model_free(model);
        model = nullptr;
    }

    vocab = nullptr;
    llama_backend_free();

    isInitialized = false;
}