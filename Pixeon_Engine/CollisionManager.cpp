// CollisionManager.cpp
#include "CollisionManager.h"
#include "BoxCollision.h"
#include "RigidBody.h"
#include "Object.h"
#include <algorithm>
#include <iostream>

// CollisionContactCallback implementation
CollisionContactCallback::CollisionContactCallback(BoxCollision* owner)
	: m_Owner(owner)
{
}

btScalar CollisionContactCallback::addSingleResult(btManifoldPoint& cp,
	const btCollisionObjectWrapper* colObj0Wrap, int partId0, int index0,
	const btCollisionObjectWrapper* colObj1Wrap, int partId1, int index1)
{
	// 相手のオブジェクトを特定
	const btCollisionObject* otherObj = nullptr;
	Object* hitObject = nullptr;

	// m_Ownerが所属するRigidBodyを取得
	RigidBody* ownerRigidBody = m_Owner->GetParent()->GetComponent<RigidBody>();
	if (ownerRigidBody && ownerRigidBody->GetBtRigidBody())
	{
		btRigidBody* ownerBtRigidBody = ownerRigidBody->GetBtRigidBody();

		// どちらが相手のオブジェクトかを判定
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
		// UserPointerからRigidBodyComponentを取得
		RigidBody* rigidBodyComp = static_cast<RigidBody*>(otherObj->getUserPointer());
		if (rigidBodyComp && rigidBodyComp->GetParent())
		{
			hitObject = rigidBodyComp->GetParent();
		}
	}

	if (!hitObject) return 0;

	// 重複チェック：同じオブジェクトとの衝突が既に記録されているかチェック
	for (const auto& existingCollision : m_Collisions)
	{
		if (existingCollision.HitObject == hitObject)
		{
			// 既に同じオブジェクトとの衝突が記録されている場合は追加しない
			return 0;
		}
	}

	// 新しい衝突情報を作成
	CollisionInfo info;

	// 衝突点を取得
	btVector3 hitPoint = cp.getPositionWorldOnA();
	info.HitPoint = DirectX::XMFLOAT3(hitPoint.getX(), hitPoint.getY(), hitPoint.getZ());

	// 法線を取得
	btVector3 normal = cp.m_normalWorldOnB;
	info.HitNormal = DirectX::XMFLOAT3(normal.getX(), normal.getY(), normal.getZ());

	// 距離
	info.Distance = cp.getDistance();

	// オブジェクト情報
	info.HitObject = hitObject;
	info.HitObjectName = hitObject->GetObjectName();

	// 衝突情報を記録
	m_Collisions.push_back(info);

	return 0; // 衝突処理を続行
}

// CollisionManager implementation（完全な非Singleton版）
void CollisionManager::Initialize(btDiscreteDynamicsWorld* dynamicsWorld)
{
	m_DynamicsWorld = dynamicsWorld;
}

void CollisionManager::Update()
{
	// まず手動衝突検出（RigidBodyなしのオブジェクト用）
	CheckManualCollisions();

	// 次にBullet衝突検出（RigidBodyありのオブジェクト用）
	ProcessBulletCollisions();
}

void CollisionManager::Shutdown()
{
	// ContactCallbackをクリーンアップ
	for (auto& pair : m_ContactCallbacks)
	{
		delete pair.second;
	}
	m_ContactCallbacks.clear();

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

		// Bulletコールバックを作成
		m_ContactCallbacks[collision] = new CollisionContactCallback(collision);
	}
}

