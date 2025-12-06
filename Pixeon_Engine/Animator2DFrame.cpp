#include "Animator2DFrame.h"
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
	// çXêVèàóù
}

void Animator2DFrame::Draw()
{
	// ï`âÊèàóù
}

void Animator2DFrame::UnInit()
{
	if (timelineEditor_)
	{
		delete timelineEditor_;
		timelineEditor_ = nullptr;
	}
}

