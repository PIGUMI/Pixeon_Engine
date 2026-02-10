// IScript.h
#ifndef _ISCRIPT_H_
#define _ISCRIPT_H_

#include <string>
#include <vector>
#include <memory>
#include <functional>
#include "API.h"
#include "Pixeon2_API.h"
#include "Scriptproperty.h"

class ScripComponent;

// ========================================
// 自動メモリ管理クラス
// ========================================
class IScript : public ScriptPropertyBase
{
public:
    virtual ~IScript();
    virtual void BeginPlay();
    virtual void Update(float DeltaTime);
    virtual void EndPlay();

    virtual void CallCustom(const std::string& functionName);

    // 親オブジェクト/シーン設定
    void SetParentObject(object obj);
    void SetParentScene(scene scene);
    void SetOwnerComponent(ScripComponent* owner) { _ownerComponent = owner; }

    ScripComponent* GetOwnerComponent() const { return _ownerComponent; }
    void ClearParent();

public:
    // コリジョンコールバック
    virtual void OnCollisionEnter(const APICollisionInfo* info);
    virtual void OnCollisionStay(const APICollisionInfo* info);
    virtual void OnCollisionExit(const APICollisionInfo* info);

    // プロパティシステム（デフォルト実装）
    std::vector<PropertyMetadata> GetProperties() override { return {}; }
    std::string SerializeProperty(const std::string& name) override { return ""; }
    void DeserializeProperty(const std::string& name, const std::string& value) override {}

protected:
    // ========================================
    // 自動メモリ管理機能
    // ========================================

    // オブジェクト生成（自動管理）
    Object* CreateManagedObject(const std::string& prefabName);

    // ユーザー定義クラスの自動管理
    template<typename T>
    T* CreateManaged() {
        T* obj = new T();

        // 削除用のラムダを登録
        _managedResources.push_back([obj]() {
            delete obj;
            });

        return obj;
    }

    // 配列の自動管理
    template<typename T>
    T* CreateManagedArray(size_t count) {
        T* arr = new T[count];

        _managedResources.push_back([arr]() {
            delete[] arr;
            });

        return arr;
    }

    // std::shared_ptr による管理
    template<typename T>
    std::shared_ptr<T> CreateShared() {
        auto ptr = std::make_shared<T>();
        _sharedResources.push_back(ptr);
        return ptr;
    }

    // カスタムクリーンアップ処理の登録
    void RegisterCleanupCallback(std::function<void()> callback) {
        _managedResources.push_back(callback);
    }

    // オブジェクトの手動登録
    void RegisterManagedObject(Object* obj);

    // リソースの手動解放
    void ReleaseAllManagedResources();

protected:
    // ラッパークラス（所有権なし）
    Object* _parentObject = nullptr;
    Scene* _parentScene = nullptr;
    ScripComponent* _ownerComponent = nullptr;

private:
    // コリジョン管理
    std::vector<component> _registeredCollisions;
    void RegisterCollisionComponent(component collision);
    void UnregisterAllCollisions();

    // 自動メモリ管理用
    std::vector<Object*> _createdObjects;                  // 生成したオブジェクト
    std::vector<std::function<void()>> _managedResources;  // カスタムリソース
    std::vector<std::shared_ptr<void>> _sharedResources;   // shared_ptr リソース

    // クリーンアップ処理
    void CleanupAllResources();
};

#endif // _ISCRIPT_H_