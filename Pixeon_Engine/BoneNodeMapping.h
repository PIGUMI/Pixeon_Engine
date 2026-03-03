#pragma once
#include <unordered_map>
#include <string>
#include "AssetTypes.h"

inline void MapBonesToNodes(ModelSharedResource& res) {
	if (res.clips.empty() || res.bones.empty()) return;

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
}