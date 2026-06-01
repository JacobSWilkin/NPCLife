#include "Game/Building.hpp"
#include "Game/Map.hpp"
#include "GameCommon.h"
#include "Engine/Renderer/Renderer.h"
#include "Engine/Core/Vertex_PCU.h"
#include "Engine/Core/VertexUtils.h"
#include "Engine/Math/MathUtils.h"

Building::Building(Map* map, IntVec2 mins, IntVec2 maxs, DoorFacingDirection doorDirection, int maxOccupancy, BuildingType buildingType)
	:m_theMap(map),
	 m_mins(mins),
	 m_maxs(maxs),
	 m_doorDirection(doorDirection),
	 m_maxOccupancy(maxOccupancy),
	 m_buildingType(buildingType)
{
	m_shader = g_theRenderer->CreateOrGetShader("Data/Shaders/BlinnPhong", VertexType::VERTEX_PCUTBN);

	// Set building coords
	for (int tileY = mins.y; tileY <= maxs.y; ++tileY)
	{
		for (int tileX = mins.x; tileX <= maxs.x; ++tileX)
		{
			IntVec2 tileCoords(tileX, tileY);
			m_occupiedTiles.push_back(tileCoords);
		}
	}

	m_buildingDoorTileCoords = ComputeDoorTile(m_doorDirection);
	CreateDoor();
	AddDoorGeometry();
	MarkTiles();

	CreateBuilding();
	CreateDoorBuffers();
	CreateBuffers();
}

Building::~Building()
{
	DeleteBuffers();
}

void Building::Render() const
{
	g_theRenderer->SetModelConstants();
	if (m_theMap->m_isBuildingSeeThrough)
	{
		g_theRenderer->SetBlendMode(BlendMode::ALPHA);
	}
	else
	{
		g_theRenderer->SetBlendMode(BlendMode::OPAQUE);
	}
	g_theRenderer->SetLightingConstants(m_sunDirection, BUILDING_SUN_INTENSITY, BUILDING_AMBIENT_INTENSITY);
	g_theRenderer->SetRasterizerMode(RasterizerMode::SOLID_CULL_BACK);
	g_theRenderer->SetDepthMode(DepthMode::READ_WRITE_LESS_EQUAL);
	g_theRenderer->BindTexture(nullptr);
	g_theRenderer->BindShader(m_shader);
	g_theRenderer->DrawIndexedVertexBuffer(m_vertexBuffer, m_indexBuffer, static_cast<unsigned int>(m_indices.size()));

	DoorToModelTransform();
	g_theRenderer->DrawVertexBuffer(m_doorVBO, static_cast<int>(m_doorVerts.size()));
}

void Building::DoorToModelTransform() const
{
	// Draw door
	Mat44 doorModelMatrix = Mat44::MakeTranslation3D(m_buildingDoor.m_doorPosition);

	// Pivot around hinge
	Vec3 hingeOffset = Vec3::ZERO;

	switch (m_doorDirection)
	{
		case DoorFacingDirection::NORTH:
		{
			hingeOffset = Vec3(0.5f * TILE_SIZE, 0.f, 0.f);
			break;
		}
		case DoorFacingDirection::SOUTH:
		{
			hingeOffset = Vec3(-0.5f * TILE_SIZE, 0.f, 0.f);
			break;
		}
		case DoorFacingDirection::EAST:
		{
			hingeOffset = Vec3(0.f, -0.5f * TILE_SIZE, 0.f);
			break;
		}
		case DoorFacingDirection::WEST:
		{
			hingeOffset = Vec3(0.f, 0.5f * TILE_SIZE, 0.f);
			break;
		}
	}
	doorModelMatrix.AppendTranslation3D(hingeOffset);
	doorModelMatrix.AppendZRotation(m_buildingDoor.m_currentAngle);
	doorModelMatrix.AppendTranslation3D(-hingeOffset);

	g_theRenderer->SetModelConstants(doorModelMatrix);
}

void Building::Update(float deltaSeconds)
{
	float diff = m_buildingDoor.m_targetAngle - m_buildingDoor.m_currentAngle;

	if (fabsf(diff) > 0.1f)
	{
		float step = m_buildingDoor.m_openSpeed * deltaSeconds;
		m_buildingDoor.m_currentAngle += GetClamped(diff, -step, step);
	}
}

IntVec2 Building::GetInteriorTile() const
{
	int doorCenterX = (m_mins.x + m_maxs.x) / 2;
	int doorCenterY = (m_mins.y + m_maxs.y) / 2;
	return IntVec2(doorCenterX, doorCenterY);
}

IntVec2 Building::GetDoorTile() const
{
	return m_buildingDoorTileCoords;
}

