#include "Network.h"
#include <iostream>
#include <cmath>
#include <sstream>

void Script_Network::BeginPlay() {
    IScript::BeginPlay();
    AddDebugLog("BeginPlay called");
    InitializeNetwork();
	Object ParentObject = nullptr;
    FindObjectByName(_parentScene, PlayerPrefabName.c_str(), &ParentObject);
	FindChildObjectByName(ParentObject, "Body", &myPlayerObject);
    FindPrefabObjectByName(OtherPlayerPrefabName.c_str(), &otherPlayerObject);

    if (myPlayerObject != nullptr) {
        AddDebugLog("Player object found");
        // 自分のRigidBodyコンポーネントを取得
        FindComponent(myPlayerObject, "RigidBody", &myRigidBody);
        if (myRigidBody != nullptr) {
            AddDebugLog("Player RigidBody found");
        }
		FindComponent(myPlayerObject, "Animation", &Animation);
    }
    else {
        AddDebugLog("ERROR: Player object not found!");
    }

    if (otherPlayerObject != nullptr) {
        AddDebugLog("OtherPlayer prefab found");
    }
    else {
        AddDebugLog("ERROR: OtherPlayer prefab not found!");
    }
}

void Script_Network::Update(float DeltaTime) {
    IScript::Update(DeltaTime);

    if (!isInitialized) {
        ConnectionStatus = "Init Failed";
        return;
    }

    // データ受信
    ReceiveData();

    // 接続確立されていない場合は接続リトライ
    if (!isConnected) {
        connectionRetryTimer += DeltaTime;
        if (connectionRetryTimer >= ConnectionRetryInterval) {
            SendConnectionRequest();
            connectionRetryTimer = 0.0f;
        }
        ConnectionStatus = "Connecting...";
        return;
    }

    // 接続確立後の処理
    ConnectionStatus = "Connected";
    CurrentUserID = myUserID;
    OtherPlayerCount = static_cast<int>(otherPlayers.size());

    // 他プレイヤーの位置を補間
    InterpolateOtherPlayers(DeltaTime);

    // 定期的にプレイヤーデータ送信
    sendTimer += DeltaTime;
    if (sendTimer >= SendInterval)
    {
        SendPlayerData();
        sendTimer = 0.0f;
    }

    // 定期的にハートビート送信
    heartbeatTimer += DeltaTime;
    if (heartbeatTimer >= HeartbeatInterval)
    {
        SendHeartbeat();
        heartbeatTimer = 0.0f;
    }
}

void Script_Network::EndPlay() {
    // 切断メッセージを送信
    if (isInitialized && myUserID != -1)
    {
        try {
            nlohmann::json disconnectMsg;
            disconnectMsg["type"] = "disconnect";
            disconnectMsg["UserID"] = myUserID;
            std::string data_str = disconnectMsg.dump();

            sendto(sock, data_str.c_str(), static_cast<int>(data_str.length()), 0,
                (struct sockaddr*)&server_addr, sizeof(server_addr));

            AddDebugLog("Disconnect message sent");
        }
        catch (...) {
            // エラーは無視
        }
    }

    CleanupNetwork();
    IScript::EndPlay();
}


void Script_Network::InitializeNetwork()
{
    AddDebugLog("InitializeNetwork start");

    WSAData wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
    {
        AddDebugLog("ERROR: WSAStartup failed");
        ConnectionStatus = "WSAStartup Failed";
        isInitialized = false;
        return;
    }
    AddDebugLog("WSAStartup success");

    sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock == INVALID_SOCKET)
    {
        AddDebugLog("ERROR: Socket creation failed");
        ConnectionStatus = "Socket Failed";
        WSACleanup();
        isInitialized = false;
        return;
    }
    AddDebugLog("Socket created");

    // ノンブロッキングモード
    u_long val = 1;
    if (ioctlsocket(sock, FIONBIO, &val) != 0)
    {
        AddDebugLog("ERROR: Non-blocking mode failed");
        ConnectionStatus = "NonBlock Failed";
        closesocket(sock);
        WSACleanup();
        isInitialized = false;
        return;
    }
    AddDebugLog("Non-blocking mode set");

    // サーバーアドレス設定
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(static_cast<u_short>(ServerPort));
    server_addr.sin_addr.S_un.S_addr = inet_addr(ServerIP.c_str());

    isInitialized = true;
    myUserID = -1;
    sequenceNumber = 0;
    isConnected = false;

    std::stringstream ss;
    ss << "Network initialized: " << ServerIP << ":" << ServerPort;
    AddDebugLog(ss.str());
    ConnectionStatus = "Initialized";

    // 初回接続要求を送信
    SendConnectionRequest();
}

