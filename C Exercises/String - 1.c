#include <stdio.h>
#include <stdlib.h>

// C�digo para fazer input de strings no sistema
// Esse c�digo reconhece espa�o entre as palavras
void main()
{
	// Declara a vari�vel que vai salvar a string
	char cPalavra[255];
	
	// Instru��o ao usu�rio
	printf("Digite a palavra: ");
	
	// Limpa o buffer de leitura
	setbuf(stdin, 0);
	
	// L� a string digitada pelo usu�rio
	fgets(cPalavra, 255, stdin);
	
	// Garante que utiliza somente o espa�o necess�rio na mem�ria
	cPalavra[strlen(cPalavra) - 1] = '\0';
	
	// Imprime a palavra digitada
	printf("A palavra digitada eh: %s\n", cPalavra);
	
	// Pausa a execu��o do c�digo
	system("pause");
}
