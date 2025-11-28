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
	void Init(Object* Prt) override;
	void BeginPlay() override;
	void EditUpdate() override;
	void InGameUpdate() override;
	void Draw() override;
	void UInit() override;

	void DrawInspector() override;

	void SaveToFile(std::ostream& out) override;
	void LoadFromFile(std::istream& in) override;

private:



};

#endif // !_BOX_COLLISION_H_