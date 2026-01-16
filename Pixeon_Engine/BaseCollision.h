/*
* ファイル名　: BaseCollision.h
* 説　　　明　: 衝突判定基底クラスヘッダ
*/
#ifndef _BASE_COLLISION_H_
#define _BASE_COLLISION_H_

#include "Component.h"
#include "Struct.h"
#include <DirectXMath.h>
#include <functional>
#include <vector>

class RigidBody;

// 衝突コールバック関数の型定義
using OnCollisionEnterCallback = std::function<void(const CollisionInfo&)>;
using OnCollisionStayCallback = std::function<void(const CollisionInfo&)>;
using OnCollisionExitCallback = std::function<void(const CollisionInfo&)>;

class BaseCollision : public AbstractComponent
{
public:
	virtual ~BaseCollision() = default;

	// コールバック設定
	void SetOnCollisionEnter(OnCollisionEnterCallback callback) {
		OnCollisionEnter_ = callback;
		bCallBackSetAfterBeginPlay = true;
	}

	void SetOnCollisionStay(OnCollisionStayCallback callback) {
		OnCollisionStay_ = callback;
		bCallBackSetAfterBeginPlay = true;
	}

	void SetOnCollisionExit(OnCollisionExitCallback callback) {
		OnCollisionExit_ = callback;
		bCallBackSetAfterBeginPlay = true;
	}

	int GetBeginPlayCount() const { return nBeginPlayCount; }
	bool WasCallBackSetAfterBeginPlay() const { return bCallBackSetAfterBeginPlay; }

	void ResetCollisionCallbacks() {
		OnCollisionEnter_ = nullptr;
		OnCollisionStay_ = nullptr;
		OnCollisionExit_ = nullptr;
		bCallBackSetAfterBeginPlay = false;
		CollidingObjects_.clear();
		CurrentCollisions_.clear();
	}

	void ResetOnCollisionEnter() { OnCollisionEnter_ = nullptr; }
	void ResetOnCollisionStay() { OnCollisionStay_ = nullptr; }
	void ResetOnCollisionExit() { OnCollisionExit_ = nullptr; }

	bool HasCollisionEnterCallBack() const { return OnCollisionEnter_ != nullptr; }
	bool HasCollisionStayCallBack() const { return OnCollisionStay_ != nullptr; }
	bool HasCollisionExitCallBack() const { return OnCollisionExit_ != nullptr; }

	virtual void SetTrigger(bool isTrigger) { bTrigger_ = isTrigger; }
	virtual bool IsTrigger() const { return bTrigger_; }

	std::vector<CollisionInfo> GetCollisions() const { return CurrentCollisions_; }

	bool IsViewVisible() const { return bViewVisible_; }
	void SetViewVisible(bool visible) { bViewVisible_ = visible; }

	friend class CollisionManager;

protected:
	bool bTrigger_ = false;
	bool bViewVisible_ = false;

	std::vector<AbstractObject*> CollidingObjects_;
	std::vector<CollisionInfo> CurrentCollisions_;

	RigidBody* pAttachedRigidBody_ = nullptr;

	OnCollisionEnterCallback OnCollisionEnter_ = nullptr;
	OnCollisionStayCallback OnCollisionStay_ = nullptr;
	OnCollisionExitCallback OnCollisionExit_ = nullptr;

	int nBeginPlayCount = 0;
	bool bCallBackSetAfterBeginPlay = false;

	DirectX::XMFLOAT3 f3LastPosition_ = { 0.0f, 0.0f, 0.0f };
	DirectX::XMFLOAT3 f3LastRotation_ = { 0.0f, 0.0f, 0.0f };
	DirectX::XMFLOAT3 f3LastScale_ = { 1.0f, 1.0f, 1.0f };
	bool bTransformDirty_ = true;
};

#endif // _BASE_COLLISION_H_