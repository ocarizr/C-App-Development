#include "Profession.h"
#include "HourContract.h"
#include <string.h>

struct Worker
{
	string Name;
	int Age;
	Profession Profession;
	HourContract *Contracts;
};

void SetHourContract (HourContract *contracts, int size)
{
	contracts = (HourContract *) malloc(size * sizeof(HourContract));
}

void AddContract (HourContract *contracts, HourContract contract)
{
	int i = 0;
	
	while (contracts[i].Hours == 0)
	{
		i++;
	}
	contracts[i] = contract;
}

float TotalValue(HourContract *contracts, int size)
{
	float sum = 0.0f;
	int i;
	
	for (i = 0; i < size; i++) {
		sum += (contracts[i].Hours * contracts[i].HourValue);
	}
	
	return sum;
}
