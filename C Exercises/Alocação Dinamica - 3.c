#include <stdlib.h>
#include <stdio.h>

int main()
{
	int n = 3, m = 3, i, j;
	int **matrix;
	
	matrix = (int **)malloc(n * sizeof(int *));
	
	for (i = 0; i < n; i ++)
	{
		matrix[i] = (int *)malloc(m * sizeof(int));
	}
	
	for (i = 0; i < n; i++)
	{
		for (j = 0; j < m; j++)
		{
			printf("Value of M(%d,%d): ", i, j);
			scanf("%d", &matrix[i][j]);
		}
	}
	
	printf("\n");
	printf("M(%d,%d)=\n", n, m);
	
	for (i = 0; i < n; i++)
	{
		printf("|");
		for (j = 0; j < m; j++)
		{
			printf(" %d ", matrix[i][j]);
			
		}
		printf("|\n");
	}
	
	return 0;
}
