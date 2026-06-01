#include "Game/Character.hpp"
#include "Game/CharacterDefinition.hpp"
#include "Game/AI.hpp"
#include "Game/Map.hpp"
#include "Game/Game.h"
#include "Engine/Input/InputSystem.h"	
#include "Engine/Core/EngineCommon.h"
#include "Engine/Core/Vertex_PCU.h"
#include "Engine/Core/VertexUtils.h"
#include "Engine/Core/DebugRender.hpp"
#include "Engine/Math/MathUtils.h"
#include "Engine/Math/RaycastUtils.hpp"
#include "Engine/Renderer/Renderer.h"
#include "Engine/Skeleton/Skeleton.hpp"
// -----------------------------------------------------------------------------
Character::Character(Map* owner, Vec3 const& position, CharacterDefinition* charDef)
	:m_map(owner),
	 m_position(position),
	 m_charDefinition(charDef)
{
	m_shader = g_theRenderer->CreateOrGetShader("Data/Shaders/BlinnPhong_Skinned", VertexType::VERTEX_PCUTBN_SKINNED);
	m_skinningBuffer = g_theRenderer->CreateConstantBuffer(sizeof(SkinningConstants));

	m_physicsHeight = m_charDefinition->m_physicsHeight;
	m_physicsRadius = m_charDefinition->m_physicsRadius;
	m_isMovable     = m_charDefinition->m_isSimulated;

	// Character drawing
	m_skeleton = CreateTestSkeleton();
	AddVertsForCylinderZ3D(m_physicsVerts, Vec3::ZERO, m_physicsRadius, m_physicsHeight, Rgba8::BLUE);
	BuildCharacterMesh();
	CreateBuffers();
	UpdateGPUBuffer();
}

void Character::CreateBuffers()
{
	m_vertexBuffer = g_theRenderer->CreateVertexBuffer(MAX_CHARACTER_VERTS * sizeof(Vertex_PCUTBNSkinned), sizeof(Vertex_PCUTBNSkinned));
	m_indexBuffer = g_theRenderer->CreateIndexBuffer(static_cast<unsigned int>(m_charIndices.size()) * sizeof(unsigned int), sizeof(unsigned int));
}

Character::~Character()
{
	delete m_skeleton;
	m_skeleton = nullptr;

	delete m_aiController;
	m_aiController = nullptr;

	DeleteBuffers();
}

void Character::Update(float deltaSeconds)
{
	CharDebugPresses();

	// Setting characters on tiles
	m_currentTileCoords = m_map->GetTileCoordsFromWorldPos(m_position);

	// Grounded check
	if (m_position.z <= 0.f)
	{
		m_position.z = 0.f;
		m_charDefinition->m_isGrounded = true;
	}

	// Updating AI movement
	if (m_aiController)
	{
		m_aiController->Update(deltaSeconds);
	}

	// Updating physics if we are movable
	if (m_isMovable)
	{
		UpdatePhysics(deltaSeconds);
	}

	// Check animation state from updated velocity
	float speed = m_velocity.GetLength();
	if (speed > 0.1f)
	{
		m_animState = AnimationState::WALK;
	}
	else
	{
		m_animState = AnimationState::IDLE;
	}

	// Update animation and verts
	UpdateAnimation(deltaSeconds);

	if (m_debugCharForwardRaycast)
	{
		CheckObstaclesInFront();
	}
}

void Character::CharDebugPresses()
{
	if (g_theInput->WasKeyJustPressed('1'))
	{
		m_debugDrawCollisionCylinder = !m_debugDrawCollisionCylinder;
	}
	if (g_theInput->WasKeyJustPressed('2'))
	{
		m_debugCharForwardRaycast = !m_debugCharForwardRaycast;
	}
}

