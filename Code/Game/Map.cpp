#include "Game/Map.hpp"
#include "Game/Game.h"
#include "Game/Building.hpp"
#include "Game/Character.hpp"
#include "Game/CharacterDefinition.hpp"
#include "Game/AI.hpp"
#include "Engine/Input/InputSystem.h"
#include "Engine/Renderer/Renderer.h"
#include "Engine/Core/EngineCommon.h"
#include "Engine/Core/VertexUtils.h"
#include "Engine/Math/AABB3.hpp"
#include "Engine/Math/MathUtils.h"
#include "Engine/Math/RaycastUtils.hpp"

Map::Map(Game* owner)
	:m_theGame(owner)
{
	m_texture = g_theRenderer->CreateOrGetTextureFromFile("Data/Images/Terrain_8x8.png");
	m_spriteSheet = new SpriteSheet(*m_texture, IntVec2::GRID8X8);

	InitializeTiles();
	CreateBuildings();
	GenerateRoadsTiles();
	BuildBaseGeometry();
	CreateCharacters();
	CreateBuffers();
}

void Map::CreateBuildings()
{
	m_buildings.push_back(new Building(this, IntVec2(10, 10), IntVec2(12, 12), DoorFacingDirection::WEST, ONE_OCCUPANT, BuildingType::HOME));
	m_buildings.push_back(new Building(this, IntVec2(40, 20), IntVec2(44, 24), DoorFacingDirection::WEST, TWO_OCCUPANTS, BuildingType::HOME));
	m_buildings.push_back(new Building(this, IntVec2(1, 45), IntVec2(3, 47), DoorFacingDirection::SOUTH, ONE_OCCUPANT, BuildingType::HOME));
	m_buildings.push_back(new Building(this, IntVec2(21, 45), IntVec2(25, 49), DoorFacingDirection::SOUTH, TWO_OCCUPANTS, BuildingType::HOME));
	m_buildings.push_back(new Building(this, IntVec2(30, 10), IntVec2(32, 12), DoorFacingDirection::NORTH, ONE_OCCUPANT, BuildingType::HOME));

	m_buildings.push_back(new Building(this, IntVec2(58, 28), IntVec2(68, 42), DoorFacingDirection::WEST, TEN_OCCUPANTS, BuildingType::TAVERN));
}

void Map::CreateRoadToDoor(IntVec2 start, IntVec2 goal)
{
	std::vector<IntVec2> path = FindPathAStar(start, goal);

	for (int pathIndex = 0; pathIndex < static_cast<int>(path.size()); ++pathIndex)
	{
		IntVec2 const& coords = path[pathIndex];
		Tile* tile = GetTile(coords);

		if (IsTileInterior(coords))
		{
			continue;
		}

		if (tile)
		{
			tile->m_tileType = TILE_COBBLESTONE;
		}
	}
}

void Map::GenerateRoadsTiles()
{
	if (m_buildings.empty())
	{
		return;
	}

	std::vector<Building*> connectedBuildings;
	connectedBuildings.push_back(m_buildings[0]);

	// Connecting road tiles to buildings
	for (int buildingIndex = 1; buildingIndex < static_cast<int>(m_buildings.size()); ++buildingIndex)
	{
		Building* currentBuilding = m_buildings[buildingIndex];
		Building* closestBuilding = nullptr;
		float bestDist = FLT_MAX;

		for (int connectedBuildingIndex = 0; connectedBuildingIndex < static_cast<int>(connectedBuildings.size()); ++connectedBuildingIndex)
		{
			Building* connected = connectedBuildings[connectedBuildingIndex];
			float dist = GetDistance2D(Vec2(currentBuilding->GetDoorTile().GetAsVec2()), Vec2(connected->GetDoorTile().GetAsVec2()));

			if (dist < bestDist)
			{
				bestDist = dist;
				closestBuilding = connected;
			}
		}

		IntVec2 startCoords = GetTileOutsideDoor(currentBuilding);
		IntVec2 goalCoords = GetTileOutsideDoor(closestBuilding);

		CreateRoadToDoor(startCoords, goalCoords);

		connectedBuildings.push_back(currentBuilding);
	}


	// Checking for more possible road connections
	for (int buildingAIndex = 0; buildingAIndex < static_cast<int>(m_buildings.size()); ++buildingAIndex)
	{
		for (int buildingBIndex = buildingAIndex + 1; buildingBIndex < static_cast<int>(m_buildings.size()); ++buildingBIndex)
		{
			Building* buildingA = m_buildings[buildingAIndex];
			Building* buildingB = m_buildings[buildingBIndex];

			float dist = GetDistance2D(Vec2(buildingA->GetDoorTile().GetAsVec2()), Vec2(buildingB->GetDoorTile().GetAsVec2()));
			if (dist > ROAD_MAX_CONNECTION_DIST)
			{
				continue;
			}

			if (g_rng->RollRandomFloatZeroToOne() > ROAD_CONNECTION_CHANCE)
			{
				continue;
			}

			IntVec2 startCoords = GetTileOutsideDoor(buildingA);
			IntVec2 goalCoords = GetTileOutsideDoor(buildingB);
			CreateRoadToDoor(startCoords, goalCoords);
		}
	}
}

