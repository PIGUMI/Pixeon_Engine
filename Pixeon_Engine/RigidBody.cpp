#include "RigidBody.h"
#include "Object.h"
#include "Scene.h"
#include "GUI.h"
#include "CameraComponent.h"
#include <iostream>
#include <sstream>
#include <algorithm>

void RigidBodyComponent::Init(AbstractObject* Prt)
{
	_Parent = Prt;
	_ComponentName = "RigidBodyComponent";
	_Type = ComponentManager::COMPONENT_TYPE::RIGIDBODY;

	pCompoundShape_ = new btCompoundShape();

	CreateRigidBody();
}

void RigidBodyComponent::BeginPlay()
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
						// カリング状態を確認してワールドに追加
						if (bUseCulling_ && !bAlwaysActive_)
						{
							UpdateCullingState();
						}
						else
						{
							// カリング無効または常にアクティブの場合は必ずワールドに追加
							bActiveInPhysicsWorld_ = true;
						}

						if (bActiveInPhysicsWorld_)
						{
							physicsWorld->addRigidBody(pRigidBody_);
							bAddedToWorld_ = true;
						}

						// スリープ設定を適用
						if (bDisableSleep_)
						{
							pRigidBody_->setActivationState(DISABLE_DEACTIVATION);
						}
						else
						{
							pRigidBody_->setActivationState(ACTIVE_TAG);
							pRigidBody_->forceActivationState(ACTIVE_TAG);
						}

						SyncTransformToBullet();

						SetGravityEnabled(bUseGravity_);

						SetKinematic(bKinematic_);
						if (!bKinematic_ && fMass_ > 0.0f)
						{
							if (bDisableSleep_)
							{
								pRigidBody_->setActivationState(DISABLE_DEACTIVATION);
							}
							else
							{
								pRigidBody_->setActivationState(ACTIVE_TAG);
								pRigidBody_->forceActivationState(ACTIVE_TAG);
							}
							pRigidBody_->activate(true);

							pRigidBody_->setLinearVelocity(btVector3(0, -0.01f, 0));
						}
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

void RigidBodyComponent::EditUpdate()
{
	if (!pRigidBody_)return;

	if (bKinematic_ && bTransformDirty_)
	{
		SyncTransformToBullet();
		bTransformDirty_ = false;
	}
}

void RigidBodyComponent::InGameUpdate()
{
	if (!pRigidBody_)return;

	// カリング状態を更新（常にアクティブでない場合のみ）
	if (bUseCulling_ && !bAlwaysActive_)
	{
		UpdateCullingState();
	}

	// スリープ無効化が有効な場合、常にアクティブ状態を維持
	if (bDisableSleep_ && pRigidBody_ && bAddedToWorld_)
	{
		if (pRigidBody_->getActivationState() != DISABLE_DEACTIVATION)
		{
			pRigidBody_->setActivationState(DISABLE_DEACTIVATION);
		}
	}

	if (!bKinematic_ && !bManualTransformControl_ && bActiveInPhysicsWorld_)
	{
		SyncTransformFromBullet();
	}
	else if (bKinematic_ || bTransformDirty_)
	{
		SyncTransformToBullet();
		bTransformDirty_ = false;
	}
}

void RigidBodyComponent::UInit()
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

