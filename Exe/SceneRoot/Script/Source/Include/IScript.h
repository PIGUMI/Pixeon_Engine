#ifndef _ISCRIPT_H_
#define _ISCRIPT_H_

//　Scriptのインターフェースクラス
#include <string>
#include "API.h"

class IScript
{
public:
	virtual ~IScript() = default;
	virtual void BeginPlay();
	virtual void Update(float DeltaTime);
	virtual void EndPlay();

	virtual void CallCustom(const std::string& functionName);
	void SetParentObject(Object obj) { _parentObject = obj; }

public :
	virtual void OnCollisionEnter(const APICollisionInfo* info);
	virtual void OnCollisionStay(const APICollisionInfo* info);
	virtual void OnCollisionExit(const APICollisionInfo* info);
protected:
	Object _parentObject = nullptr;
};

#endif // _ISCRIPT_H_
