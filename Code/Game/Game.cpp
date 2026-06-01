#include "Game/Game.h"
#include "Game/GameCommon.h"
#include "Game/App.h"
#include "Game/CharacterDefinition.hpp"
#include "Game/Map.hpp"
#include "Game/Character.hpp"

#include "Engine/Input/InputSystem.h"
#include "Engine/Renderer/Renderer.h"
#include "Engine/Window/Window.hpp"
#include "Engine/Audio/AudioSystem.hpp"
#include "Engine/Core/Rgba8.h"
#include "Engine/Core/Vertex_PCU.h"
#include "Engine/Core/EngineCommon.h"
#include "Engine/Core/DevConsole.hpp"
#include "Engine/Core/Time.hpp"
#include "Engine/Core/VertexUtils.h"
#include "Engine/Core/DebugRender.hpp"
#include "Engine/Math/MathUtils.h"
#include "Engine/Math/AABB3.hpp"
#include "ThirdParty/ImGui/imgui.h"

Game::Game(App* owner)
	: m_app(owner)
{
}

Game::~Game()
{
}

void Game::StartUp()
{
	// Write control interface into devconsole
	g_theDevConsole->AddLine(Rgba8::CYAN, "Welcome to NPCLife!");
	g_theDevConsole->AddLine(Rgba8::SEAWEED, "----------------------------------------------------------------------");
	g_theDevConsole->AddLine(Rgba8::CYAN, "CONTROLS:");
	g_theDevConsole->AddLine(Rgba8::LIGHTYELLOW, "ESC   - Quits the game");
	g_theDevConsole->AddLine(Rgba8::LIGHTYELLOW, "SHIFT - Increase speed by factor of 10.");
	g_theDevConsole->AddLine(Rgba8::LIGHTYELLOW, "A/D   - Move left/right");
	g_theDevConsole->AddLine(Rgba8::LIGHTYELLOW, "W/S   - Move forward/backward");
	g_theDevConsole->AddLine(Rgba8::LIGHTYELLOW, "Z/C   - Move down/up");
	g_theDevConsole->AddLine(Rgba8::LIGHTYELLOW, "Y     - Increase time scale by 50x");
	g_theDevConsole->AddLine(Rgba8::LIGHTYELLOW, "1     - Toggle character physics cylinder");
	g_theDevConsole->AddLine(Rgba8::LIGHTYELLOW, "2     - Toggle character forward raycast");
	g_theDevConsole->AddLine(Rgba8::LIGHTYELLOW, "B     - Toggle building see through");
	g_theDevConsole->AddLine(Rgba8::LIGHTYELLOW, "G     - Visualize character path and goal");
	g_theDevConsole->AddLine(Rgba8::SEAWEED, "----------------------------------------------------------------------");

	// Initialize the Character Definitions
	CharacterDefinition::InitializeCharacterDefintions();

	// Get world shader and constant buffer
	m_worldShader = g_theRenderer->CreateOrGetShader("Data/Shaders/WorldShader", VertexType::VERTEX_PCUTBN);
	m_worldCBO = g_theRenderer->CreateConstantBuffer(sizeof(WorldConstants));

	// Create map and characters
	m_theMap = new Map(this);

	// Set camera position and game screen
	m_cameraPos = Vec3(0.f, 20.f, 5.f);
	m_gameScreenBox = AABB2(Vec2::ZERO, Vec2(SCREEN_SIZE_X, SCREEN_SIZE_Y));

	// Adding a plus crosshair with infinite duration
	DebugAddScreenText("+", m_gameScreenBox, 20.f, Vec2::ONEHALF, -1.f);
}

