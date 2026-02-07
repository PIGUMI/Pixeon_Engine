#include "Pixeon2_API.h"
#include <stdexcept>
#include <unordered_map>

// ========================================
// Object Implementation
// ========================================

std::string Object::GetName() {
	char nameBuffer[256];
	if (GetObjectName(m_Handle, nameBuffer, 256) == APIResult::PN_SUCCESS) {
		return std::string(nameBuffer);
	}
	return std::string();
}

void Object::SetName(const std::string& name)
{
	SetObjectName(m_Handle, name.c_str());
}

transform Object::GetTransform() {
	transform outTransform;
	if (GetObjectTransform(m_Handle, &outTransform) == APIResult::PN_SUCCESS) {
		return outTransform;
	}
	return transform{};
}

void Object::SetTransform(const transform& inTransform)
{
	SetObjectTransform(m_Handle, &inTransform);
}

transform Object::GetWorldTransform()
{
	transform outTransform;
	if (GetObjectWorldTransform(m_Handle, &outTransform) == APIResult::PN_SUCCESS) {
		return outTransform;
	}
	return transform{};
}

DirectX::XMFLOAT3 Object::GetPosition()
{
	Float3 position;
	if (GetObjectPosition(m_Handle, &position) == APIResult::PN_SUCCESS) {
		return DirectX::XMFLOAT3(position.x, position.y, position.z);
	}
	return DirectX::XMFLOAT3{};
}

void Object::SetPosition(const DirectX::XMFLOAT3& position)
{
	Float3 pos{ position.x, position.y, position.z };
	SetObjectPosition(m_Handle, pos);
}

DirectX::XMFLOAT3 Object::GetRotation()
{
	Float3 rotation;
	if (GetObjectRotation(m_Handle, &rotation) == APIResult::PN_SUCCESS) {
		return DirectX::XMFLOAT3(rotation.x, rotation.y, rotation.z);
	}
	return DirectX::XMFLOAT3{};
}

void Object::SetRotation(const DirectX::XMFLOAT3& rotation)
{
	Float3 rot{ rotation.x, rotation.y, rotation.z };
	SetObjectRotation(m_Handle, rot);
}

DirectX::XMFLOAT3 Object::GetScale()
{
	Float3 scale;
	if (GetObjectScale(m_Handle, &scale) == APIResult::PN_SUCCESS) {
		return DirectX::XMFLOAT3(scale.x, scale.y, scale.z);
	}
	return DirectX::XMFLOAT3{};
}

void Object::SetScale(const DirectX::XMFLOAT3& scale)
{
	Float3 scl{ scale.x, scale.y, scale.z };
	SetObjectScale(m_Handle, scl);
}

Object* Object::FindChildObject(const std::string& name)
{
	object childHandle = nullptr;
	if (FindChildObjectByName(m_Handle, name.c_str(), &childHandle) == APIResult::PN_SUCCESS) {
		return new Object(childHandle);
	}
	return nullptr;
}

int Object::GetChildCount()
{
	int count = 0;
	CountChildObjects(m_Handle, &count);
	return count;
}

int Object::GetInt(const std::string& varName)
{
	int value = 0;
	GetVariableInt(m_Handle, varName.c_str(), &value);
	return value;
}

void Object::SetInt(const std::string& varName, int value)
{
	SetVariableInt(m_Handle, varName.c_str(), value);
}

float Object::GetFloat(const std::string& varName)
{
	float value = 0.0f;
	GetVariableFloat(m_Handle, varName.c_str(), &value);
	return value;
}

void Object::SetFloat(const std::string& varName, float value)
{
	SetVariableFloat(m_Handle, varName.c_str(), value);
}

bool Object::GetBool(const std::string& varName)
{
	bool value = false;
	GetVariableBool(m_Handle, varName.c_str(), &value);
	return value;
}

void Object::SetBool(const std::string& varName, bool value)
{
	SetVariableBool(m_Handle, varName.c_str(), value);
}

// ========================================
// Scene Implementation
// ========================================

Scene* Scene::GetCurrent()
{
	scene sceneHandle = nullptr;
	if (GetCurrentScene(&sceneHandle) == APIResult::PN_SUCCESS) {
		return new Scene(sceneHandle);
	}
	return nullptr;
}