void RigidBodyComponent::DrawInspector()
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

		// 摩擦係数
		ImGui::TableNextRow();
		ImGui::TableSetColumnIndex(0); ImGui::Text(SJ("摩擦係数").c_str());
		float Friction = fFriction_;
		ImGui::TableSetColumnIndex(1);
		if (ImGui::SliderFloat(SJ("##FrictionSlider").c_str(), &Friction, 0.0f, 1.0f, "%.3f"))
			SetFriction(Friction);

		// 反発係数
		ImGui::TableNextRow();
		ImGui::TableSetColumnIndex(0); ImGui::Text(SJ("反発係数").c_str());
		float Restitution = fRestitution_;
		ImGui::TableSetColumnIndex(1);
		if (ImGui::SliderFloat(SJ("##RestitutionSlider").c_str(), &Restitution, 0.0f, 1.0f, "%.3f"))
			SetRestitution(Restitution);

		// 線形減衰
		ImGui::TableNextRow();
		ImGui::TableSetColumnIndex(0); ImGui::Text(SJ("線形減衰").c_str());
		float LinearDamping = fLinearDamping_;
		ImGui::TableSetColumnIndex(1);
		if (ImGui::SliderFloat(SJ("##LinearDampingSlider").c_str(), &LinearDamping, 0.0f, 1.0f, "%.3f"))
			SetLinearDamping(LinearDamping);

		// 角度減衰
		ImGui::TableNextRow();
		ImGui::TableSetColumnIndex(0); ImGui::Text(SJ("角度減衰").c_str());
		float AngularDamping = fAngularDamping_;
		ImGui::TableSetColumnIndex(1);
		if (ImGui::SliderFloat(SJ("##AngularDampingSlider").c_str(), &AngularDamping, 0.0f, 1.0f, "%.3f"))
			SetAngularDamping(AngularDamping);

		// 転がり摩擦
		ImGui::TableNextRow();
		ImGui::TableSetColumnIndex(0); ImGui::Text(SJ("転がり摩擦").c_str());
		float RollingFriction = fRollingFriction_;
		ImGui::TableSetColumnIndex(1);
		if (ImGui::SliderFloat(SJ("##RollingFrictionSlider").c_str(), &RollingFriction, 0.0f, 1.0f, "%.3f"))
			SetRollingFriction(RollingFriction);

		// 回転摩擦
		ImGui::TableNextRow();
		ImGui::TableSetColumnIndex(0); ImGui::Text(SJ("回転摩擦").c_str());
		float SpinningFriction = fSpinningFriction_;
		ImGui::TableSetColumnIndex(1);
		if (ImGui::SliderFloat(SJ("##SpinningFrictionSlider").c_str(), &SpinningFriction, 0.0f, 1.0f, "%.3f"))
			SetSpinningFriction(SpinningFriction);

		ImGui::TableNextRow();
		ImGui::TableSetColumnIndex(0); ImGui::Text(SJ("回転制約").c_str());
		ImGui::TableSetColumnIndex(1);

		bool lockX = bLockRotationX_;
		bool lockY = bLockRotationY_;
		bool lockZ = bLockRotationZ_;

		if (ImGui::BeginTable(SJ("##RotationLockTable").c_str(), 3, ImGuiTableFlags_SizingFixedFit))
		{
			ImGui::TableNextRow();
			ImGui::TableSetColumnIndex(0);
			if (ImGui::Checkbox(SJ("X##LockRotX").c_str(), &lockX))
				SetRotationConstraintX(lockX);

			ImGui::TableSetColumnIndex(1);
			if (ImGui::Checkbox(SJ("Y##LockRotY").c_str(), &lockY))
				SetRotationConstraintY(lockY);

			ImGui::TableSetColumnIndex(2);
			if (ImGui::Checkbox(SJ("Z##LockRotZ").c_str(), &lockZ))
				SetRotationConstraintZ(lockZ);

			ImGui::EndTable();
		}

		// スリープ無効化
		ImGui::TableNextRow();
		ImGui::TableSetColumnIndex(0); ImGui::Text(SJ("スリープ無効化").c_str());
		bool DisableSleep = bDisableSleep_;
		ImGui::TableSetColumnIndex(1);
		if (ImGui::Checkbox(SJ("##DisableSleepCheckbox").c_str(), &DisableSleep))
			SetDisableSleep(DisableSleep);

		// カリング設定
		ImGui::TableNextRow();
		ImGui::TableSetColumnIndex(0); ImGui::Text(SJ("距離カリング使用").c_str());
		bool UseCulling = bUseCulling_;
		ImGui::TableSetColumnIndex(1);
		if (ImGui::Checkbox(SJ("##UseCullingCheckbox").c_str(), &UseCulling))
			SetUseCulling(UseCulling);

		if (bUseCulling_)
		{
			ImGui::TableNextRow();
			ImGui::TableSetColumnIndex(0); ImGui::Text(SJ("常にアクティブ").c_str());
			bool AlwaysActive = bAlwaysActive_;
			ImGui::TableSetColumnIndex(1);
			if (ImGui::Checkbox(SJ("##AlwaysActiveCheckbox").c_str(), &AlwaysActive))
				SetAlwaysActive(AlwaysActive);

			if (!bAlwaysActive_)
			{
				ImGui::TableNextRow();
				ImGui::TableSetColumnIndex(0); ImGui::Text(SJ("カリング距離").c_str());
				float CullingDistance = fCullingDistance_;
				ImGui::TableSetColumnIndex(1);
				if (ImGui::InputFloat(SJ("##CullingDistanceInput").c_str(), &CullingDistance, 1.0f, 10.0f, "%.1f"))
					SetCullingDistance(CullingDistance);

				ImGui::TableNextRow();
				ImGui::TableSetColumnIndex(0); ImGui::Text(SJ("物理アクティブ").c_str());
				ImGui::TableSetColumnIndex(1);
				ImGui::Text(bActiveInPhysicsWorld_ ? SJ("有効").c_str() : SJ("無効").c_str());
			}
		}

		ImGui::EndTable();
	}
}