IntVec2 Building::GetRandomInteriorTile() const
{
	std::vector<IntVec2> interiorTiles;

	for (int tileIndex = 0; tileIndex < static_cast<int>(m_occupiedTiles.size()); ++tileIndex)
	{
		IntVec2 const& tile = m_occupiedTiles[tileIndex];
		bool isEdgeTile = tile.x == m_mins.x || tile.x == m_maxs.x || tile.y == m_mins.y || tile.y == m_maxs.y;

		if (tile == m_buildingDoorTileCoords || isEdgeTile)
		{
			continue;
		}

		if (m_maxOccupancy > 1)
		{
			if (IsNearDoor(tile))
			{
				continue;
			}
		}

		interiorTiles.push_back(tile);
	}

	if (interiorTiles.empty())
	{
		return m_buildingDoorTileCoords;
	}

	int randomIndex = g_rng->RollRandomIntInRange(0, static_cast<int>(interiorTiles.size()) - 1);
	return interiorTiles[randomIndex];
}

bool Building::IsNearDoor(IntVec2 const& tile) const
{
	float distSq = GetDistanceSquared2D(tile.GetAsVec2(), m_buildingDoorTileCoords.GetAsVec2());
	return distSq <= 2.f;
}

bool Building::TryAcquireDoor()
{
	if (m_isDoorInUse)
	{
		return false;
	}

	m_isDoorInUse = true;
	return true;
}

void Building::ReleaseDoor()
{
	m_isDoorInUse = false;
}

bool Building::IsInteriorTile(IntVec2 const& tileCoords) const
{
	for (int tileIndex = 0; tileIndex < static_cast<int>(m_occupiedTiles.size()); ++tileIndex)
	{
		IntVec2 const& tile = m_occupiedTiles[tileIndex];
		if (tile == tileCoords)
		{
			return true;
		}
	}
	return false;
}

bool Building::HasSpace() const
{
	return m_currentOccupancy < m_maxOccupancy;
}

void Building::AddOccupant()
{
	++m_currentOccupancy;
}

void Building::RemoveOccupant()
{
	--m_currentOccupancy;

	if (m_currentOccupancy <= 0)
	{
		CloseDoor();
	}
}

int Building::GetCurrentOccupancy() const
{
	return m_currentOccupancy;
}

BuildingType Building::GetBuildingType() const
{
	return m_buildingType;
}

void Building::OpenDoor(bool openInward)
{
	float sign = openInward ? 1.f : -1.f;
	m_buildingDoor.m_targetAngle = 90.f * sign;
	m_buildingDoor.m_isOpen = true;
}

void Building::CloseDoor()
{
	m_buildingDoor.m_targetAngle = 0.f;
	m_buildingDoor.m_isOpen = false;
}

void Building::CreateBuilding()
{
	float minX = m_mins.x * TILE_SIZE;
	float minY = m_mins.y * TILE_SIZE;
	float maxX = (m_maxs.x + 1) * TILE_SIZE;
	float maxY = (m_maxs.y + 1) * TILE_SIZE;

	float doorHeight = BUILDING_HEIGHT - 1.5f;
	Rgba8 wallColor(165, 42, 42, 125);
	Vec2 door = m_buildingDoorTileCoords.GetAsVec2();

	float doorMinX = door.x * TILE_SIZE;
	float doorMaxX = (door.x + 1) * TILE_SIZE;
	float doorMinY = door.y * TILE_SIZE;
	float doorMaxY = (door.y + 1) * TILE_SIZE;

	// --- SOUTH WALL ---
	if (door.y == m_mins.y)
	{
		AddWallSegment(minX, doorMinX, minY, minY + BUILDING_THICKNESS, 0.f, BUILDING_HEIGHT, wallColor);
		AddWallSegment(doorMaxX, maxX, minY, minY + BUILDING_THICKNESS, 0.f, BUILDING_HEIGHT, wallColor);
		AddWallSegment(doorMinX, doorMaxX, minY, minY + BUILDING_THICKNESS, doorHeight, BUILDING_HEIGHT, wallColor);
	}
	else
	{
		AddWallSegment(minX, maxX, minY, minY + BUILDING_THICKNESS, 0.f, BUILDING_HEIGHT, wallColor);
	}

	// --- NORTH WALL ---
	if (door.y == m_maxs.y)
	{
		AddWallSegment(minX, doorMinX, maxY - BUILDING_THICKNESS, maxY, 0.f, BUILDING_HEIGHT, wallColor);
		AddWallSegment(doorMaxX, maxX, maxY - BUILDING_THICKNESS, maxY, 0.f, BUILDING_HEIGHT, wallColor);
		AddWallSegment(doorMinX, doorMaxX, maxY - BUILDING_THICKNESS, maxY, doorHeight, BUILDING_HEIGHT, wallColor);
	}
	else
	{
		AddWallSegment(minX, maxX, maxY - BUILDING_THICKNESS, maxY, 0.f, BUILDING_HEIGHT, wallColor);
	}

	// --- WEST WALL ---
	if (door.x == m_mins.x)
	{
		AddWallSegment(minX, minX + BUILDING_THICKNESS, minY, doorMinY, 0.f, BUILDING_HEIGHT, wallColor);
        AddWallSegment(minX, minX + BUILDING_THICKNESS, doorMaxY, maxY, 0.f, BUILDING_HEIGHT, wallColor);
        AddWallSegment(minX, minX + BUILDING_THICKNESS, doorMinY, doorMaxY, doorHeight, BUILDING_HEIGHT, wallColor);
    }
	else
	{
		AddWallSegment(minX, minX + BUILDING_THICKNESS, minY, maxY, 0.f, BUILDING_HEIGHT, wallColor);
	}

	// --- EAST WALL ---
	if (door.x == m_maxs.x)
	{
		AddWallSegment(maxX - BUILDING_THICKNESS, maxX, minY, doorMinY, 0.f, BUILDING_HEIGHT, wallColor);
		AddWallSegment(maxX - BUILDING_THICKNESS, maxX, doorMaxY, maxY, 0.f, BUILDING_HEIGHT, wallColor);
		AddWallSegment(maxX - BUILDING_THICKNESS, maxX, doorMinY, doorMaxY, doorHeight, BUILDING_HEIGHT, wallColor);
	}
	else
	{
		AddWallSegment(maxX - BUILDING_THICKNESS, maxX, minY, maxY, 0.f, BUILDING_HEIGHT, wallColor);
	}
}

