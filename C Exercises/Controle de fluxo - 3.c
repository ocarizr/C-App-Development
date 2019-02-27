#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>

void main()
{
	// Declaração das variáveis
	float fVal1, fVal2;
	char cOp;
	
	// Input de informações
	printf("Digite o primeiro valor: ");
	scanf("%f", &fVal1);
	printf("Digite o segundo valor: ");
	scanf("%f", &fVal2);
	
	operacao:
	
	printf("Digite + - * / para selecionar a operacao: ");
	scanf(" %c", &cOp);
	
	switch(cOp)
	{
		case '+':
			printf("Resultado da operacao %.1f + %.1f = %.1f", fVal1, fVal2, fVal1 + fVal2);
			break;
		case '-':
			printf("Resultado da operacao %.1f - %.1f = %.1f", fVal1, fVal2, fVal1 - fVal2);
			break;
		case '*':
			printf("Resultado da operacao %.1f * %.1f = %.1f", fVal1, fVal2, fVal1 * fVal2);
			break;
		case '/':
			printf("Resultado da operacao %.1f / %.1f = %.1f", fVal1, fVal2, fVal1 / fVal2);
			break;
		default:
			printf("Operacao invalida.\n");
			goto operacao;
			break;
	}
	
	printf("\n");
	
	// Pausa a execução do código
	system("pause");
}