void Scene::Change(const std::string& sceneName)
{
	ChangeScene(sceneName.c_str());
}

int Scene::GetObjectCount()
{
	int count = 0;
	SceneGetObjectCount(m_Handle, &count);
	return count;
}

Object* Scene::FindObject(const std::string& name)
{
	object objHandle = nullptr;
	if (FindObjectByName(m_Handle, name.c_str(), &objHandle) == APIResult::PN_SUCCESS) {
		return new Object(objHandle);
	}
	return nullptr;
}

Object* Scene::FindPrefabObject(const std::string& name)
{
	object objHandle = nullptr;
	if (FindPrefabObjectByName(name.c_str(), &objHandle) == APIResult::PN_SUCCESS) {
		return new Object(objHandle);
	}
	return nullptr;
}

Object* Scene::AddObject(Object* obj)
{
	object cloneHandle = nullptr;
	if (AddObjectToScene(m_Handle, obj->GetHandle(), &cloneHandle) == APIResult::PN_SUCCESS) {
		return new Object(cloneHandle);
	}
	return nullptr;
}

void Scene::RemoveObject(Object* obj)
{
	RemoveObjectFromScene(m_Handle, obj->GetHandle());
}

void Scene::SetMainCamera(int cameraNumber)
{
	SetMainCameraByIndex(cameraNumber);
}

void Scene::SetMainCamera(Camera* camera)
{
	SetMainCameraByPtr(camera->GetHandle());
}

int Scene::GetMainCamera()
{
	int cameraNumber = 0;
	::GetMainCamera(&cameraNumber);
	return cameraNumber;
}

bool Scene::Raycast(const DirectX::XMFLOAT3& origin, const DirectX::XMFLOAT3& direction, float maxDistance, RayHit* outHit)
{
	Float3 orig = Pixeon::ToFloat3(origin);
	Float3 dir = Pixeon::ToFloat3(direction);
	return ::Raycast(m_Handle, orig, dir, maxDistance, outHit) == APIResult::PN_SUCCESS && outHit->bHit;
}

bool Scene::RaycastIgnoreTriggers(const DirectX::XMFLOAT3& origin, const DirectX::XMFLOAT3& direction, float maxDistance, RayHit* outHit)
{
	Float3 orig = Pixeon::ToFloat3(origin);
	Float3 dir = Pixeon::ToFloat3(direction);
	return ::RaycastIgnoreTriggers(m_Handle, orig, dir, maxDistance, outHit) == APIResult::PN_SUCCESS && outHit->bHit;
}

bool Scene::SphereCast(const DirectX::XMFLOAT3& origin, const DirectX::XMFLOAT3& direction, float radius, float maxDistance, RayHit* outHit)
{
	Float3 orig = Pixeon::ToFloat3(origin);
	Float3 dir = Pixeon::ToFloat3(direction);
	return ::SphereCast(m_Handle, orig, dir, radius, maxDistance, outHit) == APIResult::PN_SUCCESS && outHit->bHit;
}

bool Scene::RaycastIgnoreObject(const DirectX::XMFLOAT3& origin, const DirectX::XMFLOAT3& direction, float maxDistance, Object* ignoreObj, RayHit* outHit)
{
	Float3 orig = Pixeon::ToFloat3(origin);
	Float3 dir = Pixeon::ToFloat3(direction);
	return ::RaycastIgnoreObject(m_Handle, orig, dir, maxDistance, ignoreObj->GetHandle(), outHit) == APIResult::PN_SUCCESS && outHit->bHit;
}

// ========================================
// Camera Component Implementation
// ========================================

CameraTransform Camera::GetTransform()
{
	CameraTransform transform;
	GetCameraTransform(m_Handle, &transform);
	return transform;
}

void Camera::SetTransform(const CameraTransform& transform)
{
	SetCameraTransform(m_Handle, &transform);
}

float Camera::GetFov()
{
	float fov = 0.0f;
	GetCameraFov(m_Handle, &fov);
	return fov;
}

void Camera::SetFov(float fov)
{
	SetCameraFov(m_Handle, fov);
}

