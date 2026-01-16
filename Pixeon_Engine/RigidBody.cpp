#include "RigidBody.h"
#include "Object.h"
#include "Scene.h"
#include "GUI.h"
#include <iostream>
#include <sstream>
#include <algorithm>

void RigidBody::Init(AbstractObject* Prt)
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

						pRigidBody_->setActivationState(ACTIVE_TAG);
						pRigidBody_->forceActivationState(ACTIVE_TAG);

						SyncTransformToBullet();

						SetGravityEnabled(bUseGravity_);

						SetKinematic(bKinematic_);
					}
					else
					{
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
		ImGui::TableSetColumnIndex(0); ImGui::Text(SJ("¿—Ê").c_str());
		float Mass = fMass_;
		ImGui::TableSetColumnIndex(1); ImGui::InputFloat(SJ("##MassInput").c_str(), &Mass, 0.1f, 1.0f, "%.3f");
		SetMass(Mass);

		ImGui::TableNextRow();
		ImGui::TableSetColumnIndex(0); ImGui::Text(SJ("ƒLƒlƒ}ƒeƒBƒbƒN").c_str());
		bool Kinematic = bKinematic_;
		ImGui::TableSetColumnIndex(1); ImGui::Checkbox(SJ("##KinematicCheckbox").c_str(), &Kinematic);
		SetKinematic(Kinematic);

		ImGui::TableNextRow();
		ImGui::TableSetColumnIndex(0); ImGui::Text(SJ("d—Í‚ğg—p").c_str());
		bool UseGravity = bUseGravity_;
		ImGui::TableSetColumnIndex(1); ImGui::Checkbox(SJ("##UseGravityCheckbox").c_str(), &UseGravity);
		SetGravityEnabled(UseGravity);

		// –€CŒW”
		ImGui::TableNextRow();
		ImGui::TableSetColumnIndex(0); ImGui::Text(SJ("–€CŒW”").c_str());
		float Friction = fFriction_;
		ImGui::TableSetColumnIndex(1);
		if (ImGui::SliderFloat(SJ("##FrictionSlider").c_str(), &Friction, 0.0f, 1.0f, "%.3f"))
			SetFriction(Friction);

		// ”½”­ŒW”
		ImGui::TableNextRow();
		ImGui::TableSetColumnIndex(0); ImGui::Text(SJ("”½”­ŒW”").c_str());
		float Restitution = fRestitution_;
		ImGui::TableSetColumnIndex(1);
		if (ImGui::SliderFloat(SJ("##RestitutionSlider").c_str(), &Restitution, 0.0f, 1.0f, "%.3f"))
			SetRestitution(Restitution);

		// üŒ`Œ¸Š
		ImGui::TableNextRow();
		ImGui::TableSetColumnIndex(0); ImGui::Text(SJ("üŒ`Œ¸Š").c_str());
		float LinearDamping = fLinearDamping_;
		ImGui::TableSetColumnIndex(1);
		if (ImGui::SliderFloat(SJ("##LinearDampingSlider").c_str(), &LinearDamping, 0.0f, 1.0f, "%.3f"))
			SetLinearDamping(LinearDamping);

		// Šp“xŒ¸Š
		ImGui::TableNextRow();
		ImGui::TableSetColumnIndex(0); ImGui::Text(SJ("Šp“xŒ¸Š").c_str());
		float AngularDamping = fAngularDamping_;
		ImGui::TableSetColumnIndex(1);
		if (ImGui::SliderFloat(SJ("##AngularDampingSlider").c_str(), &AngularDamping, 0.0f, 1.0f, "%.3f"))
			SetAngularDamping(AngularDamping);

		// “]‚ª‚è–€C
		ImGui::TableNextRow();
		ImGui::TableSetColumnIndex(0); ImGui::Text(SJ("“]‚ª‚è–€C").c_str());
		float RollingFriction = fRollingFriction_;
		ImGui::TableSetColumnIndex(1);
		if (ImGui::SliderFloat(SJ("##RollingFrictionSlider").c_str(), &RollingFriction, 0.0f, 1.0f, "%.3f"))
			SetRollingFriction(RollingFriction);

		// ‰ñ“]–€C
		ImGui::TableNextRow();
		ImGui::TableSetColumnIndex(0); ImGui::Text(SJ("‰ñ“]–€C").c_str());
		float SpinningFriction = fSpinningFriction_;
		ImGui::TableSetColumnIndex(1);
		if (ImGui::SliderFloat(SJ("##SpinningFrictionSlider").c_str(), &SpinningFriction, 0.0f, 1.0f, "%.3f"))
			SetSpinningFriction(SpinningFriction);

		ImGui::EndTable();
	}
}

