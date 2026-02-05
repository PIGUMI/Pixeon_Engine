#pragma once
#include "Include/IScript.h"

// WinSock2のインクルード(順序重要)
#ifndef _WINSOCKAPI_
#define _WINSOCKAPI_
#endif
#include <WinSock2.h>
#include <WS2tcpip.h>

#include <map>
#include <vector>
#include <string>
#include <nlohmann/json.hpp>

#pragma comment(lib, "ws2_32.lib")
#pragma warning(disable:4996)

#define BUFFER_SIZE (4096)

struct OtherPlayerData
{
    Object playerObject = nullptr;
    Component rigidBodyComponent = nullptr;
	Component Animation = nullptr;

    // 現在の位置・回転
    Float3 Position;
    Float3 Rotation;

    // 補間用のターゲット位置・回転
    Float3 TargetPosition;
    Float3 TargetRotation;
    float InterpolationSpeed = 10.0f;

    int AnimationNo = 0;
    bool IsActive = false;

    // パケット順序管理
    int LastSequenceNumber = -1;
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
    Object myPlayerObject = nullptr;
    Object otherPlayerObject = nullptr;
    Component myRigidBody = nullptr;  // 自分のRigidBody
	Component Animation = nullptr;    // 自分のアニメーションコンポーネント
    std::vector<Object*> OtherPlayerObjects;

    // 送信タイマー
    float sendTimer = 0.0f;

    // ハートビート
    float heartbeatTimer = 0.0f;

    // 接続リトライ
    float connectionRetryTimer = 0.0f;
    bool isConnected = false;

    // パケットシーケンス番号
    int sequenceNumber = 0;

    // 初期化・終了
    void InitializeNetwork();
    void CleanupNetwork();

    // 送受信
    void SendConnectionRequest();
    void SendPlayerData();
    void SendHeartbeat();
    void ReceiveData();

    // プレイヤー管理
    void UpdateOtherPlayer(int userID, const Float3& pos, const Float3& rot, int animNo, int seqNum);
    void RemoveOtherPlayer(int userID);
    void InterpolateOtherPlayers(float DeltaTime);

    // RigidBody関連
    void SetupRigidBody(Object obj, Component& outRigidBody);
    void UpdateRigidBodyPosition(Component rigidBody, const Float3& targetPos, const Float3& targetRot, float deltaTime);

    // ユーティリティ
    Float3 LerpFloat3(const Float3& a, const Float3& b, float t);
    std::string GetSocketErrorMessage(int errorCode);
    void AddDebugLog(const std::string& message);

public:
    std::string ServerIP = "127.0.0.1";
    int ServerPort = 50008;
    float SendInterval = 0.05f;
    float HeartbeatInterval = 1.0f;
    float ConnectionRetryInterval = 2.0f;
    std::string PlayerPrefabName = "Player";
    std::string OtherPlayerPrefabName = "OtherPlayer";

    // 物理設定
    bool UsePhysicsForOtherPlayers = true;

    // デバッグ用変数(GUI表示)
    std::string DebugLog = "Initializing...";
    std::string ConnectionStatus = "Disconnected";
    int CurrentUserID = -1;
    int OtherPlayerCount = 0;

#define PROPERTY_LIST(ACTION) \
    ACTION(STRING, ServerIP) \
    ACTION(INT, ServerPort) \
    ACTION(FLOAT, SendInterval) \
    ACTION(FLOAT, HeartbeatInterval) \
    ACTION(FLOAT, ConnectionRetryInterval) \
    ACTION(STRING, PlayerPrefabName) \
    ACTION(STRING, OtherPlayerPrefabName) \
    ACTION(BOOL, UsePhysicsForOtherPlayers) \
    ACTION(STRING, DebugLog) \
    ACTION(STRING, ConnectionStatus) \
    ACTION(INT, CurrentUserID) \
    ACTION(INT, OtherPlayerCount)

    DECLARE_SCRIPT_PROPERTIES()
#undef PROPERTY_LIST
};

extern "C" __declspec(dllexport) IScript* CreateScriptInstance() {
    return new Script_Network();
}

extern "C" __declspec(dllexport) void DestroyScriptInstance(IScript* script) {
    delete script;
}