#pragma once
#include <vector>
#include "AssetTypes.h"

inline void FixZeroWeights(std::vector<ModelVertex>& vertices) {
	int fixed = 0;
	for (auto& v : vertices) {
		float sum = v.boneWeights[0] + v.boneWeights[1] + v.boneWeights[2] + v.boneWeights[3];
		if (sum < 1e-6f) {
			v.boneIndices[0] = 0;
			v.boneWeights[0] = 1.0f;
			for (int i = 1; i < 4; ++i) { v.boneIndices[i] = 0; v.boneWeights[i] = 0.f; }
			++fixed;
		}
	}
}