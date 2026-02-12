#include "attachWeapon.h"
#include <Windows.h>

void Script_attachWeapon::BeginPlay() {
    IScript::BeginPlay();
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
}

void Script_attachWeapon::Update(float DeltaTime) {
    IScript::Update(DeltaTime);

    if (Model == nullptr) return;

    DirectX::XMFLOAT3 BoneLocalPos = Model->GetBoneLocalPosition(BoneIndex);
    DirectX::XMFLOAT3 BoneLocalRot = Model->GetBoneLocalRotation(BoneIndex);

    Object* ModelObject = _parentScene->FindObject(AttachObjectName);
    if (!ModelObject) return;

    transform ModelTransform = ModelObject->GetTransform();
    delete ModelObject;

    using namespace DirectX;

    XMMATRIX scaleMatrix = XMMatrixScaling(
        ModelTransform.scale.x,
        ModelTransform.scale.y,
        ModelTransform.scale.z
    );

    XMMATRIX rotationMatrix = XMMatrixRotationRollPitchYaw(
        ModelTransform.rotation.x,
        ModelTransform.rotation.y,
        ModelTransform.rotation.z
    );

    XMMATRIX translationMatrix = XMMatrixTranslation(
        ModelTransform.position.x,
        ModelTransform.position.y,
        ModelTransform.position.z
    );

    XMVECTOR boneLocalPosVec = XMLoadFloat3(&BoneLocalPos);

    XMVECTOR worldPosVec = XMVector3Transform(
        boneLocalPosVec,
        scaleMatrix * rotationMatrix * translationMatrix
    );

    DirectX::XMFLOAT3 WorldPos;
    XMStoreFloat3(&WorldPos, worldPosVec);

    DirectX::XMFLOAT3 WorldRot;
    WorldRot.x = ModelTransform.rotation.x + BoneLocalRot.x;
    WorldRot.y = ModelTransform.rotation.y + BoneLocalRot.y;
    WorldRot.z = ModelTransform.rotation.z + BoneLocalRot.z;

    _parentObject->SetPosition(WorldPos);
    _parentObject->SetRotation(WorldRot);
}

void Script_attachWeapon::EndPlay() {
    IScript::EndPlay();
    if (Model)
    {
        delete Model;
        Model = nullptr;
    }
}