Map::~Map()
{
	DeleteBuffers();
	DeleteCharacters();
	DeleteBuildings();

	delete m_spriteSheet;
	m_spriteSheet = nullptr;
}

void Map::Update(float deltaSeconds)
{
	DebugToggles();
	UpdateCharacters(deltaSeconds);
	UpdateBuildings(deltaSeconds);

	CollideCharacters();
	CollideCharactersAgainstBuildings();
}

void Map::UpdateBuildings(float deltaSeconds)
{
	for (int buildingIndex = 0; buildingIndex < static_cast<int>(m_buildings.size()); ++buildingIndex)
	{
		Building*& building = m_buildings[buildingIndex];
		if (building)
		{
			building->Update(deltaSeconds);
		}
	}
}

void Map::DebugToggles()
{
	if (g_theInput->WasKeyJustPressed('G'))
	{
		m_areCharacterPathsBeingDrawn = !m_areCharacterPathsBeingDrawn;
	}
	if (g_theInput->WasKeyJustPressed('B'))
	{
		m_isBuildingSeeThrough = !m_isBuildingSeeThrough;
	}
}

void Map::UpdateCharacters(float deltaSeconds)
{
	for (int characterIndex = 0; characterIndex < static_cast<int>(m_allCharacters.size()); ++characterIndex)
	{
		Character*& character = m_allCharacters[characterIndex];
		if (character)
		{
			character->Update(deltaSeconds);
		}
	}
}

void Map::CollideCharacters()
{
	for (int charAIndex = 0; charAIndex < static_cast<int>(m_allCharacters.size()); ++charAIndex)
	{
		for (int charBIndex = charAIndex + 1; charBIndex < static_cast<int>(m_allCharacters.size()); ++charBIndex)
		{
			CollideCharacters(m_allCharacters[charAIndex], m_allCharacters[charBIndex]);
		}
	}
}

