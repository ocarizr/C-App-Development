#include <stdlib.h>
#include <stdio.h>

void main()
{
	int i = 0;
	
	while (i < 128)
	{
		printf ("%d - %x - %c\n", i, i, i);
		i++;
	}
	
	system("pause");
}
