#define NOMINMAX
#include "CollisionManager.h"
#include "BoxCollision.h"
#include "CapsuleCollision.h"
#include "RigidBody.h"
#include "Object.h"
#include <algorithm>
#include <iostream>

// CollisionContactCallback implementation
CollisionContactCallback::CollisionContactCallback(BaseCollision* owner)
	: m_Owner(owner)
{
}

btScalar CollisionContactCallback::addSingleResult(btManifoldPoint& cp,
	const btCollisionObjectWrapper* colObj0Wrap, int partId0, int index0,
	const btCollisionObjectWrapper* colObj1Wrap, int partId1, int index1)
{
	const btCollisionObject* otherObj = nullptr;
	AbstractObject* hitObject = nullptr;

	RigidBody* ownerRigidBody = m_Owner->GetParent()->GetComponent<RigidBody>();
	if (ownerRigidBody && ownerRigidBody->GetBtRigidBody())
	{
		btRigidBody* ownerBtRigidBody = ownerRigidBody->GetBtRigidBody();

		if (colObj0Wrap->getCollisionObject() != ownerBtRigidBody)
		{
			otherObj = colObj0Wrap->getCollisionObject();
		}
		else if (colObj1Wrap->getCollisionObject() != ownerBtRigidBody)
		{
			otherObj = colObj1Wrap->getCollisionObject();
		}
	}

	if (otherObj && otherObj->getUserPointer())
	{
		RigidBody* rigidBodyComp = static_cast<RigidBody*>(otherObj->getUserPointer());
		if (rigidBodyComp && rigidBodyComp->GetParent())
		{
			hitObject = rigidBodyComp->GetParent();
		}
	}

	if (!hitObject) return 0;

	for (const auto& existingCollision : m_Collisions)
	{
		if (existingCollision.HitObject == hitObject)
		{
			return 0;
		}
	}

	CollisionInfo info;
	btVector3 hitPoint = cp.getPositionWorldOnA();
	info.HitPoint = DirectX::XMFLOAT3(hitPoint.getX(), hitPoint.getY(), hitPoint.getZ());

	btVector3 normal = cp.m_normalWorldOnB;
	info.HitNormal = DirectX::XMFLOAT3(normal.getX(), normal.getY(), normal.getZ());

	info.Distance = cp.getDistance();
	info.HitObject = hitObject;
	info.HitObjectName = hitObject->GetObjectName();

	m_Collisions.push_back(info);
	return 0;
}

void CollisionManager::Initialize(btDiscreteDynamicsWorld* dynamicsWorld)
{
	m_DynamicsWorld = dynamicsWorld;
}

void CollisionManager::Update()
{
	CheckManualCollisions();
	ProcessBulletCollisions();
}

void CollisionManager::Shutdown()
{
	for (auto& pair : m_ContactCallbacks)
	{
		delete pair.second;
	}
	m_ContactCallbacks.clear();

	m_CapsuleCollisions.clear();
	m_BoxCollisions.clear();
	m_RigidBodies.clear();
	m_PreviousCollisions.clear();
	m_CurrentCollisions.clear();
}

void CollisionManager::RegisterBoxCollision(BoxCollision* collision)
{
	if (collision && std::find(m_BoxCollisions.begin(), m_BoxCollisions.end(), collision) == m_BoxCollisions.end())
	{
		m_BoxCollisions.push_back(collision);
		m_ContactCallbacks[collision] = new CollisionContactCallback(collision);
	}
}

void CollisionManager::UnregisterBoxCollision(BoxCollision* collision)
{
	auto it = std::find(m_BoxCollisions.begin(), m_BoxCollisions.end(), collision);
	if (it != m_BoxCollisions.end())
	{
		m_BoxCollisions.erase(it);

		auto callbackIt = m_ContactCallbacks.find(collision);
		if (callbackIt != m_ContactCallbacks.end())
		{
			delete callbackIt->second;
			m_ContactCallbacks.erase(callbackIt);
		}

		m_PreviousCollisions.erase(collision);
		m_CurrentCollisions.erase(collision);
	}
}