void Map::CollideCharacters(Character* charA, Character* charB)
{
	// Check if we have characters
	if (charA == nullptr || charB == nullptr)
	{
		return;
	}

	// Check if we are inside a building
	if (charA->GetAIController() && charA->GetAIController()->IsInsideBuilding())
	{
		return;
	}
	if (charB->GetAIController() && charB->GetAIController()->IsInsideBuilding())
	{
		return;
	}

	Vec2 actorAPosXY = charA->m_position.GetXY();
	Vec2 actorBPosXY = charB->m_position.GetXY();
	float actorAStart = charA->m_position.z;
	float actorAEnd = charA->m_position.z + charA->m_physicsHeight * CHARACTER_SCALE;
	float actorBStart = charB->m_position.z;
	float actorBEnd = charB->m_position.z + charB->m_physicsHeight * CHARACTER_SCALE;
	float charATrueRadius = charA->m_physicsRadius * CHARACTER_SCALE;
	float charBTrueRadius = charB->m_physicsRadius * CHARACTER_SCALE;

	if (!DoDiscsOverlap(actorAPosXY, charATrueRadius, actorBPosXY, charBTrueRadius))
	{
		return;
	}

	bool overlappingOnZ = (actorAStart <= actorBEnd && actorAEnd >= actorBStart);

	if (overlappingOnZ)
	{
		if (charA->m_isMovable && !charB->m_isMovable)
		{
			PushDiscOutOfDisc2D(actorAPosXY, charATrueRadius, actorBPosXY, charBTrueRadius);
			charA->m_position.x = actorAPosXY.x;
			charA->m_position.y = actorAPosXY.y;
		}
		else if (!charA->m_isMovable && charB->m_isMovable)
		{
			PushDiscOutOfDisc2D(actorBPosXY, charBTrueRadius, actorAPosXY, charATrueRadius);
			charB->m_position.x = actorBPosXY.x;
			charB->m_position.y = actorBPosXY.y;
		}
		else if (charA->m_isMovable && charB->m_isMovable)
		{
			PushDiscsOutOfEachOther2D(actorAPosXY, charATrueRadius, actorBPosXY, charBTrueRadius);
			charA->m_position.x = actorAPosXY.x;
			charA->m_position.y = actorAPosXY.y;
			charB->m_position.x = actorBPosXY.x;
			charB->m_position.y = actorBPosXY.y;
		}
	}
}

void Map::CollideCharactersAgainstBuildings()
{
	for (int characterIndex = 0; characterIndex < static_cast<int>(m_allCharacters.size()); ++characterIndex)
	{
		Character*& character = m_allCharacters[characterIndex];
		if (character)
		{
			CollideCharactersAgainstBuildings(character);
		}
	}
}

void Map::CollideCharactersAgainstBuildings(Character* character)
{
	if (!character)
	{
		return;
	}

	Vec3& pos = character->m_position;
	float radius = character->m_physicsRadius * CHARACTER_SCALE;
	float height = character->m_physicsHeight * CHARACTER_SCALE;

	for (int buildingIndex = 0; buildingIndex < static_cast<int>(m_buildings.size()); ++buildingIndex)
	{
		Building* building = m_buildings[buildingIndex];
		for (int wallIndex = 0; wallIndex < static_cast<int>(building->m_wallBounds.size()); ++ wallIndex)
		{
			AABB3 const& wall = building->m_wallBounds[wallIndex];
			PushZCylinderOutOfFixedAABB3D(pos, radius, height, wall);
		}
	}
}

RaycastResult3D Map::RaycastvsAll(Vec3 const& startPos, Vec3 const& direction, float distance, Character* owner) const
{
	float closestResult = FLT_MAX;
	RaycastResult3D raycastResult;

	// Characters
	RaycastResult3D raycastAgainstCharacters = RaycastWorldCharacters(startPos, direction, distance, owner);
	if (raycastAgainstCharacters.m_didImpact)
	{
		if (raycastAgainstCharacters.m_impactDist < closestResult)
		{
			closestResult = raycastAgainstCharacters.m_impactDist;
			raycastResult = raycastAgainstCharacters;
		}
	}

	// Buildings
	//RaycastResult3D raycastAgainstBuildings = RaycastWorldBuildings(startPos, direction, distance);
	//if (raycastAgainstBuildings.m_didImpact)
	//{
	//	if (raycastAgainstBuildings.m_impactDist < closestResult)
	//	{
	//		closestResult = raycastAgainstBuildings.m_impactDist;
	//		raycastResult = raycastAgainstBuildings;
	//	}
	//}

	return raycastResult;
}

