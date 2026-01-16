#define NOMINMAX
#include "CapsuleCollision.h"
#include "RigidBody.h"
#include "Object.h"
#include "Scene.h"
#include "GUI.h"
#include "CollisionManager.h"
#include "_Geometry.h"

void CapsuleCollision::Init(AbstractObject* Prt)
{
	_Parent = Prt;
	_ComponentName = "CapsuleCollision";
	_Type = ComponentManager::COMPONENT_TYPE::CAPSULE_COLLISION;

	CreateCapsuleShape();

	auto currentScene = _Parent->GetWorldTransform();
	f3LastPosition_ = currentScene.position;
	f3LastRotation_ = currentScene.rotation;
	f3LastScale_ = currentScene.scale;

	nBeginPlayCount = 0;
	bCallBackSetAfterBeginPlay = false;
}

void CapsuleCollision::BeginPlay()
{
	nBeginPlayCount++;

	OnCollisionEnter_ = nullptr;
	OnCollisionStay_ = nullptr;
	OnCollisionExit_ = nullptr;
	bCallBackSetAfterBeginPlay = false;

	CollidingObjects_.clear();
	CurrentCollisions_.clear();

	AttachToRigidBody();

	if (_Parent && _Parent->GetParentScene())
	{
		CollisionManager* collisionManager = _Parent->GetParentScene()->GetCollisionManager();
		if (collisionManager)
		{
			collisionManager->RegisterCapsuleCollision(this);
		}
	}
}

void CapsuleCollision::EditUpdate()
{
	if (_Parent)
	{
		auto currentTransform = _Parent->GetWorldTransform();
		bool transformChanged =
			f3LastPosition_.x != currentTransform.position.x ||
			f3LastPosition_.y != currentTransform.position.y ||
			f3LastPosition_.z != currentTransform.position.z ||
			f3LastRotation_.x != currentTransform.rotation.x ||
			f3LastRotation_.y != currentTransform.rotation.y ||
			f3LastRotation_.z != currentTransform.rotation.z ||
			f3LastScale_.x != currentTransform.scale.x ||
			f3LastScale_.y != currentTransform.scale.y ||
			f3LastScale_.z != currentTransform.scale.z;

		if (transformChanged)
		{
			bTransformDirty_ = true;
			f3LastPosition_ = currentTransform.position;
			f3LastRotation_ = currentTransform.rotation;
			f3LastScale_ = currentTransform.scale;
		}
	}

	if (bTransformDirty_)
	{
		UpdateCollisionShape();
		bTransformDirty_ = false;
	}
}

void CapsuleCollision::InGameUpdate()
{
	if (_Parent)
	{
		auto currentTransform = _Parent->GetWorldTransform();
		bool transformChanged =
			f3LastPosition_.x != currentTransform.position.x ||
			f3LastPosition_.y != currentTransform.position.y ||
			f3LastPosition_.z != currentTransform.position.z ||
			f3LastRotation_.x != currentTransform.rotation.x ||
			f3LastRotation_.y != currentTransform.rotation.y ||
			f3LastRotation_.z != currentTransform.rotation.z ||
			f3LastScale_.x != currentTransform.scale.x ||
			f3LastScale_.y != currentTransform.scale.y ||
			f3LastScale_.z != currentTransform.scale.z;

		if (transformChanged)
		{
			if (pAttachedRigidBody_ && pAttachedRigidBody_->IsKinematic())
			{
				bTransformDirty_ = true;
			}

			f3LastPosition_ = currentTransform.position;
			f3LastRotation_ = currentTransform.rotation;
			f3LastScale_ = currentTransform.scale;
		}
	}

	if (bTransformDirty_)
	{
		UpdateCollisionShape();
		bTransformDirty_ = false;
	}
}