void Character::Render() const
{
	// Debug draw character names and current tasks above heads
	std::string taskLabel = Stringf("%s [%s]", m_charDefinition->m_characterName.c_str(), GetTaskName());
	DebugAddWorldBillboardText(taskLabel, GetEyePosition() + Vec3::ZAXE, 0.25f, Vec2(0.5f, 0.5f), 0.f);

	// Draw character overlay
	g_theRenderer->SetLightingConstants(m_sunDirection, CHARACTER_SUN_LIGHTING, CHARACTER_AMBIENT_LIGHTING);
	g_theRenderer->SetModelConstants(GetModelToWorldTransform());
	g_theRenderer->SetBlendMode(BlendMode::OPAQUE);
	g_theRenderer->SetRasterizerMode(RasterizerMode::SOLID_CULL_BACK);
	g_theRenderer->SetSamplerMode(SamplerMode::POINT_CLAMP);
	g_theRenderer->BindSampler(SamplerMode::POINT_CLAMP, 0);
	g_theRenderer->BindSampler(SamplerMode::BILINEAR_WRAP, 1);
	g_theRenderer->BindSampler(SamplerMode::BILINEAR_WRAP, 2);
	g_theRenderer->SetDepthMode(DepthMode::READ_WRITE_LESS_EQUAL);
	g_theRenderer->BindTexture(nullptr);
	g_theRenderer->BindShader(m_shader);
	SetSkinningConstants();
	g_theRenderer->DrawIndexedVertexBuffer(m_vertexBuffer, m_indexBuffer, static_cast<unsigned int>(m_charIndices.size()));

	// Debug draw for physics cylinder
	if (m_debugDrawCollisionCylinder)
	{
		g_theRenderer->SetRasterizerMode(RasterizerMode::WIREFRAME_CULL_BACK);
		g_theRenderer->BindTexture(nullptr);
		g_theRenderer->BindShader(nullptr);
		g_theRenderer->DrawVertexArray(m_physicsVerts);
	}

	if (m_debugCharForwardRaycast)
	{
		g_theRenderer->SetModelConstants();
		g_theRenderer->SetRasterizerMode(RasterizerMode::SOLID_CULL_BACK);
		g_theRenderer->BindTexture(nullptr);
		g_theRenderer->BindShader(nullptr);
		g_theRenderer->DrawVertexArray(m_raycastVerts);
	}
}

void Character::SetSkinningConstants() const
{
	SkinningConstants skinningConstants;

	for (int boneIndex = 0; boneIndex < static_cast<int>(m_skeleton->m_bones.size()); ++boneIndex)
	{
		skinningConstants.m_boneMatrices[boneIndex] = m_skeleton->m_bones[boneIndex].m_worldBoneTransform * m_skeleton->m_bones[boneIndex].m_inverseBindPose;
	}

	g_theRenderer->CopyCPUToGPU(&skinningConstants, sizeof(skinningConstants), m_skinningBuffer);
	g_theRenderer->BindConstantBuffer(5, m_skinningBuffer);
}

Mat44 Character::GetModelToWorldTransform() const
{
	Mat44 modelToWorldMatrix;
	modelToWorldMatrix.SetTranslation3D(m_position);
	modelToWorldMatrix.AppendScaleUniform3D(CHARACTER_SCALE);
	EulerAngles orientation;
	orientation.m_yawDegrees = m_orientation.m_yawDegrees;
	modelToWorldMatrix.Append(orientation.GetAsMatrix_IFwd_JLeft_KUp());
	return modelToWorldMatrix;
}

void Character::SetAIController(AIController* aiController)
{
	m_aiController = aiController;
}

AIController* Character::GetAIController() const
{
	return m_aiController;
}

void Character::SetCurrentTask(TaskType newTask)
{
	if (m_currentTask == newTask)
	{
		return;
	}

	m_previousTask = m_currentTask;
	m_currentTask = newTask;

	std::string taskString = Stringf("%s task changed to %s", m_charDefinition->m_characterName.c_str(), GetTaskName());
	DebugAddMessage(taskString, 10.f, Rgba8::GOLD);
}

char const* Character::GetTaskName() const
{
	switch (m_currentTask)
	{
		case TaskType::GOING_TO: return "Going to";
		case TaskType::SLEEP: return "Sleeping";
		case TaskType::EAT: return "Eating";
		case TaskType::WORK: return "Working";
		default: return "INVALID";
	}
}

