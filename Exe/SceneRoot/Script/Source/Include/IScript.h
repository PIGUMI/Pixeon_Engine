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
	virtual void Update();
	virtual void EndPlay();

	virtual void CallCustom(const std::string& functionName);
	void SetParentObject(Object obj) { _parentObject = obj; }
protected:
	Object _parentObject = nullptr;
};

#endif // _ISCRIPT_H_
