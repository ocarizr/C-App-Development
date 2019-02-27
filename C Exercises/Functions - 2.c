#include <stdlib.h>
#include <stdio.h>

void alterA(int* a);

void main()
{
	int a;
	
	scanf("%d", &a);
	
	printf("Valor inicial = %d \n", a);
	
	alterA(&a);
	
	printf("Valor final = %d \n", a);
	
	system("pause");
}

void alterA(int* a)
{
	*a += 10;
}
