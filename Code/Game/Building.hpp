#pragma once
#include "Engine/Math/IntVec2.h"
#include "Engine/Core/Vertex_PCUTBN.hpp"
#include "Engine/Math/AABB3.hpp"
#include <vector>
// -----------------------------------------------------------------------------
constexpr int   ONE_OCCUPANT = 1;
constexpr int   TWO_OCCUPANTS = 2;
constexpr int   THREE_OCCUPANTS = 3;
constexpr int   TEN_OCCUPANTS = 10;
constexpr float BUILDING_HEIGHT = 5.f;
constexpr float BUILDING_THICKNESS = 0.1f;
constexpr float BUILDING_SUN_INTENSITY = 0.55f;
constexpr float BUILDING_AMBIENT_INTENSITY = 0.35f;
// -----------------------------------------------------------------------------
class Map;
class Shader;
class VertexBuffer;
class IndexBuffer;
// -----------------------------------------------------------------------------
enum class BuildingType
{
	INVALID = -1,
	HOME,
	TAVERN,
};
// -----------------------------------------------------------------------------
struct Door
{
	Vec3  m_doorPosition = Vec3::ZERO;
	float m_currentAngle = 0.f;
	float m_targetAngle = 0.f;
	float m_openSpeed = 180.f;
	bool  m_isOpen = false;
};
// -----------------------------------------------------------------------------
enum class DoorFacingDirection
{
	NORTH,
	SOUTH,
	EAST,
	WEST
};
// -----------------------------------------------------------------------------
class Building
{
public:
	Building(Map* map, IntVec2 mins, IntVec2 maxs, DoorFacingDirection doorDirection, int maxOccupancy, BuildingType buildingType);
	~Building();
	void Render() const;
	void DoorToModelTransform() const;
	void Update(float deltaSeconds);

	IntVec2 GetInteriorTile() const;
	IntVec2 GetDoorTile() const;
	IntVec2 GetRandomInteriorTile() const;
	bool    IsNearDoor(IntVec2 const& tile) const;
	bool    TryAcquireDoor();
	void    ReleaseDoor();
	bool    IsInteriorTile(IntVec2 const& tileCoords) const;

	bool HasSpace() const;
	int  GetCurrentOccupancy() const;
	BuildingType GetBuildingType() const;
	void AddOccupant();
	void RemoveOccupant();
	void OpenDoor(bool openInward);
	void CloseDoor();

private:
	void CreateBuilding();
	void CreateDoor();
	IntVec2 ComputeDoorTile(DoorFacingDirection doorDir) const;
	void MarkTiles();
	void CreateBuffers();
	void CreateDoorBuffers();
	void DeleteBuffers();

	void AddDoorGeometry();
	void AddWallSegment(float minX, float maxX, float minY, float maxY, float minZ, float maxZ, Rgba8 const& color);

public:
	Map*    m_theMap = nullptr;
	IntVec2 m_buildingDoorTileCoords = IntVec2::ZERO;
	IntVec2 m_mins = IntVec2::ZERO;
	IntVec2 m_maxs = IntVec2::ZERO;
	std::vector<AABB3> m_wallBounds;
	std::vector<IntVec2> m_occupiedTiles;
	std::vector<IntVec2> m_claimedSleepTiles;

	DoorFacingDirection m_doorDirection = DoorFacingDirection::EAST;
	BuildingType        m_buildingType = BuildingType::HOME;
	Door m_buildingDoor;
	bool m_isDoorInUse = false;

private:
	// Buffers
	VertexBuffer*              m_vertexBuffer = nullptr;
	std::vector<Vertex_PCUTBN> m_verts;
	IndexBuffer*               m_indexBuffer = nullptr;
	std::vector<unsigned int>  m_indices;
	VertexBuffer* m_doorVBO = nullptr;
	std::vector<Vertex_PCUTBN> m_doorVerts;

	// lighting
	Shader* m_shader = nullptr;
	Vec3 m_sunDirection = Vec3(3.f, 1.f, -2.f);

	// Occupancy
	int m_maxOccupancy = 1;
	int m_currentOccupancy = 0;
};
// -----------------------------------------------------------------------------