float Camera::GetAspect()
{
	float aspect = 0.0f;
	GetCameraAspect(m_Handle, &aspect);
	return aspect;
}

void Camera::SetAspect(float aspect)
{
	SetCameraAspect(m_Handle, aspect);
}

void Camera::GetNearFar(float* outNear, float* outFar)
{
	GetCameraNearFar(m_Handle, outNear, outFar);
}

void Camera::SetNearFar(float nearPlane, float farPlane)
{
	SetCameraNearFar(m_Handle, nearPlane, farPlane);
}

DirectX::XMFLOAT3 Camera::GetUpVector()
{
	Float3 up;
	GetCameraUpVector(m_Handle, &up);
	return Pixeon::ToXMFloat3(up);
}

DirectX::XMFLOAT3 Camera::GetRightVector()
{
	Float3 right;
	GetCameraRightVector(m_Handle, &right);
	return Pixeon::ToXMFloat3(right);
}

DirectX::XMFLOAT3 Camera::GetForwardVector()
{
	Float3 forward;
	GetCameraForwardVector(m_Handle, &forward);
	return Pixeon::ToXMFloat3(forward);
}

void Camera::SetChangeCalculation(bool isChange)
{
	SetChangeCameraCalculation(m_Handle, isChange);
}

int Camera::GetCameraNumber()
{
	int number = 0;
	GetCameraNumberAPI(m_Handle, &number);
	return number;
}

// ========================================
// Light Component Implementation
// ========================================

Light::LightType Light::GetType()
{
	int type = 0;
	GetLightType(m_Handle, &type);
	return static_cast<LightType>(type);
}

void Light::SetType(LightType type)
{
	SetLightType(m_Handle, static_cast<int>(type));
}

DirectX::XMFLOAT3 Light::GetColor()
{
	Float3 color;
	GetLightColor(m_Handle, &color);
	return Pixeon::ToXMFloat3(color);
}

void Light::SetColor(const DirectX::XMFLOAT3& color)
{
	Float3 c = Pixeon::ToFloat3(color);
	SetLightColor(m_Handle, &c);
}

float Light::GetIntensity()
{
	float intensity = 0.0f;
	GetLightIntensity(m_Handle, &intensity);
	return intensity;
}

void Light::SetIntensity(float intensity)
{
	SetLightIntensity(m_Handle, intensity);
}

float Light::GetRange()
{
	float range = 0.0f;
	GetLightRange(m_Handle, &range);
	return range;
}

void Light::SetRange(float range)
{
	SetLightRange(m_Handle, range);
}

void Light::GetSpotAngles(float* outInner, float* outOuter)
{
	GetLightSpotInnerOuter(m_Handle, outInner, outOuter);
}

void Light::SetSpotAngles(float inner, float outer)
{
	SetLightSpotInnerOuter(m_Handle, inner, outer);
}

bool Light::IsEnabled()
{
	bool enabled = false;
	GetLightEnabled(m_Handle, &enabled);
	return enabled;
}

void Light::SetEnabled(bool enabled)
{
	SetLightEnabled(m_Handle, enabled);
}

// ========================================
// Image Render Component Implementation
// ========================================

std::string Imagerender::GetTextureName()
{
	char buffer[256];
	GetImageRenderTextureName(m_Handle, buffer, 256);
	return std::string(buffer);
}

void Imagerender::SetTextureName(const std::string& name)
{
	SetImageRenderTextureName(m_Handle, name.c_str());
}

transform Imagerender::GetTransform()
{
	transform t;
	GetImageTransform(m_Handle, &t);
	return t;
}

void Imagerender::SetTransform(const transform& inTransform)
{
	SetImageTransform(m_Handle, &inTransform);
}

DirectX::XMFLOAT4 Imagerender::GetColor()
{
	Float4 color;
	GetImageRenderColor(m_Handle, &color);
	return Pixeon::ToXMFloat4(color);
}

void Imagerender::SetColor(const DirectX::XMFLOAT4& color)
{
	Float4 c = Pixeon::ToFloat4(color);
	SetImageRenderColor(m_Handle, &c);
}

DirectX::XMFLOAT4 Imagerender::GetUVRect()
{
	Float4 uv;
	GetImageRenderUVRect(m_Handle, &uv);
	return Pixeon::ToXMFloat4(uv);
}

