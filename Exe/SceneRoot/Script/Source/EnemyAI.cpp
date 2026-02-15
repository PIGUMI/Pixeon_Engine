#include "EnemyAI.h"

void Script_EnemyAI::BeginPlay() {
    IScript::BeginPlay(); // BeginPlay
}

void Script_EnemyAI::Update(float DeltaTime) {
    IScript::Update(DeltaTime);

    // レイの開始位置（目の高さ）
    DirectX::XMFLOAT3 rayOrigin = _parentObject->GetPosition();
    rayOrigin.y += 1.0f;

    DirectX::XMFLOAT3 currentRotation = _parentObject->GetRotation();

	// 回転角度を0～360度の範囲に正規化
    if(DirectX::XMConvertToDegrees(currentRotation.y) < 0.0f) {
        currentRotation.y += DirectX::XMConvertToRadians(360.0f);
	}
    if (DirectX::XMConvertToDegrees(currentRotation.y) >= 360.0f) {
        currentRotation.y -= DirectX::XMConvertToRadians(360.0f);
    }
	Y = DirectX::XMConvertToDegrees(currentRotation.y); // プロパティに回転角度を保存

    DirectX::XMFLOAT3 forwardDirection = {
        sinf(currentRotation.y),
        0.0f,
        cosf(currentRotation.y)
    };

    bool playerFound = false;
    RayHit hitInfo;

    if (_parentScene->RaycastIgnoreTriggers(rayOrigin, forwardDirection, 2.0f, &hitInfo)) {
        if (hitInfo.bHit) {
            std::string hitObjectName = hitInfo.hitObjectName;

            if (hitObjectName == "Player") {
                playerFound = true;
            }
        }
    }
	IsCheckingPlayer = playerFound;

    // プレイヤーが見つからなければ回転
    if (!playerFound) {
        const float rotationSpeed = 10.0f;
        currentRotation.y += rotationSpeed * DeltaTime;
        _parentObject->SetRotation(currentRotation);
    }
}

void Script_EnemyAI::EndPlay() {
    IScript::EndPlay();// EndPlay
}
