#include "AnimationDebug.h"

void DumpBoneChannelMapping(const ModelSharedResource* res) {
	if (!res || res->clips.empty()) { return; }
	const auto& clip = res->clips[0]; // 代表クリップ
	const auto& hierarchy = clip.nodeHierarchy;

	for (size_t b = 0; b < res->bones.size(); ++b) {
		const auto& bone = res->bones[b];
		std::string hname = (bone.nodeIndex >= 0 && bone.nodeIndex < (int)hierarchy.size())
			? hierarchy[bone.nodeIndex].name : "<OUT_OF_RANGE>";
		std::string status = (bone.nodeIndex >= 0 && hname == bone.name) ? "OK" : "Mismatch";
	}

	for (size_t ci = 0; ci < res->clips.size(); ++ci) {
		const auto& c = res->clips[ci];
		// nodeName -> index map
		std::unordered_map<std::string, int> nodeMap;
		for (size_t ni = 0; ni < c.nodeHierarchy.size(); ++ni)
			nodeMap[c.nodeHierarchy[ni].name] = (int)ni;

		for (size_t ch = 0; ch < c.channels.size(); ++ch) {
			const auto& chan = c.channels[ch];
			int expected = -1;
			auto it = nodeMap.find(chan.nodeName);
			if (it != nodeMap.end()) expected = it->second;
			std::string hname = (chan.nodeIndex >= 0 && chan.nodeIndex < (int)c.nodeHierarchy.size())
				? c.nodeHierarchy[chan.nodeIndex].name : "<OUT_OF_RANGE>";
			std::string status = (expected == chan.nodeIndex) ? "OK" : "Mismatch";
		}
	}
}

int RebindBoneNodeIndices(ModelSharedResource* res) {
	if (!res || res->clips.empty()) return 0;
	// 代表クリップの階層（すべて同一構造前提）
	const auto& hierarchy = res->clips[0].nodeHierarchy;
	std::unordered_map<std::string, int> map;
	for (size_t i = 0; i < hierarchy.size(); ++i)
		map[hierarchy[i].name] = (int)i;

	int fixed = 0;
	for (auto& b : res->bones) {
		auto it = map.find(b.name);
		if (it != map.end() && b.nodeIndex != it->second) {
			b.nodeIndex = it->second;
			fixed++;
		}
	}
	return fixed;
}

int RebindChannelNodeIndices(ModelSharedResource* res, AnimationClipRuntime& runtimeClip) {
	// runtimeClip は AnimationComponent 内の m_clips[?]
	// その runtimeClip は Resource 側 clips[同 index] と 1:1 対応している前提
	if (!res) return 0;
	int clipIdx = -1;
	for (size_t i = 0; i < res->clips.size(); ++i)
		if (res->clips[i].name == runtimeClip.name) { clipIdx = (int)i; break; }
	if (clipIdx < 0) return 0;

	const auto& srcClip = res->clips[clipIdx];
	std::unordered_map<std::string, int> nodeMap;
	for (size_t i = 0; i < srcClip.nodeHierarchy.size(); ++i)
		nodeMap[srcClip.nodeHierarchy[i].name] = (int)i;

	int fixed = 0;
	// runtimeClip.channels は nodeIndex と timeline を持つが nodeName は保持していないので
	// Resource clip の channel を参照して nodeName を取る
	for (size_t ch = 0; ch < runtimeClip.channels.size() && ch < srcClip.channels.size(); ++ch) {
		const std::string& nodeName = srcClip.channels[ch].nodeName;
		auto it = nodeMap.find(nodeName);
		if (it != nodeMap.end() && runtimeClip.channels[ch].nodeIndex != it->second) {
			runtimeClip.channels[ch].nodeIndex = it->second;
			fixed++;
		}
	}
	return fixed;
}

void QuickIntegrityReport(const ModelSharedResource* res) {
	if (!res) { return; }
	int boneMismatch = 0;
	if (!res->clips.empty()) {
		const auto& h = res->clips[0].nodeHierarchy;
		for (auto& b : res->bones) {
			if (b.nodeIndex < 0 || b.nodeIndex >= (int)h.size() || h[b.nodeIndex].name != b.name)
				boneMismatch++;
		}
	}
}