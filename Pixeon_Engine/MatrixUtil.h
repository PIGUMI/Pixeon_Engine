#pragma once
#include <assimp/matrix4x4.h>
#include <DirectXMath.h>

inline DirectX::XMMATRIX AssimpToXM_RowMajor(const aiMatrix4x4& m)
{
	return DirectX::XMMatrixSet(
		m.a1, m.b1, m.c1, m.d1,
		m.a2, m.b2, m.c2, m.d2,
		m.a3, m.b3, m.c3, m.d3,
		m.a4, m.b4, m.c4, m.d4
	);
}