RaycastResult3D Map::RaycastWorldCharacters(Vec3 const& startPos, Vec3 const& direction, float distance, Character* owner) const
{
	float closestResult = FLT_MAX;
	RaycastResult3D raycastResult;

	for (int charIndex = 0; charIndex < static_cast<int>(m_allCharacters.size()); ++charIndex)
	{
		// Check for raycasting against ourself
		Character* currentCharacter = m_allCharacters[charIndex];
		if (currentCharacter == owner)
		{
			continue;
		}

		Vec3& characterStartPos = m_allCharacters[charIndex]->m_position;
		float charPhysRadius    = m_allCharacters[charIndex]->m_physicsRadius;
		float charPhysHeight    = m_allCharacters[charIndex]->m_physicsHeight;

		RaycastResult3D raycastAgainstCharCylinder = RaycastVsCylinder3D(startPos, direction, distance, characterStartPos, charPhysRadius, charPhysHeight);
		if (raycastAgainstCharCylinder.m_didImpact)
		{
			if (raycastAgainstCharCylinder.m_impactDist < closestResult)
			{
				closestResult = raycastAgainstCharCylinder.m_impactDist;
				raycastResult = raycastAgainstCharCylinder;
			}
		}
	}

	return raycastResult;
}

RaycastResult3D Map::RaycastWorldBuildings(Vec3 const& startPos, Vec3 const& dir, float maxDist) const
{
	float closestDist = FLT_MAX;
	RaycastResult3D bestResult;

	for (int buildingIndex = 0; buildingIndex < static_cast<int>(m_buildings.size()); ++buildingIndex)
	{
		Building* building = m_buildings[buildingIndex];
		for (int wallIndex = 0; wallIndex < static_cast<int>(building->m_wallBounds.size()); ++wallIndex)
		{
			AABB3 const& wall = building->m_wallBounds[wallIndex];
			RaycastResult3D hit = RaycastVsAABB3D(startPos, dir, maxDist, wall);

			if (hit.m_didImpact && hit.m_impactDist < closestDist)
			{
				closestDist = hit.m_impactDist;
				bestResult = hit;
			}
		}
	}

	return bestResult;
}

std::vector<IntVec2> Map::FindPathAStar(IntVec2 const& startCoords, IntVec2 const& goalCoords)
{
	std::vector<PathNode*> openList;
	std::vector<PathNode*> closedList;

	PathNode* startNode = new PathNode();
	startNode->m_tileCoords = startCoords;
	startNode->m_costFromStart = 0.f;
	startNode->m_heuristicToGoal = static_cast<float>(GetTaxicabDistance2D(startCoords, goalCoords));
	startNode->m_sumCost = startNode->m_heuristicToGoal;

	openList.push_back(startNode);

	while (!openList.empty())
	{
		// Getting the lowest sum cost node
		int currentIndex = 0;
		for (int listIndex = 1; listIndex < static_cast<int>(openList.size()); ++listIndex)
		{
			if (openList[listIndex]->m_sumCost < openList[currentIndex]->m_sumCost)
			{
				currentIndex = listIndex;
			}
		}

		PathNode* currentNode = openList[currentIndex];

		// Reaching the goal
		if (currentNode->m_tileCoords == goalCoords)
		{
			std::vector<IntVec2> path;

			while (currentNode != nullptr)
			{
				path.push_back(currentNode->m_tileCoords);
				currentNode = currentNode->m_parentNode;
			}

			std::reverse(path.begin(), path.end());

			if (!path.empty() && path[0] == startCoords)
			{
				path.erase(path.begin());
			}

			DeleteNodes(openList, closedList);
			return path;
		}

		// Moving current node from open list to closed list
		openList.erase(openList.begin() + currentIndex);
		closedList.push_back(currentNode);

		// Checking taxi cab neighbors
		for (IntVec2 const& direction : m_directions)
		{
			IntVec2 neighborCoords = currentNode->m_tileCoords + direction;

			if (!IsTileWalkable(neighborCoords))
			{
				continue;
			}

			bool inClosed = false;
			for (PathNode* node : closedList)
			{
				if (node->m_tileCoords == neighborCoords)
				{
					inClosed = true;
					break;
				}
			}

			if (inClosed)
			{
				continue;
			}

			float tileCost = GetTileTraversalCost(neighborCoords);
			float costFromStart = currentNode->m_costFromStart + tileCost;
			PathNode* neighborNode = nullptr;

			// Check if we are already in open list
			for (PathNode* node : openList)
			{
				if (node->m_tileCoords == neighborCoords)
				{
					neighborNode = node;
					break;
				}
			}

			if (neighborNode == nullptr)
			{
				neighborNode = new PathNode();
				neighborNode->m_tileCoords = neighborCoords;
				openList.push_back(neighborNode);
			}
			else if (costFromStart >= neighborNode->m_costFromStart)
			{
				continue;
			}

			neighborNode->m_costFromStart = costFromStart;
			neighborNode->m_heuristicToGoal = static_cast<float>(GetTaxicabDistance2D(neighborCoords, goalCoords));
			neighborNode->m_sumCost = neighborNode->m_costFromStart + neighborNode->m_heuristicToGoal;
			neighborNode->m_parentNode = currentNode;
		}
	}
	DeleteNodes(openList, closedList);
	return {};
}