void RigidBodyComponent::SaveToFile(std::ostream& out)
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
	out << bLockRotationX_ << std::endl;
	out << bLockRotationY_ << std::endl;
	out << bLockRotationZ_ << std::endl;
	out << bDisableSleep_ << std::endl;
	out << bUseCulling_ << std::endl;
	out << bAlwaysActive_ << std::endl;
	out << fCullingDistance_ << std::endl;
}

void RigidBodyComponent::LoadFromFile(std::istream& in)
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
	in >> bLockRotationX_;
	in >> bLockRotationY_;
	in >> bLockRotationZ_;

	// スリープ設定を読み込み
	if (in >> bDisableSleep_)
	{
		// カリング設定を読み込み(古いファイルとの互換性のためデフォルト値を使用)
		if (in >> bUseCulling_)
		{
			if (in >> bAlwaysActive_)
			{
				in >> fCullingDistance_;
			}
			else
			{
				bAlwaysActive_ = false;
				fCullingDistance_ = 100.0f;
			}
		}
		else
		{
			bUseCulling_ = false;
			bAlwaysActive_ = false;
			fCullingDistance_ = 100.0f;
		}
	}
	else
	{
		bDisableSleep_ = false;
		bUseCulling_ = false;
		bAlwaysActive_ = false;
		fCullingDistance_ = 100.0f;
	}

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
		SetRotationConstraint(bLockRotationX_, bLockRotationY_, bLockRotationZ_);
		SetDisableSleep(bDisableSleep_);
	}
}

void RigidBodyComponent::SetMass(float mass)
{
	fMass_ = mass;
	UpdateMassProperties();
}

void RigidBodyComponent::SetKinematic(bool kinematic)
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

			// スリープ設定に応じてアクティベーション状態を設定
			if (bDisableSleep_)
			{
				pRigidBody_->setActivationState(DISABLE_DEACTIVATION);
			}
			else
			{
				pRigidBody_->setActivationState(ACTIVE_TAG);
			}
		}
	}
}

void RigidBodyComponent::SetGravityEnabled(bool useGravity)
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

		// スリープ設定に応じてアクティベーション状態を設定
		if (bDisableSleep_)
		{
			pRigidBody_->setActivationState(DISABLE_DEACTIVATION);
		}
		else
		{
			pRigidBody_->setActivationState(ACTIVE_TAG);
			pRigidBody_->forceActivationState(ACTIVE_TAG);
		}
	}
}

void RigidBodyComponent::AddCollisionShape(btCollisionShape* shape, const btTransform& localTransform)
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

void RigidBodyComponent::RemoveCollisionShape(btCollisionShape* shape)
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

void RigidBodyComponent::AddForce(const DirectX::XMFLOAT3& force)
{
	if (pRigidBody_ && !bKinematic_)
	{
		pRigidBody_->applyCentralForce(btVector3(force.x, force.y, force.z));
		pRigidBody_->activate(true);
	}
}

void RigidBodyComponent::AddImpulse(const DirectX::XMFLOAT3& impulse)
{
	if (pRigidBody_ && !bKinematic_)
	{
		pRigidBody_->applyCentralImpulse(btVector3(impulse.x, impulse.y, impulse.z));
		pRigidBody_->activate(true);
	}
}

