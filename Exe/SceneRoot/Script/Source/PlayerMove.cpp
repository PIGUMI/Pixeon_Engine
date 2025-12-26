#include "PlayerMove.h"
#include <Windows.h>
#include <DirectXMath.h>


void Script_PlayerMove::BeginPlay() {
	SceneHandle Temp;
	GetCurrentScene(&Temp);
	// オブジェクトの取得
	FindObjectByName(Temp, "Player", &player);
	// コンポーネントの取得
	FindComponent(player, "Animation", &Animation);
	FindComponent(player, "CameraComponent", &Camera);
	FindComponent(player, "RigidBody", &rigidBody);
	// カメラの設定
	int camNum;
	APIResult rs;
	rs = GetCameraNumber(Camera, &camNum);
	if(rs != PN_SUCCESS) {
		MessageBoxA(NULL, "Failed to get camera number", "Error", MB_OK);
	}
	else
	{
		MessageBoxA(NULL, ("Camera number: " + std::to_string(camNum)).c_str(), "Info", MB_OK);
	}
	SetMainCamera(camNum);
	// マウスカーソルを固定
	FixedMouseCursor(true);
}

void Script_PlayerMove::Update() {
	currentState = Idle;
	transform trans;
	GetObjectTransform(player, &trans);
	// カメラの前方ベクトル取得
	Float3 vec;
	GetCameraForwardVector(Camera, &vec);

	// 入力処理
	if (KeyPressed('W')) {
		if (KeyPressed(VK_SHIFT)) {
			currentState = Running;
			Float3 Force = { -vec.x * 400.0f, 0.0f, -vec.z * 400.0f };
			RigidBodyAddForce(rigidBody,&Force);
		}
		else
		{
			currentState = Walking;
			Float3 Force = { -vec.x * 200.0f, 0.0f, -vec.z * 200.0f };
			RigidBodyAddForce(rigidBody,&Force);
		}
	}

	float MouseX = 0;
	float MouseY = 0;
	MouseX = (float)GetMouseMoveX();
	MouseY = (float)GetMouseMoveY();
	MouseX = MouseX * 0.001f;
	MouseY = MouseY * 0.001f;
	
	// カメラの処理
	CameraTransform camTrans;
	GetCameraTransform(Camera,&camTrans);
	camTrans.rotation.x += MouseX;
	camTrans.rotation.y -= MouseY;
	SetCameraTransform(Camera,&camTrans);

	// プレイヤーの回転処理
	Float3 PlayerRot;
	PlayerRot = trans.rotation;
	PlayerRot.y = camTrans.rotation.x - DirectX::XMConvertToRadians(180.0f);

	SetObjectRotation(player, PlayerRot);
	

	// アニメーション
	if(previousState != currentState) {

		switch (currentState)
		{
		case Idle:
			SetAnimationClip(Animation, 1);
			break;
		case Walking:
			SetAnimationClip(Animation, 2);
			break;
		case Running:
			SetAnimationClip(Animation, 3);
			break;
		default:
			break;
		}
		previousState = currentState;
	}
}

void Script_PlayerMove::EndPlay() {
}
