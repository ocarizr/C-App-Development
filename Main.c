#include <stdlib.h>
#include <stdio.h>
#include "\Headers\Worker.h"
#include "\Headers\HourContract.h"
#include <string.h>

int main()
{
	Worker worker;
	
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
			
			int i;
			for(i = 0; i < size; i++)
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
	printf("%s has %d years old. His profession is %s %s. He work at %s department and receive $%.2f as salary");
}
