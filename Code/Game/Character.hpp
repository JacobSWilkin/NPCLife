#pragma once
#include "Game/GameCommon.h"
#include "Engine/Core/Vertex_PCU.h"
#include "Engine/Math/IntVec2.h"
#include "Engine/Math/Mat44.hpp"
#include "Engine/Math/EulerAngles.hpp"
#include <vector>
// -----------------------------------------------------------------------------
constexpr int MAX_BONES = 128;
// -----------------------------------------------------------------------------
struct CharacterDefinition;
class  AIController;
class  Map;
struct Vertex_PCUTBN;
struct Vertex_PCUTBNSkinned;
class  Skeleton;
class  Shader;
class  IndexBuffer;
class  VertexBuffer;
class  ConstantBuffer;
// -----------------------------------------------------------------------------
struct SkinningConstants
{
	Mat44 m_boneMatrices[MAX_BONES];
};
// -----------------------------------------------------------------------------
enum class AnimationState
{
	IDLE,
	WALK
};
// -----------------------------------------------------------------------------
class Character
{
public:
	Character(Map* owner, Vec3 const& position, CharacterDefinition* charDef);
	~Character();

	void Update(float deltaSeconds);
	void UpdatePhysics(float deltaSeconds);
	void UpdateAnimation(float deltaSeconds);
	void CheckObstaclesInFront();
	void CharDebugPresses();

	void Render() const;
	void SetSkinningConstants() const;
	Mat44 GetModelToWorldTransform() const;

	void SetAIController(AIController* aiController);
	AIController* GetAIController() const;
	void SetCurrentTask(TaskType newTask);
	char const* GetTaskName() const;

	Vec3 GetForwardNormal() const;
	Vec3 GetEyePosition() const;
	void AddImpulse(Vec3 appliedImpulse);
	void AddForce(Vec3 appliedForce);
	void MoveInDirection(Vec3 const& direction, float speed);
	void TurnInDirection(Vec2 const& targetPosition, float maxTurnDegrees);
	void TurnInDirection(Vec3 const& direction, float maxDegrees);
	void MoveTowardPosition(Vec3 const& targetPosition, float deltaSeconds);

private:
	Skeleton* CreateTestSkeleton();
	void      BuildCharacterMesh();
	void	  CreateBuffers();
	void      UpdateGPUBuffer();
	void      DeleteBuffers();

public:
	Map* m_map = nullptr;
	Vec3 m_position = Vec3::ZERO;
	EulerAngles m_orientation = EulerAngles::ZERO;
	Vec3  m_acceleration = Vec3::ZERO;
	Vec3  m_velocity = Vec3::ZERO;
	float m_dragForce = 9.f;
	float m_physicsRadius = 0.f;
	float m_physicsHeight = 0.f;
	bool  m_isMovable = false;

	IntVec2 m_currentTileCoords = IntVec2::ZERO;
	IntVec2 m_goalTileCoords = IntVec2::ZERO;
	std::vector<IntVec2> m_path;

	CharacterDefinition* m_charDefinition = nullptr;
	TaskType m_currentTask = TaskType::INVALID_TASK;
	TaskType m_previousTask = TaskType::INVALID_TASK;

private:
	AIController* m_aiController = nullptr;

	std::vector<Vertex_PCU> m_skeletonVerts;
	std::vector<Vertex_PCU> m_physicsVerts;
	std::vector<Vertex_PCU> m_raycastVerts;
	Skeleton* m_skeleton = nullptr;

	bool m_debugDrawCollisionCylinder = false;
	bool m_debugCharForwardRaycast = false;

	Shader* m_shader = nullptr;
	ConstantBuffer* m_skinningBuffer = nullptr;
	Vec3 m_sunDirection = Vec3(3.f, 1.f, -2.f);
	VertexBuffer* m_vertexBuffer = nullptr;
	IndexBuffer* m_indexBuffer = nullptr;
	std::vector<unsigned int> m_charIndices;
	std::vector<Vertex_PCUTBNSkinned> m_charSkinnedVerts;

	AnimationState m_animState = AnimationState::IDLE;
	float          m_animationTimer = 0.f;
};
// -----------------------------------------------------------------------------