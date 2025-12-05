#include "RigidBody.h"
#include "Object.h"
#include "Scene.h"
#include "GUI.h"
#include <iostream>
#include <sstream>
#include <algorithm>

void RigidBody::Init(Object* Prt)
{
	_Parent = Prt;
	_ComponentName = "RigidBody";
	_Type = ComponentManager::COMPONENT_TYPE::RIGIDBODY;

	pCompoundShape_ = new btCompoundShape();

	CreateRigidBody();
}

void RigidBody::BeginPlay()
{
	if (_Parent && _Parent->GetParentScene())
	{
		btDiscreteDynamicsWorld* physicsWorld = _Parent->GetParentScene()->GetPhysicsWorld();
		if (physicsWorld)
		{
			try
			{
				if (pRigidBody_ && bAddedToWorld_)
				{
					physicsWorld->removeRigidBody(pRigidBody_);
					bAddedToWorld_ = false;
				}

				CreateRigidBody();

				if (pRigidBody_ && !bAddedToWorld_)
				{
					pRigidBody_->setUserPointer(this);

					if (pRigidBody_->getCollisionShape() && pRigidBody_->getMotionState())
					{
						physicsWorld->addRigidBody(pRigidBody_);
						bAddedToWorld_ = true;

						EditrGUI::GetInstance()->WriteLog("[RigidBody] " +
							_Parent->GetObjectName() + "を物理世界に追加しました（UserPointer: " +
							std::to_string(reinterpret_cast<uintptr_t>(pRigidBody_->getUserPointer())) + ")");

						pRigidBody_->setActivationState(ACTIVE_TAG);
						pRigidBody_->forceActivationState(ACTIVE_TAG);

						SyncTransformToBullet();

						SetGravityEnabled(bUseGravity_);

						SetKinematic(bKinematic_);
					}
					else
					{
						EditrGUI::GetInstance()->WriteLog("[RigidBody] " +
							_Parent->GetObjectName() + " - " +
							"RigidBodyが無効です");
						if (pRigidBody_)
						{
							if (pRigidBody_->getMotionState())
							{
								delete pRigidBody_->getMotionState();
							}
							delete pRigidBody_;
							pRigidBody_ = nullptr;
						}
						bAddedToWorld_ = false;
					}
				}
			}
			catch (...)
			{
				EditrGUI::GetInstance()->WriteLog("[RigidBody] " +
					_Parent->GetObjectName() + " - " +
					"RigidBodyの追加中に例外が発生しました");
				if (pRigidBody_)
				{
					if (pRigidBody_->getMotionState())
					{
						delete pRigidBody_->getMotionState();
					}
					delete pRigidBody_;
					pRigidBody_ = nullptr;
				}
				if (pMotionState_)
				{
					delete pMotionState_;
					pMotionState_ = nullptr;
				}
				bAddedToWorld_ = false;
			}
		}
		else
		{
			EditrGUI::GetInstance()->WriteLog("[RigidBody] " +
				_Parent->GetObjectName() + " - " +
				"物理世界が存在しません");
		}
	}
}

void RigidBody::EditUpdate()
{
	if (!pRigidBody_)return;

	if (bKinematic_ && bTransformDirty_)
	{
		SyncTransformToBullet();
		bTransformDirty_ = false;
	}
}

void RigidBody::InGameUpdate()
{
	if (!pRigidBody_)return;

	if (!bKinematic_ && !bManualTransformControl_)
	{
		SyncTransformFromBullet();
	}
	else if (bKinematic_ || bTransformDirty_)
	{
		SyncTransformToBullet();
		bTransformDirty_ = false;
	}
}

void RigidBody::UInit()
{
	if (pRigidBody_ && _Parent && _Parent->GetParentScene())
	{
		btDiscreteDynamicsWorld* physicsWorld = _Parent->GetParentScene()->GetPhysicsWorld();
		if (physicsWorld && bAddedToWorld_)
		{
			try
			{
				physicsWorld->removeRigidBody(pRigidBody_);
			}
			catch (...)
			{
			}
			bAddedToWorld_ = false;
		}

		if (pRigidBody_->getMotionState())
		{
			btMotionState* motionState = pRigidBody_->getMotionState();
			pRigidBody_->setMotionState(nullptr);
			if (motionState == pMotionState_)
			{
				pMotionState_ = nullptr;
			}
			delete motionState;
		}
		if (pRigidBody_)
		{
			if (_Parent && _Parent->GetParentScene())
			{
				btDiscreteDynamicsWorld* physicsWorld = _Parent->GetParentScene()->GetPhysicsWorld();
				if (physicsWorld)
				{
					physicsWorld->removeRigidBody(pRigidBody_);
				}
			}
			pRigidBody_->setMotionState(nullptr);
			delete pRigidBody_;
			pRigidBody_ = nullptr;
		}
	}

	if (pCompoundShape_)
	{
		delete pCompoundShape_;
		pCompoundShape_ = nullptr;
	}

	if (pMotionState_)
	{
		delete pMotionState_;
		pMotionState_ = nullptr;
	}

	RegisteredColliders_.clear();
}

