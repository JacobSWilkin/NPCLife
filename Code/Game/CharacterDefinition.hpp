#pragma once
#include "Game/GameCommon.h"
#include "Engine/Core/XmlUtils.hpp"
// -----------------------------------------------------------------------------
struct TaskScheduleEntry
{
	TaskType m_taskType = TaskType::INVALID_TASK;
	float    m_taskStartHour = 0.f;
	float    m_taskEndHour = 0.f;
};
// -----------------------------------------------------------------------------
struct CharacterDefinition
{
	CharacterDefinition(XmlElement const& characterDefElement);
	static std::vector<CharacterDefinition*> s_characterDefs;
	static void InitializeCharacterDefintions();
	static void ClearCharacterDefinitions();
	static CharacterDefinition* GetCharacterByName(std::string const& characterName);
	static CharacterDefinition* GetCharacterByID(int characterID);
// -----------------------------------------------------------------------------
	void ParseCollision(XmlElement const& characterDefElement);
	void ParsePhysics(XmlElement const& characterDefElement);
	void ParseCamera(XmlElement const& characterDefElement);
	void ParseTaskSchedule(XmlElement const& characterDefElement);
// -----------------------------------------------------------------------------
	std::string m_characterName = "default";
	int         m_characterID = -1;
	bool		m_isVisible = false;
	float		m_physicsRadius = 0.0f;
	float		m_physicsHeight = 0.0f;
	bool        m_collidesWithWorld = false;
	bool        m_collidesWithChars = false;
	bool		m_isGrounded = false;
	bool		m_isSimulated = false;
	float		m_walkSpeed = 0.0f;
	float       m_runSpeed  = 0.0f;
	float       m_eyeHeight = 0.0f;
	float		m_cameraFOV = 0.0f;
	std::vector<TaskScheduleEntry> m_taskSchedule;
};
// -----------------------------------------------------------------------------