void CollisionManager::UnregisterBoxCollision(BoxCollision* collision)
{
	auto it = std::find(m_BoxCollisions.begin(), m_BoxCollisions.end(), collision);
	if (it != m_BoxCollisions.end())
	{
		m_BoxCollisions.erase(it);

		// ContactCallbackを削除
		auto callbackIt = m_ContactCallbacks.find(collision);
		if (callbackIt != m_ContactCallbacks.end())
		{
			delete callbackIt->second;
			m_ContactCallbacks.erase(callbackIt);
		}

		// 衝突履歴を削除
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
	// RigidBodyを持たないBoxCollisionComponent同士の衝突検出
	std::vector<BoxCollision*> manualCollisions;

	for (BoxCollision* collision : m_BoxCollisions)
	{
		// RigidBodyを持たないコリジョンのみを対象
		RigidBody* rigidBody = collision->GetParent()->GetComponent<RigidBody>();
		if (!rigidBody)
		{
			manualCollisions.push_back(collision);
		}
	}

	// 総当りで衝突検出
	for (size_t i = 0; i < manualCollisions.size(); ++i)
	{
		std::vector<CollisionInfo> newCollisions;

		for (size_t j = i + 1; j < manualCollisions.size(); ++j)
		{
			CollisionInfo info;
			if (manualCollisions[i]->CheckCollision(manualCollisions[j], info))
			{
				info.HitObject = manualCollisions[j]->GetParent();
				info.HitObjectName = info.HitObject ? info.HitObject->GetObjectName() : "";
				newCollisions.push_back(info);

				// 逆方向の衝突も処理
				CollisionInfo reverseInfo = info;
				reverseInfo.HitObject = manualCollisions[i]->GetParent();
				reverseInfo.HitObjectName = reverseInfo.HitObject ? reverseInfo.HitObject->GetObjectName() : "";
				reverseInfo.HitNormal = DirectX::XMFLOAT3(-info.HitNormal.x, -info.HitNormal.y, -info.HitNormal.z);

				std::vector<CollisionInfo> reverseCollisions;
				reverseCollisions.push_back(reverseInfo);
				ProcessCollisionEvents(manualCollisions[j], reverseCollisions);
			}
		}

		ProcessCollisionEvents(manualCollisions[i], newCollisions);
	}
}

void CollisionManager::ProcessBulletCollisions()
{
	if (!m_DynamicsWorld) return;

	// デバッグ出力

	// 各BoxCollisionComponentについてBulletの衝突検出を実行
	for (BoxCollision* collision : m_BoxCollisions)
	{
		if (!collision || !collision->GetParent()) continue;

		RigidBody* rigidBody = collision->GetParent()->GetComponent<RigidBody>();
		if (!rigidBody || !rigidBody->GetBtRigidBody())
		{
			continue;
		}

		auto callbackIt = m_ContactCallbacks.find(collision);
		if (callbackIt == m_ContactCallbacks.end())
		{
			continue;
		}

		CollisionContactCallback* callback = callbackIt->second;
		callback->m_Collisions.clear();

		try
		{
			// Bulletの衝突検出を実行
			m_DynamicsWorld->contactTest(rigidBody->GetBtRigidBody(), *callback);

			// 衝突情報を処理
			ProcessCollisionEvents(collision, callback->m_Collisions);
		}
		catch (...)
		{
		}
	}
}

void CollisionManager::ProcessCollisionEvents(BoxCollision* collision,
	const std::vector<CollisionInfo>& newCollisions)
{
	if (!collision) return;

	// 重複排除：同じオブジェクトとの衝突を1つにまとめる
	std::vector<CollisionInfo> uniqueCollisions;
	for (const CollisionInfo& info : newCollisions)
	{
		if (!info.HitObject) continue;

		// 既に同じオブジェクトとの衝突が記録されているかチェック
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
	std::vector<BoxCollision*> previousObjects;
	auto prevIt = m_PreviousCollisions.find(collision);
	if (prevIt != m_PreviousCollisions.end())
	{
		previousObjects = prevIt->second;
	}

	std::vector<BoxCollision*> currentObjects;

	// 新しい衝突を処理
	for (const CollisionInfo& info : uniqueCollisions)
	{
		BoxCollision* otherCollision = info.HitObject->GetComponent<BoxCollision>();
		if (!otherCollision) continue;

		currentObjects.push_back(otherCollision);

		// 新しい衝突かチェック
		auto it = std::find(previousObjects.begin(), previousObjects.end(), otherCollision);
		if (it == previousObjects.end())
		{
			// OnCollisionEnter（初回のみ）

			if (collision->OnCollisionEnter_)
			{
				collision->OnCollisionEnter_(info);
			}
		}
		else
		{
			// OnCollisionStay（継続中）

			if (collision->OnCollisionStay_)
			{
				collision->OnCollisionStay_(info);
			}
		}
	}

	// OnCollisionExit処理
	for (BoxCollision* prevObject : previousObjects)
	{
		auto it = std::find(currentObjects.begin(), currentObjects.end(), prevObject);
		if (it == currentObjects.end())
		{
			// 衝突が終了した
			if (collision->OnCollisionExit_)
			{
				CollisionInfo exitInfo;
				exitInfo.HitObject = prevObject->GetParent();
				exitInfo.HitObjectName = exitInfo.HitObject ? exitInfo.HitObject->GetObjectName() : "";

				collision->OnCollisionExit_(exitInfo);
			}
		}
	}

	// 状態を更新（重複排除済みのデータを使用）
	m_PreviousCollisions[collision] = currentObjects;
	m_CurrentCollisions[collision] = uniqueCollisions;

	// 重要：BoxCollisionComponent の m_CurrentCollisions を直接更新
	collision->CurrentCollisions_ = uniqueCollisions;
}

void CollisionManager::DrawDebugInfo()
{
	// すべてのBoxCollisionComponentのデバッグ描画
	for (BoxCollision* collision : m_BoxCollisions)
	{
		collision->DrawDebugWireframe();
	}
}