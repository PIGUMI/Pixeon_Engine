#include "attachWeapon.h"
#include <Windows.h>
void Script_attachWeapon::BeginPlay() {
    IScript::BeginPlay(); // BeginPlay
    Object* AttachObject = nullptr;
    AttachObject = _parentScene->FindObject(AttachObjectName);
    if (AttachObject)
    {
		Model = AttachObject->GetComponent<ModelRender>("ModelRender");
        delete AttachObject;
		AttachObject = nullptr;
    }
    if (!Model)
    {
		MessageBoxA(NULL, "ModelRenderコンポーネントが見つかりませんでした。", "エラー", MB_OK | MB_ICONERROR);
    }
	OffsetPosition = _parentObject->GetPosition();
    
}

void Script_attachWeapon:: Update(float DeltaTime) {
    IScript::Update(DeltaTime);// Update
	if (Model == nullptr)return;
	DirectX::XMFLOAT3 BonePos = Model->GetBoneWorldPosition(BoneIndex);
	DirectX::XMFLOAT3 BoneRot = Model->GetBoneWorldRotation(BoneIndex);
    BonePos.x *= -0.001f;
	BonePos.y *= 0.001f;
	BonePos.z *= 0.001f;
    BonePos.x += OffsetPosition.x;
    BonePos.y += OffsetPosition.y;
    BonePos.z += OffsetPosition.z;
    _parentObject->SetPosition(BonePos);
	//_parentObject->SetRotation(BoneRot);
}

void Script_attachWeapon::EndPlay() {
    IScript::EndPlay();// EndPlay
    if(Model)
    {
		delete Model;
        Model = nullptr;
	}
}