void RigidBody::SaveToFile(std::ostream& out)
{
	out << fMass_ << std::endl;
	out << bKinematic_ << std::endl;
	out << bUseGravity_ << std::endl;
	out << fFriction_ << std::endl;
	out << fRestitution_ << std::endl;
	out << fLinearDamping_ << std::endl;
	out << fAngularDamping_ << std::endl;
	out << fRollingFriction_ << std::endl;
	out << fSpinningFriction_ << std::endl;
}

void RigidBody::LoadFromFile(std::istream& in)
{
	in >> fMass_;
	in >> bKinematic_;
	in >> bUseGravity_;
	in >> fFriction_;
	in >> fRestitution_;
	in >> fLinearDamping_;
	in >> fAngularDamping_;
	in >> fRollingFriction_;
	in >> fSpinningFriction_;

	if (pRigidBody_)
	{
		UpdateMassProperties();
		SetKinematic(bKinematic_);
		SetGravityEnabled(bUseGravity_);
		SetFriction(fFriction_);
		SetRestitution(fRestitution_);
		SetDamping(fLinearDamping_, fAngularDamping_);
		SetRollingFriction(fRollingFriction_);
		SetSpinningFriction(fSpinningFriction_);
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
	btQuaternion rotation = worldTransform.getRotation();

	DirectX::XMFLOAT3 worldPos(origin.getX(), origin.getY(), origin.getZ());
	DirectX::XMFLOAT3 worldRot = QuaternionToEuler(rotation);

	if (_Parent->GetParent()) {
		Transform parentWorld = _Parent->GetParent()->GetWorldTransform();

		DirectX::XMMATRIX parentRotMat = DirectX::XMMatrixRotationRollPitchYaw(
			parentWorld.rotation.x,
			parentWorld.rotation.y,
			parentWorld.rotation.z
		);

		DirectX::XMMATRIX invParentRotMat = DirectX::XMMatrixInverse(nullptr, parentRotMat);

		DirectX::XMVECTOR worldPosVec = DirectX::XMLoadFloat3(&worldPos);
		DirectX::XMVECTOR parentPosVec = DirectX::XMLoadFloat3(&parentWorld.position);
		DirectX::XMVECTOR relativePos = DirectX::XMVectorSubtract(worldPosVec, parentPosVec);

		DirectX::XMVECTOR localPosVec = DirectX::XMVector3Transform(relativePos, invParentRotMat);

		DirectX::XMFLOAT3 localPos;
		DirectX::XMStoreFloat3(&localPos, localPosVec);
		localPos.x /= parentWorld.scale.x;
		localPos.y /= parentWorld.scale.y;
		localPos.z /= parentWorld.scale.z;

		DirectX::XMFLOAT3 localRot;
		localRot.x = worldRot.x - parentWorld.rotation.x;
		localRot.y = worldRot.y - parentWorld.rotation.y;
		localRot.z = worldRot.z - parentWorld.rotation.z;

		Transform localTransform = _Parent->GetTransform();
		localTransform.position = localPos;
		localTransform.rotation = localRot;
		_Parent->SetTransform(localTransform);
	}
	else {
		Transform currentTransform = _Parent->GetTransform();
		currentTransform.position = worldPos;
		currentTransform.rotation = worldRot;
		_Parent->SetTransform(currentTransform);
	}
}

void RigidBody::SyncTransformToBullet()
{
	if (!pRigidBody_ || !_Parent) return;

	auto worldTransform = _Parent->GetWorldTransform();

	btTransform bulletWorldTransform;
	bulletWorldTransform.setOrigin(btVector3(
		worldTransform.position.x,
		worldTransform.position.y,
		worldTransform.position.z
	));

	bulletWorldTransform.setRotation(EulerToQuaternion(worldTransform.rotation));

	pRigidBody_->setWorldTransform(bulletWorldTransform);
	pRigidBody_->getMotionState()->setWorldTransform(bulletWorldTransform);

	if (bKinematic_) {
		pRigidBody_->activate();
	}
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

	if (bKinematic_) {
		pRigidBody_->setLinearVelocity(linearVel);
		pRigidBody_->setAngularVelocity(angularVel);
		pRigidBody_->activate();
	}
}

void RigidBody::SyncRotationToBullet(const DirectX::XMFLOAT3& rotation)
{
	if (!pRigidBody_) return;

	btVector3 linearVel = pRigidBody_->getLinearVelocity();
	btVector3 angularVel = pRigidBody_->getAngularVelocity();

	btTransform transform = pRigidBody_->getWorldTransform();
	transform.setRotation(EulerToQuaternion(rotation));
	pRigidBody_->setWorldTransform(transform);
	pRigidBody_->getMotionState()->setWorldTransform(transform);

	if (bKinematic_) {
		pRigidBody_->setLinearVelocity(linearVel);
		pRigidBody_->setAngularVelocity(angularVel);
		pRigidBody_->activate();
	}
}

void RigidBody::WarpTo(const DirectX::XMFLOAT3& position)
{
	if (!pRigidBody_)return;

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
		// ƒ[ƒ‹ƒhÀ•W‚ÌTransform‚ğg—p
		auto worldTransform = _Parent->GetWorldTransform();
		startTransform.setOrigin(btVector3(
			worldTransform.position.x,
			worldTransform.position.y,
			worldTransform.position.z
		));
		startTransform.setRotation(EulerToQuaternion(worldTransform.rotation));
	}
	pMotionState_ = new btDefaultMotionState(startTransform);

	UpdateMassProperties();

	if (!pCompoundShape_) {
		pCompoundShape_ = new btCompoundShape();
	}

	btRigidBody::btRigidBodyConstructionInfo rbInfo(fMass_, pMotionState_, pCompoundShape_, localInertia_);
	pRigidBody_ = new btRigidBody(rbInfo);

	pRigidBody_->setUserPointer(this);

	pRigidBody_->setCcdMotionThreshold(0.01f);
	pRigidBody_->setCcdSweptSphereRadius(0.1f);
	if (pCompoundShape_)
	{
		pCompoundShape_->setMargin(0.04f);
	}
	pRigidBody_->setContactProcessingThreshold(0.001f);
	pRigidBody_->setSleepingThresholds(0.1f, 0.1f);

	// •¨—ƒpƒ‰ƒ[ƒ^‚ğİ’è
	SetFriction(fFriction_);
	SetRestitution(fRestitution_);
	SetDamping(fLinearDamping_, fAngularDamping_);
	SetRollingFriction(fRollingFriction_);
	SetSpinningFriction(fSpinningFriction_);

	SetKinematic(bKinematic_);
	SetGravityEnabled(bUseGravity_);

	bAddedToWorld_ = false;
}