void Character::UpdatePhysics(float deltaSeconds)
{
	// Add a drag force equal to our drag times our negative current velocity
	Vec3 dragForce = -m_dragForce * m_velocity;

	// Integrate acceleration, velocity, and position
	m_acceleration += dragForce;
	m_velocity += m_acceleration * deltaSeconds;
	m_position += m_velocity * deltaSeconds;

	// Clear out acceleration for next frame
	m_acceleration = Vec3::ZERO;
}

void Character::UpdateAnimation(float deltaSeconds)
{
	m_animationTimer += deltaSeconds;
	m_animationTimer = fmodf(m_animationTimer, 1000.f);

	// Reset to bind pose
	for (int boneIndex = 0; boneIndex < static_cast<int>(m_skeleton->m_bones.size()); ++boneIndex)
	{
		Bone& bone = m_skeleton->m_bones[boneIndex];
		bone.ResetToBindPose();
	}

	if (m_animState == AnimationState::IDLE)
	{
		m_skeleton->UpdateSkeletonPose();
		return;
	}
	else if (m_animState == AnimationState::WALK)
	{
		float angle = sinf(m_animationTimer * CHARACTER_CYCLE_RATE);

		Quat forwardSwing = Quat::MakeFromAxisAngle(-Vec3::YAXE, angle);
		Quat backwardSwing = Quat::MakeFromAxisAngle(-Vec3::YAXE, -angle);

		// Legs opposite phase
		m_skeleton->m_bones[1].m_localRotation = m_skeleton->m_bones[1].m_bindLocalRotation * forwardSwing;
		m_skeleton->m_bones[4].m_localRotation = m_skeleton->m_bones[4].m_bindLocalRotation * backwardSwing;

		float kneeAngleL = GetMax(0.f, angle) * 0.7f;
		float kneeAngleR = GetMax(0.f, -angle) * 0.7f;
		Quat kneeBendL = Quat::MakeFromAxisAngle(-Vec3::YAXE, -kneeAngleL);
		Quat kneeBendR = Quat::MakeFromAxisAngle(-Vec3::YAXE, -kneeAngleR);

		// Knee bend
		m_skeleton->m_bones[2].m_localRotation = m_skeleton->m_bones[2].m_bindLocalRotation * kneeBendL;
		m_skeleton->m_bones[5].m_localRotation = m_skeleton->m_bones[5].m_bindLocalRotation * kneeBendR;

		// Arms opposite phase
		// Shoulder swings big
		m_skeleton->m_bones[12].m_localRotation = m_skeleton->m_bones[12].m_bindLocalRotation * backwardSwing;
		m_skeleton->m_bones[15].m_localRotation = m_skeleton->m_bones[15].m_bindLocalRotation * forwardSwing;

		// Elbow bend
		Quat elbowBend = Quat::MakeFromAxisAngle(-Vec3::YAXE, fabsf(angle) * 0.6f);

		// Arm swing
		m_skeleton->m_bones[13].m_localRotation = m_skeleton->m_bones[13].m_bindLocalRotation * elbowBend;
        m_skeleton->m_bones[16].m_localRotation = m_skeleton->m_bones[16].m_bindLocalRotation * elbowBend;

		m_skeleton->UpdateSkeletonPose();
	}
}

void Character::CheckObstaclesInFront()
{
	m_raycastVerts.clear();

	Vec3 fwdDirection = GetForwardNormal();
	float rayDistance = m_physicsRadius * 0.5f;
	Vec3 rayEndPos = GetEyePosition() + fwdDirection * rayDistance;

	RaycastResult3D raycastResult = m_map->RaycastWorldCharacters(GetEyePosition(), fwdDirection, rayDistance, this);
	if (raycastResult.m_didImpact)
	{
		AddVertsForCylinder3D(m_raycastVerts, GetEyePosition(), raycastResult.m_impactPos, 0.1f, Rgba8::RED);
	}
	else
	{
		AddVertsForCylinder3D(m_raycastVerts, GetEyePosition(), rayEndPos, 0.1f, Rgba8::BLUE);
	}
}

void Character::AddImpulse(Vec3 appliedImpulse)
{
	m_velocity += appliedImpulse;
}

