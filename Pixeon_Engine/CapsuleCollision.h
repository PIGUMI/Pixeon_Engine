#ifndef _CAPSULE_COLLISION_H_
#define _CAPSULE_COLLISION_H_

#include "BaseCollision.h"
#include "BulletPhysics/btBulletDynamicsCommon.h"

class CapsuleCollision : public BaseCollision
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

	void SetRadius(float radius);
	float GetRadius() const { return fRadius_; }

	void SetHeight(float height);
	float GetHeight() const { return fHeight_; }

	void SetCenter(const DirectX::XMFLOAT3& center);
	DirectX::XMFLOAT3 GetCenter() const { return f3Center_; }

	void SetTrigger(bool isTrigger) override;

	btCapsuleShape* GetCapsuleShape() const { return pCapsuleShape_; }

	void DrawDebugWireframe();

	friend class CollisionManager;

private:
	void CreateCapsuleShape();
	void UpdateCollisionShape();
	void AttachToRigidBody();
	void DetachFromRigidBody();

private:
	float fRadius_ = 0.5f;
	float fHeight_ = 2.0f;
	DirectX::XMFLOAT3 f3Center_ = { 0.0f, 0.0f, 0.0f };
	bool m_b_CapsuleLine = true;

	btCapsuleShape* pCapsuleShape_ = nullptr;
};

#endif // _CAPSULE_COLLISION_H_