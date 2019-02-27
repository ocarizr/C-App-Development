#include <stdio.h>
#include <stdlib.h>

// Código para fazer input de strings no sistema
// Esse código reconhece espaço entre as palavras
void main()
{
	// Declara a variável que vai salvar a string
	char cPalavra[255];
	
	// Instrução ao usuário
	printf("Digite a palavra: ");
	
	// Limpa o buffer de leitura
	setbuf(stdin, 0);
	
	// Lê a string digitada pelo usuário
	fgets(cPalavra, 255, stdin);
	
	// Garante que utiliza somente o espaço necessário na memória
	cPalavra[strlen(cPalavra) - 1] = '\0';
	
	// Imprime a palavra digitada
	printf("A palavra digitada eh: %s\n", cPalavra);
	
	// Pausa a execução do código
	system("pause");
}
