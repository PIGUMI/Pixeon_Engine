#ifndef _RIGID_BODY_H_
#define _RIGID_BODY_H_

#include "Component.h"
#include "BulletPhysics/btBulletDynamicsCommon.h"
#include "Struct.h"
#include <DirectXMath.h>
#include <vector>

class RigidBodyComponent : public AbstractComponent
{
public:
	void Init(AbstractObject* Prt) override;
	void BeginPlay() override;
	void EditUpdate() override;
	void InGameUpdate() override;
	void UInit() override;

	void DrawInspector() override;

	void SaveToFile(std::ostream& out) override;
	void LoadFromFile(std::istream& in) override;

	void SetMass(float mass);
	float GetMass() const { return fMass_; }

	void SetKinematic(bool kinematic);
	bool IsKinematic() const { return bKinematic_; }

	void SetGravityEnabled(bool useGravity);
	bool IsGravityEnabled() const { return bUseGravity_; }

	void AddCollisionShape(btCollisionShape* shape, const btTransform& localTransform);
	void RemoveCollisionShape(btCollisionShape* shape);

	void AddForce(const DirectX::XMFLOAT3& force);
	void AddImpulse(const DirectX::XMFLOAT3& impulse);
	void SetVelocity(const DirectX::XMFLOAT3& velocity);
	DirectX::XMFLOAT3 GetVelocity() const;

	btRigidBody* GetBtRigidBody() const { return pRigidBody_; }
	btCompoundShape* GetBtCompoundShape() const { return pCompoundShape_; }

	void SyncTransformFromBullet();
	void SyncTransformToBullet();

	void ResetAddedToWorldFlag() { bAddedToWorld_ = false; }

	void SyncPositionToBullet(const DirectX::XMFLOAT3& position);
	void SyncRotationToBullet(const DirectX::XMFLOAT3& rotation);
	void WarpTo(const DirectX::XMFLOAT3& position);
	void SetTransformDirty(bool dirty) { bTransformDirty_ = dirty; }
	bool IsTransformDirty() const { return bTransformDirty_; }
	void SetFriction(float friction);
	float GetFriction() const { return fFriction_; }

	void SetRestitution(float restitution);
	float GetRestitution() const { return fRestitution_; }

	void SetLinearDamping(float damping);
	float GetLinearDamping() const { return fLinearDamping_; }

	void SetAngularDamping(float damping);
	float GetAngularDamping() const { return fAngularDamping_; }

	void SetDamping(float linearDamping, float angularDamping);

	void SetRollingFriction(float rollingFriction);
	float GetRollingFriction() const { return fRollingFriction_; }

	void SetSpinningFriction(float spinningFriction);
	float GetSpinningFriction() const { return fSpinningFriction_; }

	void SetRotationConstraint(bool lockX, bool lockY, bool lockZ);
	void SetRotationConstraintX(bool lock);
	void SetRotationConstraintY(bool lock);
	void SetRotationConstraintZ(bool lock);

	bool IsRotationConstraintX() const { return bLockRotationX_; }
	bool IsRotationConstraintY() const { return bLockRotationY_; }
	bool IsRotationConstraintZ() const { return bLockRotationZ_; }

	// カメラ距離に基づく制御
	void SetCullingDistance(float distance);
	float GetCullingDistance() const { return fCullingDistance_; }
	void SetUseCulling(bool useCulling);
	bool IsUseCulling() const { return bUseCulling_; }
	void SetAlwaysActive(bool alwaysActive);
	bool IsAlwaysActive() const { return bAlwaysActive_; }
	void UpdateCullingState();
	bool IsActiveInPhysicsWorld() const { return bActiveInPhysicsWorld_; }

	// スリープ制御
	void SetDisableSleep(bool disableSleep);
	bool IsDisableSleep() const { return bDisableSleep_; }

	// CollisionからのWorld操作を許可するフラグ
	bool CanModifyPhysicsWorld() const { return !bUseCulling_ || bAlwaysActive_; }

private:
	void CreateRigidBody();
	void UpdateMassProperties();
	btQuaternion EulerToQuaternion(const DirectX::XMFLOAT3& euler);
	DirectX::XMFLOAT3 QuaternionToEuler(const btQuaternion& quat);
	void AddToPhysicsWorld();
	void RemoveFromPhysicsWorld();
	float CalculateDistanceToCamera();

private:
	float fMass_ = 1.0f;
	bool bKinematic_ = false;
	bool bUseGravity_ = true;
	float fFriction_ = 0.5f;
	float fRestitution_ = 0.0f;
	float fLinearDamping_ = 0.0f;
	float fAngularDamping_ = 0.05f;
	float fRollingFriction_ = 0.0f;
	float fSpinningFriction_ = 0.0f;

	btRigidBody* pRigidBody_ = nullptr;
	btCompoundShape* pCompoundShape_ = nullptr;
	btDefaultMotionState* pMotionState_ = nullptr;
	btVector3 localInertia_ = btVector3(0, 0, 0);

	std::vector<btCollisionShape*> RegisteredColliders_;

	DirectX::XMFLOAT3 LastPosition_ = { 0.0f, 0.0f, 0.0f };
	DirectX::XMFLOAT3 LastRotation_ = { 0.0f, 0.0f, 0.0f };
	DirectX::XMFLOAT3 LastScale_ = { 1.0f, 1.0f, 1.0f };
	bool bTransformDirty_ = false;

	bool bAddedToWorld_ = false;
	bool bManualTransformControl_ = false;

	bool bLockRotationX_ = false;
	bool bLockRotationY_ = false;
	bool bLockRotationZ_ = false;

	// カリング関連
	bool bUseCulling_ = false;
	bool bAlwaysActive_ = false;  // 常にアクティブにするフラグ
	float fCullingDistance_ = 100.0f;
	bool bActiveInPhysicsWorld_ = true;
	bool bWasActiveLastFrame_ = true;

	// スリープ制御
	bool bDisableSleep_ = false;  // スリープを無効化するフラグ
};

#endif // _RIGID_BODY_H_