#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>

void main()
{
	// Declara a variavel
	float iVal;
	
	// atribui valor à variavel
	printf("Digite o valor do preco: R$");
	scanf("%f", &iVal);
	
	// Operador ternário para calcular o valor inflacionado
	iVal = (iVal < 100) ? (iVal * 1.1) : (iVal * 1.2);
	
	printf("Valor do preco inflacionado: R$%.1f\n", iVal);
	
	// pausa a execução do código
	system("pause");
}
