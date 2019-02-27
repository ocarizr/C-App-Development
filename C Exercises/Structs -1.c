#include <stdio.h>
#include <stdlib.h>

struct Endereco
{
	char Logradouro[100];
	int Numero;
	char Bairro[100];
	char Cidade[100];
	char UF[2]; 	
};

struct Perfil
{
	char Nome[100];
	int Idade;
	float Altura;
	struct Endereco endereco;
};

int main()
{
	struct Perfil perfil;
	
	printf("Digite o nome completo: ");
	gets(perfil.Nome);
	fflush(stdin);
	printf("Digite a idade: ");
	scanf("%d", &perfil.Idade);
	fflush(stdin);
	printf("Digite a altura: ");
	scanf("%f", &perfil.Altura);
	fflush(stdin);
	printf("Digite o logradouro: ");
	gets(perfil.endereco.Logradouro);
	fflush(stdin);
	printf("Digite o numero da residencia: ");
	scanf("%d", &perfil.endereco.Numero);
	fflush(stdin);
	printf("Digite o bairro: ");
	gets(perfil.endereco.Bairro);
	fflush(stdin);
	printf("Digite a cidade: ");
	gets(perfil.endereco.Cidade);
	fflush(stdin);
	printf("Digite a UF: ");
	scanf("%s", &perfil.endereco.UF);
	fflush(stdin);
	
	printf("\n");
	printf("Dados do individuo:\n");
	
	printf(
	"Nome: %s \nIdade: %d \nAltura: %.2fm \nEndereco: \nLogradouro: %s, %d \nBairro: %s \nCidade / UF: %s / %s \n", 
	perfil.Nome, 
	perfil.Idade, 
	perfil.Altura, 
	perfil.endereco.Logradouro, 
	perfil.endereco.Numero, 
	perfil.endereco.Bairro, 
	perfil.endereco.Cidade, 
	perfil.endereco.UF);
	
	return 0;
}
