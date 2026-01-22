#include "Bullet.h"

void Script_Bullet::BeginPlay()
{
    // シーン取得
    if (GetCurrentScene(&_scene) != PN_SUCCESS || !_scene) {
        return;
    }

    // 自分の Rigidbody を取得
    if (FindComponent(_parentObject, "RigidBody", &_rigidBody) != PN_SUCCESS || !_rigidBody) {
        return;
    }

    // Player オブジェクト取得
    Object player = nullptr;
    if (FindObjectByName(_scene, "Player", &player) != PN_SUCCESS || !player) {
        return;
    }

    // Player の子 Body オブジェクト取得
    Object body = nullptr;
    if (FindChildObjectByName(player, "Body", &body) != PN_SUCCESS || !body) {
        return;
    }

    // Body 上のカメラコンポーネント取得
    Component camera = nullptr;
    if (FindComponent(body, "CameraComponent", &camera) != PN_SUCCESS || !camera) {
        return;
    }

    // カメラの前方ベクトル取得
    Float3 forward{};
    if (GetCameraForwardVector(camera, &forward) != PN_SUCCESS) {
        return;
    }

    // 弾の初期位置を Body から少し手前にずらす
    transform bodyTransform{};
    if (GetObjectWorldTransform(body, &bodyTransform) != PN_SUCCESS) {
        return;
    }

    // カメラの前方方向に 0.5f だけ戻した位置に配置
    bodyTransform.position.x -= forward.x * 0.5f;
    bodyTransform.position.z -= forward.z * 0.5f;
    SetObjectTransform(_parentObject, &bodyTransform);

    // 発射インパルスを付与
    Float3 impulse{};
    impulse.x = -forward.x * 50.0f;
    impulse.y = 0.0f;
    impulse.z = -forward.z * 50.0f;

    RigidBodyAddImpulse(_rigidBody, &impulse);
}

void Script_Bullet::Update(float DeltaTime)
{
    // シーン取得に失敗していた場合・RigidBody が取れていない場合は何もしない
    if (!_scene || !_rigidBody) {
        return;
    }

    // 寿命カウント
    if (--LifeTime > 0) {
        return;
    }

    // 寿命が尽きたらシーンから削除
    RemoveObjectFromScene(_scene, _parentObject);
}

void Script_Bullet::EndPlay()
{
    // 必要であればここで後処理
    _scene = nullptr;
    _rigidBody = nullptr;
}