void RigidBody::DrawInspector()
{
	auto SJ = [](const char* s)->std::string { return GUI::GetInstance()->ShiftJISToUTF8(s); };
	std::string label = _ComponentName + "##" + std::to_string(reinterpret_cast<uintptr_t>(this));
	if (!ImGui::CollapsingHeader(SJ(label.c_str()).c_str()))return;
	label = "RigidBodyTable##" + std::to_string(reinterpret_cast<uintptr_t>(this));
	if (ImGui::BeginTable(SJ(label.c_str()).c_str(), 2, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV)) {
		ImGui::TableNextRow();
		ImGui::TableSetColumnIndex(0); ImGui::Text(SJ("質量").c_str());
		float Mass = fMass_;
		ImGui::TableSetColumnIndex(1); ImGui::InputFloat(SJ("##MassInput").c_str(), &Mass, 0.1f, 1.0f, "%.3f");
		SetMass(Mass);

		ImGui::TableNextRow();
		ImGui::TableSetColumnIndex(0); ImGui::Text(SJ("キネマティック").c_str());
		bool Kinematic = bKinematic_;
		ImGui::TableSetColumnIndex(1); ImGui::Checkbox(SJ("##KinematicCheckbox").c_str(), &Kinematic);
		SetKinematic(Kinematic);

		ImGui::TableNextRow();
		ImGui::TableSetColumnIndex(0); ImGui::Text(SJ("重力を使用").c_str());
		bool UseGravity = bUseGravity_;
		ImGui::TableSetColumnIndex(1); ImGui::Checkbox(SJ("##UseGravityCheckbox").c_str(), &UseGravity);
		SetGravityEnabled(UseGravity);

		ImGui::EndTable();
	}
}

void RigidBody::SaveToFile(std::ostream& out)
{
	out << fMass_ << std::endl;
	out << bKinematic_ << std::endl;
	out << bUseGravity_ << std::endl;
}

void RigidBody::LoadFromFile(std::istream& in)
{
	in >> fMass_;
	in >> bKinematic_;
	in >> bUseGravity_;
	if (pRigidBody_)
	{
		UpdateMassProperties();
		SetKinematic(bKinematic_);
		SetGravityEnabled(bUseGravity_);
	}
}

void RigidBody::SetMass(float mass)
{
	fMass_ = mass;
	UpdateMassProperties();
}

void RigidBody::SetKinematic(bool kinematic)
{
	bKinematic_ = kinematic;
	if (pRigidBody_)
	{
		if (bKinematic_)
		{
			pRigidBody_->setCollisionFlags(pRigidBody_->getCollisionFlags() | btCollisionObject::CF_KINEMATIC_OBJECT);
			pRigidBody_->setActivationState(DISABLE_DEACTIVATION);
		}
		else
		{
			pRigidBody_->setCollisionFlags(pRigidBody_->getCollisionFlags() & ~btCollisionObject::CF_KINEMATIC_OBJECT);
			pRigidBody_->setActivationState(ACTIVE_TAG);
		}
	}
}

void RigidBody::SetGravityEnabled(bool useGravity)
{
	bUseGravity_ = useGravity;
	if (pRigidBody_)
	{
		if (bUseGravity_)
		{
			if (_Parent && _Parent->GetParentScene())
			{
				btVector3 gravity = _Parent->GetParentScene()->GetPhysicsWorld()->getGravity();
				pRigidBody_->setGravity(gravity);
			}
		}
		else
		{
			pRigidBody_->setGravity(btVector3(0, 0, 0));
		}
		pRigidBody_->setActivationState(ACTIVE_TAG);
		pRigidBody_->forceActivationState(ACTIVE_TAG);
	}
}