void Map::Render() const
{
	RenderMap();
	RenderCharacters();
	RenderBuildings();
}

void Map::RenderMap() const
{
	g_theRenderer->SetModelConstants();
	g_theRenderer->SetBlendMode(BlendMode::OPAQUE);
	g_theRenderer->SetRasterizerMode(RasterizerMode::SOLID_CULL_BACK);
	g_theRenderer->SetDepthMode(DepthMode::READ_WRITE_LESS_EQUAL);
	g_theRenderer->BindTexture(m_texture);
	g_theRenderer->BindShader(nullptr);
	g_theRenderer->DrawIndexedVertexBuffer(m_vertexBuffer, m_indexBuffer, static_cast<unsigned int>(m_indices.size()));
}

void Map::RenderBuildings() const
{
	for (int buildingIndex = 0; buildingIndex < static_cast<int>(m_buildings.size()); ++buildingIndex)
	{
		Building const* building = m_buildings[buildingIndex];
		if (building)
		{
			building->Render();
		}
	}
}

void Map::RenderCharacters() const
{
	for (int characterIndex = 0; characterIndex < static_cast<int>(m_allCharacters.size()); ++characterIndex)
	{
		Character const* character = m_allCharacters[characterIndex];
		if (character)
		{
			character->Render();
		}
	}
}

bool Map::AreTileCoordsValid(IntVec2 const& coords) const
{
	return (coords.x >= 0 && coords.y >= 0) && (coords.x < MAP_WIDTH && coords.y < MAP_HEIGHT);
}

bool Map::IsTileWalkable(int tileX, int tileY)
{
	if (!AreTileCoordsValid(IntVec2(tileX, tileY)))
	{
		return false;
	}
	return GetTile(IntVec2(tileX, tileY))->m_isWalkable;
}

bool Map::IsTileWalkable(IntVec2 const& coords)
{
	return IsTileWalkable(coords.x, coords.y);
}

int Map::GetTileIndex(IntVec2 const& coords) const
{
	return coords.y * MAP_WIDTH + coords.x;
}

Tile* Map::GetTile(IntVec2 const& coords)
{
	if (!AreTileCoordsValid(coords))
	{
		return nullptr;
	}
	return &m_tiles[GetTileIndex(coords)];
}

IntVec2 Map::GetTileCoordsFromWorldPos(Vec3 const& worldPos) const
{
	int tileX = RoundDownToInt(worldPos.x / TILE_SIZE);
	int tileY = RoundDownToInt(worldPos.y / TILE_SIZE);
	return IntVec2(tileX, tileY);
}

Vec3 Map::GetWorldCenterForTile(IntVec2 const& coords) const
{
	float centerX = (coords.x + 0.5f) * TILE_SIZE;
	float centerY = (coords.y + 0.5f) * TILE_SIZE;
	return Vec3(centerX, centerY, MAP_GROUND_Z);
}

Tile* Map::GetTileFromWorldPos(Vec3 const& worldPos)
{
	IntVec2 coords = GetTileCoordsFromWorldPos(worldPos);
	return GetTile(coords);
}