void CollisionManager::RegisterCapsuleCollision(CapsuleCollision* collision)
{
	if (collision && std::find(m_CapsuleCollisions.begin(), m_CapsuleCollisions.end(), collision) == m_CapsuleCollisions.end())
	{
		m_CapsuleCollisions.push_back(collision);
		m_ContactCallbacks[collision] = new CollisionContactCallback(collision);
	}
}

void CollisionManager::UnregisterCapsuleCollision(CapsuleCollision* collision)
{
	auto it = std::find(m_CapsuleCollisions.begin(), m_CapsuleCollisions.end(), collision);
	if (it != m_CapsuleCollisions.end())
	{
		m_CapsuleCollisions.erase(it);

		auto callbackIt = m_ContactCallbacks.find(collision);
		if (callbackIt != m_ContactCallbacks.end())
		{
			delete callbackIt->second;
			m_ContactCallbacks.erase(callbackIt);
		}

		m_PreviousCollisions.erase(collision);
		m_CurrentCollisions.erase(collision);
	}
}

void CollisionManager::RegisterRigidBody(RigidBody* rigidBody)
{
	if (rigidBody && std::find(m_RigidBodies.begin(), m_RigidBodies.end(), rigidBody) == m_RigidBodies.end())
	{
		m_RigidBodies.push_back(rigidBody);
	}
}

void CollisionManager::UnregisterRigidBody(RigidBody* rigidBody)
{
	auto it = std::find(m_RigidBodies.begin(), m_RigidBodies.end(), rigidBody);
	if (it != m_RigidBodies.end())
	{
		m_RigidBodies.erase(it);
	}
}

void CollisionManager::CheckManualCollisions()
{
	std::vector<BoxCollision*> manualBoxCollisions;
	std::vector<CapsuleCollision*> manualCapsuleCollisions;

	for (BoxCollision* collision : m_BoxCollisions)
	{
		RigidBody* rigidBody = collision->GetParent()->GetComponent<RigidBody>();
		if (!rigidBody)
		{
			manualBoxCollisions.push_back(collision);
		}
	}

	for (CapsuleCollision* collision : m_CapsuleCollisions)
	{
		RigidBody* rigidBody = collision->GetParent()->GetComponent<RigidBody>();
		if (!rigidBody)
		{
			manualCapsuleCollisions.push_back(collision);
		}
	}

	// Box ⇔ Box の衝突判定
	for (size_t i = 0; i < manualBoxCollisions.size(); ++i)
	{
		std::vector<CollisionInfo> newCollisions;

		for (size_t j = i + 1; j < manualBoxCollisions.size(); ++j)
		{
			CollisionInfo info;
			if (manualBoxCollisions[i]->CheckCollision(manualBoxCollisions[j], info))
			{
				info.HitObject = manualBoxCollisions[j]->GetParent();
				info.HitObjectName = info.HitObject ? info.HitObject->GetObjectName() : "";
				newCollisions.push_back(info);

				CollisionInfo reverseInfo = info;
				reverseInfo.HitObject = manualBoxCollisions[i]->GetParent();
				reverseInfo.HitObjectName = reverseInfo.HitObject ? reverseInfo.HitObject->GetObjectName() : "";
				reverseInfo.HitNormal = DirectX::XMFLOAT3(-info.HitNormal.x, -info.HitNormal.y, -info.HitNormal.z);

				std::vector<CollisionInfo> reverseCollisions;
				reverseCollisions.push_back(reverseInfo);
				ProcessCollisionEvents(manualBoxCollisions[j], reverseCollisions);
			}
		}

		ProcessCollisionEvents(manualBoxCollisions[i], newCollisions);
	}

	// Box ⇔ Capsule の衝突判定
	for (BoxCollision* boxCol : manualBoxCollisions)
	{
		std::vector<CollisionInfo> boxNewCollisions;

		for (CapsuleCollision* capsuleCol : manualCapsuleCollisions)
		{
			CollisionInfo info;
			if (CheckBoxCapsuleCollision(boxCol, capsuleCol, info))
			{
				info.HitObject = capsuleCol->GetParent();
				info.HitObjectName = info.HitObject ? info.HitObject->GetObjectName() : "";
				boxNewCollisions.push_back(info);

				CollisionInfo capsuleInfo = info;
				capsuleInfo.HitObject = boxCol->GetParent();
				capsuleInfo.HitObjectName = capsuleInfo.HitObject ? capsuleInfo.HitObject->GetObjectName() : "";
				capsuleInfo.HitNormal = DirectX::XMFLOAT3(-info.HitNormal.x, -info.HitNormal.y, -info.HitNormal.z);

				std::vector<CollisionInfo> capsuleCollisions;
				capsuleCollisions.push_back(capsuleInfo);
				ProcessCollisionEvents(capsuleCol, capsuleCollisions);
			}
		}

		ProcessCollisionEvents(boxCol, boxNewCollisions);
	}

	// Capsule ⇔ Capsule の衝突判定
	for (size_t i = 0; i < manualCapsuleCollisions.size(); ++i)
	{
		std::vector<CollisionInfo> newCollisions;

		for (size_t j = i + 1; j < manualCapsuleCollisions.size(); ++j)
		{
			CollisionInfo info;
			if (CheckCapsuleCapsuleCollision(manualCapsuleCollisions[i], manualCapsuleCollisions[j], info))
			{
				info.HitObject = manualCapsuleCollisions[j]->GetParent();
				info.HitObjectName = info.HitObject ? info.HitObject->GetObjectName() : "";
				newCollisions.push_back(info);

				CollisionInfo reverseInfo = info;
				reverseInfo.HitObject = manualCapsuleCollisions[i]->GetParent();
				reverseInfo.HitObjectName = reverseInfo.HitObject ? reverseInfo.HitObject->GetObjectName() : "";
				reverseInfo.HitNormal = DirectX::XMFLOAT3(-info.HitNormal.x, -info.HitNormal.y, -info.HitNormal.z);

				std::vector<CollisionInfo> reverseCollisions;
				reverseCollisions.push_back(reverseInfo);
				ProcessCollisionEvents(manualCapsuleCollisions[j], reverseCollisions);
			}
		}

		ProcessCollisionEvents(manualCapsuleCollisions[i], newCollisions);
	}
}

