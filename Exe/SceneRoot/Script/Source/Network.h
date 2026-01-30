#pragma once
#include "Include/IScript.h"
#include <nlohmann/json.hpp>
#include <WinSock2.h>
#include <map>
#include <string>

#pragma comment(lib, "ws2_32.lib")
#pragma warning(disable:4996)

#define BUFFER_SIZE (4096)

struct OtherPlayerData
{
    Object playerObject = nullptr;
    Float3 Position;
    Float3 Rotation;
    int AnimationNo = 0;
    bool IsActive = false;
};

class Script_Network : public IScript {
public:
    void BeginPlay() override;
    void Update(float DeltaTime) override;
    void EndPlay() override;

private:
    // ネットワーク関連
    int sock = -1;
    struct sockaddr_in server_addr;
    bool isInitialized = false;
    int myUserID = -1;

    // 送受信バッファ
    char recvBuffer[BUFFER_SIZE];

    // 他プレイヤー管理
    std::map<int, OtherPlayerData> otherPlayers;

    // 送信タイマー
    float sendTimer = 0.0f;

    // 初期化・終了
    void InitializeNetwork();
    void CleanupNetwork();

    // 送受信
    void SendPlayerData();
    void ReceiveData();

    // プレイヤー管理
    void UpdateOtherPlayer(int userID, const Float3& pos, const Float3& rot, int animNo);
    void RemoveOtherPlayer(int userID);

public:
    // プロパティ
    std::string ServerIP = "127.0.0.1";
    int ServerPort = 50008;
    float SendInterval = 0.05f;  // 送信間隔(秒)
    std::string PlayerPrefabName = "Player";  // 他プレイヤー用のPrefab名

#define PROPERTY_LIST(ACTION) \
    ACTION(INT, ServerPort) \
    ACTION(FLOAT, SendInterval)

    DECLARE_SCRIPT_PROPERTIES()
#undef PROPERTY_LIST
};

extern "C" __declspec(dllexport) IScript* CreateScriptInstance() {
    return new Script_Network();
}

extern "C" __declspec(dllexport) void DestroyScriptInstance(IScript* script) {
    delete script;
}