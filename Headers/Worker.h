#include "Profession.h"
#include <stdlib.h>
#include "HourContract.h"
#include <string.h>

struct Worker
{
	char Name[100];
	int Age;
	struct Profession Profession;
	struct HourContract *Contracts;
};

void SetHourContract (struct HourContract *contracts, int size)
{
	contracts = (struct HourContract *) malloc(size * sizeof(struct HourContract));
}

void AddContract (struct HourContract *contracts, struct HourContract contract)
{
	int i = 0;
	
	while (contracts[i].Hours == 0)
	{
		i++;
	}
	contracts[i] = contract;
}

float TotalValue(struct HourContract *contracts, int size)
{
	float sum = 0.0f;
	int i;
	
	for (i = 0; i < size; i++) {
		sum += (contracts[i].Hours * contracts[i].HourValue);
	}
	
	return sum;
}