void CollisionManager::ProcessBulletCollisions()
{
	if (!m_DynamicsWorld) return;

	for (BoxCollision* collision : m_BoxCollisions)
	{
		if (!collision || !collision->GetParent()) continue;

		RigidBody* rigidBody = collision->GetParent()->GetComponent<RigidBody>();
		if (!rigidBody || !rigidBody->GetBtRigidBody()) continue;

		auto callbackIt = m_ContactCallbacks.find(collision);
		if (callbackIt == m_ContactCallbacks.end()) continue;

		CollisionContactCallback* callback = callbackIt->second;
		callback->m_Collisions.clear();

		try
		{
			m_DynamicsWorld->contactTest(rigidBody->GetBtRigidBody(), *callback);
			ProcessCollisionEvents(collision, callback->m_Collisions);
		}
		catch (...) {}
	}

	for (CapsuleCollision* collision : m_CapsuleCollisions)
	{
		if (!collision || !collision->GetParent()) continue;

		RigidBody* rigidBody = collision->GetParent()->GetComponent<RigidBody>();
		if (!rigidBody || !rigidBody->GetBtRigidBody()) continue;

		auto callbackIt = m_ContactCallbacks.find(collision);
		if (callbackIt == m_ContactCallbacks.end()) continue;

		CollisionContactCallback* callback = callbackIt->second;
		callback->m_Collisions.clear();

		try
		{
			m_DynamicsWorld->contactTest(rigidBody->GetBtRigidBody(), *callback);
			ProcessCollisionEvents(collision, callback->m_Collisions);
		}
		catch (...) {}
	}
}

