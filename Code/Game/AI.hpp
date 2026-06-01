#pragma once
#include "Engine/Math/IntVec2.h"
// -----------------------------------------------------------------------------
class Character;
class Building;
struct Vec3;
struct RaycastResult3D;
// -----------------------------------------------------------------------------
constexpr int MAX_GOALTILE_ATTEMPTS = 20;
// -----------------------------------------------------------------------------
enum class SleepPhase
{
	NONE = -1,
	GOING_TO_DOOR,
	GOING_INSIDE,
	SLEEPING,
	EXITING_BUILDING,
	GOING_OUTSIDE
};
// -----------------------------------------------------------------------------
enum class EatPhase
{
	NONE = -1,
	GOING_TO_DOOR,
	GOING_INSIDE,
	EATING,
	EXITING_BUILDING,
	GOING_OUTSIDE
};
// -----------------------------------------------------------------------------
class AIController
{
public:
	AIController(Character* character);

	// HFSM
	void Update(float deltaSeconds);
	void UpdateTaskFromTime();
	void HandleSleepBehavior(float deltaSeconds);
	//void HandleEatBehavior(float deltaSeconds);
	void HandleAvoidance(RaycastResult3D const& raycastResult);

	// Helpers
	void AvoidObstacleWaitAndKeepPath(Vec3 const& obstaclePosition);
	bool IsSleepBuildingValid() const;
	bool IsInsideBuilding() const;

private:
	Character* m_character = nullptr;

	Building*  m_targetBuilding = nullptr;
	IntVec2    m_targetTile = IntVec2::ZERO;
	bool       m_hasChosenSleepTarget = false;
	bool       m_hasHomeDoorKey = false;
	SleepPhase m_sleepPhase = SleepPhase::GOING_TO_DOOR;

	EatPhase   m_eatPhase = EatPhase::NONE;
	Building*  m_targetTavern = nullptr;
	bool       m_hasChosenEatTarget = false;
	bool       m_hasTavernDoorKey = false;
	float      m_eatTimer = 0.f;
};
// -----------------------------------------------------------------------------