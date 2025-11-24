#include "ResourceService.h"
#include "TextureManager.h"
#include "ModelManager.h"
#include "SoundManager.h"

ResourceService* ResourceService::s_instance = nullptr;

ResourceService& ResourceService::Instance() {
	if (!s_instance) {
		s_instance = new ResourceService();
	}
	return *s_instance;
}

void ResourceService::DeleteInstance() {
	if (s_instance) {
		delete s_instance;
		s_instance = nullptr;
	}
}

std::shared_ptr<TextureResource> ResourceService::GetTexture(const std::string& name) {
	return TextureManager::Instance()->LoadOrGet(name);
}
std::shared_ptr<ModelSharedResource> ResourceService::GetModel(const std::string& name) {
	return ModelManager::Instance()->LoadOrGet(name);
}
std::shared_ptr<SoundResource> ResourceService::GetSound(const std::string& name, bool streaming) {
	return SoundManager::Instance()->LoadOrGet(name, streaming);
}

bool ResourceService::AutoResolve(const std::string& name) {
	std::string ext;
	if (auto p = name.find_last_of('.'); p != std::string::npos)
		ext = name.substr(p + 1);
	for (auto& c : ext) c = (char)tolower(c);

	if (ext == "png" || ext == "jpg" || ext == "jpeg" || ext == "dds" || ext == "tga")
		return (bool)GetTexture(name);
	if (ext == "fbx" || ext == "obj" || ext == "gltf" || ext == "glb")
		return (bool)GetModel(name);
	if (ext == "wav" || ext == "mp3" || ext == "ogg")
		return (bool)GetSound(name);
	return false;
}