void CollisionManager::ProcessCollisionEvents(BaseCollision* collision,
	const std::vector<CollisionInfo>& newCollisions)
{
	if (!collision) return;

	// 重複排除
	std::vector<CollisionInfo> uniqueCollisions;
	for (const CollisionInfo& info : newCollisions)
	{
		if (!info.HitObject) continue;

		bool isDuplicate = false;
		for (const auto& existing : uniqueCollisions)
		{
			if (existing.HitObject == info.HitObject)
			{
				isDuplicate = true;
				break;
			}
		}

		if (!isDuplicate)
		{
			uniqueCollisions.push_back(info);
		}
	}

	// 前回の衝突オブジェクトリストを取得
	std::vector<BaseCollision*> previousObjects;
	auto prevIt = m_PreviousCollisions.find(collision);
	if (prevIt != m_PreviousCollisions.end())
	{
		previousObjects = prevIt->second;
	}

	std::vector<BaseCollision*> currentObjects;

	// 新しい衝突を処理
	for (const CollisionInfo& info : uniqueCollisions)
	{
		BaseCollision* otherCollision = nullptr;

		// BoxCollisionとCapsuleCollisionの両方をチェック
		BoxCollision* otherBoxCollision = info.HitObject->GetComponent<BoxCollision>();
		if (otherBoxCollision)
		{
			otherCollision = otherBoxCollision;
		}
		else
		{
			CapsuleCollision* otherCapsuleCollision = info.HitObject->GetComponent<CapsuleCollision>();
			if (otherCapsuleCollision)
			{
				otherCollision = otherCapsuleCollision;
			}
		}

		if (!otherCollision) continue;

		currentObjects.push_back(otherCollision);

		// 新しい衝突かチェック
		auto it = std::find(previousObjects.begin(), previousObjects.end(), otherCollision);
		if (it == previousObjects.end())
		{
			// OnCollisionEnter
			if (collision->OnCollisionEnter_)
			{
				collision->OnCollisionEnter_(info);
			}
		}
		else
		{
			// OnCollisionStay
			if (collision->OnCollisionStay_)
			{
				collision->OnCollisionStay_(info);
			}
		}
	}

	// OnCollisionExitの処理
	for (BaseCollision* prevObject : previousObjects)
	{
		auto it = std::find(currentObjects.begin(), currentObjects.end(), prevObject);
		if (it == currentObjects.end())
		{
			if (collision->OnCollisionExit_)
			{
				CollisionInfo exitInfo;
				exitInfo.HitObject = prevObject->GetParent();
				exitInfo.HitObjectName = exitInfo.HitObject ? exitInfo.HitObject->GetObjectName() : "";

				collision->OnCollisionExit_(exitInfo);
			}
		}
	}

	// 状態を更新
	m_PreviousCollisions[collision] = currentObjects;
	m_CurrentCollisions[collision] = uniqueCollisions;

	// CurrentCollisions_を直接更新
	collision->CurrentCollisions_ = uniqueCollisions;
}

void CollisionManager::DrawDebugInfo()
{
	for (BoxCollision* collision : m_BoxCollisions)
	{
		collision->DrawDebugWireframe();
	}
}

float CollisionManager::ClosestPointsBetweenLineSegments(
	const DirectX::XMVECTOR& p1, const DirectX::XMVECTOR& q1,
	const DirectX::XMVECTOR& p2, const DirectX::XMVECTOR& q2,
	DirectX::XMFLOAT3& point1, DirectX::XMFLOAT3& point2)
{
	DirectX::XMVECTOR d1 = DirectX::XMVectorSubtract(q1, p1);
	DirectX::XMVECTOR d2 = DirectX::XMVectorSubtract(q2, p2);
	DirectX::XMVECTOR r = DirectX::XMVectorSubtract(p1, p2);

	float a = DirectX::XMVectorGetX(DirectX::XMVector3Dot(d1, d1));
	float b = DirectX::XMVectorGetX(DirectX::XMVector3Dot(d1, d2));
	float c = DirectX::XMVectorGetX(DirectX::XMVector3Dot(d1, r));
	float e = DirectX::XMVectorGetX(DirectX::XMVector3Dot(d2, d2));
	float f = DirectX::XMVectorGetX(DirectX::XMVector3Dot(d2, r));

	float s, t;
	float denom = a * e - b * b;

	if (denom != 0.0f)
	{
		s = (b * f - c * e) / denom;
		s = std::max(0.0f, std::min(1.0f, s));
	}
	else
	{
		s = 0.0f;
	}

	t = (b * s + f) / e;
	t = std::max(0.0f, std::min(1.0f, t));

	DirectX::XMVECTOR c1 = DirectX::XMVectorAdd(p1, DirectX::XMVectorScale(d1, s));
	DirectX::XMVECTOR c2 = DirectX::XMVectorAdd(p2, DirectX::XMVectorScale(d2, t));

	DirectX::XMStoreFloat3(&point1, c1);
	DirectX::XMStoreFloat3(&point2, c2);

	DirectX::XMVECTOR diff = DirectX::XMVectorSubtract(c1, c2);
	return DirectX::XMVectorGetX(DirectX::XMVector3Length(diff));
}

