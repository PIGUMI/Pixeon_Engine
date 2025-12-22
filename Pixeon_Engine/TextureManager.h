#ifndef TEXTURE_MANAGER_H
#define TEXTURE_MANAGER_H

#include "AssetTypes.h"
#include <unordered_map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

class TextureManager {
public:
	static TextureManager* Instance();
	static void DeleteInstance();

	void UnInit();

	std::shared_ptr<TextureResource> LoadOrGet(const std::string& logicalName);

	void SetMemoryBudget(size_t bytes) { m_budget = bytes; }
	void GarbageCollect();
	void DrawDebugGUI();

	// GUI 用 API
	bool LoadTexture(const std::string& name);
	bool Reload(const std::string& name);
	bool RemoveFromCache(const std::string& name);
	bool IsLoaded(const std::string& name);
	bool Pin(const std::string& name);
	bool Unpin(const std::string& name);
	bool IsPinned(const std::string& name);

	// 追加: 失敗理由取得
	std::string GetLastFailReason(const std::string& name) const;

private:
	TextureManager() = default;
	std::shared_ptr<TextureResource> LoadInternal(const std::string& logicalName);
	void SetFail(const std::string& name, const std::string& reason);

	struct Entry {
		std::weak_ptr<TextureResource> weak;
		uint64_t lastUse = 0;
		size_t   bytes = 0;
	};
	std::unordered_map<std::string, Entry> m_cache;
	std::unordered_map<std::string, std::shared_ptr<TextureResource>> m_pinned;

	// 追加: 名前 -> 失敗理由
	std::unordered_map<std::string, std::string> m_failReasons;

	size_t    m_budget = 512ull * 1024 * 1024;
	uint64_t  m_frame = 0;
	mutable std::mutex m_mtx;

	static TextureManager* s_instance;
};

#endif // TEXTURE_MANAGER_H