void CapsuleCollision::Draw(int Layer)
{
	if (Layer != _LayerNumber) return;
	if (!m_b_CapsuleLine) return;
	if (!_Parent) return;

	auto transform = _Parent->GetWorldTransform();
	DirectX::XMFLOAT3 pos = transform.position;
	DirectX::XMFLOAT3 center = f3Center_;
	DirectX::XMFLOAT3 rot = transform.rotation;
	DirectX::XMFLOAT3 scale = transform.scale;

	// 回転行列を作成
	DirectX::XMMATRIX matRot = DirectX::XMMatrixRotationRollPitchYaw(rot.x, rot.y, rot.z);

	// スケールを適用した半径と高さ
	float scaledRadius = fRadius_ * std::max(scale.x, scale.z);
	float cylinderHeight = (fHeight_ - 2.0f * fRadius_) * scale.y;

	// カプセルの中心位置を計算
	DirectX::XMVECTOR centerVec = DirectX::XMVectorSet(center.x, center.y, center.z, 1.0f);
	centerVec = DirectX::XMVector3TransformNormal(centerVec, matRot);
	centerVec = DirectX::XMVectorAdd(centerVec, DirectX::XMVectorSet(pos.x, pos.y, pos.z, 0.0f));

	DirectX::XMFLOAT3 worldCenter;
	DirectX::XMStoreFloat3(&worldCenter, centerVec);

	DirectX::XMFLOAT4 color = bTrigger_
		? DirectX::XMFLOAT4(0.0f, 1.0f, 1.0f, 1.0f)  // シアン
		: DirectX::XMFLOAT4(0.0f, 1.0f, 0.0f, 1.0f); // 緑

	auto Cam = _Parent->GetParentScene()->GetMainCamera();
	if (!Cam) return;

	DirectX::XMFLOAT4X4 Proj = Cam->GetProjectionMatrix();
	DirectX::XMFLOAT4X4 view = Cam->GetViewMatrix();
	DirectX::XMFLOAT4X4 world;
	DirectX::XMStoreFloat4x4(&world, DirectX::XMMatrixIdentity());

	// カプセルの描画（簡略化：円柱部分と上下の円）
	const int segments = 16;
	const float angleStep = DirectX::XM_2PI / segments;

	// Y軸方向のベクトル（回転適用後）
	DirectX::XMVECTOR upVec = DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
	upVec = DirectX::XMVector3TransformNormal(upVec, matRot);

	// 上下の半球の中心
	DirectX::XMVECTOR topCenter = DirectX::XMVectorAdd(
		DirectX::XMLoadFloat3(&worldCenter),
		DirectX::XMVectorScale(upVec, cylinderHeight * 0.5f)
	);
	DirectX::XMVECTOR bottomCenter = DirectX::XMVectorAdd(
		DirectX::XMLoadFloat3(&worldCenter),
		DirectX::XMVectorScale(upVec, -cylinderHeight * 0.5f)
	);

	DirectX::XMFLOAT3 topCenterF, bottomCenterF;
	DirectX::XMStoreFloat3(&topCenterF, topCenter);
	DirectX::XMStoreFloat3(&bottomCenterF, bottomCenter);

	// 円柱の円を描画（上下）
	DirectX::XMVECTOR rightVec = DirectX::XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f);
	rightVec = DirectX::XMVector3TransformNormal(rightVec, matRot);

	DirectX::XMVECTOR forwardVec = DirectX::XMVector3Cross(upVec, rightVec);
	forwardVec = DirectX::XMVector3Normalize(forwardVec);

	for (int i = 0; i < segments; ++i)
	{
		float angle1 = angleStep * i;
		float angle2 = angleStep * ((i + 1) % segments);

		DirectX::XMVECTOR offset1 = DirectX::XMVectorAdd(
			DirectX::XMVectorScale(rightVec, scaledRadius * cosf(angle1)),
			DirectX::XMVectorScale(forwardVec, scaledRadius * sinf(angle1))
		);

		DirectX::XMVECTOR offset2 = DirectX::XMVectorAdd(
			DirectX::XMVectorScale(rightVec, scaledRadius * cosf(angle2)),
			DirectX::XMVectorScale(forwardVec, scaledRadius * sinf(angle2))
		);

		// 上の円
		DirectX::XMFLOAT3 p1Top, p2Top;
		DirectX::XMStoreFloat3(&p1Top, DirectX::XMVectorAdd(topCenter, offset1));
		DirectX::XMStoreFloat3(&p2Top, DirectX::XMVectorAdd(topCenter, offset2));
		LineRenderer::GetInstance()->DrawLine(p1Top, p2Top, color, world, view, Proj, 0.05f);

		// 下の円
		DirectX::XMFLOAT3 p1Bottom, p2Bottom;
		DirectX::XMStoreFloat3(&p1Bottom, DirectX::XMVectorAdd(bottomCenter, offset1));
		DirectX::XMStoreFloat3(&p2Bottom, DirectX::XMVectorAdd(bottomCenter, offset2));
		LineRenderer::GetInstance()->DrawLine(p1Bottom, p2Bottom, color, world, view, Proj, 0.05f);

		// 縦のライン（4本）
		if (i % (segments / 4) == 0)
		{
			LineRenderer::GetInstance()->DrawLine(p1Top, p1Bottom, color, world, view, Proj, 0.05f);
		}
	}
}

