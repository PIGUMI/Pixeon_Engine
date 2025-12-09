#ifndef MATH_H
#define MATH_H
#include <DirectXMath.h>

bool XMFLOAT2Equal(const DirectX::XMFLOAT2& a, const DirectX::XMFLOAT2& b);
DirectX::XMFLOAT2 XMFLOAT2Add(const DirectX::XMFLOAT2& a, const DirectX::XMFLOAT2& b);
DirectX::XMFLOAT2 XMFLOAT2Subtract(const DirectX::XMFLOAT2& a, const DirectX::XMFLOAT2& b);
DirectX::XMFLOAT2 XMFLOAT2Multiply(const DirectX::XMFLOAT2& a, const DirectX::XMFLOAT2& b);
DirectX::XMFLOAT2 XMFLOAT2Multiply(const DirectX::XMFLOAT2& a, float b);

#endif // MATH_H