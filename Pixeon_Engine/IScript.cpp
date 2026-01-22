// IScript.cpp
#include "IScript.h"
#include <unordered_map>
#include <mutex>

// スレッドセーフな管理のためmutexを追加
static std::unordered_map<Component, IScript*> g_scriptInstances;
static std::mutex g_scriptInstancesMutex;

// グローバルコールバック関数
void OnEnterCallback(Component collision, const APICollisionInfo* info)
{
    std::lock_guard<std::mutex> lock(g_scriptInstancesMutex);
    auto it = g_scriptInstances.find(collision);
    if (it != g_scriptInstances.end() && it->second != nullptr) {
        it->second->OnCollisionEnter(info);
    }
}

void OnStayCallback(Component collision, const APICollisionInfo* info)
{
    std::lock_guard<std::mutex> lock(g_scriptInstancesMutex);
    auto it = g_scriptInstances.find(collision);
    if (it != g_scriptInstances.end() && it->second != nullptr) {
        it->second->OnCollisionStay(info);
    }
}

void OnExitCallback(Component collision, const APICollisionInfo* info)
{
    std::lock_guard<std::mutex> lock(g_scriptInstancesMutex);
    auto it = g_scriptInstances.find(collision);
    if (it != g_scriptInstances.end() && it->second != nullptr) {
        it->second->OnCollisionExit(info);
    }
}

// デストラクタで確実にクリーンアップ
IScript::~IScript()
{
    UnregisterAllCollisions();
}

void IScript::BeginPlay()
{
    if (_parentObject == nullptr) {
        return; // 親オブジェクトが設定されていない場合は何もしない
    }

    // 複数のコリジョンタイプに対応
    const char* collisionTypes[] = {
        "BoxCollision",
        "CapsuleCollision",
        "SphereCollision",
        "MeshCollision"
        // 必要に応じて追加
    };

    for (const char* typeName : collisionTypes) {
        Component collisionComp = nullptr;
        if (FindComponent(_parentObject, typeName, &collisionComp) == PN_SUCCESS) {
            RegisterCollisionComponent(collisionComp);
        }
    }
}

void IScript::RegisterCollisionComponent(Component collision)
{
    if (collision == nullptr) {
        return;
    }

    // コールバックを登録
    APIResult result;
    result = CollisionSetCollisionEnterCallback(collision, OnEnterCallback);
    if (result != PN_SUCCESS) {
        // エラーログ出力（実装に応じて）
        return;
    }

    result = CollisionSetCollisionStayCallback(collision, OnStayCallback);
    if (result != PN_SUCCESS) {
        return;
    }

    result = CollisionSetCollisionExitCallback(collision, OnExitCallback);
    if (result != PN_SUCCESS) {
        return;
    }

    // マップに登録
    {
        std::lock_guard<std::mutex> lock(g_scriptInstancesMutex);
        g_scriptInstances[collision] = this;
    }

    // 登録したコンポーネントを記録
    _registeredCollisions.push_back(collision);
}

void IScript::UnregisterAllCollisions()
{
    std::lock_guard<std::mutex> lock(g_scriptInstancesMutex);

    // すべての登録を解除
    for (Component collision : _registeredCollisions) {
        auto it = g_scriptInstances.find(collision);
        if (it != g_scriptInstances.end()) {
            g_scriptInstances.erase(it);
        }
    }

    _registeredCollisions.clear();
}

void IScript::Update(float DeltaTime)
{
    // ユーザーがオーバーライドする
}

void IScript::EndPlay()
{
    // スクリプト終了時にコールバックを解除
    UnregisterAllCollisions();
}

void IScript::CallCustom(const std::string& functionName)
{
    // ユーザーがオーバーライドする
}

// デフォルト実装（ユーザーがオーバーライド）
void IScript::OnCollisionEnter(const APICollisionInfo* info)
{
    // オーバーライドされない場合は何もしない
}

void IScript::OnCollisionStay(const APICollisionInfo* info)
{
    // オーバーライドされない場合は何もしない
}

void IScript::OnCollisionExit(const APICollisionInfo* info)
{
    // オーバーライドされない場合は何もしない
}