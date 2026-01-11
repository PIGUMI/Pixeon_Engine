#include "Object.h"
#include "Component.h"
#include "ImageRender.h"
#include "Animator2DComponent.h"
#include <DirectXMath.h>

void AbstractObject::Init() {
}

void AbstractObject::BeginPlay() {
	for (auto comp : _components) {
		if (comp) comp->BeginPlay();
	}
	// 子オブジェクトのBeginPlayも呼ぶ
	for (auto child : _children) {
		if (child) child->BeginPlay();
	}
}

void AbstractObject::EditUpdate() {
	for (auto comp : _components) {
		if (comp && comp->GetComponentType() == ComponentManager::COMPONENT_TYPE::CAMERA) continue;
		if (comp) comp->EditUpdate();
	}

	for (auto child : _children) {
		if (child) child->EditUpdate();
	}
}

void AbstractObject::InGameUpdate() {
	for (auto comp : _components) {
		if (comp && comp->GetComponentType() == ComponentManager::COMPONENT_TYPE::CAMERA) continue;
		if (comp) comp->InGameUpdate();
	}

	for (auto child : _children) {
		if (child) child->InGameUpdate();
	}
}

void AbstractObject::Draw(int Layer) {
	for (auto comp : _components) {
		comp->Draw(Layer);
	}

	for (auto child : _children) {
		if (child) child->Draw(Layer);
	}
}

void AbstractObject::UInit() {

	RemoveParent();

	for (auto child : _children) {
		if (child) {
			child->_parentObject = nullptr;
		}
	}
	_children.clear();

	for (auto comp : _components) {
		if (comp) {
			comp->UInit();
			delete comp;
		}
	}
	_components.clear();
}

Transform AbstractObject::GetWorldTransform() {
	if (!_parentObject) {
		return _transform;
	}

	// 親のワールドトランスフォームを取得
	Transform parentWorld = _parentObject->GetWorldTransform();

	// 親の回転行列を作成
	DirectX::XMMATRIX parentRotMat = DirectX::XMMatrixRotationRollPitchYaw(
		parentWorld.rotation.x,
		parentWorld.rotation.y,
		parentWorld.rotation.z
	);

	// ローカル位置を親の回転で変換
	DirectX::XMVECTOR localPosVec = DirectX::XMLoadFloat3(&_transform.position);
	DirectX::XMVECTOR rotatedPosVec = DirectX::XMVector3Transform(localPosVec, parentRotMat);

	DirectX::XMFLOAT3 rotatedPos;
	DirectX::XMStoreFloat3(&rotatedPos, rotatedPosVec);

	// ワールドトランスフォームを計算
	Transform worldTransform;
	worldTransform.position.x = parentWorld.position.x + rotatedPos.x * parentWorld.scale.x;
	worldTransform.position.y = parentWorld.position.y + rotatedPos.y * parentWorld.scale.y;
	worldTransform.position.z = parentWorld.position.z + rotatedPos.z * parentWorld.scale.z;

	worldTransform.rotation.x = parentWorld.rotation.x + _transform.rotation.x;
	worldTransform.rotation.y = parentWorld.rotation.y + _transform.rotation.y;
	worldTransform.rotation.z = parentWorld.rotation.z + _transform.rotation.z;

	worldTransform.scale.x = parentWorld.scale.x * _transform.scale.x;
	worldTransform.scale.y = parentWorld.scale.y * _transform.scale.y;
	worldTransform.scale.z = parentWorld.scale.z * _transform.scale.z;

	return worldTransform;
}

DirectX::XMFLOAT3 AbstractObject::GetWorldPosition() {
	Transform worldTransform = GetWorldTransform();
	return worldTransform.position;
}

void AbstractObject::SetParent(AbstractObject* parent) {
	if (_parentObject == parent) return;

	// 既存の親から削除
	RemoveParent();

	// 新しい親を設定
	_parentObject = parent;
	if (_parentObject) {
		_parentObject->AddChild(this);
	}
}

void AbstractObject::RemoveParent() {
	if (_parentObject) {
		_parentObject->RemoveChild(this);
		_parentObject = nullptr;
	}
}

void AbstractObject::AddChild(AbstractObject* child) {
	if (!child) return;

	// 既に子リストにある場合は追加しない
	auto it = std::find(_children.begin(), _children.end(), child);
	if (it != _children.end()) return;

	_children.push_back(child);
}

void AbstractObject::RemoveChild(AbstractObject* child) {
	if (!child) return;

	auto it = std::remove(_children.begin(), _children.end(), child);
	if (it != _children.end()) {
		_children.erase(it, _children.end());
	}
}

AbstractObject* AbstractObject::Clone() {
	AbstractObject* newObj = new AbstractObject();
	newObj->_transform = this->_transform;
	newObj->_ObjectName = this->_ObjectName;
	newObj->SetParentScene(this->GetParentScene());

	// コンポーネントのクローン
	for (auto comp : _components) {
		if (comp) {
			AbstractComponent* newComp = ComponentManager::GetInstance()->AddComponent(newObj, comp->GetComponentType());
			if (newComp) {
				newComp->SetComponentName(comp->GetComponentName());
				std::stringstream ss;
				comp->SaveToFile(ss);
				newComp->LoadFromFile(ss);
			}
		}
	}

	// 子オブジェクトのクローン
	for (auto child : _children) {
		if (child) {
			AbstractObject* newChild = child->Clone();
			newChild->SetParent(newObj);
		}
	}

	return newObj;
}

AbstractComponent* AbstractObject::GetComponent(const std::string& name)
{
	for (auto comp : _components) {
		if (comp->GetComponentName() == name) {
			return comp;
		}
	}
	return nullptr;
}

std::vector<AbstractComponent*> AbstractObject::GetComponentsByTypeID(int typeID)
{
	std::vector<AbstractComponent*> result;
	for (auto comp : _components) {
		if (static_cast<int>(comp->GetComponentType()) == typeID) {
			result.push_back(comp);
		}
	}
	return result;
}

void AbstractObject::RemoveComponent(AbstractComponent* comp) {
	if (comp == nullptr) return;
	auto it = std::remove(_components.begin(), _components.end(), comp);
	if (it != _components.end()) {
		_components.erase(it, _components.end());
		comp->UInit();
		delete comp;
	}
}