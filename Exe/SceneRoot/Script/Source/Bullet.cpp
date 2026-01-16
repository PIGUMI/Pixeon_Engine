#include "Bullet.h"

void Script_Bullet::BeginPlay() {
	Component Physics;
	Object Player;
	// 物理演算コンポーネントを取得
	FindComponent(_parentObject, "RigidBody", &Physics);
	// プレイヤーオブジェクトとカメラコンポーネントを取得
	SceneHandle current_Scene;
	GetCurrentScene(&current_Scene);
	FindObjectByName(current_Scene,"Player", &Player);
	Component Player_Camera;
	FindComponent(Player, "CameraComponent", &Player_Camera);

	Float3 Forward;
	// プレイヤーカメラの前方ベクトルを取得
	GetCameraForwardVector(Player_Camera, &Forward);
	// 弾丸に前方ベクトル方向の力を加える
	Forward.x *= 500.0f;
	Forward.y *= 500.0f;
	Forward.z *= 500.0f;
	RigidBodyAddImpulse(Physics,&Forward);

}

void Script_Bullet:: Update() {
}

void Script_Bullet::EndPlay() {
}