void Imagerender::SetUVRect(const DirectX::XMFLOAT4& uvRect)
{
	Float4 uv = Pixeon::ToFloat4(uvRect);
	SetImageRenderUVRect(m_Handle, &uv);
}

// ========================================
// Model Render Component Implementation
// ========================================

DirectX::XMFLOAT4 ModelRender::GetColor()
{
	Float4 color;
	GetModelColor(m_Handle, &color);
	return Pixeon::ToXMFloat4(color);
}

void ModelRender::SetColor(const DirectX::XMFLOAT4& color)
{
	Float4 c = Pixeon::ToFloat4(color);
	SetModelColor(m_Handle, &c);
}

void ModelRender::SetMaterialTexture(int materialIndex, const std::string& texturePath)
{
	::SetMaterialTexture(m_Handle, materialIndex, texturePath.c_str());
}

DirectX::XMFLOAT3 ModelRender::GetOffsetPosition()
{
	Float3 pos;
	::GetOffsetPosition(m_Handle, &pos);
	return Pixeon::ToXMFloat3(pos);
}

void ModelRender::SetOffsetPosition(const DirectX::XMFLOAT3& position)
{
	Float3 pos = Pixeon::ToFloat3(position);
	::SetOffsetPosition(m_Handle, &pos);
}

DirectX::XMFLOAT3 ModelRender::GetOffsetRotation()
{
	Float3 rot;
	::GetOffsetRotation(m_Handle, &rot);
	return Pixeon::ToXMFloat3(rot);
}

void ModelRender::SetOffsetRotation(const DirectX::XMFLOAT3& rotation)
{
	Float3 rot = Pixeon::ToFloat3(rotation);
	::SetOffsetRotation(m_Handle, &rot);
}

DirectX::XMFLOAT3 ModelRender::GetOffsetScale()
{
	Float3 scl;
	::GetOffsetScale(m_Handle, &scl);
	return Pixeon::ToXMFloat3(scl);
}

void ModelRender::SetOffsetScale(const DirectX::XMFLOAT3& scale)
{
	Float3 scl = Pixeon::ToFloat3(scale);
	::SetOffsetScale(m_Handle, &scl);
}

std::string ModelRender::GetBoneName(int boneIndex)
{
	char buffer[256];
	::GetBoneName(m_Handle, boneIndex, buffer, 256);
	return std::string(buffer);
}

DirectX::XMFLOAT3 ModelRender::GetBoneWorldPosition(int boneIndex)
{
	Float3 pos;
	::GetBoneWorldPosition(m_Handle, boneIndex, &pos);
	return Pixeon::ToXMFloat3(pos);
}

DirectX::XMFLOAT3 ModelRender::GetBoneWorldRotation(int boneIndex)
{
	Float3 rot;
	::GetBoneWorldRotation(m_Handle, boneIndex, &rot);
	return Pixeon::ToXMFloat3(rot);
}

// ========================================
// Animation Component Implementation
// ========================================

void Animation::Play()
{
	PlayAnimation(m_Handle);
}

void Animation::Pause()
{
	PauseAnimation(m_Handle);
}

void Animation::Resume()
{
	ResumeAnimation(m_Handle);
}

void Animation::Stop()
{
	StopAnimation(m_Handle);
}

void Animation::Restart()
{
	RestartAnimation(m_Handle);
}

void Animation::SetClip(int clipIndex)
{
	SetAnimationClip(m_Handle, clipIndex);
}

int Animation::GetClip()
{
	int clip = 0;
	GetAnimationClip(m_Handle, &clip);
	return clip;
}

void Animation::SetPlaybackSpeed(float speed)
{
	SetAnimationPlaybackSpeed(m_Handle, speed);
}

void Animation::SetLoop(bool loop)
{
	SetAnimationLoop(m_Handle, loop);
}

// ========================================
// Rigidbody Component Implementation
// ========================================

void Rigidbody::AddForce(const DirectX::XMFLOAT3& force)
{
	Float3 f = Pixeon::ToFloat3(force);
	RigidBodyAddForce(m_Handle, &f);
}

