#include "Game/App.h"
#include "Engine/Core/EventSystem.hpp"
#include "Engine/Renderer/Renderer.h"
#include "Engine/Renderer/Camera.h"
#include "Engine/Window/Window.hpp"
#include "Engine/Input/InputSystem.h"
#include "Engine/Audio/AudioSystem.hpp"
#include "Engine/Core/Vertex_PCU.h"
#include "Engine/Core/Time.hpp"
#include "Engine/Core/EngineCommon.h"
#include "Engine/Core/DevConsole.hpp"
#include "Engine/Core/Clock.hpp"
#include "Engine/Core/DebugRender.hpp"

RandomNumberGenerator* g_rng = nullptr; // Created and owned by the App
App* g_theApp = nullptr;				// Created and owned by Main_Windows.cpp
Game* g_theGame = nullptr;				// Owns the game instance
Renderer* g_theRenderer = nullptr;		// Created and owned by the App
AudioSystem* g_theAudio = nullptr;		// Created and owned by the App
Window* g_theWindow = nullptr;			// Created and owned by the App


App::App()
{
}

App::~App()
{
}

void App::Startup()
{
	// Create all Engine Subsystems
	EventSystemConfig eventSystemConfig;
	g_theEventSystem = new EventSystem(eventSystemConfig);

	InputSystemConfig inputConfig;
	g_theInput = new InputSystem(inputConfig);

	WindowConfig windowConfig;
	windowConfig.m_isWindowFullScreen = true;
	windowConfig.m_aspectRatio = 2.f;
	windowConfig.m_inputSystem = g_theInput;
	windowConfig.m_windowTitle = "NPCLife";
	g_theWindow = new Window(windowConfig);

	RendererConfig rendererConfig;
	rendererConfig.m_window = g_theWindow;
	g_theRenderer = new Renderer(rendererConfig);

	DevConsoleConfig devConsoleConfig;
	devConsoleConfig.m_renderer = g_theRenderer;
	devConsoleConfig.m_fontName = "Data/Fonts/SquirrelFixedFont";
	Camera* devConsoleCamera = new Camera();
	devConsoleCamera->SetOrthoView(Vec2::ZERO, Vec2(SCREEN_SIZE_X, SCREEN_SIZE_Y));
	devConsoleConfig.m_camera = devConsoleCamera;
	g_theDevConsole = new DevConsole(devConsoleConfig);

	g_theEventSystem->Startup();
	g_theDevConsole->Startup();
	g_theInput->Startup();
	g_theWindow->Startup();
	g_theRenderer->Startup();

	DebugRenderConfig debugRenderConfig;
	debugRenderConfig.m_renderer = g_theRenderer;
	debugRenderConfig.m_fontName = "Data/Fonts/SquirrelFixedFont";
	DebugRenderSystemStartup(debugRenderConfig);

	g_theGame = new Game(this);
	g_theGame->StartUp();

	SubscribeToEvents();
}

void App::Shutdown()
{
	g_theGame->Shutdown();
	delete g_theGame;
	g_theGame = nullptr;

	DebugRenderSystemShutdown();

	g_theRenderer->Shutdown();
	g_theWindow->Shutdown();
	g_theInput->Shutdown();
	g_theDevConsole->Shutdown();
	g_theEventSystem->Shutdown();

	delete g_theRenderer;
	delete g_theEventSystem;
	delete g_theWindow;
	delete g_theInput;
	delete g_theDevConsole;

	g_theRenderer = nullptr;
	g_theEventSystem = nullptr;
	g_theWindow = nullptr;
	g_theInput = nullptr;
	g_theDevConsole = nullptr;
}

void App::BeginFrame()
{
	Clock::TickSystemClock();

	g_theRenderer->BeginFrame();
	g_theEventSystem->BeginFrame();
	g_theWindow->BeginFrame();
	g_theInput->BeginFrame();
	g_theDevConsole->BeginFrame();

	DebugRenderBeginFrame();
}

void App::Render() const
{
	g_theRenderer->ClearScreen(Rgba8(150, 150, 150, 255));
	g_theGame->Render();
	g_theDevConsole->Render(AABB2(Vec2::ZERO, Vec2(SCREEN_SIZE_X, SCREEN_SIZE_Y)));
}

void App::Update()
{
	if (g_theDevConsole->GetMode() == DevConsoleMode::OPEN_FULL || GetActiveWindow() != Window::s_mainWindow->GetHwnd())
	{
		g_theInput->SetCursorMode(CursorMode::POINTER);
	}
	else
	{
		g_theInput->SetCursorMode(CursorMode::FPS);
	}

	if (g_theInput->WasKeyJustPressed(KEYCODE_F8))
	{
		g_theGame->Shutdown();
		g_theGame->StartUp();
	}

	if (g_theInput->WasKeyJustPressed(KEYCODE_TILDE))
	{
		g_theDevConsole->ToggleMode(DevConsoleMode::OPEN_FULL);
	}

	g_theGame->Update();
}

void App::EndFrame()
{
	g_theEventSystem->EndFrame();
	g_theInput->EndFrame();
	g_theWindow->EndFrame();
	g_theRenderer->EndFrame();
	g_theDevConsole->EndFrame();

	DebugRenderEndFrame();
}

void App::SubscribeToEvents()
{
	SubscribeEventCallbackFunction("Quit", HandleQuitRequested);
}

void App::RunFrame()
{
	BeginFrame();	
	Update();		
	Render();		
	EndFrame();
}

void App::RunMainLoop()
{
	while (!IsQuitting())
	{
		RunFrame();
	}
}

bool App::HandleQuitRequested(EventArgs& args)
{
	UNUSED(args);
	g_theApp->m_isQuitting = true;
	return true;
}
