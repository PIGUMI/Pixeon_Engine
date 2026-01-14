// リソース管理サービス
#ifndef RESOURCE_SERVICE_H
#define RESOURCE_SERVICE_H

#include <string>
#include <memory>
#include "AssetTypes.h"

class ResourceService {
public:
	static ResourceService& Instance();
	static void DeleteInstance();
	std::shared_ptr<TextureResource> GetTexture(const std::string& name);
	std::shared_ptr<ModelSharedResource> GetModel(const std::string& name);
	std::shared_ptr<SoundResource> GetSound(const std::string& name, bool streaming = false);

	bool AutoResolve(const std::string& name);

private:
	ResourceService() = default;
	static ResourceService* _instance;
};

#endif // RESOURCE_SERVICE_H