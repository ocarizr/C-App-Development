#include <stdlib.h>
#include <stdio.h>

void preencheVetor(int* vetor, int size);

void main()
{
	int size;
	
	scanf("%d", &size);
	
	int v[size];
	
	preencheVetor(v, size);
	
	int i;
	for (i = 0; i < size; i++)
	{
		printf("%d \n", v[i]);
	}
	
	system("pause");
}

void preencheVetor(int* vetor, int size) // Fill the array with the Fibonacci numbers
{
	int i;
	for (i = 0; i < size; i++)
	{
		if (i == 0)
		{
			vetor[i] = 0;
		} else if (i == 1)
		{
			vetor[i] = 1;
		} else
		{
			vetor[i] = vetor[i - 2] + vetor[i - 1];
		}
	}
}
