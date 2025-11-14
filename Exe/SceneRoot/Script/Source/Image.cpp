#include "Image.h"
#include <Windows.h>

void Script_Image::BeginPlay() {
    // BeginPlay
    SceneHandle NowScene;
	GetCurrentScene(&NowScene);
	GetGameObject(NowScene,"Img",&Object);
	APIResult rec;
	rec = GetComponent(Object, "ImageRender", &Img);
	if(rec != APIResult::PN_SUCCESS){
		MessageBoxA(NULL, "Image Component not found!", "Error", MB_OK | MB_ICONERROR);
	}
}

void Script_Image::Update() {
    // Update
	Float2 Offset2D;
	ImageRender_GetOffset2D(Img, &Offset2D);
	Offset2D.x += 1.0f;
	ImageRender_SetOffset2D(Img,Offset2D);
}

void Script_Image::EndPlay() {
    // EndPlay
}
