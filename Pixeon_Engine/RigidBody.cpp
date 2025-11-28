#include "RigidBody.h"
#include "Object.h"
#include "Scene.h"
#include "EditrGUI.h"
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
							_Parent->GetObjectName() + "Çï®óùê¢äEÇ…í«â¡ÇµÇ‹ÇµÇΩÅiUserPointer: " +
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
							"RigidBodyÇ™ñ≥å¯Ç≈Ç∑");
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
					"RigidBodyÇÃí«â¡íÜÇ…ó·äOÇ™î≠ê∂ÇµÇ‹ÇµÇΩ");
				if (pRigidBody_)
				{
					if(pRigidBody_->getMotionState())
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
				"ï®óùê¢äEÇ™ë∂ç›ÇµÇ‹ÇπÇÒ");
		}
	}
}

void RigidBody::EditUpdate()
{
	if (!pRigidBody_)return;

	if(bKinematic_ && bTransformDirty_)
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
		if(physicsWorld && bAddedToWorld_)
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
			if(motionState == pMotionState_)
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

void RigidBody::AddCollisionShape(btCollisionShape* shape, const btTransform& localTransform)
{
	if(pCompoundShape_ && shape)
	{
		auto it = std::find(RegisteredColliders_.begin(), RegisteredColliders_.end(), shape);
		if(it == RegisteredColliders_.end())
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
		if(it != RegisteredColliders_.end())
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
	if(!pRigidBody_ || !_Parent) return;
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
		if(_Parent && _Parent->GetParentScene())
		{
			btDiscreteDynamicsWorld* physicsWorld = _Parent->GetParentScene()->GetPhysicsWorld();
			if(physicsWorld && bAddedToWorld_)
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
		}
	}
}