void CapsuleCollision::UInit()
{
	if (_Parent && _Parent->GetParentScene())
	{
		CollisionManager* collisionManager = _Parent->GetParentScene()->GetCollisionManager();
		if (collisionManager)
		{
			collisionManager->UnregisterCapsuleCollision(this);
		}
	}

	DetachFromRigidBody();
	if (pCapsuleShape_)
	{
		delete pCapsuleShape_;
		pCapsuleShape_ = nullptr;
	}
	CollidingObjects_.clear();
	CurrentCollisions_.clear();
}

void CapsuleCollision::DrawInspector()
{
	auto SJ = [](const char* s)->std::string { return GUI::GetInstance()->ShiftJISToUTF8(s); };
	std::string label = _ComponentName + "##" + std::to_string(reinterpret_cast<uintptr_t>(this));
	if (!ImGui::CollapsingHeader(SJ(label.c_str()).c_str()))return;

	label = "CapsuleParams##" + std::to_string(reinterpret_cast<uintptr_t>(this));
	if (ImGui::BeginTable(SJ(label.c_str()).c_str(), 2, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV)) {
		ImGui::TableNextRow();
		ImGui::TableSetColumnIndex(0); ImGui::Text(SJ("レイヤー").c_str());
		ImGui::TableSetColumnIndex(1);
		ImGui::InputInt("##LayerInput", &_LayerNumber);

		ImGui::TableNextRow();
		ImGui::TableSetColumnIndex(0); ImGui::Text(SJ("半径").c_str());
		ImGui::TableSetColumnIndex(1);
		float radius = fRadius_;
		if (ImGui::InputFloat("##RadiusInput", &radius, 0.1f, 1.0f, "%.3f"))
		{
			SetRadius(radius);
		}

		ImGui::TableNextRow();
		ImGui::TableSetColumnIndex(0); ImGui::Text(SJ("高さ").c_str());
		ImGui::TableSetColumnIndex(1);
		float height = fHeight_;
		if (ImGui::InputFloat("##HeightInput", &height, 0.1f, 1.0f, "%. 3f"))
		{
			SetHeight(height);
		}

		ImGui::TableNextRow();
		ImGui::TableSetColumnIndex(0); ImGui::Text(SJ("中心位置").c_str());
		ImGui::TableSetColumnIndex(1);
		DirectX::XMFLOAT3 center = f3Center_;
		if (ImGui::InputFloat3("##CenterInput", &center.x, "%.3f"))
		{
			SetCenter(center);
		}

		ImGui::TableNextRow();
		ImGui::TableSetColumnIndex(0); ImGui::Text(SJ("トリガー").c_str());
		ImGui::TableSetColumnIndex(1);
		bool isTrigger = bTrigger_;
		if (ImGui::Checkbox("##TriggerInput", &isTrigger))
		{
			SetTrigger(isTrigger);
		}

		ImGui::TableNextRow();
		ImGui::TableSetColumnIndex(0); ImGui::Text(SJ("線描画").c_str());
		ImGui::TableSetColumnIndex(1);
		ImGui::Checkbox("##DrawWireframeInput", &m_b_CapsuleLine);

		ImGui::EndTable();
	}
}

void CapsuleCollision::SaveToFile(std::ostream& out)
{
	out << fRadius_ << " " << fHeight_ << " "
		<< f3Center_.x << " " << f3Center_.y << " " << f3Center_.z << " "
		<< bTrigger_ << " " << _LayerNumber << " " << m_b_CapsuleLine << "\n";
}

void CapsuleCollision::LoadFromFile(std::istream& in)
{
	in >> fRadius_ >> fHeight_
		>> f3Center_.x >> f3Center_.y >> f3Center_.z
		>> bTrigger_ >> _LayerNumber >> m_b_CapsuleLine;

	SetRadius(fRadius_);
	SetHeight(fHeight_);
	SetCenter(f3Center_);
	SetTrigger(bTrigger_);
}

void CapsuleCollision::SetRadius(float radius)
{
	fRadius_ = radius;
	CreateCapsuleShape();
	UpdateCollisionShape();
}

void CapsuleCollision::SetHeight(float height)
{
	fHeight_ = height;
	CreateCapsuleShape();
	UpdateCollisionShape();
}

void CapsuleCollision::SetCenter(const DirectX::XMFLOAT3& center)
{
	f3Center_ = center;
	UpdateCollisionShape();
}

