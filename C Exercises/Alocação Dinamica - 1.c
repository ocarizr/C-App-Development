#include <stdlib.h>
#include <stdio.h>

int* alocaVetor(int tamanho);

void main()
{
	int* vetor;
	int tamanho, i;
	
	printf("Digite um tamanho para o vetor: ");
	scanf("%d", &tamanho);
	
	vetor = alocaVetor(tamanho);
	
	for(i = 0; i < tamanho; i++)
	{
		vetor[i] = i * i;
		printf("Vetor[%d] = %d\n", i, vetor[i]);
	}
	
	free(vetor);
	
	system("pause");
}

int* alocaVetor(int tamanho)
{
	int* temp;
	
	temp = (int*)malloc(tamanho * sizeof(int));
	
	return temp;
}