DirectX::XMFLOAT3 CollisionManager::ClosestPointOnLineSegmentToAABB(
	const DirectX::XMFLOAT3& lineStart,
	const DirectX::XMFLOAT3& lineEnd,
	const DirectX::XMFLOAT3& boxMin,
	const DirectX::XMFLOAT3& boxMax)
{
	DirectX::XMFLOAT3 boxCenter = {
		(boxMin.x + boxMax.x) * 0.5f,
		(boxMin.y + boxMax.y) * 0.5f,
		(boxMin.z + boxMax.z) * 0.5f
	};

	// ボックスの中心に最も近い線分上の点を計算
	DirectX::XMVECTOR start = DirectX::XMLoadFloat3(&lineStart);
	DirectX::XMVECTOR end = DirectX::XMLoadFloat3(&lineEnd);
	DirectX::XMVECTOR center = DirectX::XMLoadFloat3(&boxCenter);

	DirectX::XMVECTOR lineVec = DirectX::XMVectorSubtract(end, start);
	DirectX::XMVECTOR toCenter = DirectX::XMVectorSubtract(center, start);

	float t = DirectX::XMVectorGetX(DirectX::XMVector3Dot(toCenter, lineVec)) /
		DirectX::XMVectorGetX(DirectX::XMVector3Dot(lineVec, lineVec));
	t = std::max(0.0f, std::min(1.0f, t));

	DirectX::XMVECTOR pointOnLine = DirectX::XMVectorAdd(start, DirectX::XMVectorScale(lineVec, t));

	DirectX::XMFLOAT3 result;
	DirectX::XMStoreFloat3(&result, pointOnLine);

	// AABBの範囲内にクランプ
	result.x = std::max(boxMin.x, std::min(boxMax.x, result.x));
	result.y = std::max(boxMin.y, std::min(boxMax.y, result.y));
	result.z = std::max(boxMin.z, std::min(boxMax.z, result.z));

	return result;
}