void CapsuleCollision::SetTrigger(bool isTrigger)
{
	bTrigger_ = isTrigger;

	if (pAttachedRigidBody_ && pAttachedRigidBody_->GetBtRigidBody())
	{
		btRigidBody* rigidBody = pAttachedRigidBody_->GetBtRigidBody();
		if (bTrigger_)
		{
			rigidBody->setCollisionFlags(rigidBody->getCollisionFlags() | btCollisionObject::CF_NO_CONTACT_RESPONSE);
		}
		else
		{
			rigidBody->setCollisionFlags(rigidBody->getCollisionFlags() & ~btCollisionObject::CF_NO_CONTACT_RESPONSE);
		}
	}
}

void CapsuleCollision::DrawDebugWireframe()
{
	// 必要に応じて実装
}

void CapsuleCollision::CreateCapsuleShape()
{
	if (pCapsuleShape_)
	{
		delete pCapsuleShape_;
	}

	DirectX::XMFLOAT3 worldScale = { 1.0f, 1.0f, 1.0f };
	if (_Parent)
	{
		worldScale = _Parent->GetWorldTransform().scale;
	}

	// カプセルの円柱部分の高さを計算
	float cylinderHeight = fHeight_ - 2.0f * fRadius_;
	if (cylinderHeight < 0.0f) cylinderHeight = 0.0f;

	// BulletのカプセルはY軸方向
	pCapsuleShape_ = new btCapsuleShape(
		fRadius_ * std::max(worldScale.x, worldScale.z),
		cylinderHeight * worldScale.y
	);
}

void CapsuleCollision::UpdateCollisionShape()
{
	if (!pCapsuleShape_) return;

	bool wasInWorld = false;
	btDiscreteDynamicsWorld* physicsWorld = nullptr;

	if (pAttachedRigidBody_ && pAttachedRigidBody_->GetBtRigidBody())
	{
		if (_Parent && _Parent->GetParentScene())
		{
			physicsWorld = _Parent->GetParentScene()->GetPhysicsWorld();
			if (physicsWorld)
			{
				physicsWorld->removeRigidBody(pAttachedRigidBody_->GetBtRigidBody());
				wasInWorld = true;
			}
		}
		pAttachedRigidBody_->RemoveCollisionShape(pCapsuleShape_);
	}

	// 形状の再作成
	if (pCapsuleShape_)
	{
		delete pCapsuleShape_;
		pCapsuleShape_ = nullptr;
	}

	DirectX::XMFLOAT3 worldScale = { 1.0f, 1.0f, 1.0f };
	if (_Parent)
	{
		worldScale = _Parent->GetWorldTransform().scale;
	}

	float cylinderHeight = fHeight_ - 2.0f * fRadius_;
	if (cylinderHeight < 0.0f) cylinderHeight = 0.0f;

	pCapsuleShape_ = new btCapsuleShape(
		fRadius_ * std::max(worldScale.x, worldScale.z),
		cylinderHeight * worldScale.y
	);

	pCapsuleShape_->setMargin(0.02f);

	if (pAttachedRigidBody_ && pCapsuleShape_)
	{
		btTransform localTransform;
		localTransform.setIdentity();
		localTransform.setOrigin(btVector3(f3Center_.x, f3Center_.y, f3Center_.z));

		pAttachedRigidBody_->AddCollisionShape(pCapsuleShape_, localTransform);

		if (wasInWorld && physicsWorld)
		{
			physicsWorld->addRigidBody(pAttachedRigidBody_->GetBtRigidBody());
		}

		if (pAttachedRigidBody_->GetBtRigidBody())
		{
			pAttachedRigidBody_->GetBtRigidBody()->activate(true);
		}

		SetTrigger(bTrigger_);
	}
}

void CapsuleCollision::AttachToRigidBody()
{
	if (_Parent)
	{
		pAttachedRigidBody_ = _Parent->GetComponent<RigidBody>();
		if (pAttachedRigidBody_ && pCapsuleShape_)
		{
			btTransform localTransform;
			localTransform.setIdentity();
			localTransform.setOrigin(btVector3(f3Center_.x, f3Center_.y, f3Center_.z));

			pAttachedRigidBody_->AddCollisionShape(pCapsuleShape_, localTransform);
			SetTrigger(bTrigger_);
		}
	}
}

void CapsuleCollision::DetachFromRigidBody()
{
	if (pAttachedRigidBody_ && pCapsuleShape_)
	{
		pAttachedRigidBody_->RemoveCollisionShape(pCapsuleShape_);
		pAttachedRigidBody_ = nullptr;
	}
}