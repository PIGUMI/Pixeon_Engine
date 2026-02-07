#ifndef PIXEON2_API
#define PIXEON2_API
#include "API.h"
#include <string>
#include <DirectXMath.h>
#include <functional>
#include <memory>

// Forward declarations
class Object;
class Scene;
class Component;
class Camera;
class Light;
class Imagerender;
class ModelRender;
class Animation;
class Rigidbody;
class Boxcollision;
class Animator2d;
class Effect;
class Script;

// ========================================
// Wrapper for Object
// ========================================
class Object {
public:
	Object(object handle) : m_Handle(handle) {}
	~Object() = default;

	// 名前操作
	std::string GetName();
	void SetName(const std::string& name);

	// トランスフォーム操作
	transform GetTransform();
	void SetTransform(const transform& inTransform);
	transform GetWorldTransform();

	// 位置・回転・スケール操作
	DirectX::XMFLOAT3 GetPosition();
	void SetPosition(const DirectX::XMFLOAT3& position);
	DirectX::XMFLOAT3 GetRotation();
	void SetRotation(const DirectX::XMFLOAT3& rotation);
	DirectX::XMFLOAT3 GetScale();
	void SetScale(const DirectX::XMFLOAT3& scale);

	// 階層操作
	Object* FindChildObject(const std::string& name);
	int GetChildCount();

	// コンポーネント操作
	template<typename T>
	T* GetComponent(const std::string& componentName);

	// 変数操作
	int GetInt(const std::string& varName);
	void SetInt(const std::string& varName, int value);
	float GetFloat(const std::string& varName);
	void SetFloat(const std::string& varName, float value);
	bool GetBool(const std::string& varName);
	void SetBool(const std::string& varName, bool value);

	object GetHandle() const { return m_Handle; }

private:
	object m_Handle;
};

// ========================================
// Wrapper for Scene
// ========================================
class Scene {
public:
	Scene(scene handle) : m_Handle(handle) {}
	~Scene() = default;

	// シーン操作
	static Scene* GetCurrent();
	static void Change(const std::string& sceneName);

	// オブジェクト操作
	int GetObjectCount();
	Object* FindObject(const std::string& name);
	static Object* FindPrefabObject(const std::string& name);
	Object* AddObject(Object* obj);
	void RemoveObject(Object* obj);

	// カメラ操作
	void SetMainCamera(int cameraNumber);
	void SetMainCamera(Camera* camera);
	int GetMainCamera();

	// レイキャスト
	bool Raycast(const DirectX::XMFLOAT3& origin, const DirectX::XMFLOAT3& direction, float maxDistance, RayHit* outHit);
	bool RaycastIgnoreTriggers(const DirectX::XMFLOAT3& origin, const DirectX::XMFLOAT3& direction, float maxDistance, RayHit* outHit);
	bool SphereCast(const DirectX::XMFLOAT3& origin, const DirectX::XMFLOAT3& direction, float radius, float maxDistance, RayHit* outHit);
	bool RaycastIgnoreObject(const DirectX::XMFLOAT3& origin, const DirectX::XMFLOAT3& direction, float maxDistance, Object* ignoreObj, RayHit* outHit);

	scene GetHandle() const { return m_Handle; }

private:
	scene m_Handle;
};

// ========================================
// Base Component Wrapper
// ========================================
class Component {
public:
	Component(component handle) : m_Handle(handle) {}
	virtual ~Component() = default;

	component GetHandle() const { return m_Handle; }

protected:
	component m_Handle;
};

// ========================================
// Camera Component
// ========================================
class Camera : public Component {
public:
	Camera(component handle) : Component(handle) {}

	// トランスフォーム
	CameraTransform GetTransform();
	void SetTransform(const CameraTransform& transform);

	// カメラパラメータ
	float GetFov();
	void SetFov(float fov);
	float GetAspect();
	void SetAspect(float aspect);
	void GetNearFar(float* outNear, float* outFar);
	void SetNearFar(float nearPlane, float farPlane);