void Character::AddForce(Vec3 appliedForce)
{
	m_acceleration += appliedForce;
}

void Character::MoveInDirection(Vec3 const& direction, float speed)
{
	Vec3 movement = direction;
	movement.Normalize();
	float acceleration = speed * m_dragForce;
	AddForce(acceleration * movement);
}

void Character::TurnInDirection(Vec2 const& targetPosition, float maxTurnDegrees)
{
	Vec2 charPositionXY = m_position.GetXY();
	Vec2 charToTargetDisplacement = targetPosition - charPositionXY;
	float charOrientationToTarget = charToTargetDisplacement.GetOrientationDegrees();
	m_orientation.m_yawDegrees = GetTurnedTowardDegrees(m_orientation.m_yawDegrees, charOrientationToTarget, maxTurnDegrees);
}

void Character::TurnInDirection(Vec3 const& direction, float maxDegrees)
{
	Vec2 currentDirection = Vec2::MakeFromPolarDegrees(m_orientation.m_yawDegrees);
	float charGoalAngleDegrees = direction.GetAngleAboutZDegrees();
	float charTurnDegrees = GetTurnedTowardDegrees(m_orientation.m_yawDegrees, charGoalAngleDegrees, maxDegrees);
	m_orientation.m_yawDegrees = charTurnDegrees;
}

void Character::MoveTowardPosition(Vec3 const& targetPosition, float deltaSeconds)
{
	Vec3 displacement = targetPosition - m_position;
	displacement.z = MAP_GROUND_Z;

	float distance = displacement.GetLength();
	if (distance < ARRIVAL_THRESHOLD)
	{
		return;
	}

	Vec3 direction = displacement.GetNormalized();
	Vec3 forward = GetForwardNormal();
	float dot = DotProduct3D(forward, direction);

	if (dot < 0.99f)
	{
		TurnInDirection(direction, 180.f * deltaSeconds);
	}

	MoveInDirection(direction, m_charDefinition->m_walkSpeed);
}

Vec3 Character::GetForwardNormal() const
{
	return Vec3::MakeFromPolarDegrees(m_orientation.m_pitchDegrees, m_orientation.m_yawDegrees);
}

Vec3 Character::GetEyePosition() const
{
	return m_position + Vec3(0.f, 0.f, m_charDefinition->m_eyeHeight * CHARACTER_SCALE);
}

