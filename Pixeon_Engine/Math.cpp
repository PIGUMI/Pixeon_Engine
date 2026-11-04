#include "Math.h"

bool XMFLOAT2Equal(const DirectX::XMFLOAT2& a, const DirectX::XMFLOAT2& b)
{
	return (a.x == b.x) && (a.y == b.y);
}

DirectX::XMFLOAT2 XMFLOAT2Add(const DirectX::XMFLOAT2& a, const DirectX::XMFLOAT2& b)
{
	return DirectX::XMFLOAT2(a.x + b.x, a.y + b.y);
}

DirectX::XMFLOAT2 XMFLOAT2Subtract(const DirectX::XMFLOAT2& a, const DirectX::XMFLOAT2& b)
{
	return DirectX::XMFLOAT2(a.x - b.x, a.y - b.y);
}

DirectX::XMFLOAT2 XMFLOAT2Multiply(const DirectX::XMFLOAT2& a, const DirectX::XMFLOAT2& b)
{
	return DirectX::XMFLOAT2(a.x * b.x, a.y * b.y);
}

DirectX::XMFLOAT2 XMFLOAT2Multiply(const DirectX::XMFLOAT2& a, float b)
{
	return DirectX::XMFLOAT2(a.x * b, a.y * b);
}
