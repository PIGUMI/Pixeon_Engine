#ifndef _BOX_COLLISION_H_
#define _BOX_COLLISION_H_

#include "Component.h"
#include "Struct.h"
#include "BulletPhysics/btBulletDynamicsCommon.h"
#include <DirectXMath.h>
#include <functional>
#include <vector>

class RigidBody;

// 衝突コールバック関数の型定義
using OnCollisionEnterCallback = std::function<void(const CollisionInfo&)>;
using OnCollisionStayCallback = std::function<void(const CollisionInfo&)>;
using OnCollisionExitCallback = std::function<void(const CollisionInfo&)>;

class BoxCollision : public Component
{
public:
	void Init(AbstractObject* Prt) override;
	void BeginPlay() override;
	void EditUpdate() override;
	void InGameUpdate() override;
	void Draw(int Layer) override;
	void UInit() override;

	void DrawInspector() override;

	void SaveToFile(std::ostream& out) override;
	void LoadFromFile(std::istream& in) override;

	void SetSize(const DirectX::XMFLOAT3& size);
	DirectX::XMFLOAT3 GetSize() const;

	void SetCenter(const DirectX::XMFLOAT3& center);
	DirectX::XMFLOAT3 GetCenter() const;

	void SetTrigger(bool isTrigger);
	bool IsTrigger() const { return bTrigger_; }

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
	};

	void ResetOnCollisionEnter() { OnCollisionEnter_ = nullptr; }
	void ResetOnCollisionStay() { OnCollisionStay_ = nullptr; }
	void ResetOnCollisionExit() { OnCollisionExit_ = nullptr; }

	bool HasCollisionEnterCallBack() const { return OnCollisionEnter_ != nullptr; }
	bool HasCollisionStayCallBack() const { return OnCollisionStay_ != nullptr; }
	bool HasCollisionExitCallBack() const { return OnCollisionExit_ != nullptr; }

	std::string GetCallbackStatus() const
	{
		std::string status = "CallBack : ";
		status += "Enter=" + std::string(HasCollisionEnterCallBack() ? "Set" : "None") + ", ";
		status += "Stay=" + std::string(HasCollisionStayCallBack() ? "Set" : "None") + ", ";
		status += "Exit=" + std::string(HasCollisionExitCallBack() ? "Set" : "None");
		return status;
	}

	std::string GetDetailCallBackStatus() const
	{
		std::string status = "[" + (_Parent ? _Parent->GetObjectName() : "Unknown") + "] ";
		status += "BeginPlay回数: " + std::to_string(nBeginPlayCount) + ", ";
		status += "Enter=" + std::string(HasCollisionEnterCallBack() ? "Set" : "None") + ", ";
		status += "Stay=" + std::string(HasCollisionStayCallBack() ? "Set" : "None") + ", ";
		status += "Exit=" + std::string(HasCollisionExitCallBack() ? "Set" : "None") + ", ";
		status += "設定済み=" + std::string(bCallBackSetAfterBeginPlay ? "Yes" : "No");
		return status;
	}

	bool CheckCollision(BoxCollision* otherBox, CollisionInfo& outCollisionInfo);
	std::vector<CollisionInfo> GetCollisions();

	btBoxShape* GetBoxShape() const { return pBoxShape_; }

	void DrawDebugWireframe();

	bool IsViewVisible() const { return bViewVisible_; }
	void SetViewVisible(bool visible) { bViewVisible_ = visible; }

	friend class CollisionManager;
private:
	void CreateBoxShape();
	void UpdateCollisionShape();
	void AttachToRigidBody();
	void DetachFromRigidBody();
	void ProcessCollisionCallBacks();
	bool OBBIntersection(const DirectX::XMFLOAT3& pos1, const DirectX::XMFLOAT3& rot1, const DirectX::XMFLOAT3& size1,
		const DirectX::XMFLOAT3& pos2, const DirectX::XMFLOAT3& rot2, const DirectX::XMFLOAT3& size2,
		CollisionInfo& info);
private:
	DirectX::XMFLOAT3 f3Size_ = { 1.0f, 1.0f, 1.0f };
	DirectX::XMFLOAT3 f3Center_ = { 0.0f, 0.0f, 0.0f };
	bool bTrigger_ = false;
	bool m_b_BoxLine = true;

	btBoxShape* pBoxShape_ = nullptr;

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
	bool bViewVisible_ = false;
};

#endif // !_BOX_COLLISION_H_