#include "API.h"

extern "C"{

PIXEON_API APIResult GetCurrentScene(Scene* outHandle)
{
    return PIXEON_API APIResult();
}

PIXEON_API APIResult GetGameObject(Scene scene, const char* objectname, GameObject* outObject)
{
    return PIXEON_API APIResult();
}

PIXEON_API APIResult GetGameObjectTransform(GameObject gameObject, TransformData* outTransform)
{
    return PIXEON_API APIResult();
}

PIXEON_API APIResult SetGameObjectTransform(GameObject gameObject, const TransformData* inTransform)
{
    return PIXEON_API APIResult();
}

}
