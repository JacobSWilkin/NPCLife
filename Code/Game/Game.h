#pragma once
#include "Game/GameCommon.h"
#include "Engine/Renderer/Camera.h"
#include "Engine/Core/Clock.hpp"
#include "Engine/Core/Rgba8.h"
// -----------------------------------------------------------------------------
class Map;
class Character;
class ConstantBuffer;
class Shader;
// -----------------------------------------------------------------------------
class Game
{
public:
	App* m_app;
	Game(App* owner);
	~Game();
	void StartUp();

	void Update();
	void UpdateCameras(float deltaSeconds);
	void UpdateTimeOfDay(float deltaSeconds);
	void FreeFlyCameraControls(float deltaSeconds);
	void AdjustForPauseAndTimeDistortion(float deltaSeconds);

	void Render() const;
	void SetWorldConstants();

	void Shutdown();

public:
	float GetTimeOfDay() const;

public:
	AABB2 m_gameScreenBox = AABB2::ZERO_TO_ONE;
	Vec3 m_cameraPos = Vec3::ZERO;
	EulerAngles m_cameraOrientation = EulerAngles::ZERO;

	ConstantBuffer* m_worldCBO = nullptr;
	Shader* m_worldShader = nullptr;

private:
	Camera		m_screenCamera;
	Camera      m_gameWorldCamera;
	Clock		m_gameClock;

	Map* m_theMap = nullptr;
	float m_timeOfDay = 8.f;
	Rgba8 m_skyColor = Rgba8::WHITE;
};