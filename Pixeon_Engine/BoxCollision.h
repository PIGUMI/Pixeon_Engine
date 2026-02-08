#ifndef _BOX_COLLISION_H_
#define _BOX_COLLISION_H_

#include "BaseCollision.h"
#include "BulletPhysics/btBulletDynamicsCommon.h"

class BoxCollisionComponent : public BaseCollision
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
	DirectX::XMFLOAT3 GetSize() { return f3Size_; }

	void SetCenter(const DirectX::XMFLOAT3& center);
	DirectX::XMFLOAT3 GetCenter() { return f3Center_; }

	void SetTrigger(bool isTrigger) override;

	bool CheckCollision(BoxCollisionComponent* otherBox, CollisionInfo& outCollisionInfo);

	btBoxShape* GetBoxShape() const { return pBoxShape_; }

	void DrawDebugWireframe();

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
	bool m_b_BoxLine = true;

	btBoxShape* pBoxShape_ = nullptr;
};

#endif // ! _BOX_COLLISION_H_