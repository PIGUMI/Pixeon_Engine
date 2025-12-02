#include "SkyBox.h"
#include <Windows.h>

void Script_SkyBox::BeginPlay() {
    APIResult rst;
	rst = GetCurrentScene(&CurrentScene);
    if(rst != APIResult::PN_SUCCESS) {
		MessageBox(nullptr, "Failed to get current scene in SkyBox script.", "Error", MB_OK | MB_ICONERROR);
	}
	rst = GetGameObject(CurrentScene, "SkyBox", &skyboxObject);
	if(rst != APIResult::PN_SUCCESS) {
		MessageBox(nullptr, "Failed to find SkyBox object in SkyBox script.", "Error", MB_OK | MB_ICONERROR);
	}
	rst = GetGameObject(CurrentScene, "Camera", &mainCameraObject);
	if(rst != APIResult::PN_SUCCESS) {
		MessageBox(nullptr, "Failed to find MainCamera object in SkyBox script.", "Error", MB_OK | MB_ICONERROR);
	}
}

void Script_SkyBox::Update() {
	APIResult rst;
	ComponentHandle Camera;
	rst = GetComponent(mainCameraObject, "CameraComponent", &Camera);
	if(rst != APIResult::PN_SUCCESS) {
		MessageBox(nullptr, "Failed to get Camera component in SkyBox script.", "Error", MB_OK | MB_ICONERROR);
		return;
	}
	CameraTransform Temp;
	rst = CameraComponent_GetTransform(Camera, &Temp);
	if(rst != APIResult::PN_SUCCESS) {
		MessageBox(nullptr, "Failed to get Camera transform in SkyBox script.", "Error", MB_OK | MB_ICONERROR);
		return;
	}
	TransformData SkyboxTransform;
	rst = GetGameObjectTransform(skyboxObject, &SkyboxTransform);
	if(rst != APIResult::PN_SUCCESS) {
		MessageBox(nullptr, "Failed to get SkyBox transform in SkyBox script.", "Error", MB_OK | MB_ICONERROR);
		return;
	}
	SkyboxTransform.position = Temp.position;
	rst = SetGameObjectTransform(skyboxObject, &SkyboxTransform);
	if(rst != APIResult::PN_SUCCESS) {
		MessageBox(nullptr, "Failed to set SkyBox transform in SkyBox script.", "Error", MB_OK | MB_ICONERROR);
		return;
	}
}

void Script_SkyBox::EndPlay() {
}
