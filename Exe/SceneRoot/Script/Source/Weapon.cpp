#include "Weapon.h"
#include <Windows.h>
#include <DirectXMath.h>

void Script_Weapon::BeginPlay() {
    IScript::BeginPlay();
    GetObjectTransform(_parentObject, &_idleTransform);

    {
        _ReadyTransform.position.x = -0.3f;
        _ReadyTransform.position.y =  0.9f;
        _ReadyTransform.position.z =  0.0f;
        _ReadyTransform.rotation.x = DirectX::XMConvertToRadians(16.0f);
        _ReadyTransform.rotation.y = DirectX::XMConvertToRadians(90.0f);
        _ReadyTransform.rotation.z = DirectX::XMConvertToRadians( 9.5f);
        _ReadyTransform.scale.x = _ReadyTransform.scale.y = _ReadyTransform.scale.z = 0.5f;
        _transitionProgress = 0.0f;
        _coolTime = 0;
        _isAiming = false;
    }

    FindPrefabObjectByName("Expl", &_ExplosionEffect);
    FindChildObjectByName(_parentObject, "Body", &_Body);
}

void Script_Weapon:: Update(float DeltaTime) {
    IScript::Update(DeltaTime);
    float transitionSpeed = 0.1f;

    if (KeyPressed(VK_RBUTTON))
    {
        _isAiming = true;
        _transitionProgress += transitionSpeed;
        if (_transitionProgress > 1.0f)_transitionProgress = 1.0f;
    }
    else
    {
        _isAiming = false;
        _transitionProgress -= transitionSpeed;
        if (_transitionProgress < 0.0f)_transitionProgress = 0.0f;
    }

    Animation();
}

void Script_Weapon::EndPlay() {
    IScript::EndPlay();
}

void Script_Weapon::Animation() {
    transform current_Transform;
    float t = _transitionProgress;


    current_Transform.position.x = _idleTransform.position.x + (_ReadyTransform.position.x - _idleTransform.position.x) * t;
    current_Transform.position.y = _idleTransform.position.y + (_ReadyTransform.position.y - _idleTransform.position.y) * t;
    current_Transform.position.z = _idleTransform.position.z + (_ReadyTransform.position.z - _idleTransform.position.z) * t;

    current_Transform.rotation.x = _idleTransform.rotation.x + (_ReadyTransform.rotation.x - _idleTransform.rotation.x) * t;
    current_Transform.rotation.y = _idleTransform.rotation.y + (_ReadyTransform.rotation.y - _idleTransform.rotation.y) * t;
    current_Transform.rotation.z = _idleTransform.rotation.z + (_ReadyTransform.rotation.z - _idleTransform.rotation.z) * t;

    current_Transform.scale.x = _idleTransform.scale.x + (_ReadyTransform.scale.x - _idleTransform.scale.x) * t;
    current_Transform.scale.y = _idleTransform.scale.y + (_ReadyTransform.scale.y - _idleTransform.scale.y) * t;
    current_Transform.scale.z = _idleTransform.scale.z + (_ReadyTransform.scale.z - _idleTransform.scale.z) * t;

    SetObjectTransform(_parentObject, &current_Transform);
}