void RigidBody::UpdateMassProperties()
{
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

			pCompoundShape_->recalculateLocalAabb();

			pRigidBody_->activate(true);
		}
	}
}

btQuaternion RigidBody::EulerToQuaternion(const DirectX::XMFLOAT3& euler)
{
	DirectX::XMMATRIX rotMatrix = DirectX::XMMatrixRotationRollPitchYaw(
		euler.x,
		euler.y,
		euler.z
	);

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

void RigidBody::SetFriction(float friction)
{
	fFriction_ = friction;
	if (pRigidBody_)
	{
		pRigidBody_->setFriction(friction);
	}
}

void RigidBody::SetRestitution(float restitution)
{
	fRestitution_ = restitution;
	if (pRigidBody_)
	{
		pRigidBody_->setRestitution(restitution);
	}
}

void RigidBody::SetLinearDamping(float damping)
{
	fLinearDamping_ = damping;
	if (pRigidBody_)
	{
		pRigidBody_->setDamping(fLinearDamping_, fAngularDamping_);
	}
}

void RigidBody::SetAngularDamping(float damping)
{
	fAngularDamping_ = damping;
	if (pRigidBody_)
	{
		pRigidBody_->setDamping(fLinearDamping_, fAngularDamping_);
	}
}

void RigidBody::SetDamping(float linearDamping, float angularDamping)
{
	fLinearDamping_ = linearDamping;
	fAngularDamping_ = angularDamping;
	if (pRigidBody_)
	{
		pRigidBody_->setDamping(linearDamping, angularDamping);
	}
}

void RigidBody::SetRollingFriction(float rollingFriction)
{
	fRollingFriction_ = rollingFriction;
	if (pRigidBody_)
	{
		pRigidBody_->setRollingFriction(rollingFriction);
	}
}

void RigidBody::SetSpinningFriction(float spinningFriction)
{
	fSpinningFriction_ = spinningFriction;
	if (pRigidBody_)
	{
		pRigidBody_->setSpinningFriction(spinningFriction);
	}
}