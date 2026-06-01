#include "Game/GameCommon.h"

TaskType GetTaskTypeFromString(std::string const& taskString)
{
	if (taskString == "Sleep") return TaskType::SLEEP;
	if (taskString == "Eat") return TaskType::EAT;
	if (taskString == "Work") return TaskType::WORK;
	if (taskString == "Going_To") return TaskType::GOING_TO;
	return TaskType::INVALID_TASK;
}
