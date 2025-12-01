#include "SkyBox.h"
#include <Windows.h>

void Script_SkyBox::BeginPlay() {
    // BeginPlay
    SceneHandle CurrentScene;
	GetCurrentScene(&CurrentScene);
	APIResult res;

	res = GetGameObject(CurrentScene,"SkyBox",&SkyBox);
	if (res != APIResult::PN_SUCCESS) {
		std::string Number = std::to_string(static_cast<int>(res));
		Number += ":Error getting SkyBox game object in SkyBox script";
		MessageBox(NULL,Number.c_str(), "Error", MB_OK);
	}

	res = GetGameObject(CurrentScene,"Cam",&Cam);
	if (res != APIResult::PN_SUCCESS) {
		std::string Number = std::to_string(static_cast<int>(res));
		Number += ":Error getting Cam game object in SkyBox script";
		MessageBox(NULL,Number.c_str(), "Error", MB_OK);
	}	
}

void Script_SkyBox::Update() {
	TransformData CamTransform;
	APIResult res;
	if (Cam == nullptr||SkyBox == nullptr)
	{
		SceneHandle CurrentScene;
		GetCurrentScene(&CurrentScene);
		APIResult res;
		res = GetGameObject(CurrentScene, "SkyBox", &SkyBox);
		if (res != APIResult::PN_SUCCESS) {
			std::string Number = std::to_string(static_cast<int>(res));
			Number += ":Error getting SkyBox game object in SkyBox script";
			MessageBox(NULL, Number.c_str(), "Error", MB_OK);
		}
		res = GetGameObject(CurrentScene, "Cam", &Cam);
		if (res != APIResult::PN_SUCCESS) {
			std::string Number = std::to_string(static_cast<int>(res));
			Number += ":Error getting Cam game object in SkyBox script";
			MessageBox(NULL, Number.c_str(), "Error", MB_OK);
		}
	}
	res = GetGameObjectTransform(Cam,&CamTransform);
	if (res != APIResult::PN_SUCCESS) {
		std::string Number = std::to_string(static_cast<int>(res));
		Number += ":Error getting camera transform in SkyBox script";
		MessageBox(NULL,Number.c_str(), "Error", MB_OK);
	}

	SetGameObjectTransform(SkyBox,&CamTransform);
	if (res != APIResult::PN_SUCCESS) {
		std::string Number = std::to_string(static_cast<int>(res));
		Number += ":Error setting skybox transform in SkyBox script";
		MessageBox(NULL, Number.c_str(), "Error", MB_OK);
	}
	
}

void Script_SkyBox::EndPlay() {
	 
}