Skeleton* Character::CreateTestSkeleton()
{
	Skeleton* skeleton = new Skeleton();

	Bone root;
	root.m_boneName = "Root/Hip";
	root.SetLocalBonePosition(Vec3(0.f, 0.f, 2.75f));
	root.m_bindLocalPosition = root.m_localPosition;
	root.m_bindLocalRotation = root.m_localRotation;
	skeleton->m_bones.push_back(root);

	Bone lleg;
	lleg.m_boneName = "LLeg";
	lleg.m_parentBoneIndex = 0;
	lleg.SetLocalBonePosition(Vec3(0.f, 1.f, -0.5f));
	lleg.m_bindLocalPosition = lleg.m_localPosition;
	lleg.m_bindLocalRotation = lleg.m_localRotation;
	skeleton->m_bones.push_back(lleg);

	Bone lknee;
	lknee.m_boneName = "LKnee";
	lknee.m_parentBoneIndex = 1;
	lknee.SetLocalBonePosition(-Vec3::ZAXE);
	lknee.m_bindLocalPosition = lknee.m_localPosition;
	lknee.m_bindLocalRotation = lknee.m_localRotation;
	skeleton->m_bones.push_back(lknee);

	Bone lfoot;
	lfoot.m_boneName = "LFoot";
	lfoot.m_parentBoneIndex = 2;
	lfoot.SetLocalBonePosition(-Vec3::ZAXE);
	lfoot.m_bindLocalPosition = lfoot.m_localPosition;
	lfoot.m_bindLocalRotation = lfoot.m_localRotation;
	skeleton->m_bones.push_back(lfoot);

	Bone rleg;
	rleg.m_boneName = "RLeg";
	rleg.m_parentBoneIndex = 0;
	rleg.SetLocalBonePosition(Vec3(0.f, -1.f, -0.5f));
	rleg.m_bindLocalPosition = rleg.m_localPosition;
	rleg.m_bindLocalRotation = rleg.m_localRotation;
	skeleton->m_bones.push_back(rleg);

	Bone rknee;
	rknee.m_boneName = "RKnee";
	rknee.m_parentBoneIndex = 4;
	rknee.SetLocalBonePosition(-Vec3::ZAXE);
	rknee.m_bindLocalPosition = rknee.m_localPosition;
	rknee.m_bindLocalRotation = rknee.m_localRotation;
	skeleton->m_bones.push_back(rknee);

	Bone rfoot;
	rfoot.m_boneName = "RFoot";
	rfoot.m_parentBoneIndex = 5;
	rfoot.SetLocalBonePosition(-Vec3::ZAXE);
	rfoot.m_bindLocalPosition = rfoot.m_localPosition;
	rfoot.m_bindLocalRotation = rfoot.m_localRotation;
	skeleton->m_bones.push_back(rfoot);

	Bone spine3;
	spine3.m_boneName = "Spine3";
	spine3.m_parentBoneIndex = 0;
	spine3.SetLocalBonePosition(Vec3::ZAXE);
	spine3.m_bindLocalPosition = spine3.m_localPosition;
	spine3.m_bindLocalRotation = spine3.m_localRotation;
	skeleton->m_bones.push_back(spine3);

	Bone spine2;
	spine2.m_boneName = "Spine2";
	spine2.m_parentBoneIndex = 7;
	spine2.SetLocalBonePosition(Vec3::ZAXE);
	spine2.m_bindLocalPosition = spine2.m_localPosition;
	spine2.m_bindLocalRotation = spine2.m_localRotation;
	skeleton->m_bones.push_back(spine2);

	Bone spine1;
	spine1.m_boneName = "Spine1";
	spine1.m_parentBoneIndex = 8;
	spine1.SetLocalBonePosition(Vec3::ZAXE);
	spine1.m_bindLocalPosition = spine1.m_localPosition;
	spine1.m_bindLocalRotation = spine1.m_localRotation;
	skeleton->m_bones.push_back(spine1);

	Bone neck;
	neck.m_boneName = "Neck";
	neck.m_parentBoneIndex = 9;
	neck.SetLocalBonePosition(Vec3(0.f, 0.f, 0.5f));
	neck.m_bindLocalPosition = neck.m_localPosition;
	neck.m_bindLocalRotation = neck.m_localRotation;
	skeleton->m_bones.push_back(neck);

	Bone head;
	head.m_boneName = "Head";
	head.m_parentBoneIndex = 10;
	head.SetLocalBonePosition(Vec3(0.f, 0.f, 0.75f));
	head.m_bindLocalPosition = head.m_localPosition;
	head.m_bindLocalRotation = head.m_localRotation;
	skeleton->m_bones.push_back(head);

	Bone lShoulder;
	lShoulder.m_boneName = "LShoulder";
	lShoulder.m_parentBoneIndex = 9;
	lShoulder.SetLocalBonePosition(Vec3::YAXE);
	lShoulder.m_bindLocalPosition = lShoulder.m_localPosition;
	lShoulder.m_bindLocalRotation = lShoulder.m_localRotation;
	skeleton->m_bones.push_back(lShoulder);

	Bone lArm;
	lArm.m_boneName = "LArm";
	lArm.m_parentBoneIndex = 12;
	lArm.SetLocalBonePosition(Vec3(0.f, 0.25f, -1.f));
	lArm.m_bindLocalPosition = lArm.m_localPosition;
	lArm.m_bindLocalRotation = lArm.m_localRotation;
	skeleton->m_bones.push_back(lArm);

	Bone lHand;
	lHand.m_boneName = "LHand";
	lHand.m_parentBoneIndex = 13;
	lHand.SetLocalBonePosition(Vec3(0.f, 0.25f, -1.f));
	lHand.m_bindLocalPosition = lHand.m_localPosition;
	lHand.m_bindLocalRotation = lHand.m_localRotation;
	skeleton->m_bones.push_back(lHand);

	Bone rShoulder;
	rShoulder.m_boneName = "RShoulder";
	rShoulder.m_parentBoneIndex = 9;
	rShoulder.SetLocalBonePosition(-Vec3::YAXE);
	rShoulder.m_bindLocalPosition = rShoulder.m_localPosition;
	rShoulder.m_bindLocalRotation = rShoulder.m_localRotation;
	skeleton->m_bones.push_back(rShoulder);

	Bone rArm;
	rArm.m_boneName = "RArm";
	rArm.m_parentBoneIndex = 15;
	rArm.SetLocalBonePosition(Vec3(0.f, -0.25f, -1.f));
	rArm.m_bindLocalPosition = rArm.m_localPosition;
	rArm.m_bindLocalRotation = rArm.m_localRotation;
	skeleton->m_bones.push_back(rArm);

	Bone rHand;
	rHand.m_boneName = "RHand";
	rHand.m_parentBoneIndex = 16;
	rHand.SetLocalBonePosition(Vec3(0.f, -0.25f, -1.f));
	rHand.m_bindLocalPosition = rHand.m_localPosition;
	rHand.m_bindLocalRotation = rHand.m_localRotation;
	skeleton->m_bones.push_back(rHand);

	skeleton->UpdateSkeletonPose();

	for (int boneIndex = 0; boneIndex < static_cast<int>(skeleton->m_bones.size()); ++boneIndex)
	{
		Bone& bone = skeleton->m_bones[boneIndex];
		bone.m_bindWorldPosition = bone.m_worldBoneTransform.GetTranslation3D();
		bone.m_inverseBindPose = bone.m_worldBoneTransform.GetOrthonormalInverse();
	}

	return skeleton;
}