	// ベクトル取得
	DirectX::XMFLOAT3 GetUpVector();
	DirectX::XMFLOAT3 GetRightVector();
	DirectX::XMFLOAT3 GetForwardVector();

	// その他
	void SetChangeCalculation(bool isChange);
	int GetCameraNumber();
};

// ========================================
// Light Component
// ========================================
class Light : public Component {
public:
	Light(component handle) : Component(handle) {}

	enum class LightType {
		Directional = 0,
		Point = 1,
		Spot = 2
	};

	// ライトタイプ
	LightType GetType();
	void SetType(LightType type);

	// 色と強度
	DirectX::XMFLOAT3 GetColor();
	void SetColor(const DirectX::XMFLOAT3& color);
	float GetIntensity();
	void SetIntensity(float intensity);

	// 範囲
	float GetRange();
	void SetRange(float range);

	// スポットライト
	void GetSpotAngles(float* outInner, float* outOuter);
	void SetSpotAngles(float inner, float outer);

	// 有効/無効
	bool IsEnabled();
	void SetEnabled(bool enabled);
};

// ========================================
// Image Render Component
// ========================================
class Imagerender : public Component {
public:
	Imagerender(component handle) : Component(handle) {}

	// テクスチャ
	std::string GetTextureName();
	void SetTextureName(const std::string& name);

	// トランスフォーム
	transform GetTransform();
	void SetTransform(const transform& inTransform);

	// 色
	DirectX::XMFLOAT4 GetColor();
	void SetColor(const DirectX::XMFLOAT4& color);

	// UV
	DirectX::XMFLOAT4 GetUVRect();
	void SetUVRect(const DirectX::XMFLOAT4& uvRect);
};

// ========================================
// Model Render Component
// ========================================
class ModelRender : public Component {
public:
	ModelRender(component handle) : Component(handle) {}

	// 色
	DirectX::XMFLOAT4 GetColor();
	void SetColor(const DirectX::XMFLOAT4& color);

	// マテリアル
	void SetMaterialTexture(int materialIndex, const std::string& texturePath);

	// オフセット
	DirectX::XMFLOAT3 GetOffsetPosition();
	void SetOffsetPosition(const DirectX::XMFLOAT3& position);
	DirectX::XMFLOAT3 GetOffsetRotation();
	void SetOffsetRotation(const DirectX::XMFLOAT3& rotation);
	DirectX::XMFLOAT3 GetOffsetScale();
	void SetOffsetScale(const DirectX::XMFLOAT3& scale);

	// ボーン
	std::string GetBoneName(int boneIndex);
	DirectX::XMFLOAT3 GetBoneWorldPosition(int boneIndex);
	DirectX::XMFLOAT3 GetBoneWorldRotation(int boneIndex);
};

// ========================================
// Animation Component
// ========================================
class Animation : public Component {
public:
	Animation(component handle) : Component(handle) {}

	void Play();
	void Pause();
	void Resume();
	void Stop();
	void Restart();

	void SetClip(int clipIndex);
	int GetClip();

	void SetPlaybackSpeed(float speed);
	void SetLoop(bool loop);
};

// ========================================
// Rigidbody Component
// ========================================
class Rigidbody : public Component {
public:
	Rigidbody(component handle) : Component(handle) {}

	// 力の追加
	void AddForce(const DirectX::XMFLOAT3& force);
	void AddImpulse(const DirectX::XMFLOAT3& impulse);

	// 速度
	DirectX::XMFLOAT3 GetVelocity();
	void SetVelocity(const DirectX::XMFLOAT3& velocity);

	// キネマティック
	bool IsKinematic();
	void SetKinematic(bool kinematic);

	// 質量
	float GetMass();
	void SetMass(float mass);

	// 重力
	bool IsUseGravity();
	void SetUseGravity(bool useGravity);

