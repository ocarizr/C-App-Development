#include "Department.h"
#include "Enums\WorkerLevel.h"
#include <string.h>

struct Profession
{
	char Name[100];
	float Salary;
	WorkerLevel workerLevel;
	struct Department Department;
};
