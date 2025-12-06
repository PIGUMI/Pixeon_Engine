#include "Animator2DFrame.h"
#include "Input.h"
#include "TimelineEditor.h"

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
	timelineEditor_ = new TimelineEditor();
}

void Animator2DFrame::Update()
{
	// XVˆ—
	if(IsKeyPress(VK_CONTROL) && IsKeyTrigger('S'))
	{
		if (animator_)
		{
			animator_->SaveFile();
		}
	}
}

void Animator2DFrame::Draw()
{
	// •`‰æˆ—
}

void Animator2DFrame::UnInit()
{
	MessageBox(nullptr, "Animator2DFrame UnInit", "Info", MB_OK);
	if (animator_)
	{
		animator_->SaveFile();
		delete animator_;
		animator_ = nullptr;
	}
	if (timelineEditor_)
	{
		delete timelineEditor_;
		timelineEditor_ = nullptr;
	}
}

