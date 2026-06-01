#pragma once
#include "Game/Game.h"
#include "Engine/Math/Vec2.hpp"
#include "Engine/Core/EventSystem.hpp"

class App
{
public:
	App();
	~App();
	void Startup();
	void Shutdown();
	void RunFrame();

	void RunMainLoop();
	bool IsQuitting() const { return m_isQuitting; }
	static bool HandleQuitRequested(EventArgs& args);
	
private:
	void BeginFrame();
	void Update();
	void Render() const;
	void EndFrame();

	void SubscribeToEvents();

private:
	bool  m_isQuitting = false;
};