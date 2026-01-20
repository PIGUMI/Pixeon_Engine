#include "Weapon.h"
#include <Windows.h>
#include <DirectXMath.h>
#include <algorithm>

void Script_Weapon::BeginPlay() {
	GetObjectTransform(_parentObject, &_StartTransform);
	_EndTransform.position.x = -0.3f;
	_EndTransform.position.y = 0.9f;
	_EndTransform.position.z = 0.0f;
	_EndTransform.rotation.x = DirectX::XMConvertToRadians(16.0f);
	_EndTransform.rotation.y = DirectX::XMConvertToRadians(90.0f);
	_EndTransform.rotation.z = DirectX::XMConvertToRadians(9.5f);
	_EndTransform.scale.x	 = 0.5f;
	_EndTransform.scale.y    = 0.5f;
	_EndTransform.scale.z    = 0.5f;
	_transitionProgress = 0.0f;
	_isAiming = false;

	GetCurrentScene(&scene);
	FindObjectByName(scene, "Player", &Player);
	FindChildObjectByName(Player, "Head", &Head);
	Object Body;
	FindChildObjectByName(Player, "Body", &Body);
	FindComponent(Body, "CameraComponent", &CameraComp);
	FindPrefabObjectByName("Expl", &ExplosionEffect);
}

void Script_Weapon::Update() {
	float transitionSpeed = 0.1f;

	// Aiming
	if (KeyPressed(VK_RBUTTON)) {
		_isAiming = true;
		_transitionProgress += transitionSpeed;
		if (_transitionProgress > 1.0f) {
			_transitionProgress = 1.0f;
		}
	}
	else {
		_isAiming = false;
		_transitionProgress -= transitionSpeed;
		if (_transitionProgress < 0.0f) {
			_transitionProgress = 0.0f;
		}
	}

	Animation();

	if (KeyTriggered(VK_LBUTTON) && _isAiming) {
		Effect();
	}
}

void Script_Weapon::Animation() 
{
	transform currentTransform;
	float t = _transitionProgress;

	currentTransform.position.x = _StartTransform.position.x + (_EndTransform.position.x - _StartTransform.position.x) * t;
	currentTransform.position.y = _StartTransform.position.y + (_EndTransform.position.y - _StartTransform.position.y) * t;
	currentTransform.position.z = _StartTransform.position.z + (_EndTransform.position.z - _StartTransform.position.z) * t;

	currentTransform.rotation.x = _StartTransform.rotation.x + (_EndTransform.rotation.x - _StartTransform.rotation.x) * t;
	currentTransform.rotation.y = _StartTransform.rotation.y + (_EndTransform.rotation.y - _StartTransform.rotation.y) * t;
	currentTransform.rotation.z = _StartTransform.rotation.z + (_EndTransform.rotation.z - _StartTransform.rotation.z) * t;

	currentTransform.scale.x = _StartTransform.scale.x + (_EndTransform.scale.x - _StartTransform.scale.x) * t;
	currentTransform.scale.y = _StartTransform.scale.y + (_EndTransform.scale.y - _StartTransform.scale.y) * t;
	currentTransform.scale.z = _StartTransform.scale.z + (_EndTransform.scale.z - _StartTransform.scale.z) * t;

	SetObjectTransform(_parentObject, &currentTransform);
}

void Script_Weapon::Effect() 
{
	transform explosionTransform;
	Float3 Forward;

	GetObjectTransform(Player, &explosionTransform);
	GetCameraForwardVector(CameraComp, &Forward);

	float length = sqrtf(Forward.x * Forward.x + Forward.y * Forward.y + Forward.z * Forward.z);
	if (length != 0.0f) {
		Forward.x /= length;
		Forward.y /= length;
		Forward.z /= length;
	}
	explosionTransform.position.x -= Forward.x * 5.0f;
	explosionTransform.position.z -= Forward.z * 5.0f;

	explosionTransform.position.y += 1.25f;
	explosionTransform.scale.x = 0.002f;
	explosionTransform.scale.y = 0.002f;

	SetObjectTransform(ExplosionEffect, &explosionTransform);
	AddObjectToScene(scene, ExplosionEffect);
}

void Script_Weapon::EndPlay() {
}