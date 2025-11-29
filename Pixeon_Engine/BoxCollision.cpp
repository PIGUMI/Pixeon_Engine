#include "BoxCollision.h"
#include "RigidBody.h"
#include "Object.h"
#include "CollisionManager.h"

void BoxCollision::Init(Object* Prt)
{
}

void BoxCollision::BeginPlay()
{
}

void BoxCollision::EditUpdate()
{
}

void BoxCollision::InGameUpdate()
{
}

void BoxCollision::Draw()
{
}

void BoxCollision::UInit()
{
}

void BoxCollision::DrawInspector()
{
}

void BoxCollision::SaveToFile(std::ostream& out)
{
}

void BoxCollision::LoadFromFile(std::istream& in)
{
}

bool BoxCollision::CheckCollision(BoxCollision* otherBox, CollisionInfo& outCollisionInfo)
{
	return false;
}

std::vector<CollisionInfo> BoxCollision::GetCollisions()
{
	return std::vector<CollisionInfo>();
}

void BoxCollision::DrawDebugWireframe()
{
}

void BoxCollision::CreateBoxShape()
{
}

void BoxCollision::UpdateCollisionShape()
{
}

void BoxCollision::AttachToRigidBody()
{
}

void BoxCollision::DetachFromRigidBody()
{
}

void BoxCollision::ProcessCollisionCallBacks()
{
}
