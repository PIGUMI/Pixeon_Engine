#ifndef __INPUT_H__
#define __INPUT_H__

#include <Windows.h>
#undef max
#undef min

HRESULT InitInput();
void UninitInput();
void UpdateInput(HWND hWnd);

bool IsKeyPress(BYTE key);
bool IsKeyTrigger(BYTE key);
bool IsKeyRelease(BYTE key);
bool IsKeyRepeat(BYTE key);

int MouseMoveX();
int MouseMoveY();

void SetMouseFreeze(bool bFreeze);
float GetMouseSensitivity();

#endif // __INPUT_H__