void Rigidbody::AddImpulse(const DirectX::XMFLOAT3& impulse)
{
	Float3 i = Pixeon::ToFloat3(impulse);
	RigidBodyAddImpulse(m_Handle, &i);
}

DirectX::XMFLOAT3 Rigidbody::GetVelocity()
{
	Float3 vel;
	RigidBodyGetVelocity(m_Handle, &vel);
	return Pixeon::ToXMFloat3(vel);
}

void Rigidbody::SetVelocity(const DirectX::XMFLOAT3& velocity)
{
	Float3 vel = Pixeon::ToFloat3(velocity);
	RigidBodySetVelocity(m_Handle, &vel);
}

bool Rigidbody::IsKinematic()
{
	bool kinematic = false;
	RigidBodyGetKinematic(m_Handle, &kinematic);
	return kinematic;
}

void Rigidbody::SetKinematic(bool kinematic)
{
	RigidBodySetKinematic(m_Handle, kinematic);
}

float Rigidbody::GetMass()
{
	float mass = 0.0f;
	RigidBodyGetMass(m_Handle, &mass);
	return mass;
}

void Rigidbody::SetMass(float mass)
{
	RigidBodySetMass(m_Handle, mass);
}

bool Rigidbody::IsUseGravity()
{
	bool useGravity = false;
	RigidBodyGetUseGravity(m_Handle, &useGravity);
	return useGravity;
}

void Rigidbody::SetUseGravity(bool useGravity)
{
	RigidBodySetUseGravity(m_Handle, useGravity);
}

float Rigidbody::GetFriction()
{
	float friction = 0.0f;
	RigidBodyGetFriction(m_Handle, &friction);
	return friction;
}

void Rigidbody::SetFriction(float friction)
{
	RigidBodySetFriction(m_Handle, friction);
}

float Rigidbody::GetRestitution()
{
	float restitution = 0.0f;
	RigidBodyGetRestitution(m_Handle, &restitution);
	return restitution;
}

void Rigidbody::SetRestitution(float restitution)
{
	RigidBodySetRestitution(m_Handle, restitution);
}

float Rigidbody::GetLinearDamping()
{
	float damping = 0.0f;
	RigidBodyGetLinearDamping(m_Handle, &damping);
	return damping;
}

void Rigidbody::SetLinearDamping(float damping)
{
	RigidBodySetLinearDamping(m_Handle, damping);
}

float Rigidbody::GetAngularDamping()
{
	float damping = 0.0f;
	RigidBodyGetAngularDamping(m_Handle, &damping);
	return damping;
}

void Rigidbody::SetAngularDamping(float damping)
{
	RigidBodySetAngularDamping(m_Handle, damping);
}

void Rigidbody::SetDamping(float linearDamping, float angularDamping)
{
	RigidBodySetDamping(m_Handle, linearDamping, angularDamping);
}

float Rigidbody::GetRollingFriction()
{
	float friction = 0.0f;
	RigidBodyGetRollingFriction(m_Handle, &friction);
	return friction;
}

void Rigidbody::SetRollingFriction(float friction)
{
	RigidBodySetRollingFriction(m_Handle, friction);
}

float Rigidbody::GetSpinningFriction()
{
	float friction = 0.0f;
	RigidBodyGetSpinningFriction(m_Handle, &friction);
	return friction;
}

void Rigidbody::SetSpinningFriction(float friction)
{
	RigidBodySetSpinningFriction(m_Handle, friction);
}

// ========================================
// Box Collision Component Implementation
// ========================================

// コールバック保存用グローバルマップ
static std::unordered_map<component, Boxcollision*> g_CollisionMap;

DirectX::XMFLOAT3 Boxcollision::GetSize()
{
	Float3 size;
	BoxCollisionGetSize(m_Handle, &size);
	return Pixeon::ToXMFloat3(size);
}

void Boxcollision::SetSize(const DirectX::XMFLOAT3& size)
{
	Float3 s = Pixeon::ToFloat3(size);
	BoxCollisionSetSize(m_Handle, s);
}

DirectX::XMFLOAT3 Boxcollision::GetCenter()
{
	Float3 center;
	BoxCollisionGetCenter(m_Handle, &center);
	return Pixeon::ToXMFloat3(center);
}

