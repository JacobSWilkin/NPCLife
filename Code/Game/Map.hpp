#pragma once
#include "Game/GameCommon.h"
#include "Game/Character.hpp"
#include "Engine/Core/Vertex_PCUTBN.hpp"
#include "Engine/Math/AABB3.hpp"
#include "Engine/Math/IntVec2.h"
#include <vector>
// -----------------------------------------------------------------------------
typedef std::vector<Character*> CharacterList;
// -----------------------------------------------------------------------------
class Building;
class VertexBuffer;
class IndexBuffer;
class Texture;
class SpriteSheet;
struct RaycastResult3D;
// -----------------------------------------------------------------------------
struct Tile
{
	IntVec2  m_tileCoords = IntVec2::ZERO;
	AABB3    m_bounds;
	bool     m_isWalkable = true;
	TileType m_tileType = TILE_GRASS;
};
// -----------------------------------------------------------------------------
struct PathNode
{
	IntVec2   m_tileCoords = IntVec2::ZERO;
	float     m_costFromStart = FLT_MAX;
	float     m_heuristicToGoal = 0.f;
	float	  m_sumCost = FLT_MAX;
	PathNode* m_parentNode = nullptr;

	PathNode() = default;
};
// -----------------------------------------------------------------------------
class Map
{
public:
	// Construction
	Map(Game* owner);
	~Map();

	// Updating
	void Update(float deltaSeconds);
	void UpdateBuildings(float deltaSeconds);
	void DebugToggles();
	void UpdateCharacters(float deltaSeconds);

	// Collision
	void CollideCharacters();
	void CollideCharacters(Character* charA, Character* charB);
	void CollideCharactersAgainstBuildings();
	void CollideCharactersAgainstBuildings(Character* character);

	// Raycasting
	RaycastResult3D RaycastvsAll(Vec3 const& startPos, Vec3 const& direction, float distance, Character* owner) const;
	RaycastResult3D RaycastWorldCharacters(Vec3 const& startPos, Vec3 const& direction, float distance, Character* owner) const;
	RaycastResult3D RaycastWorldBuildings(Vec3 const& startPos, Vec3 const& dir, float maxDist) const;

	// Pathfinding
	std::vector<IntVec2> FindPathAStar(IntVec2 const& startCoords, IntVec2 const& goalCoords);

	// Rendering
	void Render() const;
	void RenderMap() const;
	void RenderBuildings() const;
	void RenderCharacters() const;

	// Helpers
	bool      AreTileCoordsValid(IntVec2 const& coords) const;
	bool	  IsTileWalkable(int tileX, int tileY);
	bool	  IsTileWalkable(IntVec2 const& coords);
	int       GetTileIndex(IntVec2 const& coords) const;
	Tile*     GetTile(IntVec2 const& coords);
	IntVec2   GetTileCoordsFromWorldPos(Vec3 const& worldPos) const;
	Vec3      GetWorldCenterForTile(IntVec2 const& coords) const;
	Tile*     GetTileFromWorldPos(Vec3 const& worldPos);
	Building* GetAndReserveRandomAvailableBuilding() const;
	Building* GetRandomTavern() const;
	bool      IsTileInterior(IntVec2 const& tileCoords) const;
	IntVec2   GetTileOutsideDoor(Building* building) const;
	float     GetTileTraversalCost(IntVec2 const& tileCoords);

public:
	// Debug toggles
	bool m_isBuildingSeeThrough = false;
	bool m_areCharacterPathsBeingDrawn = false;

private:
	// Creation methods
	void InitializeTiles();
	void BuildBaseGeometry();
	void CreateBuffers();
	void CreateCharacters();
	void CreateBuildings();
	void CreateRoadToDoor(IntVec2 start, IntVec2 goal);
	void GenerateRoadsTiles();

	// Deletion methods
	void DeleteCharacters();
	void DeleteBuffers();
	void DeleteBuildings();
	void DeleteNodes(std::vector<PathNode*>& openList, std::vector<PathNode*>& closedList);

	Character* SpawnCharacterWithID(int charDefID);

private:
	Game* m_theGame = nullptr;

	// Sprite Sheet
	Texture* m_texture = nullptr;
	SpriteSheet* m_spriteSheet = nullptr;

	// Buffers
	VertexBuffer* m_vertexBuffer = nullptr;
	std::vector<Vertex_PCUTBN> m_mapVerts;
	IndexBuffer* m_indexBuffer = nullptr;
	std::vector<unsigned int> m_indices;

	// Tiles and buildings
	std::vector<Tile> m_tiles;

	// Characters
	CharacterList m_allCharacters;
	std::vector<Building*> m_buildings;
};
// -----------------------------------------------------------------------------