bool CollisionManager::CheckBoxCapsuleCollision(BoxCollision* box, CapsuleCollision* capsule, CollisionInfo& info)
{
	if (!box || !capsule || !box->GetParent() || !capsule->GetParent())
		return false;

	// ボックスの情報を取得
	DirectX::XMFLOAT3 boxPos = box->GetParent()->GetWorldTransform().position;
	DirectX::XMFLOAT3 boxRot = box->GetParent()->GetWorldTransform().rotation;
	DirectX::XMFLOAT3 boxScale = box->GetParent()->GetWorldTransform().scale;
	DirectX::XMFLOAT3 boxSize = box->GetSize();
	DirectX::XMFLOAT3 boxCenter = box->GetCenter();

	boxPos.x += boxCenter.x;
	boxPos.y += boxCenter.y;
	boxPos.z += boxCenter.z;

	// カプセルの情報を取得
	DirectX::XMFLOAT3 capsulePos = capsule->GetParent()->GetWorldTransform().position;
	DirectX::XMFLOAT3 capsuleRot = capsule->GetParent()->GetWorldTransform().rotation;
	DirectX::XMFLOAT3 capsuleScale = capsule->GetParent()->GetWorldTransform().scale;
	DirectX::XMFLOAT3 capsuleCenter = capsule->GetCenter();

	capsulePos.x += capsuleCenter.x;
	capsulePos.y += capsuleCenter.y;
	capsulePos.z += capsuleCenter.z;

	float radius = capsule->GetRadius() * std::max(capsuleScale.x, capsuleScale.z);
	float height = capsule->GetHeight();
	float cylinderHeight = (height - 2.0f * capsule->GetRadius()) * capsuleScale.y;

	// カプセルの上下の中心点を計算
	DirectX::XMMATRIX capsuleRotMat = DirectX::XMMatrixRotationRollPitchYaw(
		capsuleRot.x, capsuleRot.y, capsuleRot.z
	);

	DirectX::XMVECTOR upVec = DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
	upVec = DirectX::XMVector3TransformNormal(upVec, capsuleRotMat);

	DirectX::XMVECTOR capsulePosVec = DirectX::XMLoadFloat3(&capsulePos);
	DirectX::XMVECTOR topCenter = DirectX::XMVectorAdd(capsulePosVec, DirectX::XMVectorScale(upVec, cylinderHeight * 0.5f));
	DirectX::XMVECTOR bottomCenter = DirectX::XMVectorAdd(capsulePosVec, DirectX::XMVectorScale(upVec, -cylinderHeight * 0.5f));

	DirectX::XMFLOAT3 topCenterF, bottomCenterF;
	DirectX::XMStoreFloat3(&topCenterF, topCenter);
	DirectX::XMStoreFloat3(&bottomCenterF, bottomCenter);

	// ボックスのAABBを計算（簡易版）
	DirectX::XMFLOAT3 boxMin = {
		boxPos.x - boxSize.x * boxScale.x * 0.5f,
		boxPos.y - boxSize.y * boxScale.y * 0.5f,
		boxPos.z - boxSize.z * boxScale.z * 0.5f
	};
	DirectX::XMFLOAT3 boxMax = {
		boxPos.x + boxSize.x * boxScale.x * 0.5f,
		boxPos.y + boxSize.y * boxScale.y * 0.5f,
		boxPos.z + boxSize.z * boxScale.z * 0.5f
	};

	// カプセルの線分とボックスの最近接点を計算
	DirectX::XMFLOAT3 closestPoint = ClosestPointOnLineSegmentToAABB(
		topCenterF, bottomCenterF, boxMin, boxMax
	);

	// 最近接点までの距離を計算
	DirectX::XMVECTOR closestVec = DirectX::XMLoadFloat3(&closestPoint);
	DirectX::XMVECTOR lineStart = DirectX::XMLoadFloat3(&topCenterF);
	DirectX::XMVECTOR lineEnd = DirectX::XMLoadFloat3(&bottomCenterF);

	DirectX::XMVECTOR lineVec = DirectX::XMVectorSubtract(lineEnd, lineStart);
	DirectX::XMVECTOR toClosest = DirectX::XMVectorSubtract(closestVec, lineStart);

	float t = DirectX::XMVectorGetX(DirectX::XMVector3Dot(toClosest, lineVec)) /
		DirectX::XMVectorGetX(DirectX::XMVector3Dot(lineVec, lineVec));
	t = std::max(0.0f, std::min(1.0f, t));

	DirectX::XMVECTOR pointOnLine = DirectX::XMVectorAdd(lineStart, DirectX::XMVectorScale(lineVec, t));
	DirectX::XMVECTOR diff = DirectX::XMVectorSubtract(closestVec, pointOnLine);
	float distance = DirectX::XMVectorGetX(DirectX::XMVector3Length(diff));

	// 衝突判定
	if (distance <= radius)
	{
		DirectX::XMFLOAT3 pointOnLineF;
		DirectX::XMStoreFloat3(&pointOnLineF, pointOnLine);

		info.HitPoint = closestPoint;
		info.Distance = radius - distance;

		if (distance > 0.0001f)
		{
			DirectX::XMVECTOR normal = DirectX::XMVector3Normalize(diff);
			DirectX::XMStoreFloat3(&info.HitNormal, normal);
		}
		else
		{
			info.HitNormal = DirectX::XMFLOAT3(0.0f, 1.0f, 0.0f);
		}

		return true;
	}

	return false;
}