void RigidBody::AddCollisionShape(btCollisionShape* shape, const btTransform& localTransform)
{
	if (pCompoundShape_ && shape)
	{
		auto it = std::find(RegisteredColliders_.begin(), RegisteredColliders_.end(), shape);
		if (it == RegisteredColliders_.end())
		{
			pCompoundShape_->addChildShape(localTransform, shape);
			RegisteredColliders_.push_back(shape);
			UpdateMassProperties();
		}
	}
}

void RigidBody::RemoveCollisionShape(btCollisionShape* shape)
{
	if (pCompoundShape_ && shape)
	{
		auto it = std::find(RegisteredColliders_.begin(), RegisteredColliders_.end(), shape);
		if (it != RegisteredColliders_.end())
		{
			int numChildren = pCompoundShape_->getNumChildShapes();
			for (int i = 0; i < numChildren; ++i)
			{
				if (pCompoundShape_->getChildShape(i) == shape)
				{
					pCompoundShape_->removeChildShapeByIndex(i);
					break;
				}
			}
			RegisteredColliders_.erase(it);
			UpdateMassProperties();
		}
	}
}

void RigidBody::AddForce(const DirectX::XMFLOAT3& force)
{
	if (pRigidBody_ && !bKinematic_)
	{
		pRigidBody_->applyCentralForce(btVector3(force.x, force.y, force.z));
	}
}

void RigidBody::AddImpulse(const DirectX::XMFLOAT3& impulse)
{
	if (pRigidBody_ && !bKinematic_)
	{
		pRigidBody_->applyCentralImpulse(btVector3(impulse.x, impulse.y, impulse.z));
	}
}

void RigidBody::SetVelocity(const DirectX::XMFLOAT3& velocity)
{
	if (pRigidBody_ && !bKinematic_)
	{
		pRigidBody_->setLinearVelocity(btVector3(velocity.x, velocity.y, velocity.z));
	}
}

DirectX::XMFLOAT3 RigidBody::GetVelocity() const
{
	if (pRigidBody_)
	{
		btVector3 vel = pRigidBody_->getLinearVelocity();
		return DirectX::XMFLOAT3(vel.getX(), vel.getY(), vel.getZ());
	}
	return DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f);
}

void RigidBody::SyncTransformFromBullet()
{
	if (!pRigidBody_ || !_Parent) return;

	btTransform worldTransform;
	pRigidBody_->getMotionState()->getWorldTransform(worldTransform);

	btVector3 origin = worldTransform.getOrigin();
	auto currentTransform = _Parent->GetTransform();
	currentTransform.position = DirectX::XMFLOAT3(origin.getX(), origin.getY(), origin.getZ());

	btQuaternion rotation = worldTransform.getRotation();
	currentTransform.rotation = QuaternionToEuler(rotation);

	_Parent->SetTransform(currentTransform);
}

void RigidBody::SyncTransformToBullet()
{
	if (!pRigidBody_ || !_Parent) return;
	auto currentTransform = _Parent->GetTransform();

	btTransform worldTransform;
	worldTransform.setOrigin(btVector3
	(
		currentTransform.position.x,
		currentTransform.position.y,
		currentTransform.position.z
	));

	worldTransform.setRotation(EulerToQuaternion(currentTransform.rotation));

	pRigidBody_->setWorldTransform(worldTransform);
	pRigidBody_->getMotionState()->setWorldTransform(worldTransform);
}

void RigidBody::SyncPositionToBullet(const DirectX::XMFLOAT3& position)
{
	if (!pRigidBody_) return;

	btVector3 linearVel = pRigidBody_->getLinearVelocity();
	btVector3 angularVel = pRigidBody_->getAngularVelocity();

	btTransform transform = pRigidBody_->getWorldTransform();
	transform.setOrigin(btVector3(position.x, position.y, position.z));
	pRigidBody_->setWorldTransform(transform);
	pRigidBody_->getMotionState()->setWorldTransform(transform);

	if (bKinematic_)
	{
		pRigidBody_->setLinearVelocity(linearVel);
		pRigidBody_->setAngularVelocity(angularVel);
		pRigidBody_->activate();
	}
}

void RigidBody::SyncRotationToBullet(const DirectX::XMFLOAT3& rotation)
{
	if (pRigidBody_) return;

	btVector3 linearVel = pRigidBody_->getLinearVelocity();
	btVector3 angularVel = pRigidBody_->getAngularVelocity();

	btTransform transform = pRigidBody_->getWorldTransform();
	transform.setRotation(EulerToQuaternion(rotation));
	pRigidBody_->setWorldTransform(transform);
	pRigidBody_->getMotionState()->setWorldTransform(transform);

	if (bKinematic_)
	{
		pRigidBody_->setLinearVelocity(linearVel);
		pRigidBody_->setAngularVelocity(angularVel);
		pRigidBody_->activate();
	}
}