void Building::CreateDoor()
{
	Vec2 doorTile = m_buildingDoorTileCoords.GetAsVec2();

	float doorCenterX = (doorTile.x + 0.5f) * TILE_SIZE;
	float doorCenterY = (doorTile.y + 0.5f) * TILE_SIZE;

	float halfTile = TILE_SIZE * 0.5f;

	switch (m_doorDirection)
	{
		case DoorFacingDirection::NORTH:
		{
			doorCenterY += halfTile;
			break;
		}
		case DoorFacingDirection::SOUTH:
		{
			doorCenterY -= halfTile;
			break;
		}
		case DoorFacingDirection::EAST:
		{
			doorCenterX += halfTile;
			break;
		}
		case DoorFacingDirection::WEST:
		{
			doorCenterX -= halfTile;
			break;
		}
	}

	m_buildingDoor.m_doorPosition = Vec3(doorCenterX, doorCenterY, 0.f);
}

IntVec2 Building::ComputeDoorTile(DoorFacingDirection doorDir) const
{
	int doorCenterX = (m_mins.x + m_maxs.x) / 2;
	int doorCenterY = (m_mins.y + m_maxs.y) / 2;

	switch (doorDir)
	{
		case DoorFacingDirection::NORTH: return IntVec2(doorCenterX, m_maxs.y);
		case DoorFacingDirection::SOUTH: return IntVec2(doorCenterX, m_mins.y);
		case DoorFacingDirection::EAST:  return IntVec2(m_maxs.x, doorCenterY);
		case DoorFacingDirection::WEST:  return IntVec2(m_mins.x, doorCenterY);
	}

	return m_mins;
}

void Building::MarkTiles()
{
	for (int tileIndex = 0; tileIndex < static_cast<int>(m_occupiedTiles.size()); ++tileIndex)
	{
		IntVec2 const& tileCoords = m_occupiedTiles[tileIndex];

		Tile* tile = m_theMap->GetTile(tileCoords);
		bool isEdgeTile = tileCoords.x == m_mins.x || tileCoords.x == m_maxs.x || tileCoords.y == m_mins.y || tileCoords.y == m_maxs.y;

		if (tileCoords == m_buildingDoorTileCoords)
		{
			tile->m_isWalkable = true;
		}
		else if (isEdgeTile)
		{
			tile->m_isWalkable = false;
		}
		else
		{
			tile->m_isWalkable = true;
		}
	}
}

void Building::CreateBuffers()
{
	if (m_verts.empty())
	{
		return;
	}

	if (m_indices.empty())
	{
		return;
	}

	m_vertexBuffer = g_theRenderer->CreateVertexBuffer(static_cast<unsigned int>(m_verts.size()) * sizeof(Vertex_PCUTBN), sizeof(Vertex_PCUTBN));
	m_indexBuffer = g_theRenderer->CreateIndexBuffer(static_cast<unsigned int>(m_indices.size()) * sizeof(unsigned int), sizeof(unsigned int));
	g_theRenderer->CopyCPUToGPU(m_verts.data(), m_vertexBuffer->GetSize(), m_vertexBuffer);
	g_theRenderer->CopyCPUToGPU(m_indices.data(), m_indexBuffer->GetSize(), m_indexBuffer);
}

