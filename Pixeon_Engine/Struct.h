#pragma once
#include <DirectXMath.h>
#include <string>

class Object;

struct Transform
{
	DirectX::XMFLOAT3 position = { 0.0f, 0.0f, 0.0f };
	DirectX::XMFLOAT3 rotation = { 0.0f, 0.0f, 0.0f };
	DirectX::XMFLOAT3 scale = { 1.0f, 1.0f, 1.0f };
};

struct CollisionInfo
{
	Object* HitObject = nullptr;
	DirectX::XMFLOAT3 HitPoint = { 0.0f, 0.0f, 0.0f };
	DirectX::XMFLOAT3 HitNormal = { 0.0f, 0.0f, 0.0f };
	std::string HitObjectName = "";
	float Distance = 0.0f;
};