void Script_Network::CleanupNetwork()
{
    if (sock != -1)
    {
        closesocket(sock);
        sock = -1;
    }
    WSACleanup();

    isInitialized = false;
    isConnected = false;
    AddDebugLog("Network shutdown");
    ConnectionStatus = "Disconnected";
}

void Script_Network::SendConnectionRequest()
{
    if (!isInitialized) return;

    try {
        nlohmann::json connectMsg;
        connectMsg["type"] = "connect";
        connectMsg["UserID"] = -1;  // 初回は-1

        std::string data_str = connectMsg.dump();

        int sent = sendto(sock, data_str.c_str(), static_cast<int>(data_str.length()), 0,
            (struct sockaddr*)&server_addr, sizeof(server_addr));

        if (sent == SOCKET_ERROR) {
            int error = WSAGetLastError();
            if (error != WSAEWOULDBLOCK) {
                std::stringstream ss;
                ss << "Connection request send error: " << GetSocketErrorMessage(error);
                AddDebugLog(ss.str());
            }
        }
        else {
            AddDebugLog("Connection request sent");
        }
    }
    catch (...) {
        AddDebugLog("ERROR: Connection request exception");
        return;
    }
}

void Script_Network::SendPlayerData()
{
    if (!isInitialized || !isConnected || myPlayerObject == nullptr || myUserID == -1) return;

    transform trans;
    int AnimationNo = 0;
    if (GetObjectWorldTransform(myPlayerObject, &trans) != PN_SUCCESS) return;
    if (GetAnimationClip(Animation, &AnimationNo) != PN_SUCCESS) {
        AnimationNo = 0;
	}
    

    try {
        nlohmann::json sendData;
        sendData["type"] = "update";
        sendData["UserID"] = myUserID;
        sendData["seq"] = sequenceNumber++;
        sendData["Pos"]["X"] = trans.position.x;
        sendData["Pos"]["Y"] = trans.position.y;
        sendData["Pos"]["Z"] = trans.position.z;
        sendData["Rot"]["X"] = trans.rotation.x;
        sendData["Rot"]["Y"] = trans.rotation.y;
        sendData["Rot"]["Z"] = trans.rotation.z;
        sendData["AnimationNo"] = AnimationNo;

        std::string data_str = sendData.dump();

        int server_addr_len = sizeof(server_addr);
        int sent = sendto(sock, data_str.c_str(), static_cast<int>(data_str.length()), 0,
            (struct sockaddr*)&server_addr, server_addr_len);

        if (sent == SOCKET_ERROR)
        {
            int error = WSAGetLastError();
            if (error != WSAEWOULDBLOCK)
            {
                std::stringstream ss;
                ss << "Send error: " << GetSocketErrorMessage(error);
                AddDebugLog(ss.str());
            }
        }
    }
    catch (...) {
        AddDebugLog("ERROR: SendPlayerData exception");
        return;
    }
}

void Script_Network::SendHeartbeat()
{
    if (!isInitialized || myUserID == -1) return;

    try {
        nlohmann::json heartbeat;
        heartbeat["type"] = "ping";
        heartbeat["UserID"] = myUserID;

        std::string data_str = heartbeat.dump();
        sendto(sock, data_str.c_str(), static_cast<int>(data_str.length()), 0,
            (struct sockaddr*)&server_addr, sizeof(server_addr));
    }
    catch (...) {
        // エラーは無視
        return;
    }
}