void Boxcollision::SetCenter(const DirectX::XMFLOAT3& center)
{
	Float3 c = Pixeon::ToFloat3(center);
	BoxCollisionSetCenter(m_Handle, c);
}

bool Boxcollision::IsTrigger()
{
	bool trigger = false;
	BoxCollisionGetIsTrigger(m_Handle, &trigger);
	return trigger;
}

void Boxcollision::SetTrigger(bool isTrigger)
{
	BoxCollisionSetIsTrigger(m_Handle, isTrigger);
}

void Boxcollision::StaticOnEnter(component comp, const APICollisionInfo* info)
{
	auto it = g_CollisionMap.find(comp);
	if (it != g_CollisionMap.end() && it->second->m_OnEnter) {
		it->second->m_OnEnter(*info);
	}
}

void Boxcollision::StaticOnStay(component comp, const APICollisionInfo* info)
{
	auto it = g_CollisionMap.find(comp);
	if (it != g_CollisionMap.end() && it->second->m_OnStay) {
		it->second->m_OnStay(*info);
	}
}

void Boxcollision::StaticOnExit(component comp, const APICollisionInfo* info)
{
	auto it = g_CollisionMap.find(comp);
	if (it != g_CollisionMap.end() && it->second->m_OnExit) {
		it->second->m_OnExit(*info);
	}
}

void Boxcollision::SetOnCollisionEnter(CollisionCallback callback)
{
	m_OnEnter = callback;
	g_CollisionMap[m_Handle] = this;
	CollisionSetCollisionEnterCallback(m_Handle, StaticOnEnter);
}

void Boxcollision::SetOnCollisionStay(CollisionCallback callback)
{
	m_OnStay = callback;
	g_CollisionMap[m_Handle] = this;
	CollisionSetCollisionStayCallback(m_Handle, StaticOnStay);
}

void Boxcollision::SetOnCollisionExit(CollisionCallback callback)
{
	m_OnExit = callback;
	g_CollisionMap[m_Handle] = this;
	CollisionSetCollisionExitCallback(m_Handle, StaticOnExit);
}

// ========================================
// Effect Component Implementation
// ========================================

void Effect::Play()
{
	EffectPlay(m_Handle);
}

void Effect::Stop()
{
	EffectStop(m_Handle);
}

bool Effect::IsPlaying()
{
	bool playing = false;
	EffectIsPlaying(m_Handle, &playing);
	return playing;
}

// ========================================
// Script Component Implementation
// ========================================

void Script::CallFunction(const std::string& functionName)
{
	CallScriptFunction(m_Handle, functionName.c_str());
}

// ========================================
// Keyframe Implementation
// ========================================

void Keyframe::SetVertexOffsetUp(float offset)
{
	::SetVertexOffsetUp(m_Handle, offset);
}

void Keyframe::SetVertexOffsetDown(float offset)
{
	::SetVertexOffsetDown(m_Handle, offset);
}

void Keyframe::SetVertexOffsetLeft(float offset)
{
	::SetVertexOffsetLeft(m_Handle, offset);
}

void Keyframe::SetVertexOffsetRight(float offset)
{
	::SetVertexOffsetRight(m_Handle, offset);
}

float Keyframe::GetVertexOffsetUp()
{
	float offset = 0.0f;
	::GetVertexOffsetUp(m_Handle, &offset);
	return offset;
}

float Keyframe::GetVertexOffsetDown()
{
	float offset = 0.0f;
	::GetVertexOffsetDown(m_Handle, &offset);
	return offset;
}

float Keyframe::GetVertexOffsetLeft()
{
	float offset = 0.0f;
	::GetVertexOffsetLeft(m_Handle, &offset);
	return offset;
}

float Keyframe::GetVertexOffsetRight()
{
	float offset = 0.0f;
	::GetVertexOffsetRight(m_Handle, &offset);
	return offset;
}

// ========================================
// Animator2d Implementation
// ========================================

void Animator2d::Play()
{
	Animator2DPlay(m_Handle);
}

void Animator2d::Stop()
{
	Animator2DStop(m_Handle);
}