void RigidBodyComponent::SetVelocity(const DirectX::XMFLOAT3& velocity)
{
	if (pRigidBody_ && !bKinematic_)
	{
		pRigidBody_->setLinearVelocity(btVector3(velocity.x, velocity.y, velocity.z));
		pRigidBody_->activate(true);
	}
}

DirectX::XMFLOAT3 RigidBodyComponent::GetVelocity() const
{
	if (pRigidBody_)
	{
		btVector3 vel = pRigidBody_->getLinearVelocity();
		return DirectX::XMFLOAT3(vel.getX(), vel.getY(), vel.getZ());
	}
	return DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f);
}

void RigidBodyComponent::SyncTransformFromBullet()
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

void RigidBodyComponent::SyncTransformToBullet()
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

void RigidBodyComponent::SyncPositionToBullet(const DirectX::XMFLOAT3& position)
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

void RigidBodyComponent::SyncRotationToBullet(const DirectX::XMFLOAT3& rotation)
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

void RigidBodyComponent::WarpTo(const DirectX::XMFLOAT3& position)
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

void RigidBodyComponent::CreateRigidBody()
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
		// ワールド座標のTransformを使用
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

	pRigidBody_->setCcdMotionThreshold(0.0f);

	pRigidBody_->setCcdSweptSphereRadius(0.3f);

	if (pCompoundShape_)
	{
		pCompoundShape_->setMargin(0.02f);
	}

	pRigidBody_->setContactProcessingThreshold(0.0001f);

	// スリープ閾値の設定（スリープ無効化時は高い値を設定）
	if (bDisableSleep_)
	{
		pRigidBody_->setSleepingThresholds(0.0f, 0.0f);
	}
	else
	{
		pRigidBody_->setSleepingThresholds(0.2f, 0.2f);
	}

	pRigidBody_->setDeactivationTime(2.0f);

	pRigidBody_->setAnisotropicFriction(pCompoundShape_->getAnisotropicRollingFrictionDirection(),
		btCollisionObject::CF_ANISOTROPIC_ROLLING_FRICTION);

	if (fMass_ == 0.0f || (bKinematic_ && fMass_ <= 1.0f))
	{
		pRigidBody_->setCollisionFlags(
			pRigidBody_->getCollisionFlags() |
			btCollisionObject::CF_STATIC_OBJECT
		);
		pRigidBody_->setActivationState(DISABLE_DEACTIVATION);
	}
	else if (bDisableSleep_)
	{
		// スリープ無効化が有効な場合
		pRigidBody_->setActivationState(DISABLE_DEACTIVATION);
	}

	SetFriction(fFriction_);
	SetRestitution(fRestitution_);
	SetDamping(fLinearDamping_, fAngularDamping_);
	SetRollingFriction(fRollingFriction_);
	SetSpinningFriction(fSpinningFriction_);

	SetKinematic(bKinematic_);
	SetGravityEnabled(bUseGravity_);
	SetRotationConstraint(bLockRotationX_, bLockRotationY_, bLockRotationZ_);

	bAddedToWorld_ = false;
}

void RigidBodyComponent::UpdateMassProperties()
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

btQuaternion RigidBodyComponent::EulerToQuaternion(const DirectX::XMFLOAT3& euler)
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

DirectX::XMFLOAT3 RigidBodyComponent::QuaternionToEuler(const btQuaternion& quat)
{
	DirectX::XMVECTOR q = DirectX::XMVectorSet(quat.getX(), quat.getY(), quat.getZ(), quat.getW());
	DirectX::XMMATRIX rotMatrix = DirectX::XMMatrixRotationQuaternion(q);

	DirectX::XMFLOAT3 euler;
	euler.x = asinf(-rotMatrix.r[2].m128_f32[1]);                        // Pitch
	euler.y = atan2f(rotMatrix.r[2].m128_f32[0], rotMatrix.r[2].m128_f32[2]); // Yaw
	euler.z = atan2f(rotMatrix.r[0].m128_f32[1], rotMatrix.r[1].m128_f32[1]); // Roll

	return euler;
}

