/*
 * Complete the 'superBitstrings' function below.
 *
 * The function is expected to return an INTEGER.
 * The function accepts following parameters:
 *  1. INTEGER n
 *  2. INTEGER_ARRAY bitStrings
 */
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

char* convertIntToBitString(int n, int integer)
{
    char* aux = (char*)malloc(n * sizeof(char));
    int i;
    int j = 0;
    for (i = (n - 1); i >= 0; i--)
    {
        int potence = pow(2, i);
        if (potence <= integer)
        {
            *(aux + j) = '1';
            integer -= potence;
        }
        else
        {
            *(aux + j) = '0';
        }
        j++;
    }
    return aux;
}

int intersects(int n)
{
    if (n == 1) return 2;

    int pos;
    if ((n % 2) == 1) pos = pow(n, 2) - 1;
    else pos = pow(n, 2);

    return pos;
}

int superBitstrings(int n, int bitStrings_count, int* bitStrings) {
    int i;
    int ammoutOfSuperBitStrings = 0;
    for (i = 0; i < bitStrings_count; i++)
    {
        char* bitString = convertIntToBitString(n, *(bitStrings + i));

        printf("%s\n", bitString);
        int j;
        int ammountOfZeros = 0;
        for (j = 0; j < n; j++)
        {
            if (*(bitString + j) == '0')
            {
                ammountOfZeros++;
            }
        }
        if (ammoutOfSuperBitStrings < intersects(ammountOfZeros))
        {
            ammoutOfSuperBitStrings = intersects(ammountOfZeros);
        }
    }

    return ammoutOfSuperBitStrings;
}

int main()
{
    int n = 5;
    int k = 3;
    int *bitString = (int*)malloc(k * sizeof(int));

    *(bitString + 0) = 2;
    *(bitString + 1) = 10;
    *(bitString + 2) = 26;

    int number = superBitstrings(n, k, bitString);

    printf("%d\n", number);

    system("pause");
    return 0;
}
