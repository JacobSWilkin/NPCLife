#include "Game/CharacterDefinition.hpp"
#include "Engine/Core/ErrorWarningAssert.hpp"
// -----------------------------------------------------------------------------
std::vector<CharacterDefinition*> CharacterDefinition::s_characterDefs;
// -----------------------------------------------------------------------------
CharacterDefinition::CharacterDefinition(XmlElement const& characterDefElement)
{
	// Parsing general character information
	m_characterName = ParseXmlAttribute(characterDefElement, "name", m_characterName);
	m_characterID = ParseXmlAttribute(characterDefElement, "id", m_characterID);
	m_isVisible = ParseXmlAttribute(characterDefElement, "visible", m_isVisible);

	// Parsing Collision information
	ParseCollision(characterDefElement);

	// Parsing Physics information
	ParsePhysics(characterDefElement);

	// Parsing Camera information
	ParseCamera(characterDefElement);

	// Parsing Task Scheduling information
	ParseTaskSchedule(characterDefElement);
}

void CharacterDefinition::InitializeCharacterDefintions()
{
	XmlDocument characterDefsXml;
	char const* filePath = "Data/Definitions/CharacterDefinitions.xml";
	XmlError result = characterDefsXml.LoadFile(filePath);
	GUARANTEE_OR_DIE(result == tinyxml2::XML_SUCCESS, Stringf("Failed to open required character definitions file \"s\"", filePath));

	XmlElement* rootElement = characterDefsXml.RootElement();
	GUARANTEE_OR_DIE(rootElement, "RootElement not found!");

	XmlElement* charDefElement = rootElement->FirstChildElement();
	while (charDefElement)
	{
		std::string elementName = charDefElement->Name();
		GUARANTEE_OR_DIE(elementName == "CharacterDefinition", Stringf("Root child element in %s was <%s>, must be <CharacterDefinitions>!", filePath, elementName.c_str()));
		CharacterDefinition* newCharacterDef = new CharacterDefinition(*charDefElement);
		s_characterDefs.push_back(newCharacterDef);
		charDefElement = charDefElement->NextSiblingElement();
	}
}

void CharacterDefinition::ClearCharacterDefinitions()
{
	for (int charDefIndex = 0; charDefIndex < static_cast<int>(s_characterDefs.size()); ++charDefIndex)
	{
		delete s_characterDefs[charDefIndex];
		s_characterDefs[charDefIndex] = nullptr;
	}
	s_characterDefs.clear();
}

CharacterDefinition* CharacterDefinition::GetCharacterByName(std::string const& characterName)
{
	for (int charDefIndex = 0; charDefIndex < static_cast<int>(s_characterDefs.size()); ++charDefIndex)
	{
		if (s_characterDefs[charDefIndex]->m_characterName == characterName)
		{
			return s_characterDefs[charDefIndex];
		}
	}
	return nullptr;
}

CharacterDefinition* CharacterDefinition::GetCharacterByID(int characterID)
{
	for (int charDefIndex = 0; charDefIndex < static_cast<int>(s_characterDefs.size()); ++charDefIndex)
	{
		if (s_characterDefs[charDefIndex]->m_characterID == characterID)
		{
			return s_characterDefs[charDefIndex];
		}
	}
	return nullptr;
}

void CharacterDefinition::ParseCollision(XmlElement const& characterDefElement)
{
	XmlElement const* collisionElement = characterDefElement.FirstChildElement("Collision");
	if (!collisionElement)
	{
		return;
	}

	m_physicsRadius = ParseXmlAttribute(*collisionElement, "physicsRadius", m_physicsRadius);
	m_physicsHeight = ParseXmlAttribute(*collisionElement, "physicsHeight", m_physicsHeight);
	m_collidesWithWorld = ParseXmlAttribute(*collisionElement, "collidesWithWorld", m_collidesWithWorld);
	m_collidesWithChars = ParseXmlAttribute(*collisionElement, "collidesWithCharacters", m_collidesWithChars);
	m_isGrounded = ParseXmlAttribute(*collisionElement, "grounded", m_isGrounded);
}

void CharacterDefinition::ParsePhysics(XmlElement const& characterDefElement)
{
	XmlElement const* physicsElement = characterDefElement.FirstChildElement("Physics");
	if (!physicsElement)
	{
		return;
	}

	m_isSimulated = ParseXmlAttribute(*physicsElement, "simulated", m_isSimulated);
	m_walkSpeed = ParseXmlAttribute(*physicsElement, "walkSpeed", m_walkSpeed);
	m_runSpeed = ParseXmlAttribute(*physicsElement, "runSpeed", m_runSpeed);
}

void CharacterDefinition::ParseCamera(XmlElement const& characterDefElement)
{
	XmlElement const* cameraElement = characterDefElement.FirstChildElement("Camera");
	if (!cameraElement)
	{
		return;
	}

	m_eyeHeight = ParseXmlAttribute(*cameraElement, "eyeHeight", m_eyeHeight);
	m_cameraFOV = ParseXmlAttribute(*cameraElement, "cameraFOV", m_cameraFOV);
}

void CharacterDefinition::ParseTaskSchedule(XmlElement const& characterDefElement)
{
	XmlElement const* scheduleElement = characterDefElement.FirstChildElement("Schedule");
	if (scheduleElement)
	{
		for (XmlElement const* taskElement = scheduleElement->FirstChildElement("Task"); taskElement != nullptr; taskElement = taskElement->NextSiblingElement("Task"))
		{
			TaskScheduleEntry taskScheduleEntry;

			std::string taskTypeString = ParseXmlAttribute(*taskElement, "type", "INVALID_TASK");
			taskScheduleEntry.m_taskType = GetTaskTypeFromString(taskTypeString);

			taskScheduleEntry.m_taskStartHour = ParseXmlAttribute(*taskElement, "start", 0.f);
			taskScheduleEntry.m_taskEndHour = ParseXmlAttribute(*taskElement, "end", 0.f);

			m_taskSchedule.push_back(taskScheduleEntry);
		}
	}
}