Building* Map::GetAndReserveRandomAvailableBuilding() const
{
	std::vector<Building*> shuffledBuildings = m_buildings;

	// Shuffling the buildings
	for (int buildingIndex = 0; buildingIndex < static_cast<int>(shuffledBuildings.size()); ++buildingIndex)
	{
		int swapIndex = g_rng->RollRandomIntInRange(0, static_cast<int>(shuffledBuildings.size()) - 1);
		std::swap(shuffledBuildings[buildingIndex], shuffledBuildings[swapIndex]);
	}

	for (int buildingIndex = 0; buildingIndex < static_cast<int>(shuffledBuildings.size()); ++buildingIndex)
	{
		Building* building = shuffledBuildings[buildingIndex];
		if (building->HasSpace() && building->GetBuildingType() != BuildingType::TAVERN)
		{
			building->AddOccupant();
			return building;
		}
	}

	return nullptr;
}

Building* Map::GetRandomTavern() const
{
	std::vector<Building*> taverns;

	for (int buildingIndex = 0; buildingIndex < static_cast<int>(m_buildings.size()); ++buildingIndex)
	{
		Building* building = m_buildings[buildingIndex];
		if (building->GetBuildingType() == BuildingType::TAVERN && building->HasSpace())
		{
			taverns.push_back(building);
		}
	}

	if (taverns.empty())
	{
		return nullptr;
	}

	int randomIndex = g_rng->RollRandomIntInRange(0, static_cast<int>(taverns.size()) - 1);

	Building* tavern = taverns[randomIndex];
	tavern->AddOccupant();
	return tavern;
}

bool Map::IsTileInterior(IntVec2 const& tileCoords) const
{
	for (int buildingIndex = 0; buildingIndex < static_cast<int>(m_buildings.size()); ++buildingIndex)
	{
		Building const* building = m_buildings[buildingIndex];
		if (building->IsInteriorTile(tileCoords))
		{
			return true;
		}
	}
	return false;
}

IntVec2 Map::GetTileOutsideDoor(Building* building) const
{
	IntVec2 doorCoords = building->GetDoorTile();

	switch (building->m_doorDirection)
	{
		case DoorFacingDirection::NORTH: return doorCoords + IntVec2::NORTH;
		case DoorFacingDirection::SOUTH: return doorCoords + IntVec2::SOUTH;
		case DoorFacingDirection::EAST:  return doorCoords + IntVec2::EAST;
		case DoorFacingDirection::WEST:  return doorCoords + IntVec2::WEST;
	}

	return doorCoords;
}

float Map::GetTileTraversalCost(IntVec2 const& tileCoords)
{
	Tile const* tile = GetTile(tileCoords);

	if (!tile)
	{
		return 9999.f;
	}

	if (tile->m_tileType == TILE_COBBLESTONE)
	{
		return 0.3f;
	}

	return 1.f;
}

void Map::InitializeTiles()
{
	m_tiles.clear();
	m_tiles.reserve(MAP_SIZE);

	for (int tileY = 0; tileY < MAP_HEIGHT; ++tileY)
	{
		for (int tileX = 0; tileX < MAP_WIDTH; ++tileX)
		{
			float minX = tileX * TILE_SIZE;
			float minY = tileY * TILE_SIZE;
			float maxX = minX + TILE_SIZE;
			float maxY = minY + TILE_SIZE;

			Vec3 mins = Vec3(minX, minY, -TILE_THICKNESS);
			Vec3 maxs = Vec3(maxX, maxY, 0.f);

			AABB3 tileBounds = AABB3(mins, maxs);

			Tile tile;
			tile.m_tileCoords = IntVec2(tileX, tileY);
			tile.m_bounds = tileBounds;
			tile.m_isWalkable = true;
			m_tiles.push_back(tile);
		}
	}
}

