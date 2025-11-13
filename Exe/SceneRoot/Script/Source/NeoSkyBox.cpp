#include "NeoSkyBox.h"
#include <Windows.h>

void Script_NeoSkyBox::BeginPlay() {
    // BeginPlay
    SceneHandle CurrentScene;
    GetCurrentScene(&CurrentScene);
	APIResult res;
	res = GetGameObject(CurrentScene, "SkyBox", &skyboxObject);
	if (res != APIResult::PN_SUCCESS) {
		std::string Number = std::to_string(static_cast<int>(res));
		Number += ":Error getting SkyBox game object in SkyBox script";
		MessageBox(NULL, Number.c_str(), "Error", MB_OK);
	}
	res = GetGameObject(CurrentScene, "Cam", &mainCameraObject);
	if (res != APIResult::PN_SUCCESS) {
		std::string Number = std::to_string(static_cast<int>(res));
		Number += ":Error getting Cam game object in SkyBox script";
		MessageBox(NULL, Number.c_str(), "Error", MB_OK);
	}
	res = GetComponent(mainCameraObject, "CameraComponent", &CameraComp);
	if (res != APIResult::PN_SUCCESS) {
		std::string Number = std::to_string(static_cast<int>(res));
		Number += ":Error getting CameraComponent in SkyBox script";
		MessageBox(NULL, Number.c_str(), "Error", MB_OK);
	}
}

void Script_NeoSkyBox::Update() {
    // Update
	CameraTransform camTransform;
	CameraComponent_GetTrsform(CameraComp, &camTransform);
	TransformData skyboxTransform;
	GetGameObjectTransform(skyboxObject, &skyboxTransform);
	skyboxTransform.position = camTransform.position;
	SetGameObjectTransform(skyboxObject, &skyboxTransform);
}

void Script_NeoSkyBox::EndPlay() {
    // EndPlay
}
