#ifndef __INPUT_H__
#define __INPUT_H__

#include <Windows.h>
#undef max
#undef min

HRESULT InitInput();
void UninitInput();
void UpdateInput(HWND hWnd);

void OnMouseWheel(short delta);
void OnMouseHWheel(short delta);

bool IsKeyPress(BYTE key);
bool IsKeyTrigger(BYTE key);
bool IsKeyRelease(BYTE key);
bool IsKeyRepeat(BYTE key);

int MouseMoveX();
int MouseMoveY();


int MouseWheelDelta();
float MouseWheel();

int MouseWheelForwardDelta();
int MouseWheelBackwardDelta();
float MouseWheelForward();
float MouseWheelBackward();

int MouseHWheelDelta();
float MouseHWheel();

int MouseHWheelForwardDelta();
int MouseHWheelBackwardDelta();
float MouseHWheelForward();
float MouseHWheelBackward();

float GetMousePositionX();
float GetMousePositionY();

void SetMouseFreeze(bool bFreeze);
float GetMouseSensitivity();

#endif // __INPUT_H__