void Character::BuildCharacterMesh()
{
	m_charSkinnedVerts.clear();

	for (int boneIndex = 0; boneIndex < static_cast<int>(m_skeleton->m_bones.size()); ++boneIndex)
	{
		Bone const& bone = m_skeleton->m_bones[boneIndex];
		float radius = 0.25f;
		Vec3 bonePos = bone.m_bindWorldPosition;

		// Spheres for joints
		if (boneIndex == 11)
		{
			AddSkinnedVertsForIndexedSphere(m_charSkinnedVerts, m_charIndices, 0.45f, boneIndex, bonePos, Rgba8::SKIN, AABB2::ZERO_TO_ONE, 16, 16);
		}
		else
		{
			AddSkinnedVertsForIndexedSphere(m_charSkinnedVerts, m_charIndices, radius, boneIndex, bonePos, Rgba8::SKIN, AABB2::ZERO_TO_ONE, 12, 12);
		}

		// Cylinder to each parent bone
		if (bone.m_parentBoneIndex == -1)
		{
			continue;
		}

		Bone const& parent = m_skeleton->m_bones[bone.m_parentBoneIndex];
		Vec3 parentPos = parent.m_bindWorldPosition;
		Vec3 childPos = bone.m_bindWorldPosition;

		AddSkinnedVertsForIndexedCylinder_Blended(m_charSkinnedVerts, m_charIndices, parentPos, childPos, 0.26f, bone.m_parentBoneIndex, boneIndex, Rgba8::BROWN);
	}
}

void Character::UpdateGPUBuffer()
{
	if (!m_vertexBuffer)
	{
		return;
	}

	unsigned int sizeInBytes = static_cast<unsigned int>(m_charSkinnedVerts.size()) * sizeof(Vertex_PCUTBNSkinned);
	g_theRenderer->CopyCPUToGPU(m_charSkinnedVerts.data(), sizeInBytes, m_vertexBuffer);

	unsigned int indexBytes = static_cast<unsigned int>(m_charIndices.size()) * sizeof(unsigned int);
	g_theRenderer->CopyCPUToGPU(m_charIndices.data(), indexBytes, m_indexBuffer);
}

void Character::DeleteBuffers()
{
	delete m_vertexBuffer;
	m_vertexBuffer = nullptr;

	delete m_indexBuffer;
	m_indexBuffer = nullptr;

	delete m_skinningBuffer;
	m_skinningBuffer = nullptr;
}
