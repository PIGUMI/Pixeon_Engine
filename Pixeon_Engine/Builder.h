#pragma once
class Builder
{
public:
	Builder* GetInstance();
	void DeleteInstance();

	int Build();
private:
	Builder() {}
	~Builder() {}
private:
	static Builder* _instance;
};

