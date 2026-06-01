#include "Game/AI.hpp"
#include "Game/Map.hpp"
#include "Game/Building.hpp"
#include "Game/Game.h"
#include "Game/GameCommon.h"
#include "Game/Character.hpp"
#include "Game/CharacterDefinition.hpp"
#include "Engine/Core/EngineCommon.h"
#include "Engine/Core/DebugRender.hpp"
#include "Engine/Math/RaycastUtils.hpp"
#include "Engine/Math/MathUtils.h"

AIController::AIController(Character* character)
	:m_character(character)
{
}

void AIController::Update(float deltaSeconds)
{
	if (!m_character)
	{
		return;
	}

	UpdateTaskFromTime();

	bool isSleepActive = m_character->m_currentTask == TaskType::SLEEP || m_sleepPhase == SleepPhase::EXITING_BUILDING || m_sleepPhase == SleepPhase::GOING_OUTSIDE;

	// Check if character is sleeping, if so go into building and stop movement
	if (isSleepActive)
	{
		HandleSleepBehavior(deltaSeconds);
		return;
	}

	Map* theMap = m_character->m_map;

	if (m_character->m_path.empty())
	{
		IntVec2 startCoords = theMap->GetTileCoordsFromWorldPos(m_character->m_position);

		// Pick a goal tile
		IntVec2 goalTile = IntVec2::ZERO;
		bool foundValidTile = false;

		for (int goalIndex = 0; goalIndex < MAX_GOALTILE_ATTEMPTS; ++goalIndex)
		{
			IntVec2 candidateTile = IntVec2(g_rng->RollRandomIntInRange(0, MAP_WIDTH - 1), g_rng->RollRandomIntInRange(0, MAP_HEIGHT - 1));

			// Check first if we can walk to the tile
			if (!theMap->IsTileWalkable(candidateTile))
			{
				continue;
			}

			// Check to not go inside if working
			// ToDo: Maybe need to change this when eating behavior is added
			if (m_character->m_currentTask != TaskType::SLEEP && theMap->IsTileInterior(candidateTile))
			{
				continue;
			}

			goalTile = candidateTile;
			foundValidTile = true;
			break;
		}

		if (foundValidTile)
		{
			m_targetTile = goalTile;
			m_character->m_path = theMap->FindPathAStar(startCoords, goalTile);
		}
	}

	if (!m_character->m_path.empty())
	{
		Vec3 targetPosition = theMap->GetWorldCenterForTile(m_character->m_path[0]);
		m_character->MoveTowardPosition(targetPosition, deltaSeconds);
		
		// Check if we are debug drawing the characters paths
		if (theMap->m_areCharacterPathsBeingDrawn)
		{
			for (int pathIndex = 0; pathIndex < static_cast<int>(m_character->m_path.size() - 1); ++pathIndex)
			{
				Vec3 from = theMap->GetWorldCenterForTile(m_character->m_path[pathIndex]);
				Vec3 to = theMap->GetWorldCenterForTile(m_character->m_path[pathIndex + 1]);
				DebugAddWorldArrow(from, to, 0.1f, 0.f, Rgba8::CYAN);
			}
			Vec3 goalPosition = theMap->GetWorldCenterForTile(m_targetTile);
			DebugAddWorldSphere(goalPosition, 0.3f, 0.f, Rgba8::GOLD);
		}

		RaycastResult3D raycastResult = m_character->m_map->RaycastWorldCharacters(m_character->GetEyePosition(), m_character->GetForwardNormal(), m_character->m_physicsRadius * 0.5f, m_character);
		
		// Check for obstacles/other characters and steer away
		if (raycastResult.m_didImpact)
		{
			HandleAvoidance(raycastResult);
			return;
		}
		else
		{
			Vec2  charPosXY = m_character->m_position.GetXY();
			Vec2  targetPosXY = targetPosition.GetXY();
			float distSquared = GetDistanceSquared2D(charPosXY, targetPosXY);

			if (distSquared < ARRIVAL_THRESHOLD * ARRIVAL_THRESHOLD)
			{
				m_character->m_path.erase(m_character->m_path.begin());
			}
		}
	}
}