void Map::BuildBaseGeometry()
{
	m_mapVerts.clear();
	m_indices.clear();

	for (int tileIndex = 0; tileIndex < static_cast<int>(m_tiles.size()); ++tileIndex)
	{
		Tile& tile = m_tiles[tileIndex];

		Rgba8 tileColor = Rgba8::WHITE;
		if ((tile.m_tileCoords.x + tile.m_tileCoords.y) % 2 == 0)
		{
			tileColor = Rgba8::GRAY;
		}
		else
		{
			tileColor = Rgba8::DARKGRAY;
		}

		if (tile.m_tileType != TILE_COBBLESTONE)
		{
			if (IsTileInterior(tile.m_tileCoords))
			{
				tile.m_tileType = TILE_WOOD;
			}
			else
			{
				tile.m_tileType = TILE_GRASS;
			}
		}


		AABB2 tileUVs;
		switch (tile.m_tileType)
		{
			case TILE_GRASS:
				tileUVs = m_spriteSheet->GetSpriteUVCoords(IntVec2::ZERO);
				break;
			case TILE_COBBLESTONE:
				tileUVs = m_spriteSheet->GetSpriteUVCoords(IntVec2(5, 4));
				break;
			case TILE_WOOD:
				tileUVs = m_spriteSheet->GetSpriteUVCoords(IntVec2(1, 6));
				break;
		}

		AddVertsForAABB3D(m_mapVerts, m_indices, tile.m_bounds, Rgba8::WHITE, tileUVs);
	}
}

void Map::CreateBuffers()
{
	if (m_mapVerts.empty())
	{
		return;
	}

	if (m_indices.empty())
	{
		return;
	}

	m_vertexBuffer = g_theRenderer->CreateVertexBuffer(static_cast<unsigned int>(m_mapVerts.size()) * sizeof(Vertex_PCUTBN), sizeof(Vertex_PCUTBN));
	m_indexBuffer = g_theRenderer->CreateIndexBuffer(static_cast<unsigned int>(m_indices.size()) * sizeof(unsigned int), sizeof(unsigned int));
	g_theRenderer->CopyCPUToGPU(m_mapVerts.data(), m_vertexBuffer->GetSize(), m_vertexBuffer);
	g_theRenderer->CopyCPUToGPU(m_indices.data(), m_indexBuffer->GetSize(), m_indexBuffer);
}

void Map::CreateCharacters()
{
	std::vector<int> spawnIDs = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 };

	for (int charIDIndex = 0; charIDIndex < static_cast<int>(spawnIDs.size()); ++charIDIndex)
	{
		int iD = spawnIDs[charIDIndex];
		SpawnCharacterWithID(iD);
	}
}

void Map::DeleteCharacters()
{
	for (int characterIndex = 0; characterIndex < static_cast<int>(m_allCharacters.size()); ++characterIndex)
	{
		Character*& character = m_allCharacters[characterIndex];
		if (character)
		{
			delete character;
			character = nullptr;
		}
	}
}

void Map::DeleteBuffers()
{
	delete m_vertexBuffer;
	m_vertexBuffer = nullptr;

	delete m_indexBuffer;
	m_indexBuffer = nullptr;
}

void Map::DeleteBuildings()
{
	for (int buildingIndex = 0; buildingIndex < static_cast<int>(m_buildings.size()); ++buildingIndex)
	{
		Building*& building = m_buildings[buildingIndex];
		if (building)
		{
			delete building;
			building = nullptr;
		}
	}
}

void Map::DeleteNodes(std::vector<PathNode*>& openList, std::vector<PathNode*>& closedList)
{
	for (PathNode* pathNode : openList)
	{
		delete pathNode;
	}
	for (PathNode* pathNode : closedList)
	{
		delete pathNode;
	}
}

Character* Map::SpawnCharacterWithID(int charDefID)
{
    for (int attempt = 0; attempt < MAX_CHARACTER_SPAWN_ATTEMPTS; ++attempt)
    {
        int tileX = g_rng->RollRandomIntInRange(0, MAP_WIDTH - 1);
        int tileY = g_rng->RollRandomIntInRange(0, MAP_HEIGHT - 1);
        IntVec2 tileCoords(tileX, tileY);

        if (!IsTileWalkable(tileCoords) || IsTileInterior(tileCoords))
        {
            continue;
        }

        Vec3 spawnPos = GetWorldCenterForTile(tileCoords);

        Character* newChar = new Character(this, spawnPos, CharacterDefinition::GetCharacterByID(charDefID));
        newChar->SetAIController(new AIController(newChar));

        m_allCharacters.push_back(newChar);
        return newChar;
    }

    return nullptr;
}