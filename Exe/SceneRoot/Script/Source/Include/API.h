#ifndef API_H
#define API_H

/*
* Pixeon Engine API
* Now Version: 1.0.0
* Copyright (c) AC30W
*/

// Define PIXEON_API for DLL export/import
#ifdef PixeonEngine_EXPORTS
#define PIXEON_API __declspec(dllexport)
#else
#define PIXEON_API __declspec(dllimport)
#endif

/* ErrorCodes */
typedef enum {
	PN_SUCCESS = 0,
	PN_ERROR_NULL_POINTER = -1,
	PN_ERROR_INVALID_HANDLE = -2,
	PN_ERROR_NOT_FOUND = -3,
	PN_ERROR_BUFFER_TOO_SMALL = -4,
	PN_ERROR_INVALID_PARAMETER = -5,
}APIResult;

// Handle Types
typedef void* SceneHandle;
typedef void* GameObjectHandle;
typedef void* ComponentHandle;

#pragma pack(push, 1)  // 1バイトアライメントで構造体パディングを制御

typedef struct {
    float x;
    float y;
} Float2;

typedef struct {
    float x;
    float y;
    float z;
} Float3;

typedef struct {
    float x;
    float y;
    float z;
    float w;
} Float4;

typedef struct {
    Float3 position;
    Float3 rotation;  // Radians
    Float3 scale;
} TransformData;


#pragma pack(pop)

/* シーンに関するAPI */
extern "C"{
	/* 現在のシーンの取得 */
	PIXEON_API APIResult GetCurrentScene(SceneHandle* outHandle);

}

/* ゲームオブジェクトに関するAPI */
extern"C" {
	PIXEON_API APIResult GetGameObject(SceneHandle scene, const char* objectname, GameObjectHandle* outObject);
	PIXEON_API APIResult GetGameObjectTransform(GameObjectHandle gameObject, TransformData* outTransform);
	PIXEON_API APIResult SetGameObjectTransform(GameObjectHandle gameObject, const TransformData* inTransform);
}

/* コンポーネントに関するAPI */
extern"C"
{

}



#endif// API.h