void RigidBodyComponent::SetFriction(float friction)
{
	fFriction_ = friction;
	if (pRigidBody_)
	{
		pRigidBody_->setFriction(friction);
	}
}

void RigidBodyComponent::SetRestitution(float restitution)
{
	fRestitution_ = restitution;
	if (pRigidBody_)
	{
		pRigidBody_->setRestitution(restitution);
	}
}

void RigidBodyComponent::SetLinearDamping(float damping)
{
	fLinearDamping_ = damping;
	if (pRigidBody_)
	{
		pRigidBody_->setDamping(fLinearDamping_, fAngularDamping_);
	}
}

void RigidBodyComponent::SetAngularDamping(float damping)
{
	fAngularDamping_ = damping;
	if (pRigidBody_)
	{
		pRigidBody_->setDamping(fLinearDamping_, fAngularDamping_);
	}
}

void RigidBodyComponent::SetDamping(float linearDamping, float angularDamping)
{
	fLinearDamping_ = linearDamping;
	fAngularDamping_ = angularDamping;
	if (pRigidBody_)
	{
		pRigidBody_->setDamping(linearDamping, angularDamping);
	}
}

void RigidBodyComponent::SetRollingFriction(float rollingFriction)
{
	fRollingFriction_ = rollingFriction;
	if (pRigidBody_)
	{
		pRigidBody_->setRollingFriction(rollingFriction);
	}
}

void RigidBodyComponent::SetSpinningFriction(float spinningFriction)
{
	fSpinningFriction_ = spinningFriction;
	if (pRigidBody_)
	{
		pRigidBody_->setSpinningFriction(spinningFriction);
	}
}

void RigidBodyComponent::SetRotationConstraint(bool lockX, bool lockY, bool lockZ)
{
	bLockRotationX_ = lockX;
	bLockRotationY_ = lockY;
	bLockRotationZ_ = lockZ;

	if (pRigidBody_)
	{
		btVector3 angularFactor(
			lockX ? 0.0f : 1.0f,
			lockY ? 0.0f : 1.0f,
			lockZ ? 0.0f : 1.0f
		);
		pRigidBody_->setAngularFactor(angularFactor);
	}
}

void RigidBodyComponent::SetRotationConstraintX(bool lock)
{
	bLockRotationX_ = lock;
	SetRotationConstraint(bLockRotationX_, bLockRotationY_, bLockRotationZ_);
}

void RigidBodyComponent::SetRotationConstraintY(bool lock)
{
	bLockRotationY_ = lock;
	SetRotationConstraint(bLockRotationX_, bLockRotationY_, bLockRotationZ_);
}

void RigidBodyComponent::SetRotationConstraintZ(bool lock)
{
	bLockRotationZ_ = lock;
	SetRotationConstraint(bLockRotationX_, bLockRotationY_, bLockRotationZ_);
}

// スリープ制御
void RigidBodyComponent::SetDisableSleep(bool disableSleep)
{
	bDisableSleep_ = disableSleep;

	if (pRigidBody_)
	{
		if (bDisableSleep_)
		{
			// スリープを無効化
			pRigidBody_->setActivationState(DISABLE_DEACTIVATION);
			pRigidBody_->setSleepingThresholds(0.0f, 0.0f);
		}
		else
		{
			// 通常のスリープ設定に戻す
			pRigidBody_->setActivationState(ACTIVE_TAG);
			pRigidBody_->setSleepingThresholds(0.2f, 0.2f);
			pRigidBody_->activate(true);
		}
	}
}

// カメラ距離に基づくカリング制御
void RigidBodyComponent::SetCullingDistance(float distance)
{
	fCullingDistance_ = distance;
}

void RigidBodyComponent::SetUseCulling(bool useCulling)
{
	bool prevUseCulling = bUseCulling_;
	bUseCulling_ = useCulling;

	// カリング設定が変更された場合
	if (prevUseCulling != bUseCulling_)
	{
		if (!bUseCulling_ && !bActiveInPhysicsWorld_)
		{
			// カリングを無効にした場合、物理ワールドに追加
			AddToPhysicsWorld();
		}
	}
}

