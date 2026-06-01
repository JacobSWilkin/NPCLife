#pragma once
#include "Engine/Math/RandomNumberGenerator.h"
#include "Engine/Math/Vec2.hpp"
#include "Engine/Math/Vec4.hpp"
#include "Engine/Math/IntVec2.h"
#include <string>
// -----------------------------------------------------------------------------
class App;
class Game;
class Renderer;
class InputSystem;
class AudioSystem;
class Window;
struct Rgba8;
// -----------------------------------------------------------------------------
constexpr float SCREEN_SIZE_X   = 1600.f;
constexpr float SCREEN_SIZE_Y   = 800.f;
constexpr float SCREEN_CENTER_X = SCREEN_SIZE_X / 2.f;
constexpr float SCREEN_CENTER_Y = SCREEN_SIZE_Y / 2.f;
constexpr float DAYTIME_SCALE   = 30.f;
// -----------------------------------------------------------------------------
constexpr int   MAP_HEIGHT        = 50;
constexpr int   MAP_WIDTH         = 80;
constexpr int   MAP_SIZE          = MAP_WIDTH * MAP_HEIGHT;
constexpr float TILE_SIZE         = 2.0f;
constexpr float TILE_THICKNESS    = 0.25f;
constexpr float MAP_GROUND_Z      = 0.0f;
// -----------------------------------------------------------------------------
constexpr float CHARACTER_SCALE        = 0.4f;
constexpr float CHARACTER_SIGHT_RADIUS = 64.f;
constexpr float CHARACTER_SIGHT_ANGLE  = 60.f;
constexpr float CHARACTER_RAYFWD_DIST  = 15.f;
constexpr float CHARACTER_CYCLE_RATE   = 6.f;
constexpr float CHARACTER_AMBIENT_LIGHTING = 0.65f;
constexpr float CHARACTER_SUN_LIGHTING = 0.45f;
constexpr float ARRIVAL_THRESHOLD      = 0.05f;
constexpr unsigned int MAX_CHARACTER_VERTS = 10000;
constexpr int   MAX_CHARACTER_SPAWN_ATTEMPTS = 50;
// -----------------------------------------------------------------------------
constexpr float ROAD_MAX_CONNECTION_DIST = 75.f;
constexpr float ROAD_CONNECTION_CHANCE = 0.56f;
// -----------------------------------------------------------------------------
constexpr float DOORKNOB_HEIGHT = 1.6f;
constexpr float DOORKNOB_RADIUS = 0.1f;
constexpr float DOORKNOB_SHAFT_RADIUS = 0.08f;
constexpr float DOORKNOB_SHAFT_LENGTH = 0.12f;
// -----------------------------------------------------------------------------
struct WorldConstants
{
	Vec4 CameraPosition    = Vec4::ZERO;
	Vec4 IndoorLightColor  = Vec4::ZERO;
	Vec4 OutdoorLightColor = Vec4::ZERO;
	Vec4 SkyColor          = Vec4::ZERO;
	float FogNearDistance  = 0.f;
	float FogFarDistance   = 0.f;
	Vec2 WorldPadding      = Vec2::ZERO;
};
// -----------------------------------------------------------------------------
static IntVec2 m_directions[4] =
{
	IntVec2(1,0),
	IntVec2(-1,0),
	IntVec2(0,1),
	IntVec2(0,-1)
};
// -----------------------------------------------------------------------------
enum TileType
{
	TILE_GRASS,
	TILE_COBBLESTONE,
	TILE_WOOD
};
// -----------------------------------------------------------------------------
enum class TaskType
{
	INVALID_TASK = -1,
	GOING_TO,
	SLEEP,
	EAT,
	WORK,
	NUM_TASK_TYPES
};
// -----------------------------------------------------------------------------
extern App* g_theApp;
extern Game* g_theGame;
extern Renderer* g_theRenderer;
extern RandomNumberGenerator* g_rng;
extern InputSystem* g_theInput;
extern AudioSystem* g_theAudio;
extern Window* g_theWindow;
// -----------------------------------------------------------------------------
TaskType GetTaskTypeFromString(std::string const& taskString);