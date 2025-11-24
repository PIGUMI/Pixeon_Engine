#pragma once
#include <unordered_map>
#include <string>
#include "AssetTypes.h"

// クリップ階層から bone.nodeIndex を一括設定
inline void MapBonesToNodes(ModelSharedResource& res) {
	if (res.clips.empty() || res.bones.empty()) return;
	// 最初のクリップの階層を利用（全クリップ同一前提）
	const auto& hierarchy = res.clips[0].nodeHierarchy;
	std::unordered_map<std::string, int> map;
	for (int i = 0; i < (int)hierarchy.size(); ++i)
		map[hierarchy[i].name] = i;

	int missing = 0;
	for (auto& b : res.bones) {
		auto it = map.find(b.name);
		if (it != map.end()) b.nodeIndex = it->second;
		else { b.nodeIndex = -1; ++missing; }
	}
#ifdef _DEBUG
	char buf[128];
	sprintf_s(buf, "[BoneNodeMapping] missing=%d / total=%d\n", missing, (int)res.bones.size());
	OutputDebugStringA(buf);
#endif
}