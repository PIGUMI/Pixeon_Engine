#include "BoxCollision.h"
#include "RigidBody.h"
#include "Object.h"
#include "Scene.h"
#include "GUI.h"
#include "CollisionManager.h"
#include "_Geometry.h"

void BoxCollision::Init(AbstractObject* Prt)
{
	_Parent = Prt;
	_ComponentName = "BoxCollision";
	_Type = ComponentManager::COMPONENT_TYPE::BOX_COLLISION;

	CreateBoxShape();

	auto currentScene = _Parent->GetWorldTransform();
	f3LastPosition_ = currentScene.position;
	f3LastRotation_ = currentScene.rotation;
	f3LastScale_ = currentScene.scale;

	nBeginPlayCount = 0;
	bCallBackSetAfterBeginPlay = false;
}

void BoxCollision::BeginPlay()
{
	nBeginPlayCount++;

	bool hadCallbacks = HasCollisionEnterCallBack() || HasCollisionStayCallBack() || HasCollisionExitCallBack();

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
			collisionManager->RegisterBoxCollision(this);
		}
	}
}

void BoxCollision::EditUpdate()
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

void BoxCollision::InGameUpdate()
{
	EditUpdate();
}

void BoxCollision::Draw(int Layer)
{
	if (Layer != _LayerNumber) return;
	if (!m_b_BoxLine)return;
	if (_Parent)
	{
		auto transform = _Parent->GetWorldTransform();
		DirectX::XMFLOAT3 pos = transform.position;
		DirectX::XMFLOAT3 size = f3Size_;
		DirectX::XMFLOAT3 center = f3Center_;
		DirectX::XMFLOAT3 rot = transform.rotation;

		// 回転行列（XYZ順）- GameObjectの位置を中心に回転
		DirectX::XMMATRIX matRot =
			DirectX::XMMatrixRotationZ(rot.z) *
			DirectX::XMMatrixRotationY(rot.y) *
			DirectX::XMMatrixRotationX(rot.x);

		// 8頂点（ローカル座標系）- GameObjectの中心を原点とする
		DirectX::XMFLOAT3 localCorners[8] = {
			{center.x - size.x / 2, center.y - size.y / 2, center.z - size.z / 2},
			{center.x + size.x / 2, center.y - size.y / 2, center.z - size.z / 2},
			{center.x + size.x / 2, center.y + size.y / 2, center.z - size.z / 2},
			{center.x - size.x / 2, center.y + size.y / 2, center.z - size.z / 2},
			{center.x - size.x / 2, center.y - size.y / 2, center.z + size.z / 2},
			{center.x + size.x / 2, center.y - size.y / 2, center.z + size.z / 2},
			{center.x + size.x / 2, center.y + size.y / 2, center.z + size.z / 2},
			{center.x - size.x / 2, center.y + size.y / 2, center.z + size.z / 2}
		};

		// ワールド座標系に変換
		DirectX::XMFLOAT3 worldCorners[8];
		for (int i = 0; i < 8; ++i) {
			// ローカル頂点を回転
			DirectX::XMVECTOR v = DirectX::XMVectorSet(localCorners[i].x, localCorners[i].y, localCorners[i].z, 1.0f);
			v = DirectX::XMVector3Transform(v, matRot);
			// GameObjectの位置を加算
			v = DirectX::XMVectorAdd(v, DirectX::XMVectorSet(pos.x, pos.y, pos.z, 0.0f));
			DirectX::XMStoreFloat3(&worldCorners[i], v);
		}

		DirectX::XMFLOAT4 color = bTrigger_
			? DirectX::XMFLOAT4(1.0f, 1.0f, 0.0f, 1.0f) // Trigger: Yellow
			: DirectX::XMFLOAT4(1.0f, 0.0f, 0.0f, 1.0f); // 通常: Red

		int edges[12][2] = {
			{0,1},{1,2},{2,3},{3,0},
			{4,5},{5,6},{6,7},{7,4},
			{0,4},{1,5},{2,6},{3,7}
		};
		auto Cam = _Parent->GetParentScene()->GetMainCamera();
		DirectX::XMFLOAT4X4 Proj = Cam->GetProjectionMatrix();
		DirectX::XMFLOAT4X4 view = Cam->GetViewMatrix();
		DirectX::XMFLOAT4X4 world;
		DirectX::XMStoreFloat4x4(&world, DirectX::XMMatrixIdentity());
		for (int i = 0; i < 12; ++i) {
			LineRenderer::GetInstance()->DrawLine(
				worldCorners[edges[i][0]],
				worldCorners[edges[i][1]],
				color,
				world, view, Proj,
				0.05f // まずは小さい値で
			);
		}
	}
}

