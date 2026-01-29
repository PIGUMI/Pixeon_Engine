#pragma once
#include <string>

struct BuilderData
{
	std::string gameName;
	std::string startScene;
	std::string gameVersion;
};

class Builder
{
public:
	Builder* GetInstance();
	void DeleteInstance();
	void Init();
	int Build();

	void SetBuilderData(const BuilderData& data) { _builderData = data; }
	BuilderData GetBuilderData() const { return _builderData; }
private:
	Builder() {}
	~Builder() {}
private:
	static Builder* _instance;
	BuilderData _builderData;
};
