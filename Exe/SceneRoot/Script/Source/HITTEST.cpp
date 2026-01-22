// HITTEST.cpp
#include "HITTEST.h"
#include <Windows.h>
#include <string>
#include <unordered_map>

// コンポーネントとスクリプトインスタンスのマッピング
static std::unordered_map<Component, Script_HITTEST*> g_hitTestInstances;

// グローバルコールバック関数
void OnEnterCallback(Component collision, const APICollisionInfo* info) {
    auto it = g_hitTestInstances.find(collision);
    if (it != g_hitTestInstances.end()) {
        it->second->OnCollisionEnter(info);
    }
}

void OnStayCallback(Component collision, const APICollisionInfo* info) {
    auto it = g_hitTestInstances.find(collision);
    if (it != g_hitTestInstances.end()) {
        it->second->OnCollisionStay(info);
    }
}

void OnExitCallback(Component collision, const APICollisionInfo* info) {
    auto it = g_hitTestInstances.find(collision);
    if (it != g_hitTestInstances.end()) {
        it->second->OnCollisionExit(info);
    }
}

void Script_HITTEST::BeginPlay() {
    // 現在のシーンとオブジェクトを取得
    GetCurrentScene(&currentScene);
    thisObject = _parentObject;

    // CapsuleCollisionコンポーネントを取得
    APIResult result = FindComponent(thisObject, "CapsuleCollision", &capsuleCollision);

    if (result == PN_SUCCESS && capsuleCollision != nullptr) {
        // マップに登録
        g_hitTestInstances[capsuleCollision] = this;

        // コールバックを設定
        CollisionSetCollisionEnterCallback(capsuleCollision, OnEnterCallback);
        CollisionSetCollisionStayCallback(capsuleCollision, OnStayCallback);
        CollisionSetCollisionExitCallback(capsuleCollision, OnExitCallback);

        MessageBoxA(nullptr, "HitTest initialized successfully!", "Info", MB_OK);
    }
    else {
        MessageBoxA(nullptr, "CapsuleCollision component not found!", "Error", MB_OK | MB_ICONERROR);
    }
}

void Script_HITTEST::Update() {
    // 必要に応じて更新処理
}

void Script_HITTEST::EndPlay() {
    // マップから登録解除
    if (capsuleCollision != nullptr) {
        g_hitTestInstances.erase(capsuleCollision);
    }
}

void Script_HITTEST::OnCollisionEnter(const APICollisionInfo* info) {
    if (!info) return;

    std::string message = "Collision Enter!\n";
    message += "Hit Object: ";
    message += info->HitObjectName;
    message += "\nDistance: ";
    message += std::to_string(info->Distance);

    MessageBoxA(nullptr, message.c_str(), "Collision Enter", MB_OK | MB_ICONINFORMATION);
}

void Script_HITTEST::OnCollisionStay(const APICollisionInfo* info) {
    // Stay は頻繁に呼ばれるのでコメントアウト推奨
    /*
    if (! info) return;

    std::string message = "Collision Stay!\n";
    message += "Hit Object: ";
    message += info->HitObjectName;

    MessageBoxA(nullptr, message.c_str(), "Collision Stay", MB_OK);
    */
}

void Script_HITTEST::OnCollisionExit(const APICollisionInfo* info) {
    if (!info) return;

    std::string message = "Collision Exit!\n";
    message += "Hit Object: ";
    message += info->HitObjectName;

    MessageBoxA(nullptr, message.c_str(), "Collision Exit", MB_OK | MB_ICONWARNING);
}