void BoxCollision::UInit()
{
	if (_Parent && _Parent->GetParentScene())
	{
		CollisionManager* collisionManager = _Parent->GetParentScene()->GetCollisionManager();
		if (collisionManager)
		{
			collisionManager->UnregisterBoxCollision(this);
		}
	}

	DetachFromRigidBody();
	if (pBoxShape_)
	{
		delete pBoxShape_;
		pBoxShape_ = nullptr;
	}
	CollidingObjects_.clear();
	CurrentCollisions_.clear();
}

void BoxCollision::DrawInspector()
{
	auto SJ = [](const char* s)->std::string { return GUI::GetInstance()->ShiftJISToUTF8(s); };
	std::string label = _ComponentName + "##" + std::to_string(reinterpret_cast<uintptr_t>(this));
	if (!ImGui::CollapsingHeader(SJ(label.c_str()).c_str()))return;
	label = "Size##" + std::to_string(reinterpret_cast<uintptr_t>(this));
	if (ImGui::BeginTable(SJ(label.c_str()).c_str(), 2, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV)) {
		ImGui::TableNextRow();
		ImGui::TableSetColumnIndex(0); ImGui::Text(SJ("レイヤー").c_str());
		ImGui::TableSetColumnIndex(1);
		ImGui::InputInt("##LayerInput", &_LayerNumber);
		ImGui::TableNextRow();
		ImGui::TableSetColumnIndex(0); ImGui::Text(SJ("サイズ").c_str());
		ImGui::TableSetColumnIndex(1);
		DirectX::XMFLOAT3 size = f3Size_;
		if (ImGui::InputFloat3("##SizeInput", &size.x, "%.3f"))
		{
			SetSize(size);
		};

		ImGui::TableNextRow();
		ImGui::TableSetColumnIndex(0); ImGui::Text(SJ("中心位置").c_str());
		ImGui::TableSetColumnIndex(1);
		DirectX::XMFLOAT3 center = f3Center_;
		if (ImGui::InputFloat3("##CenterInput", &center.x, "%.3f"))
		{
			SetCenter(center);
		};

		ImGui::TableNextRow();
		ImGui::TableSetColumnIndex(0); ImGui::Text(SJ("トリガー").c_str());
		ImGui::TableSetColumnIndex(1);
		bool isTrigger = bTrigger_;
		if (ImGui::Checkbox("##TriggerInput", &isTrigger))
		{
			SetTrigger(isTrigger);
		};

		ImGui::TableNextRow();
		ImGui::TableSetColumnIndex(0); ImGui::Text(SJ("ライン描画").c_str());
		ImGui::TableSetColumnIndex(1);
		ImGui::Checkbox("##DrawWireframeInput", &m_b_BoxLine);
		ImGui::EndTable();
	}
}

void BoxCollision::SaveToFile(std::ostream& out)
{
	out << f3Size_.x << " " << f3Size_.y << " " << f3Size_.z << " "
		<< f3Center_.x << " " << f3Center_.y << " " << f3Center_.z << " "
		<< bTrigger_ <<" " << _LayerNumber << " " << m_b_BoxLine <<"\n";
}

void BoxCollision::LoadFromFile(std::istream& in)
{
	in >> f3Size_.x >> f3Size_.y >> f3Size_.z
		>> f3Center_.x >> f3Center_.y >> f3Center_.z
		>> bTrigger_ >> _LayerNumber >> m_b_BoxLine;

	SetSize(f3Size_);
	SetCenter(f3Center_);
	SetTrigger(bTrigger_);
}

void BoxCollision::SetSize(const DirectX::XMFLOAT3& size)
{
	f3Size_ = size;
	CreateBoxShape();
	UpdateCollisionShape();
}

void BoxCollision::SetCenter(const DirectX::XMFLOAT3& center)
{
	f3Center_ = center;
	UpdateCollisionShape();
}

void BoxCollision::SetTrigger(bool isTrigger)
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

bool BoxCollision::CheckCollision(BoxCollision* otherBox, CollisionInfo& outCollisionInfo)
{
	if (!otherBox || !_Parent || !otherBox->GetParent())return false;

	if (otherBox == this)return false;

	DirectX::XMFLOAT3 pos1 = _Parent->GetWorldTransform().position;
	DirectX::XMFLOAT3 rot1 = _Parent->GetWorldTransform().rotation;
	DirectX::XMFLOAT3 size1 = f3Size_;

	pos1.x += f3Center_.x;
	pos1.y += f3Center_.y;
	pos1.z += f3Center_.z;

	DirectX::XMFLOAT3 pos2 = otherBox->GetParent()->GetWorldTransform().position;
	DirectX::XMFLOAT3 rot2 = otherBox->GetParent()->GetWorldTransform().rotation;
	DirectX::XMFLOAT3 size2 = otherBox->f3Size_;

	pos2.x += otherBox->f3Center_.x;
	pos2.y += otherBox->f3Center_.y;
	pos2.z += otherBox->f3Center_.z;

	return OBBIntersection(pos1, rot1, size1, pos2, rot2, size2, outCollisionInfo);
}