bool CollisionManager::CheckCapsuleCapsuleCollision(CapsuleCollision* capsule1, CapsuleCollision* capsule2, CollisionInfo& info)
{
	if (!capsule1 || !capsule2 || !capsule1->GetParent() || !capsule2->GetParent())
		return false;

	// カプセル1の情報
	DirectX::XMFLOAT3 pos1 = capsule1->GetParent()->GetWorldTransform().position;
	DirectX::XMFLOAT3 rot1 = capsule1->GetParent()->GetWorldTransform().rotation;
	DirectX::XMFLOAT3 scale1 = capsule1->GetParent()->GetWorldTransform().scale;
	DirectX::XMFLOAT3 center1 = capsule1->GetCenter();

	pos1.x += center1.x;
	pos1.y += center1.y;
	pos1.z += center1.z;

	float radius1 = capsule1->GetRadius() * std::max(scale1.x, scale1.z);
	float cylinderHeight1 = (capsule1->GetHeight() - 2.0f * capsule1->GetRadius()) * scale1.y;

	// カプセル2の情報
	DirectX::XMFLOAT3 pos2 = capsule2->GetParent()->GetWorldTransform().position;
	DirectX::XMFLOAT3 rot2 = capsule2->GetParent()->GetWorldTransform().rotation;
	DirectX::XMFLOAT3 scale2 = capsule2->GetParent()->GetWorldTransform().scale;
	DirectX::XMFLOAT3 center2 = capsule2->GetCenter();

	pos2.x += center2.x;
	pos2.y += center2.y;
	pos2.z += center2.z;

	float radius2 = capsule2->GetRadius() * std::max(scale2.x, scale2.z);
	float cylinderHeight2 = (capsule2->GetHeight() - 2.0f * capsule2->GetRadius()) * scale2.y;

	// カプセル1の線分
	DirectX::XMMATRIX rot1Mat = DirectX::XMMatrixRotationRollPitchYaw(rot1.x, rot1.y, rot1.z);
	DirectX::XMVECTOR up1 = DirectX::XMVector3TransformNormal(DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f), rot1Mat);
	DirectX::XMVECTOR pos1Vec = DirectX::XMLoadFloat3(&pos1);
	DirectX::XMVECTOR top1 = DirectX::XMVectorAdd(pos1Vec, DirectX::XMVectorScale(up1, cylinderHeight1 * 0.5f));
	DirectX::XMVECTOR bottom1 = DirectX::XMVectorAdd(pos1Vec, DirectX::XMVectorScale(up1, -cylinderHeight1 * 0.5f));

	// カプセル2の線分
	DirectX::XMMATRIX rot2Mat = DirectX::XMMatrixRotationRollPitchYaw(rot2.x, rot2.y, rot2.z);
	DirectX::XMVECTOR up2 = DirectX::XMVector3TransformNormal(DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f), rot2Mat);
	DirectX::XMVECTOR pos2Vec = DirectX::XMLoadFloat3(&pos2);
	DirectX::XMVECTOR top2 = DirectX::XMVectorAdd(pos2Vec, DirectX::XMVectorScale(up2, cylinderHeight2 * 0.5f));
	DirectX::XMVECTOR bottom2 = DirectX::XMVectorAdd(pos2Vec, DirectX::XMVectorScale(up2, -cylinderHeight2 * 0.5f));

	// 2つの線分間の最短距離を計算
	DirectX::XMFLOAT3 point1, point2;
	float distance = ClosestPointsBetweenLineSegments(
		top1, bottom1, top2, bottom2, point1, point2
	);

	// 衝突判定
	float radiusSum = radius1 + radius2;
	if (distance <= radiusSum)
	{
		info.HitPoint = DirectX::XMFLOAT3(
			(point1.x + point2.x) * 0.5f,
			(point1.y + point2.y) * 0.5f,
			(point1.z + point2.z) * 0.5f
		);

		DirectX::XMFLOAT3 normal = {
			point2.x - point1.x,
			point2.y - point1.y,
			point2.z - point1.z
		};

		if (distance > 0.0001f)
		{
			float length = sqrtf(normal.x * normal.x + normal.y * normal.y + normal.z * normal.z);
			info.HitNormal = DirectX::XMFLOAT3(
				normal.x / length,
				normal.y / length,
				normal.z / length
			);
		}
		else
		{
			info.HitNormal = DirectX::XMFLOAT3(0.0f, 1.0f, 0.0f);
		}

		info.Distance = radiusSum - distance;
		return true;
	}

	return false;
}