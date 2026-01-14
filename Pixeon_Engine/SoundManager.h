// サウンドリソースの管理

#ifndef SOUND_MANAGER_H
#define SOUND_MANAGER_H

#include "AssetTypes.h"
#include <unordered_map>
#include <memory>
#include <mutex>

class SoundManager {
public:
	static SoundManager* Instance();
	static void DeleteInstance();
	void UnInit();
	std::shared_ptr<SoundResource> LoadOrGet(const std::string& logicalName, bool streaming = false);
	void GarbageCollect();
	void DrawDebugGUI();
private:
	SoundManager() = default;
	std::shared_ptr<SoundResource> LoadInternal(const std::string& logicalName, bool streaming);

	struct Entry { std::weak_ptr<SoundResource> weak; uint64_t lastUse = 0; size_t bytes = 0; bool streaming = false; };
	std::unordered_map<std::string, Entry> _cache;
	uint64_t _frame = 0;
	std::mutex _mtx;

	static SoundManager* _instance;
};

#endif // SOUND_MANAGER_H