std::vector<CollisionInfo> BoxCollision::GetCollisions()
{
	return CurrentCollisions_;
}

void BoxCollision::DrawDebugWireframe()
{
}

void BoxCollision::CreateBoxShape()
{
	if (pBoxShape_)
	{
		delete pBoxShape_;
	}
	pBoxShape_ = new btBoxShape(btVector3(f3Size_.x * 0.5f, f3Size_.y * 0.5f, f3Size_.z * 0.5f));
}

void BoxCollision::UpdateCollisionShape()
{
	if (pAttachedRigidBody_ && pBoxShape_)
	{
		pAttachedRigidBody_->RemoveCollisionShape(pBoxShape_);
	}
	if (pBoxShape_)
	{
		delete pBoxShape_;
		pBoxShape_ = nullptr;
	}
	pBoxShape_ = new btBoxShape(btVector3(f3Size_.x * 0.5f, f3Size_.y * 0.5f, f3Size_.z * 0.5f));
	if (pAttachedRigidBody_ && pBoxShape_)
	{
		btTransform localTransform;
		localTransform.setIdentity();
		localTransform.setOrigin(btVector3(f3Center_.x, f3Center_.y, f3Center_.z));
		pAttachedRigidBody_->AddCollisionShape(pBoxShape_, localTransform);
		SetTrigger(bTrigger_);
	}
}

void BoxCollision::AttachToRigidBody()
{
	if (_Parent)
	{
		pAttachedRigidBody_ = _Parent->GetComponent<RigidBody>();
		if (pAttachedRigidBody_ && pBoxShape_)
		{
			btTransform localTransform;
			localTransform.setIdentity();
			localTransform.setOrigin(btVector3(f3Center_.x, f3Center_.y, f3Center_.z));

			pAttachedRigidBody_->AddCollisionShape(pBoxShape_, localTransform);
			SetTrigger(bTrigger_);
		}
	}
}

void BoxCollision::DetachFromRigidBody()
{
	if (pAttachedRigidBody_ && pBoxShape_)
	{
		pAttachedRigidBody_->RemoveCollisionShape(pBoxShape_);
		pAttachedRigidBody_ = nullptr;
	}
}

void BoxCollision::ProcessCollisionCallBacks()
{
	/* 現在使用されてません */
	/* CollisionManagerが代わりに衝突処理を行います。 */
}

bool BoxCollision::OBBIntersection(const DirectX::XMFLOAT3& pos1, const DirectX::XMFLOAT3& rot1, const DirectX::XMFLOAT3& size1, const DirectX::XMFLOAT3& pos2, const DirectX::XMFLOAT3& rot2, const DirectX::XMFLOAT3& size2, CollisionInfo& info)
{
	DirectX::XMFLOAT3 min1 = { pos1.x - size1.x * 0.5f, pos1.y - size1.y * 0.5f, pos1.z - size1.z * 0.5f };
	DirectX::XMFLOAT3 max1 = { pos1.x + size1.x * 0.5f, pos1.y + size1.y * 0.5f, pos1.z + size1.z * 0.5f };

	DirectX::XMFLOAT3 min2 = { pos2.x - size2.x * 0.5f, pos2.y - size2.y * 0.5f, pos2.z - size2.z * 0.5f };
	DirectX::XMFLOAT3 max2 = { pos2.x + size2.x * 0.5f, pos2.y + size2.y * 0.5f, pos2.z + size2.z * 0.5f };

	bool intersect = (min1.x <= max2.x && max1.x >= min2.x) &&
		(min1.y <= max2.y && max1.y >= min2.y) &&
		(min1.z <= max2.z && max1.z >= min2.z);

	if (intersect)
	{
		// 衝突点と法線を計算（簡易版）
		info.HitPoint = DirectX::XMFLOAT3(
			(pos1.x + pos2.x) * 0.5f,
			(pos1.y + pos2.y) * 0.5f,
			(pos1.z + pos2.z) * 0.5f
		);

		// 簡易的な法線計算（pos1からpos2への方向）
		DirectX::XMFLOAT3 direction = {
			pos2.x - pos1.x,
			pos2.y - pos1.y,
			pos2.z - pos1.z
		};

		float length = sqrtf(direction.x * direction.x + direction.y * direction.y + direction.z * direction.z);
		if (length > 0.0f)
		{
			info.HitNormal = DirectX::XMFLOAT3(
				direction.x / length,
				direction.y / length,
				direction.z / length
			);
		}
		else
		{
			info.HitNormal = DirectX::XMFLOAT3(0.0f, 1.0f, 0.0f);
		}

		info.Distance = length;
	}

	return intersect;
}