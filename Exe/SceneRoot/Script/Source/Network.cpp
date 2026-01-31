#include "Network.h"
#include <iostream>

void Script_Network::BeginPlay() {
    IScript::BeginPlay();
    InitializeNetwork();
    FindObjectByName(_parentScene, "Player", &myPlayerObject);
    FindPrefabObjectByName("OtherPlayer", &otherPlayerObject);
}

void Script_Network::Update(float DeltaTime) {
    IScript::Update(DeltaTime);

    if (!isInitialized) return;

    // データ受信
    ReceiveData();

    // 定期的にデータ送信
    sendTimer += DeltaTime;
    if (sendTimer >= SendInterval)
    {
        SendPlayerData();
        sendTimer = 0.0f;
    }
}

void Script_Network::EndPlay() {
    CleanupNetwork();
    IScript::EndPlay();
}


void Script_Network::InitializeNetwork()
{
    WSAData wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
    {
        std::cout << "WSAStartup失敗\n";
        return;
    }

    sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock == INVALID_SOCKET)
    {
        std::cout << "ソケット作成失敗\n";
        WSACleanup();
        return;
    }

    // ノンブロッキングモード
    u_long val = 1;
    ioctlsocket(sock, FIONBIO, &val);

    // サーバーアドレス設定
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(ServerPort);
    server_addr.sin_addr.S_un.S_addr = inet_addr(ServerIP.c_str());

    isInitialized = true;
    myUserID = -1; // 初期化時はUserID未割り当て
    std::cout << "ネットワーク初期化完了: " << ServerIP << ":" << ServerPort << "\n";
    std::cout << "サーバーに接続試行中...\n";
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
    std::cout << "ネットワーク終了\n";
}


void Script_Network::SendPlayerData()
{
    if (!isInitialized || myPlayerObject == nullptr) return;

    // 自分の位置・回転を取得
    Float3 pos, rot;
    if (GetObjectPosition(myPlayerObject, &pos) != PN_SUCCESS) return;
    if (GetObjectRotation(myPlayerObject, &rot) != PN_SUCCESS) return;

    // JSON作成
    nlohmann::json sendData;
    sendData["UserID"] = myUserID;
    sendData["Pos"]["X"] = pos.x;
    sendData["Pos"]["Y"] = pos.y;
    sendData["Pos"]["Z"] = pos.z;
    sendData["Rot"]["X"] = rot.x;
    sendData["Rot"]["Y"] = rot.y;
    sendData["Rot"]["Z"] = rot.z;

    // アニメーション情報も送信する場合
    Component animComp = nullptr;
    if (FindComponent(myPlayerObject, "Animation", &animComp) == PN_SUCCESS)
    {
        // 現在のアニメーション番号を取得する処理があれば追加
        // sendData["AnimationNo"] = currentAnimNo;
    }

    std::string data_str = sendData.dump();

    // 送信
    int server_addr_len = sizeof(server_addr);
    int sent = sendto(sock, data_str.c_str(), data_str.length(), 0,
        (struct sockaddr*)&server_addr, server_addr_len);

    if (sent == SOCKET_ERROR)
    {
        int error = WSAGetLastError();
        if (error != WSAEWOULDBLOCK)
        {
            std::cout << "送信エラー: " << error << "\n";
        }
    }
}

void Script_Network::ReceiveData()
{
    if (!isInitialized) return;

    while (true)
    {
        int recv_size = recvfrom(sock, recvBuffer, BUFFER_SIZE - 1, 0, nullptr, nullptr);

        if (recv_size <= 0)
        {
            int error = WSAGetLastError();
            if (error != WSAEWOULDBLOCK)
            {
                std::cout << "受信エラー: " << error << "\n";
            }
            break;
        }

        recvBuffer[recv_size] = '\0';

        try
        {
            nlohmann::json received = nlohmann::json::parse(recvBuffer);

            // 初回接続時のUserID割り当て
            if (received.contains("MyUserID"))
            {
                myUserID = received["MyUserID"];
                std::cout << "接続成功! UserID: " << myUserID << "\n";
                continue;
            }

            // 他プレイヤーの情報
            if (received.contains("UserID"))
            {
                int otherUserID = received["UserID"];

                // 自分自身は無視
                if (otherUserID == myUserID) continue;

                Float3 pos = { 0, 0, 0 };
                Float3 rot = { 0, 0, 0 };
                int animNo = 0;

                if (received.contains("Pos"))
                {
                    pos.x = received["Pos"]["X"];
                    pos.y = received["Pos"]["Y"];
                    pos.z = received["Pos"]["Z"];
                }
                if (received.contains("Rot"))  // "Ros" から "Rot" に修正
                {
                    rot.x = received["Rot"]["X"];
                    rot.y = received["Rot"]["Y"];
                    rot.z = received["Rot"]["Z"];
                }
                if (received.contains("AnimationNo"))
                {
                    animNo = received["AnimationNo"];
                }

                UpdateOtherPlayer(otherUserID, pos, rot, animNo);
            }
        }
        catch (const nlohmann::json::parse_error& e)
        {
            std::cout << "JSONパースエラー: " << e.what() << "\n";
        }
    }
}


void Script_Network::UpdateOtherPlayer(int userID, const Float3& pos, const Float3& rot, int animNo)
{
    Scene currentScene;
    if (GetCurrentScene(&currentScene) != PN_SUCCESS) return;

    // 既存プレイヤーのチェック
    auto it = otherPlayers.find(userID);

    if (it == otherPlayers.end())
    {
        // 新規プレイヤー生成
        OtherPlayerData newPlayer;

        // Prefabからオブジェクト生成
        Object prefabObj = nullptr;
        if (FindPrefabObjectByName("OtherPlayer", &prefabObj) == PN_SUCCESS)
        {
            Object clonedObj = nullptr;
            if (AddObjectToScene(currentScene, prefabObj, &clonedObj) == PN_SUCCESS)
            {
                newPlayer.playerObject = clonedObj;

                // オブジェクト名を設定
                std::string objName = "OtherPlayer_" + std::to_string(userID);
                SetObjectName(clonedObj, objName.c_str());

                // 初期位置と回転を設定
                SetObjectPosition(clonedObj, pos);
                SetObjectRotation(clonedObj, rot);

                std::cout << "他プレイヤー生成: UserID=" << userID << " Name=" << objName << "\n";
            }
            else
            {
                std::cout << "オブジェクトのシーン追加に失敗\n";
            }
        }
        else
        {
            std::cout << "Prefab \"" << PlayerPrefabName << "\" が見つかりません\n";
        }

        newPlayer.Position = pos;
        newPlayer.Rotation = rot;
        newPlayer.AnimationNo = animNo;
        newPlayer.IsActive = true;

        otherPlayers[userID] = newPlayer;
    }
    else
    {
        // 既存プレイヤーの更新
        it->second.Position = pos;
        it->second.Rotation = rot;
        it->second.AnimationNo = animNo;

        // オブジェクトの位置・回転を更新
        if (it->second.playerObject != nullptr)
        {
            SetObjectPosition(it->second.playerObject, pos);
            SetObjectRotation(it->second.playerObject, rot);

            // アニメーション更新
            Component animComp = nullptr;
            if (FindComponent(it->second.playerObject, "Animation", &animComp) == PN_SUCCESS)
            {
                SetAnimationClip(animComp, animNo);
            }
        }
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
    std::cout << "他プレイヤー削除: UserID=" << userID << "\n";
}