#include "Animator2DFrame.h"

Animator2DFrame* Animator2DFrame::instance = nullptr;

Animator2DFrame* Animator2DFrame::GetInstance()
{
	if (instance == nullptr)
	{
		instance = new Animator2DFrame();
	}
	return instance;
}

void Animator2DFrame::DestroyInstance()
{
	if (instance != nullptr)
	{
		delete instance;
		instance = nullptr;
	}
}


void Animator2DFrame::Init()
{
	// ‰Šú‰»ˆ—
}

void Animator2DFrame::Update()
{
	// XVˆ—
}

void Animator2DFrame::Draw()
{
	// •`‰æˆ—
}

void Animator2DFrame::UnInit()
{
	// I—¹ˆ—
}