bool Animator2d::IsEnd()
{
	bool ended = false;
	Animator2DIsEnd(m_Handle, &ended);
	return ended;
}

Keyframe* Animator2d::FindKeyFrame(const std::string& keyName)
{
	keyframe kf = nullptr;
	if (::FindKeyFrame(m_Handle, keyName.c_str(), &kf) == APIResult::PN_SUCCESS) {
		return new Keyframe(kf);
	}
	return nullptr;
}

// ========================================
// Input Implementation
// ========================================

bool Input::IsKeyPressed(char keyCode)
{
	return KeyPressed(keyCode);
}

bool Input::IsKeyTriggered(char keyCode)
{
	return KeyTriggered(keyCode);
}

bool Input::IsKeyReleased(char keyCode)
{
	return KeyReleased(keyCode);
}

bool Input::IsKeyRepeated(char keyCode)
{
	return KeyRepeated(keyCode);
}

int Input::GetMouseMoveX()
{
	return ::GetMouseMoveX();
}

int Input::GetMouseMoveY()
{
	return ::GetMouseMoveY();
}

void Input::FixMouseCursor(bool enabled)
{
	FixedMouseCursor(enabled);
}

// ========================================
// Template Specializations for GetComponent
// ========================================

template<>
Camera* Object::GetComponent<Camera>(const std::string& componentName)
{
	component comp = nullptr;
	if (FindComponent(m_Handle, componentName.c_str(), &comp) == APIResult::PN_SUCCESS) {
		return new Camera(comp);
	}
	return nullptr;
}

template<>
Light* Object::GetComponent<Light>(const std::string& componentName)
{
	component comp = nullptr;
	if (FindComponent(m_Handle, componentName.c_str(), &comp) == APIResult::PN_SUCCESS) {
		return new Light(comp);
	}
	return nullptr;
}

template<>
Imagerender* Object::GetComponent<Imagerender>(const std::string& componentName)
{
	component comp = nullptr;
	if (FindComponent(m_Handle, componentName.c_str(), &comp) == APIResult::PN_SUCCESS) {
		return new Imagerender(comp);
	}
	return nullptr;
}

template<>
ModelRender* Object::GetComponent<ModelRender>(const std::string& componentName)
{
	component comp = nullptr;
	if (FindComponent(m_Handle, componentName.c_str(), &comp) == APIResult::PN_SUCCESS) {
		return new ModelRender(comp);
	}
	return nullptr;
}

template<>
Animation* Object::GetComponent<Animation>(const std::string& componentName)
{
	component comp = nullptr;
	if (FindComponent(m_Handle, componentName.c_str(), &comp) == APIResult::PN_SUCCESS) {
		return new Animation(comp);
	}
	return nullptr;
}

template<>
Rigidbody* Object::GetComponent<Rigidbody>(const std::string& componentName)
{
	component comp = nullptr;
	if (FindComponent(m_Handle, componentName.c_str(), &comp) == APIResult::PN_SUCCESS) {
		return new Rigidbody(comp);
	}
	return nullptr;
}

template<>
Boxcollision* Object::GetComponent<Boxcollision>(const std::string& componentName)
{
	component comp = nullptr;
	if (FindComponent(m_Handle, componentName.c_str(), &comp) == APIResult::PN_SUCCESS) {
		return new Boxcollision(comp);
	}
	return nullptr;
}

template<>
Animator2d* Object::GetComponent<Animator2d>(const std::string& componentName)
{
	component comp = nullptr;
	if (FindComponent(m_Handle, componentName.c_str(), &comp) == APIResult::PN_SUCCESS) {
		return new Animator2d(comp);
	}
	return nullptr;
}

template<>
Effect* Object::GetComponent<Effect>(const std::string& componentName)
{
	component comp = nullptr;
	if (FindComponent(m_Handle, componentName.c_str(), &comp) == APIResult::PN_SUCCESS) {
		return new Effect(comp);
	}
	return nullptr;
}

template<>
Script* Object::GetComponent<Script>(const std::string& componentName)
{
	component comp = nullptr;
	if (FindComponent(m_Handle, componentName.c_str(), &comp) == APIResult::PN_SUCCESS) {
		return new Script(comp);
	}
	return nullptr;
}