void Building::CreateDoorBuffers()
{
	if (m_doorVerts.empty())
	{
		return;
	}

	m_doorVBO = g_theRenderer->CreateVertexBuffer(static_cast<unsigned int>(m_doorVerts.size()) * sizeof(Vertex_PCUTBN), sizeof(Vertex_PCUTBN));
	g_theRenderer->CopyCPUToGPU(m_doorVerts.data(), m_doorVBO->GetSize(), m_doorVBO);
}

void Building::DeleteBuffers()
{
	delete m_vertexBuffer;
	m_vertexBuffer = nullptr;

	delete m_indexBuffer;
	m_indexBuffer = nullptr;

	delete m_doorVBO;
	m_doorVBO = nullptr;
}

void Building::AddDoorGeometry()
{
	float width = TILE_SIZE;
	float height = BUILDING_HEIGHT - 1.5f;

	Vec3 mins = Vec3::ZERO;
	Vec3 maxs = Vec3::ZERO;

	if (m_doorDirection == DoorFacingDirection::NORTH || m_doorDirection == DoorFacingDirection::SOUTH)
	{
		mins.x -= width * 0.5f;
		maxs.x += width * 0.5f;

		mins.y -= BUILDING_THICKNESS * 0.5f;
		maxs.y += BUILDING_THICKNESS * 0.5f;
	}
	else
	{
		mins.y -= width * 0.5f;
		maxs.y += width * 0.5f;

		mins.x -= BUILDING_THICKNESS * 0.5f;
		maxs.x += BUILDING_THICKNESS * 0.5f;
	}

	mins.z = 0.f;
	maxs.z = height;

	AddVertsForAABB3D(m_doorVerts, AABB3(mins, maxs), Rgba8::SAPPHIRE);

	// Doorknob
	Vec3 doorCenter = (mins + maxs) * 0.5f;
	Vec3 shaftStart, shaftEnd;

	switch (m_doorDirection)
	{
		case DoorFacingDirection::NORTH:
		{
			shaftStart = Vec3(doorCenter.x, maxs.y, DOORKNOB_HEIGHT) - Vec3(0.6f, 0.f, 0.f);
			shaftEnd = Vec3(doorCenter.x, maxs.y + DOORKNOB_SHAFT_LENGTH, DOORKNOB_HEIGHT) - Vec3(0.6f, 0.f, 0.f);
			break;
		}
		case DoorFacingDirection::SOUTH:
		{
			shaftStart = Vec3(doorCenter.x, mins.y, DOORKNOB_HEIGHT) + Vec3(0.6f, 0.f, 0.f);
			shaftEnd = Vec3(doorCenter.x, mins.y - DOORKNOB_SHAFT_LENGTH, DOORKNOB_HEIGHT) + Vec3(0.6f, 0.f, 0.f);
			break;
		}
		case DoorFacingDirection::EAST:
		{
			shaftStart = Vec3(maxs.x, doorCenter.y, DOORKNOB_HEIGHT);
			shaftEnd = Vec3(maxs.x + DOORKNOB_SHAFT_LENGTH, doorCenter.y, DOORKNOB_HEIGHT);
			break;
		}
		case DoorFacingDirection::WEST:
		{
			shaftStart = Vec3(mins.x, doorCenter.y, DOORKNOB_HEIGHT) - Vec3(0.f, 0.6f, 0.f);
			shaftEnd = Vec3(mins.x - DOORKNOB_SHAFT_LENGTH, doorCenter.y, DOORKNOB_HEIGHT) - Vec3(0.f, 0.6f, 0.f);
			break;
		}
	}

	// Doorknob base
	AddVertsForCylinder3D(m_doorVerts, shaftStart, shaftEnd, DOORKNOB_SHAFT_RADIUS, Rgba8::GOLD, AABB2::ZERO_TO_ONE, 64);

	// DoorKnob handle
	AddVertsForSphere3D(m_doorVerts, shaftEnd, DOORKNOB_RADIUS, Rgba8::GOLD);
}

void Building::AddWallSegment(float minX, float maxX, float minY, float maxY, float minZ, float maxZ, Rgba8 const& color)
{
	AABB3 wall(Vec3(minX, minY, minZ), Vec3(maxX, maxY, maxZ));
	m_wallBounds.push_back(wall);
	AddVertsForAABB3D(m_verts, m_indices, wall, color);
}