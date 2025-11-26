#ifndef _RIGID_BODY_H_
#define _RIGID_BODY_H_

#include "Component.h"
#include "BulletPhysics/btBulletDynamicsCommon.h"
#include "Struct.h"
#include <DirectXMath.h>
#include <vector>

class BoxCollider;

class RigidBody : public Component
{
public:
	void Init(Object* Prt) override;
	void BeginPlay() override;
	void InGameUpdate() override;
	void UInit() override;

	void DrawInspector() override;

	void SaveToFile(std::ostream& out) override;
	void LoadFromFile(std::istream& in) override;

	void SetMass(float mass) { fMass_ = mass; };
	float GetMass() const { return fMass_; }

	void SetKinematic(bool kinematic) { bKinematic_ = kinematic; };
	bool IsKinematic() const { return bKinematic_; }

	void SetGravityEnabled(bool useGravity) { bUseGravity_ = useGravity; };
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



private:
	float fMass_ = 1.0f;
	bool bKinematic_ = false;
	bool bUseGravity_ = true;

	btRigidBody* pRigidBody_ = nullptr;
	btCompoundShape* pCompoundShape_ = nullptr;
	btDefaultMotionState* pMotionState_ = nullptr;
	btVector3 localInertia_ = btVector3(0, 0, 0);

	std::vector<btCollisionShape*> RegisteredColliders_;

	DirectX::XMFLOAT3 LastPosition_ = { 0.0f, 0.0f, 0.0f };
	DirectX::XMFLOAT3 LastRotation_ = { 0.0f, 0.0f, 0.0f };
	DirectX::XMFLOAT3 LastScale_ = { 1.0f, 1.0f, 1.0f };
	bool TransformDirty_ = false;

	bool bAddedToWorld_ = false;
	bool bManualTransformControl_ = false;
};

#endif // _RIGID_BODY_H_

