#include <stdlib.h>
#include <stdio.h>
#include "Headers\Worker.h"
#include "Headers\HourContract.h"
#include <string.h>

int main()
{
	Worker worker;
	int index;
	
	printf("Worker's name: ");
	scanf("%s", &worker.Name);
	printf("Worker's age: ");
	scanf("%d", &worker.Age);
	printf("Worker's Profession: ");
	scanf("%s", &worker.Profession.Name);
	printf("Worker's Level: ");
	string workerLevel;
	scanf("%s", &workerLevel);
	
	if (strcmp(workerLevel, "Trainee"))
	{
		worker.Profession.workerLevel = 0;
	} else if (strcmp(workerLevel, "Junior"))
	{
		worker.Profession.workerLevel = 1;
	} else if (strcmp(workerLevel, "MidLevel"))
	{
		worker.Profession.workerLevel = 2;
	} else if (strcmp(workerLevel, "Senior"))
	{
		worker.Profession.workerLevel = 3;
	}
	
	printf("Worker's Salary: ");
	scanf("%f", &worker.Profession.Salary);
	printf("Worker's Department: ");
	scanf("%s", &worker.Profession.Department.Name);
	
	char option;
	
	while (option != 'n' || option != 'y')
	{
		printf("There's any freelancer contract for this worker(y/n)? ");
		scanf("%c", &option);
		
		if (option != 'y' && option != 'n')
		{
			printf("Invalid input.");
		} else if (option == 'n')
		{
			printf ("That's fine, thanks.")
		} else
		{
			int size;
			
			printf("How many contracts? ");
			scanf("%d", &size);
			SetHourContract(&worker.Contracts, size);

			for(index = 0; index < size; index++)
			{
				HourContract contract;
				
				printf("How many hours fot the contract number %d: ", (i + 1));
				scanf("%d", &contract.Hours);
				printf("How many he received by hour: ");
				scanf("%d", &contract.HourValue);
				
				AddContract(&worker.Contracts, contract);
			}
		}
	}
	
	printf("\n");
	printf("%s has %d years old.\n", worker.Name, worker.Age);
	printf("His profession is %s %s.\n", worker.Profession.workerLevel, worker.Profession.Name);
	printf("He work at %s department and receive $%.2f as salary.\n", worker.Profession.Department.Name, worker.Profession.Salary);
	printf("Worker freelancer contracts:\n");
	
	index = 0;
	while (worker.Contracts[index] != null)
	{
		printf("Contract #%d:\n", index + 1);
		printf("Hours: %d\n", worker.Contracts[index].Hours);
		printf("Revenue per hour: $%d\n", worker.Contracts[index].HourValue);
		printf("Total Revenue: $%d\n", worker.Contracts[index].Hours * worker.Contracts[index].HourValue);
		printf("\n");
	}
}