void AIController::HandleSleepBehavior(float deltaSeconds)
{
	Map* theMap = m_character->m_map;

	// Validation check
	if (m_sleepPhase != SleepPhase::NONE && m_targetBuilding == nullptr)
	{
		m_sleepPhase = SleepPhase::NONE;
		m_hasHomeDoorKey = false;
		m_hasChosenSleepTarget = false;

		m_character->m_velocity = Vec3::ZERO;
		m_character->m_path.clear();

		return;
	}

	if (m_sleepPhase == SleepPhase::EXITING_BUILDING)
	{
		if (!m_hasHomeDoorKey)
		{
			if (!m_targetBuilding->TryAcquireDoor())
			{
				m_character->m_velocity = Vec3::ZERO;
				return;
			}

			m_hasHomeDoorKey = true;

			IntVec2 interior = m_character->m_map->GetTileCoordsFromWorldPos(m_character->m_position);
			IntVec2 doorTile = m_targetBuilding->GetDoorTile();

			m_character->m_path = m_character->m_map->FindPathAStar(interior, doorTile);
			return;
		}
	}

	// The character picks a building once
	if (!m_hasChosenSleepTarget)
	{
		// ToDo: Change this to home destination instead of random building
		Building* building = theMap->GetAndReserveRandomAvailableBuilding();

		if (building)
		{
			m_targetBuilding = building;
			m_hasChosenSleepTarget = true;

			m_sleepPhase = SleepPhase::GOING_TO_DOOR;

			IntVec2 startCoords = theMap->GetTileCoordsFromWorldPos(m_character->m_position);
			IntVec2 outsideTile = theMap->GetTileOutsideDoor(m_targetBuilding);

			m_character->m_path = theMap->FindPathAStar(startCoords, outsideTile);
		}
	}

	if (!m_targetBuilding)
	{
		m_sleepPhase = SleepPhase::NONE;
		m_hasChosenSleepTarget = false;
		return;
	}

	// The character moves along the path to door
	if (!m_character->m_path.empty())
	{
		Vec3 targetPosition = theMap->GetWorldCenterForTile(m_character->m_path[0]);
		m_character->MoveTowardPosition(targetPosition, deltaSeconds);

		if (!IsInsideBuilding())
		{
			RaycastResult3D raycastResult = m_character->m_map->RaycastWorldCharacters(m_character->GetEyePosition(), m_character->GetForwardNormal(), m_character->m_physicsRadius * 0.5f, m_character);

			// Avoidance check in case our character comes across another character when moving to a building
			if (raycastResult.m_didImpact)
			{
				HandleAvoidance(raycastResult);
				return;
			}
		}

		float distSquared = GetDistanceSquared2D(m_character->m_position.GetXY(), targetPosition.GetXY());

		// Character debug draw path check for sleeping behavior
		if (theMap->m_areCharacterPathsBeingDrawn)
		{
			for (int pathIndex = 0; pathIndex < static_cast<int>(m_character->m_path.size() - 1); ++pathIndex)
			{
				Vec3 from = theMap->GetWorldCenterForTile(m_character->m_path[pathIndex]);
				Vec3 to = theMap->GetWorldCenterForTile(m_character->m_path[pathIndex + 1]);
				DebugAddWorldArrow(from, to, 0.1f, 0.f, Rgba8::CYAN);
			}
		}

		if (distSquared < ARRIVAL_THRESHOLD * ARRIVAL_THRESHOLD)
		{
			m_character->m_path.erase(m_character->m_path.begin());

			// The character arrives at door, time to go inside and sleep!
			if (m_character->m_path.empty())
			{
				if (m_sleepPhase == SleepPhase::GOING_TO_DOOR)
				{
					if (m_targetBuilding)
					{
						m_targetBuilding->OpenDoor(true);
					}

					m_sleepPhase = SleepPhase::GOING_INSIDE;

					IntVec2 doorTile = m_targetBuilding->GetDoorTile();
					IntVec2 interiorTile = m_targetBuilding->GetRandomInteriorTile();

					m_character->m_path = theMap->FindPathAStar(doorTile, interiorTile);
					return;
				}
				else if (m_sleepPhase == SleepPhase::GOING_INSIDE)
				{
					m_sleepPhase = SleepPhase::SLEEPING;

					if (m_targetBuilding)
					{
						m_targetBuilding->CloseDoor();
					}
				}
				else if (m_sleepPhase == SleepPhase::EXITING_BUILDING)
				{
					m_sleepPhase = SleepPhase::GOING_OUTSIDE;

					IntVec2 doorTile = m_targetBuilding->GetDoorTile();
					IntVec2 outsideTile = theMap->GetTileOutsideDoor(m_targetBuilding);

					m_character->m_path = theMap->FindPathAStar(doorTile, outsideTile);
					return;
				}
				else if (m_sleepPhase == SleepPhase::GOING_OUTSIDE)
				{
					// We are fully outside
					m_targetBuilding->RemoveOccupant();
					m_targetBuilding->ReleaseDoor();

					m_hasHomeDoorKey = false;
					m_hasChosenSleepTarget = false;

					m_targetBuilding = nullptr;
					m_sleepPhase = SleepPhase::NONE;

					m_character->m_path.clear();
				}
			}
		}
	}
}