void Script_Network::ReceiveData()
{
    if (!isInitialized) return;

    int packetsReceived = 0;
    while (true)
    {
        int recv_size = recvfrom(sock, recvBuffer, BUFFER_SIZE - 1, 0, nullptr, nullptr);

        if (recv_size <= 0)
        {
            int error = WSAGetLastError();
            if (error != WSAEWOULDBLOCK)
            {
            }
            break;
        }

        packetsReceived++;

        recvBuffer[recv_size] = '\0';

        try
        {
            nlohmann::json received = nlohmann::json::parse(recvBuffer);

            // メッセージタイプ判定
            std::string msgType = "update";  // デフォルト
            if (received.contains("type"))
            {
                msgType = received["type"];
            }

            // 初回接続時のUserID割り当て
            if (received.contains("MyUserID"))
            {
                myUserID = received["MyUserID"];
                CurrentUserID = myUserID;
                isConnected = true;

                std::stringstream ss;
                ss << "Connected! UserID: " << myUserID;
                AddDebugLog(ss.str());
                ConnectionStatus = "Connected";
                continue;
            }

            // 切断通知
            if (msgType == "disconnect")
            {
                if (received.contains("UserID"))
                {
                    int disconnectedUserID = received["UserID"];
                    std::stringstream ss;
                    ss << "Player disconnected: UserID=" << disconnectedUserID;
                    AddDebugLog(ss.str());
                    RemoveOtherPlayer(disconnectedUserID);
                }
                continue;
            }

            // 他プレイヤーの情報更新
            if (msgType == "update" && received.contains("UserID"))
            {
                int otherUserID = received["UserID"];

                // 自分自身は無視
                if (otherUserID == myUserID) continue;

                Float3 pos = { 0, 0, 0 };
                Float3 rot = { 0, 0, 0 };
                int animNo = 0;
                int seqNum = -1;

                if (received.contains("Pos"))
                {
                    pos.x = received["Pos"]["X"];
                    pos.y = received["Pos"]["Y"];
                    pos.z = received["Pos"]["Z"];
                }
                if (received.contains("Rot"))
                {
                    rot.x = received["Rot"]["X"];
                    rot.y = received["Rot"]["Y"];
                    rot.z = received["Rot"]["Z"];
                }
                if (received.contains("AnimationNo"))
                {
                    animNo = received["AnimationNo"];
                }
                if (received.contains("seq"))
                {
                    seqNum = received["seq"];
                }

                UpdateOtherPlayer(otherUserID, pos, rot, animNo, seqNum);
            }
        }
        catch (...)
        {
            // JSONパースエラーは無視
            continue;
        }
    }
}


void Script_Network::UpdateOtherPlayer(int userID, const Float3& pos, const Float3& rot, int animNo, int seqNum)
{
    auto it = otherPlayers.find(userID);

    if (it == otherPlayers.end())
    {
        // 新規プレイヤー生成
        OtherPlayerData newPlayer;

        Object clonedObj = nullptr;
		Component AnimComp = nullptr;
        if (AddObjectToScene(_parentScene, otherPlayerObject, &clonedObj) == PN_SUCCESS)
        {
            newPlayer.playerObject = clonedObj;

            // オブジェクト名を設定
            std::stringstream ss;
            ss << "OtherPlayer_" << userID;
            SetObjectName(clonedObj, ss.str().c_str());

            // 初期位置と回転を設定
            SetObjectPosition(clonedObj, pos);
            SetObjectRotation(clonedObj, rot);
            
            // Animationコンポーネントのセットアップ
            FindComponent(clonedObj, "Animation", &AnimComp);
            newPlayer.Animation = AnimComp;
			SetAnimationClip(AnimComp, animNo);

            // RigidBodyのセットアップ
            if (UsePhysicsForOtherPlayers) {
                SetupRigidBody(clonedObj, newPlayer.rigidBodyComponent);
            }

            ss.str("");
            ss << "New player created: UserID=" << userID;
            AddDebugLog(ss.str());
        }

        newPlayer.Position = pos;
        newPlayer.Rotation = rot;
        newPlayer.TargetPosition = pos;
        newPlayer.TargetRotation = rot;
        newPlayer.AnimationNo = animNo;
        newPlayer.IsActive = true;
        newPlayer.LastSequenceNumber = seqNum;

        otherPlayers[userID] = newPlayer;
        OtherPlayerCount = static_cast<int>(otherPlayers.size());
    }
    else
    {
        if (seqNum != -1 && seqNum <= it->second.LastSequenceNumber)
        {
            return;
        }

        it->second.TargetPosition = pos;
        it->second.TargetRotation = rot;
        it->second.AnimationNo = animNo;
        it->second.LastSequenceNumber = seqNum;
    }
}

void Script_Network::RemoveOtherPlayer(int userID)
{
    auto it = otherPlayers.find(userID);
    if (it == otherPlayers.end()) return;

    Scene currentScene;
    if (GetCurrentScene(&currentScene) == PN_SUCCESS)
    {
        if (it->second.playerObject != nullptr)
        {
            RemoveObjectFromScene(currentScene, it->second.playerObject);
        }
    }

    otherPlayers.erase(it);
    OtherPlayerCount = static_cast<int>(otherPlayers.size());
}