void Game::Update()
{
	// Setting clock time variables
	double deltaSeconds = m_gameClock.GetDeltaSeconds();
	double totalTime    = Clock::GetSystemClock().GetTotalSeconds();
	double frameRate    = Clock::GetSystemClock().GetFrameRate();

	//ImGui::Begin("Debug Controls");

	//if (ImGui::Button("Toggle Physics Cylinder"))
	//{
	//	m_debugDrawCollisionCylinder = !m_debugDrawCollisionCylinder;
	//}

	//if (ImGui::Button("Toggle Skeleton"))
	//{
	//	m_debugDrawCharacterSkeleton = !m_debugDrawCharacterSkeleton;
	//}

	//ImGui::Text("Physics Cylinder: %s", m_debugDrawCollisionCylinder ? "ON" : "OFF");
	//ImGui::Text("Skeleton: %s", m_debugDrawCharacterSkeleton ? "ON" : "OFF");

	//ImGui::End();

	// Set text for time, FPS, and scale
	std::string timeScaleText = Stringf("Time: %0.2fs FPS: %0.2f", totalTime, GetClamped(frameRate, 0.0, 60.0));
	DebugAddScreenText(timeScaleText, m_gameScreenBox, 15.f, Vec2(0.98f, 0.97f), 0.f);
	DebugAddScreenText(Stringf("Current Time of day: %0.2fs", m_timeOfDay), m_gameScreenBox, 15.f, Vec2(0.45f, 0.97f), 0.f);

	// Update day time and map
	UpdateTimeOfDay(static_cast<float>(deltaSeconds));
	if (m_theMap)
	{
		m_theMap->Update(static_cast<float>(deltaSeconds));
	}

	AdjustForPauseAndTimeDistortion(static_cast<float>(deltaSeconds));
	UpdateCameras(static_cast<float>(deltaSeconds));
}

void Game::Render() const
{
	g_theRenderer->BeginCamera(m_screenCamera);
	g_theRenderer->EndCamera(m_screenCamera);
	g_theRenderer->BeginCamera(m_gameWorldCamera);
	g_theGame->SetWorldConstants();
	g_theRenderer->BindShader(m_worldShader);
	g_theRenderer->ClearScreen(m_skyColor);
	m_theMap->Render();
	g_theRenderer->EndCamera(m_gameWorldCamera);

	DebugRenderWorld(m_gameWorldCamera);
	DebugRenderScreen(m_screenCamera);
}

void Game::SetWorldConstants()
{
	WorldConstants worldConstants;
	worldConstants.CameraPosition = Vec4(m_cameraPos, 1);

	float timeOfDay = GetTimeOfDay();

	// Sky color
	Rgba8 nightSky(20, 20, 40, 255);
	Rgba8 daySky(200, 230, 255, 255);

	// Map daytime
	float dayFactor = 0.f;
	if (timeOfDay >= 6.f && timeOfDay <= 18.f)
	{
		dayFactor = RangeMapClamped(timeOfDay, 6.f, 18.f, 0.f, 1.f);
		dayFactor = 1.f - fabsf((timeOfDay - 12.f) / 6.f);
	}

	Vec4 nightSkyAsVec4 = nightSky.GetAsVec4();
	Vec4 daySkyAsVec4 = daySky.GetAsVec4();
	Vec4 skyColor = Interpolate(nightSkyAsVec4, daySkyAsVec4, dayFactor);
	Rgba8 skyColorRGBA = Rgba8(
		static_cast<unsigned char>((skyColor.x * 255.f)),
		static_cast<unsigned char>((skyColor.y * 255.f)),
		static_cast<unsigned char>((skyColor.z * 255.f)),
		static_cast<unsigned char>((skyColor.w * 255.f))
	);
	m_skyColor = skyColorRGBA;

	worldConstants.SkyColor = skyColor;

	g_theRenderer->CopyCPUToGPU((void*)&worldConstants, sizeof(worldConstants), m_worldCBO);
	g_theRenderer->BindConstantBuffer(4, m_worldCBO);
}

void Game::Shutdown()
{
	// Delete the map
	delete m_theMap;
	m_theMap = nullptr;

	// Clear out character definitions
	CharacterDefinition::ClearCharacterDefinitions();

	// Delete world constant buffer
	delete m_worldCBO;
	m_worldCBO = nullptr;
}

float Game::GetTimeOfDay() const
{
	return m_timeOfDay;
}

