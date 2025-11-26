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

private:
	float fMass_ = 1.0f;
	bool bKinematic_ = false;
	bool bUseGravity_ = true;

	btRigidBody* pRigidBody_ = nullptr;
	btCompoundShape* pCompoundShape_ = nullptr;
	btDefaultMotionState* pMotionState_ = nullptr;
	btVector3 localInertia_ = btVector3(0, 0, 0);
};

#endif // _RIGID_BODY_H_