	// 物理パラメータ
	float GetFriction();
	void SetFriction(float friction);
	float GetRestitution();
	void SetRestitution(float restitution);
	float GetLinearDamping();
	void SetLinearDamping(float damping);
	float GetAngularDamping();
	void SetAngularDamping(float damping);
	void SetDamping(float linearDamping, float angularDamping);
	float GetRollingFriction();
	void SetRollingFriction(float friction);
	float GetSpinningFriction();
	void SetSpinningFriction(float friction);
};

// ========================================
// Box Collision Component
// ========================================
class Boxcollision : public Component {
public:
	Boxcollision(component handle) : Component(handle) {}

	// サイズとセンター
	DirectX::XMFLOAT3 GetSize();
	void SetSize(const DirectX::XMFLOAT3& size);
	DirectX::XMFLOAT3 GetCenter();
	void SetCenter(const DirectX::XMFLOAT3& center);

	// トリガー
	bool IsTrigger();
	void SetTrigger(bool isTrigger);

	// コールバック
	using CollisionCallback = std::function<void(const APICollisionInfo&)>;
	void SetOnCollisionEnter(CollisionCallback callback);
	void SetOnCollisionStay(CollisionCallback callback);
	void SetOnCollisionExit(CollisionCallback callback);

private:
	static void StaticOnEnter(component comp, const APICollisionInfo* info);
	static void StaticOnStay(component comp, const APICollisionInfo* info);
	static void StaticOnExit(component comp, const APICollisionInfo* info);

	CollisionCallback m_OnEnter;
	CollisionCallback m_OnStay;
	CollisionCallback m_OnExit;
};

// ========================================
// Effect Component
// ========================================
class Effect : public Component {
public:
	Effect(component handle) : Component(handle) {}

	void Play();
	void Stop();
	bool IsPlaying();
};

// ========================================
// Script Component
// ========================================
class Script : public Component {
public:
	Script(component handle) : Component(handle) {}

	void CallFunction(const std::string& functionName);
};

// ========================================
// Animator2d Wrapper
// ========================================
class Keyframe {
public:
	Keyframe(keyframe handle) : m_Handle(handle) {}

	void SetVertexOffsetUp(float offset);
	void SetVertexOffsetDown(float offset);
	void SetVertexOffsetLeft(float offset);
	void SetVertexOffsetRight(float offset);

	float GetVertexOffsetUp();
	float GetVertexOffsetDown();
	float GetVertexOffsetLeft();
	float GetVertexOffsetRight();

	keyframe GetHandle() const { return m_Handle; }

private:
	keyframe m_Handle;
};

class Animator2d {
public:
	Animator2d(animator2d handle) : m_Handle(handle) {}

	void Play();
	void Stop();
	bool IsEnd();

	Keyframe* FindKeyFrame(const std::string& keyName);

	Animator2d GetHandle() const { return m_Handle; }

private:
	animator2d m_Handle;
};

// ========================================
// Input Helper
// ========================================
class Input {
public:
	static bool IsKeyPressed(char keyCode);
	static bool IsKeyTriggered(char keyCode);
	static bool IsKeyReleased(char keyCode);
	static bool IsKeyRepeated(char keyCode);

	static int GetMouseMoveX();
	static int GetMouseMoveY();

	static void FixMouseCursor(bool enabled);
};

// ========================================
// Helper Functions
// ========================================
namespace Pixeon {
	inline Float3 ToFloat3(const DirectX::XMFLOAT3& v) {
		Float3 result;
		result.x = v.x;
		result.y = v.y;
		result.z = v.z;
		return result;
	}

	inline DirectX::XMFLOAT3 ToXMFloat3(const Float3& v) {
		return DirectX::XMFLOAT3(v.x, v.y, v.z);
	}

	inline Float4 ToFloat4(const DirectX::XMFLOAT4& v) {
		Float4 result;
		result.x = v.x;
		result.y = v.y;
		result.z = v.z;
		result.w = v.w;
		return result;
	}

	inline DirectX::XMFLOAT4 ToXMFloat4(const Float4& v) {
		return DirectX::XMFLOAT4(v.x, v.y, v.z, v.w);
	}
}

#endif // !PIXEON2_API