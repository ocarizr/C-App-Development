#include "Department.h"
#include "Enums\WorkerLevel.h"
#include <string.h>

typdef struct Profession
{
	string Name;
	float Salary;
	WorkerLevel workerLevel;
	Department Department;
}Profession;