void Script_Network::InterpolateOtherPlayers(float DeltaTime)
{
    for (auto it = otherPlayers.begin(); it != otherPlayers.end(); ++it)
    {
        OtherPlayerData& playerData = it->second;

        if (playerData.playerObject == nullptr) continue;

        // RigidBodyを使用する場合は専用の更新処理
        if (UsePhysicsForOtherPlayers && playerData.rigidBodyComponent != nullptr)
        {
            UpdateRigidBodyPosition(playerData.rigidBodyComponent,
                playerData.TargetPosition,
                playerData.TargetRotation,
                DeltaTime);
        }
        else
        {
            // 通常の補間処理
            Float3 currentPos, currentRot;
            if (GetObjectPosition(playerData.playerObject, &currentPos) != PN_SUCCESS) continue;
            if (GetObjectRotation(playerData.playerObject, &currentRot) != PN_SUCCESS) continue;

            // 補間係数を計算(指数減衰)
            double expValue = exp(-static_cast<double>(playerData.InterpolationSpeed) * static_cast<double>(DeltaTime));
            float t = 1.0f - static_cast<float>(expValue);

            // 位置を補間
            Float3 newPos = LerpFloat3(currentPos, playerData.TargetPosition, t);
            SetObjectPosition(playerData.playerObject, newPos);

            // 回転を補間
            Float3 newRot = LerpFloat3(currentRot, playerData.TargetRotation, t);
            SetObjectRotation(playerData.playerObject, newRot);

            // 内部状態も更新
            playerData.Position = newPos;
            playerData.Rotation = newRot;
        }
		// アニメーション更新
        if (playerData.Animation != nullptr)
        {
			int NowAnimNo = -1;
			GetAnimationClip(playerData.Animation, &NowAnimNo);
			if (NowAnimNo != playerData.AnimationNo)SetAnimationClip(playerData.Animation, playerData.AnimationNo);
        }
    }
}

void Script_Network::SetupRigidBody(Object obj, Component& outRigidBody)
{
    if (FindComponent(obj, "RigidBody", &outRigidBody) != PN_SUCCESS)
    {
        AddDebugLog("WARNING: RigidBody not found on OtherPlayer");
        outRigidBody = nullptr;
        return;
    }

    if (RigidBodySetKinematic(outRigidBody, true) == PN_SUCCESS)
    {
        AddDebugLog("OtherPlayer RigidBody set to Kinematic");
    }

    if (RigidBodySetUseGravity(outRigidBody, false) == PN_SUCCESS)
    {
        AddDebugLog("OtherPlayer gravity disabled");
    }
}

void Script_Network::UpdateRigidBodyPosition(Component rigidBody, const Float3& targetPos, const Float3& targetRot, float deltaTime)
{
    if (rigidBody == nullptr) return;

    Float3 currentPos;
    Object parentObj = nullptr;

    Float3 currentVel;
    if (RigidBodyGetVelocity(rigidBody, &currentVel) == PN_SUCCESS)
    {
    }

    for (auto& pair : otherPlayers)
    {
        if (pair.second.rigidBodyComponent == rigidBody)
        {
            SetObjectPosition(pair.second.playerObject, targetPos);
            SetObjectRotation(pair.second.playerObject, targetRot);
            pair.second.Position = targetPos;
            pair.second.Rotation = targetRot;
            break;
        }
    }
}

Float3 Script_Network::LerpFloat3(const Float3& a, const Float3& b, float t)
{
    Float3 result;
    result.x = a.x + (b.x - a.x) * t;
    result.y = a.y + (b.y - a.y) * t;
    result.z = a.z + (b.z - a.z) * t;
    return result;
}

std::string Script_Network::GetSocketErrorMessage(int errorCode)
{
    switch (errorCode)
    {
    case WSAEWOULDBLOCK:
        return "WSAEWOULDBLOCK";
    case WSAENOTCONN:
        return "WSAENOTCONN";
    case WSAECONNRESET:
        return "WSAECONNRESET";
    case WSAENETUNREACH:
        return "WSAENETUNREACH";
    case WSAEHOSTUNREACH:
        return "WSAEHOSTUNREACH";
    case WSAETIMEDOUT:
        return "WSAETIMEDOUT";
    default:
        std::stringstream ss;
        ss << "Error:" << errorCode;
        return ss.str();
    }
}

void Script_Network::AddDebugLog(const std::string& message)
{
    std::string newLog = message + "\n" + DebugLog;
    if (newLog.length() > 1000) {
        newLog = newLog.substr(0, 1000);
    }
    DebugLog = newLog;
}