void RigidBody::WarpTo(const DirectX::XMFLOAT3& position)
{
	if (pRigidBody_)return;

	btTransform transform = pRigidBody_->getWorldTransform();
	transform.setOrigin(btVector3(position.x, position.y, position.z));
	pRigidBody_->setWorldTransform(transform);
	pRigidBody_->getMotionState()->setWorldTransform(transform);

	pRigidBody_->setLinearVelocity(btVector3(0, 0, 0));
	pRigidBody_->setAngularVelocity(btVector3(0, 0, 0));
	pRigidBody_->activate();
}

void RigidBody::CreateRigidBody()
{
	if (pRigidBody_)
	{
		if (_Parent && _Parent->GetParentScene())
		{
			btDiscreteDynamicsWorld* physicsWorld = _Parent->GetParentScene()->GetPhysicsWorld();
			if (physicsWorld && bAddedToWorld_) {
				physicsWorld->removeRigidBody(pRigidBody_);
			}
		}
		pRigidBody_->setMotionState(nullptr);
		delete pRigidBody_;
		pRigidBody_ = nullptr;
	}

	if (pMotionState_) {
		delete pMotionState_;
		pMotionState_ = nullptr;
	}

	btTransform startTransform;
	startTransform.setIdentity();
	if (_Parent)
	{
		auto currentTransform = _Parent->GetTransform();
		startTransform.setOrigin(btVector3(
			currentTransform.position.x,
			currentTransform.position.y,
			currentTransform.position.z
		));
		startTransform.setRotation(EulerToQuaternion(currentTransform.rotation));
	}
	pMotionState_ = new btDefaultMotionState(startTransform);

	UpdateMassProperties();

	if (!pCompoundShape_) {
		pCompoundShape_ = new btCompoundShape();
	}

	btRigidBody::btRigidBodyConstructionInfo rbInfo(fMass_, pMotionState_, pCompoundShape_, localInertia_);
	pRigidBody_ = new btRigidBody(rbInfo);

	pRigidBody_->setUserPointer(this);

	SetKinematic(bKinematic_);
	SetGravityEnabled(bUseGravity_);

	bAddedToWorld_ = false;
}

void RigidBody::UpdateMassProperties() {
	if (pCompoundShape_)
	{
		if (bKinematic_ || fMass_ == 0.0f)
		{
			localInertia_ = btVector3(0, 0, 0);
		}
		else
		{
			pCompoundShape_->calculateLocalInertia(fMass_, localInertia_);
		}
		if (pRigidBody_)
		{
			pRigidBody_->setMassProps(fMass_, localInertia_);
			pRigidBody_->updateInertiaTensor();
		}
	}
}

btQuaternion RigidBody::EulerToQuaternion(const DirectX::XMFLOAT3& euler)
{
	DirectX::XMMATRIX rotX = DirectX::XMMatrixRotationX(euler.x);
	DirectX::XMMATRIX rotY = DirectX::XMMatrixRotationY(euler.y);
	DirectX::XMMATRIX rotZ = DirectX::XMMatrixRotationZ(euler.z);
	DirectX::XMMATRIX rotMatrix = rotX * rotY * rotZ;

	DirectX::XMVECTOR quat_vec = DirectX::XMQuaternionRotationMatrix(rotMatrix);
	DirectX::XMFLOAT4 quat_float;
	DirectX::XMStoreFloat4(&quat_float, quat_vec);

	return btQuaternion(quat_float.x, quat_float.y, quat_float.z, quat_float.w);
}

DirectX::XMFLOAT3 RigidBody::QuaternionToEuler(const btQuaternion& quat)
{
	DirectX::XMVECTOR q = DirectX::XMVectorSet(quat.getX(), quat.getY(), quat.getZ(), quat.getW());
	DirectX::XMMATRIX rotMatrix = DirectX::XMMatrixRotationQuaternion(q);

	DirectX::XMFLOAT3 euler;
	euler.x = asinf(-rotMatrix.r[2].m128_f32[1]);                        // Pitch
	euler.y = atan2f(rotMatrix.r[2].m128_f32[0], rotMatrix.r[2].m128_f32[2]); // Yaw
	euler.z = atan2f(rotMatrix.r[0].m128_f32[1], rotMatrix.r[1].m128_f32[1]); // Roll

	return euler;
}