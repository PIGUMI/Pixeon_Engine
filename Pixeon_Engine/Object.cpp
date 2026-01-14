/*
* ファイル名　: Object
* 説　　　明　: オブジェクトクラス実装
*/
#include "Object.h"
#include "Component.h"
#include "ImageRender.h"
#include "Animator2DComponent.h"
#include <DirectXMath.h>

/*
* 関数名　: Init
* 引　数　: なし
* 戻り値　: なし
* 説　明　: 初期化処理
*/
void AbstractObject::Init() {
}

/*
* 関数名　: BeginPlay
* 引　数　: なし
* 戻り値　: なし
* 説　明　: ゲーム開始時の処理
*/
void AbstractObject::BeginPlay() {
	for (auto comp : _components) {
		if (comp) comp->BeginPlay();
	}
	// 子オブジェクトのBeginPlayも呼ぶ
	for (auto child : _children) {
		if (child) child->BeginPlay();
	}
}

/*
* 関数名　: EditUpdate
* 引　数　: なし
* 戻り値　: なし
* 説　明　: エディタ更新処理
*/
void AbstractObject::EditUpdate() {
	for (auto comp : _components) {
		if (comp && comp->GetComponentType() == ComponentManager::COMPONENT_TYPE::CAMERA) continue;
		if (comp) comp->EditUpdate();
	}

	for (auto child : _children) {
		if (child) child->EditUpdate();
	}
}

/*
* 関数名　: InGameUpdate
* 引　数　: なし
* 戻り値　: なし
* 説　明　: ゲーム更新処理
*/
void AbstractObject::InGameUpdate() {
	for (auto comp : _components) {
		if (comp && comp->GetComponentType() == ComponentManager::COMPONENT_TYPE::CAMERA) continue;
		if (comp) comp->InGameUpdate();
	}

	for (auto child : _children) {
		if (child) child->InGameUpdate();
	}
}

/*
* 関数名　: Draw
* 引　数　: Layer	描画レイヤー
* 戻り値　: なし
* 説　明　: 描画処理
*/
void AbstractObject::Draw(int Layer) {
	for (auto comp : _components) {
		comp->Draw(Layer);
	}

	for (auto child : _children) {
		if (child) child->Draw(Layer);
	}
}

/*
* 関数名　: UnInit
* 引　数　: なし
* 戻り値　: なし
* 説　明　: 終了処理
*/
void AbstractObject::UnInit() {

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

/*
* 関数名　: GetWorldTransform
* 引　数　: なし
* 戻り値　: ワールドトランスフォーム
* 説　明　: ワールドトランスフォームの取得
*/
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

/*
* 関数名　: GetWorldPosition
* 引　数　: なし
* 戻り値　: ワールド位置
* 説　明　: ワールド位置の取得
*/
DirectX::XMFLOAT3 AbstractObject::GetWorldPosition() {
	Transform worldTransform = GetWorldTransform();
	return worldTransform.position;
}

/*
* 関数名　: SetParent
* 引　数　: parent	新しい親オブジェクト
* 戻り値　: なし
* 説　明　: 親オブジェクトの設定
*/
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

/*
* 関数名　: RemoveParent
* 引　数　: なし
* 戻り値　: なし
* 説　明　: 親オブジェクトの削除
*/
void AbstractObject::RemoveParent() {
	if (_parentObject) {
		_parentObject->RemoveChild(this);
		_parentObject = nullptr;
	}
}

/*
* 関数名　: FindChildByName
* 引　数　: name	子オブジェクト名
* 戻り値　: 見つかった子オブジェクト、見つからなければnullptr
* 説　明　: 名前から子オブジェクトを検索
*/
AbstractObject* AbstractObject::FindChildByName(const std::string& name)
{
	for (auto child : _children) {
		if (child && child->GetObjectName() == name) {
			return child;
		}
	}
	return nullptr;
}

/*
* 関数名　: AddChild
* 引　数　: child	追加する子オブジェクト
* 戻り値　: なし
* 説　明　: 子オブジェクトの追加
*/
void AbstractObject::AddChild(AbstractObject* child) {
	if (!child) return;

	// 既に子リストにある場合は追加しない
	auto it = std::find(_children.begin(), _children.end(), child);
	if (it != _children.end()) return;

	_children.push_back(child);
}

/*
* 関数名　: RemoveChild
* 引　数　: child	削除する子オブジェクト
* 戻り値　: なし
* 説　明　: 子オブジェクトの削除
*/
void AbstractObject::RemoveChild(AbstractObject* child) {
	if (!child) return;

	auto it = std::remove(_children.begin(), _children.end(), child);
	if (it != _children.end()) {
		_children.erase(it, _children.end());
	}
}

/*
* 関数名　: Clone
* 引　数　: なし
* 戻り値　: クローンされたオブジェクト
* 説　明　: オブジェクトのクローン作成
*/
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

/*
* 関数名　: GetComponent
* 引　数　: name	コンポーネント名
* 戻り値　: 見つかったコンポーネント、見つからなければnullptr
* 説　明　: 名前からコンポーネントを取得
*/
AbstractComponent* AbstractObject::GetComponent(const std::string& name)
{
	for (auto comp : _components) {
		if (comp->GetComponentName() == name) {
			return comp;
		}
	}
	return nullptr;
}

/*
* 関数名　: GetComponentsByTypeID
* 引　数　: typeID	コンポーネントの型ID
* 戻り値　: 指定型のコンポーネントリスト
* 説　明　: 型IDからコンポーネントを取得
*/
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

/*
* 関数名　: RemoveComponent
* 引　数　: comp	削除するコンポーネント
* 戻り値　: なし
* 説　明　: コンポーネントの削除
*/
void AbstractObject::RemoveComponent(AbstractComponent* comp) {
	if (comp == nullptr) return;
	auto it = std::remove(_components.begin(), _components.end(), comp);
	if (it != _components.end()) {
		_components.erase(it, _components.end());
		comp->UInit();
		delete comp;
	}
}