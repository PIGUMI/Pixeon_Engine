#pragma once
#include "Component.h"
#include "ModelManager.h"
#include "ModelRender.h"
#include <DirectXMath.h>
#include <vector>
#include <string>
#include <map>
#include <unordered_map>

struct BoneTransform {
	DirectX::XMFLOAT3 position{ 0,0,0 };
	DirectX::XMFLOAT4 rotation{ 0,0,0,1 };
	DirectX::XMFLOAT3 scale{ 1,1,1 };
	bool isValid = true;
};

struct AnimationChannelRuntime {
	int nodeIndex = -1;
	std::map<float, BoneTransform> timeline;
};

struct AnimationClipRuntime {
	std::string name;
	double duration = 0.0;
	double tps = 25.0;
	std::vector<AnimationChannelRuntime> channels;
	bool isLoaded = false;
};

class AnimationComponent : public AbstractComponent {
public:
	void Init(AbstractObject* owner) override;
	void BeginPlay() override;
	void EditUpdate() override;
	void InGameUpdate() override;
	void DrawInspector() override;

	void Play();
	void Pause();
	void Resume();
	void Stop();
	void Restart();

	bool SetAnimationClip(int clipIndex);
	void SetPlaybackSpeed(float s) { m_speed = s; }
	void SetLoop(bool b) { m_loop = b; }

	bool IsPlaying() const;
	bool IsPaused()  const;
	bool IsFinished() const;
	float GetAnimationProgress() const;

	void SaveToFile(std::ostream& out) override;
	void LoadFromFile(std::istream& in) override;

	void AddAnimationFile(const std::string& path);
	void ClearAnimationFiles();

	bool SetAnimationByExternalIndex(int externalFileIndex, int clipIndexInFile);
	int  GetExternalFileCount() const { return (int)m_externalAnimationFiles.size(); }
	int  GetClipCountInExternalFile(int externalFileIndex) const;

private:
	void UpdateAnimation(float dt);
	void RebuildBoneMatrices();
	void ApplyToModel();

	BoneTransform InterpChannel(const AnimationChannelRuntime& ch, float t) const;
	DirectX::XMMATRIX BuildMatrix(const BoneTransform& bt) const;

	void BuildClipPose(const AnimationClipRuntime& rtClip, float time, std::vector<DirectX::XMFLOAT4X4>& outFinal);

	bool IsValidMatrix(const DirectX::XMMATRIX& m) const;

	void RebuildAnimationClips();
	void LazyLoadClip(int clipIndex);

	void EnsureLinked();

	ModelRenderComponent* GetRenderer();
	std::shared_ptr<ModelSharedResource> GetResource();

private:
	ModelRenderComponent* m_renderer = nullptr;
	std::shared_ptr<ModelSharedResource> m_resource;

	std::vector<AnimationClipRuntime> m_clips;

	struct ClipOrigin {
		enum class Source { Model, External } source = Source::Model;
		int externalFileIndex = -1;
		int clipIndexInSource = -1;
	};
	std::vector<ClipOrigin> m_clipOrigins;

	int   m_currentClip = -1;
	bool  m_playing = false;
	bool  m_paused = false;
	bool  m_loop = true;
	float m_speed = 1.0f;
	float m_time = 0.0f;

	std::vector<DirectX::XMFLOAT4X4> m_boneMatrices;

	std::unordered_map<std::string, int> m_nodeToBone;

	std::vector<std::string> m_externalAnimationFiles;

	int m_skeletonClipIndex = 0;

	std::string m_linkedModelPath;

	std::vector<std::string> m_cachedFBXList;
	std::vector<int> m_filteredIndices;
	char m_filterBuffer[128] = "";
	int m_highlightIndex = -1;
	int m_extIdxUI = 0;
	int m_clipIdxUI = 0;

	bool m_clipNamesNeedUpdate = true;
	std::vector<std::string> m_clipNamesCache;
	std::vector<const char*> m_clipNamePtrs;
};