void Game::AdjustForPauseAndTimeDistortion(float deltaSeconds) {

	UNUSED(deltaSeconds);

	if (g_theInput->IsKeyDown('T'))
	{
		m_gameClock.SetTimeScale(0.1);
	}
	else if (g_theInput->IsKeyDown('Y'))
	{
		m_gameClock.SetTimeScale(50.0);
	}
	else
	{
		m_gameClock.SetTimeScale(1.0);
	}

	if (g_theInput->WasKeyJustPressed('P'))
	{
		m_gameClock.TogglePause();
	}

	if (g_theInput->WasKeyJustPressed('O'))
	{
		m_gameClock.StepSingleFrame();
	}

	if (g_theInput->WasKeyJustPressed(KEYCODE_ESC))
	{
		g_theEventSystem->FireEvent("Quit");
	}
}

void Game::UpdateCameras(float deltaSeconds)
{
	// Update Screen camera
	m_screenCamera.SetOrthoView(m_gameScreenBox);

	// Update game perspective camera
	Mat44 cameraToRender(Vec3::ZAXE, -Vec3::XAXE, Vec3::YAXE, Vec3::ZERO);
	m_gameWorldCamera.SetCameraToRenderTransform(cameraToRender);

	FreeFlyCameraControls(deltaSeconds);

	m_cameraOrientation.m_pitchDegrees = GetClamped(m_cameraOrientation.m_pitchDegrees, -85.f, 85.f);
	m_cameraOrientation.m_rollDegrees = GetClamped(m_cameraOrientation.m_rollDegrees, -45.f, 45.f);

	m_gameWorldCamera.SetPositionAndOrientation(m_cameraPos, m_cameraOrientation);

	m_gameWorldCamera.SetPerspectiveView(2.f, 60.f, 0.1f, 500.f);
}

void Game::UpdateTimeOfDay(float deltaSeconds)
{
	m_timeOfDay += (deltaSeconds / DAYTIME_SCALE);

	if (m_timeOfDay >= 24.f)
	{
		m_timeOfDay -= 24.f;
	}
}

void Game::FreeFlyCameraControls(float deltaSeconds)
{
	UNUSED(deltaSeconds);

	// Yaw and Pitch with mouse
	m_cameraOrientation.m_yawDegrees += 0.08f * g_theInput->GetCursorClientDelta().x;
	m_cameraOrientation.m_pitchDegrees -= 0.08f * g_theInput->GetCursorClientDelta().y;

	float movementSpeed = 3.f;
	// Increase speed by a factor of 10
	if (g_theInput->IsKeyDown(KEYCODE_SHIFT))
	{
		movementSpeed *= 10.f;
	}

	// Move left or right
	if (g_theInput->IsKeyDown('A'))
	{
		m_cameraPos += movementSpeed * m_cameraOrientation.GetAsMatrix_IFwd_JLeft_KUp().GetJBasis3D() * static_cast<float>(Clock::GetSystemClock().GetDeltaSeconds());
	}
	if (g_theInput->IsKeyDown('D'))
	{
		m_cameraPos += -movementSpeed * m_cameraOrientation.GetAsMatrix_IFwd_JLeft_KUp().GetJBasis3D() * static_cast<float>(Clock::GetSystemClock().GetDeltaSeconds());
	}

	// Move Forward and Backward
	if (g_theInput->IsKeyDown('W'))
	{
		m_cameraPos += movementSpeed * m_cameraOrientation.GetAsMatrix_IFwd_JLeft_KUp().GetIBasis3D() * static_cast<float>(Clock::GetSystemClock().GetDeltaSeconds());
	}
	if (g_theInput->IsKeyDown('S'))
	{
		m_cameraPos += -movementSpeed * m_cameraOrientation.GetAsMatrix_IFwd_JLeft_KUp().GetIBasis3D() * static_cast<float>(Clock::GetSystemClock().GetDeltaSeconds());
	}

	// Move Up and Down
	if (g_theInput->IsKeyDown('Z'))
	{
		m_cameraPos += -movementSpeed * Vec3::ZAXE * static_cast<float>(Clock::GetSystemClock().GetDeltaSeconds());
	}
	if (g_theInput->IsKeyDown('C'))
	{
		m_cameraPos += movementSpeed * Vec3::ZAXE * static_cast<float>(Clock::GetSystemClock().GetDeltaSeconds());
	}
}
