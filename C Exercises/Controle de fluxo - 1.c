#include <stdlib.h>
#include <stdio.h>

void main()
{
	int iVal1, iVal2, cont;
	
	printf("Digite o primeiro valor: ");
	scanf("%d", &iVal1);
	printf("Digite o segundo valor: ");
	scanf("%d", &iVal2);
	
	if (iVal1 < iVal2)
	{
		cont = iVal1;
		while (cont <= iVal2)
		{
			printf("%d\n", cont);
			cont++;
		}
		
	} else if (iVal1 > iVal2)
	{
		cont = iVal2;
		while (cont <= iVal1)
		{
			printf("%d\n", cont);
			cont++;
		}
	} else
	{
		printf("Valores iguais.");
	}
	
	system("pause");
}