void RigidBodyComponent::SetAlwaysActive(bool alwaysActive)
{
	bool prevAlwaysActive = bAlwaysActive_;
	bAlwaysActive_ = alwaysActive;

	// 常にアクティブ設定が変更された場合
	if (prevAlwaysActive != bAlwaysActive_)
	{
		if (bAlwaysActive_ && !bActiveInPhysicsWorld_)
		{
			// 常にアクティブに設定した場合、物理ワールドに追加
			AddToPhysicsWorld();
		}
		else if (!bAlwaysActive_ && bUseCulling_)
		{
			// 常にアクティブを解除した場合、カリング状態を更新
			UpdateCullingState();
		}
	}
}

void RigidBodyComponent::UpdateCullingState()
{
	if (!bUseCulling_ || bAlwaysActive_ || !_Parent || !_Parent->GetParentScene())
		return;

	float distance = CalculateDistanceToCamera();
	bool shouldBeActive = distance <= fCullingDistance_;

	// 状態が変化した場合のみ処理
	if (shouldBeActive != bActiveInPhysicsWorld_)
	{
		if (shouldBeActive)
		{
			AddToPhysicsWorld();
		}
		else
		{
			RemoveFromPhysicsWorld();
		}
	}

	bWasActiveLastFrame_ = bActiveInPhysicsWorld_;
}

float RigidBodyComponent::CalculateDistanceToCamera()
{
	if (!_Parent || !_Parent->GetParentScene())
		return 0.0f;

	CameraComponent* mainCamera = _Parent->GetParentScene()->GetMainCamera();
	if (!mainCamera || !mainCamera->GetParent())
		return 0.0f;

	DirectX::XMFLOAT3 cameraPos = mainCamera->GetParent()->GetWorldTransform().position;
	DirectX::XMFLOAT3 objectPos = _Parent->GetWorldTransform().position;

	float dx = objectPos.x - cameraPos.x;
	float dy = objectPos.y - cameraPos.y;
	float dz = objectPos.z - cameraPos.z;

	return sqrtf(dx * dx + dy * dy + dz * dz);
}

void RigidBodyComponent::AddToPhysicsWorld()
{
	if (!pRigidBody_ || bAddedToWorld_ || !_Parent || !_Parent->GetParentScene())
		return;

	btDiscreteDynamicsWorld* physicsWorld = _Parent->GetParentScene()->GetPhysicsWorld();
	if (physicsWorld)
	{
		try
		{
			physicsWorld->addRigidBody(pRigidBody_);
			bAddedToWorld_ = true;
			bActiveInPhysicsWorld_ = true;

			// ワールドに追加したときに現在の状態を同期
			SyncTransformToBullet();
			pRigidBody_->activate(true);

			// スリープ設定を適用
			if (bDisableSleep_)
			{
				pRigidBody_->setActivationState(DISABLE_DEACTIVATION);
			}
		}
		catch (...)
		{
			bAddedToWorld_ = false;
			bActiveInPhysicsWorld_ = false;
		}
	}
}

void RigidBodyComponent::RemoveFromPhysicsWorld()
{
	if (!pRigidBody_ || !bAddedToWorld_ || !_Parent || !_Parent->GetParentScene())
		return;

	btDiscreteDynamicsWorld* physicsWorld = _Parent->GetParentScene()->GetPhysicsWorld();
	if (physicsWorld)
	{
		try
		{
			// 現在の状態を保存してからワールドから削除
			btTransform currentTransform = pRigidBody_->getWorldTransform();
			btVector3 currentVelocity = pRigidBody_->getLinearVelocity();
			btVector3 currentAngularVelocity = pRigidBody_->getAngularVelocity();

			physicsWorld->removeRigidBody(pRigidBody_);
			bAddedToWorld_ = false;
			bActiveInPhysicsWorld_ = false;

			// 状態を復元（再度アクティブになったときのため）
			pRigidBody_->setWorldTransform(currentTransform);
			pRigidBody_->setLinearVelocity(currentVelocity);
			pRigidBody_->setAngularVelocity(currentAngularVelocity);
		}
		catch (...)
		{
		}
	}
}