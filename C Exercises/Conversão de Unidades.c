#include <stdio.h>
#include <stdlib.h>

void main()
{
	float fMetros;
	
	// Input do valor em metros
	printf("Digite um valor em metros: ");
	scanf("%f", &fMetros);
	printf("\n");
	
	// Impressão dos valores convertidos para outras unidades
	printf("Conversao do valor para:\n");
	printf("Decimetros: %.1f\n", fMetros * 10);
	printf("Centimetros: %.1f\n", fMetros * 100);
	printf("Milimetros: %.1f\n", fMetros * 1000);
	
	// Pausa a execução
	system("pause");
}