void AIController::HandleAvoidance(RaycastResult3D const& raycastResult)
{
	Vec3 myForward = m_character->GetForwardNormal();
	Vec3 toOther = (raycastResult.m_impactPos - m_character->m_position).GetNormalized();

	Vec3 right = CrossProduct3D(Vec3::ZAXE, myForward);
	float side = (m_character->m_charDefinition->m_characterID != m_character->m_charDefinition->m_characterID) ? 1.f : -1.f;
	Vec3 sideDirection = right * side;

	// Picking a detour position
	float detourDistance = m_character->m_physicsRadius * 2.f;
	Vec3 detourPosition = m_character->m_position + sideDirection * detourDistance;

	// Converting detour to tile
	IntVec2 detourTile = m_character->m_map->GetTileCoordsFromWorldPos(detourPosition);

	// Making sure we can walk to the detour before inserting
	if (m_character->m_map->IsTileWalkable(detourTile) && !m_character->m_map->IsTileInterior(detourTile))
	{
		if (m_character->m_path.empty() || m_character->m_path.front() != detourTile)
		{
			m_character->m_path.insert(m_character->m_path.begin(), detourTile);
		}
	}
	else
	{
		// If side is blocked we go back to wait and keep
		AvoidObstacleWaitAndKeepPath(raycastResult.m_impactPos);
	}
}

void AIController::AvoidObstacleWaitAndKeepPath(Vec3 const& obstaclePosition)
{
	UNUSED(obstaclePosition);
	m_character->m_velocity = Vec3::ZERO;
}

void AIController::UpdateTaskFromTime()
{
	float time = g_theGame->GetTimeOfDay();
	CharacterDefinition* charDef = m_character->m_charDefinition;

	for (int entryIndex = 0; entryIndex < static_cast<int>(charDef->m_taskSchedule.size()); ++entryIndex)
	{
		TaskScheduleEntry const& taskEntry = charDef->m_taskSchedule[entryIndex];
		bool isWrappingMidnight = taskEntry.m_taskStartHour > taskEntry.m_taskEndHour;
		bool isInTimeRange = false;

		if (!isWrappingMidnight)
		{
			isInTimeRange = (time >= taskEntry.m_taskStartHour && time < taskEntry.m_taskEndHour);
		}
		else
		{
			isInTimeRange = (time >= taskEntry.m_taskStartHour || time < taskEntry.m_taskEndHour);
		}

		if (isInTimeRange)
		{
			TaskType previousTask = m_character->m_currentTask;

			if (previousTask != taskEntry.m_taskType)
			{
				// Checking out of our building when leaving
				if (previousTask == TaskType::SLEEP && taskEntry.m_taskType != TaskType::SLEEP)
				{
					if (m_targetBuilding)
					{
						m_sleepPhase = SleepPhase::EXITING_BUILDING;

						IntVec2 interior = m_character->m_map->GetTileCoordsFromWorldPos(m_character->m_position);
						IntVec2 doorTile = m_targetBuilding->GetDoorTile();

						m_targetBuilding->OpenDoor(false);
					}
				}

				m_character->SetCurrentTask(taskEntry.m_taskType);
			}
			return;
		}
	}
}

bool AIController::IsSleepBuildingValid() const
{
	return (m_targetBuilding != nullptr);
}

bool AIController::IsInsideBuilding() const
{
	return (m_sleepPhase == SleepPhase::SLEEPING || m_sleepPhase == SleepPhase::GOING_INSIDE || m_sleepPhase == SleepPhase::EXITING_BUILDING);
}