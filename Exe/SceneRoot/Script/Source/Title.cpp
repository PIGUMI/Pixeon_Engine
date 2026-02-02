#include "Title.h"
#include <Windows.h>

void Script_Title::BeginPlay() {
    IScript::BeginPlay();
	FindObjectByName(_parentScene,"Select", &_SelectObject);
}

void Script_Title:: Update(float DeltaTime) {
    IScript::Update(DeltaTime);// Update
    if (KeyPressed(VK_UP) || KeyPressed('W')) {
        SelectIndex--;
        if (SelectIndex < 0) {
            SelectIndex = 0;
		}
		Float3 pos = { -5.0f, 1.5f, 0.0f };
		SetObjectPosition(_SelectObject,pos);
    }
    if (KeyPressed(VK_DOWN) || KeyPressed('S')) {
        SelectIndex++;
        if (SelectIndex > 1) {
            SelectIndex = 1;
        }
		Float3 pos = { -5.0f, -1.5f, 0.0f };
		SetObjectPosition(_SelectObject,pos);
    }
    if (KeyTriggered(VK_RETURN))
    {
        switch (SelectIndex)
        {
        case 0:
			ChangeScene(Solo.c_str());
			break;
		case 1:
			ChangeScene(Multi.c_str());
			break;
        }
    }
}

void Script_Title::EndPlay() {
    IScript::EndPlay();// EndPlay
}
