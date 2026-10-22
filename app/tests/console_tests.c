#include "console.h"
#include "test.h"

#include <string.h>

static FILE *temporary_file(void)
{
    FILE *file = NULL;
#ifdef _MSC_VER
    CHECK(tmpfile_s(&file) == 0);
#else
    file = tmpfile();
#endif
    CHECK(file != NULL);
    return file;
}

static void session(const char *script, char *output, size_t capacity)
{
    FILE *input = temporary_file();
    FILE *result = temporary_file();
    size_t length;
    WorkforceStore *store = NULL;
    StoreOptions options = {":memory:", 0};
    bool created;
    char error[WORKFORCE_STORE_ERROR_SIZE];
    CHECK(store_open(&options, &store, &created, error) == WORKFORCE_OK);
    CHECK(fputs(script, input) >= 0);
    CHECK(fseek(input, 0, SEEK_SET) == 0);
    CHECK(console_run(input, result, store) == EXIT_SUCCESS);
    store_close(store);
    CHECK(fseek(result, 0, SEEK_SET) == 0);
    length = fread(output, 1, capacity - 1, result);
    output[length] = '\0';
    CHECK(!ferror(result));
    CHECK(fgetc(result) == EOF); /* Fail instead of silently truncating a transcript. */
    CHECK(fclose(input) == 0);
    CHECK(fclose(result) == 0);
}

static void test_full_session(void)
{
    char output[20000];
    session("2\n"
            "1\nAna Silva\n30\nSoftware Developer\nEngineering\nSenior\n3000.00\n"
            "1\nBruno Costa\n25\nQA Engineer\nQuality Assurance\nJunior\n2000\n"
            "3\n1\n2026\n10\n10\n25.50\n"
            "3\n1\n2026\n11\n3\n10\n"
            "2\n5\n2026\n10\n4\n1\n1\n5\n2026\n10\n"
            "6\n1\n2\n5\n2026\n10\n0\n",
            output, sizeof(output));
    CHECK(strstr(output, "Nenhum funcionario cadastrado.") != NULL);
    CHECK(strstr(output, "ID: 1") != NULL && strstr(output, "ID: 2") != NULL);
    CHECK(strstr(output, "Senior Software Developer | Engineering") != NULL);
    CHECK(strstr(output, "Contrato 1 | 2026-10 | 10 horas | Valor/hora: 25.50") != NULL);
    CHECK(strstr(output, "Ana Silva: 3255.00") != NULL);
    CHECK(strstr(output, "Total da equipe: 5255.00") != NULL);
    CHECK(strstr(output, "Total da equipe: 5000.00") != NULL);
    CHECK(strstr(output, "Total da equipe: 2000.00") != NULL);
    CHECK(strstr(output, "Sessao encerrada.") != NULL);
}

static void test_bad_input(void)
{
    char output[20000];
    session("banana\n7\n1\n\nAna\nabc\n0\n30\nDeveloper\nEngineering\n"
            "senior\nSenior\nnan\n-1\n1,50\n1.001\n1000.50\n"
            "3\n999\n4\n1\n1\n6\n999\n0\n",
            output, sizeof(output));
    CHECK(strstr(output, "Informe um inteiro") != NULL);
    CHECK(strstr(output, "Texto vazio") != NULL);
    CHECK(strstr(output, "Nivel invalido.") != NULL);
    CHECK(strstr(output, "Valor invalido.") != NULL);
    CHECK(strstr(output, "Funcionario cadastrado. ID: 1") != NULL);
    CHECK(strstr(output, "Registro nao encontrado.") != NULL);
}

static void test_long_line_and_eof(void)
{
    char script[600];
    char output[20000];
    size_t i;
    script[0] = '1';
    script[1] = '\n';
    for (i = 2; i < 502; ++i)
    {
        script[i] = 'x';
    }
    memcpy(script + 502, "\nAna\n30\nDeveloper\nEngineering\nJunior\n1\n0\n",
           sizeof("\nAna\n30\nDeveloper\nEngineering\nJunior\n1\n0\n"));
    session(script, output, sizeof(output));
    CHECK(strstr(output, "Entrada longa demais.") != NULL);
    CHECK(strstr(output, "Funcionario cadastrado. ID: 1") != NULL);
    session("1\nAna\n30\nDeveloper\nEngineering\nJunior\n1\n3\n1\n2026\n", output, sizeof(output));
    CHECK(strstr(output, "Sessao encerrada.") != NULL);
    session("", output, sizeof(output));
    CHECK(strstr(output, "Sessao encerrada.") != NULL);
    session("0", output, sizeof(output));
    CHECK(strstr(output, "Sessao encerrada.") != NULL);
}

static void test_payroll_overflow(void)
{
    char output[20000];
    session("1\nAna\n30\nDeveloper\nEngineering\nSenior\n92233720368547758.07\n"
            "3\n1\n2026\n10\n1\n0.01\n"
            "1\nBruno\n30\nDeveloper\nEngineering\nJunior\n1\n"
            "5\n2026\n10\n0\n",
            output, sizeof(output));
    CHECK(strstr(output, "Limite numerico excedido.") != NULL);
    CHECK(strstr(output, "Total da equipe:") == NULL);
}

int main(void)
{
    test_full_session();
    test_bad_input();
    test_long_line_and_eof();
    test_payroll_overflow();
    CHECK(console_run(NULL, stdout, NULL) == EXIT_FAILURE);
    CHECK(console_run(stdin, NULL, NULL) == EXIT_FAILURE);
    puts("Console tests passed.");
    return EXIT_SUCCESS;
}
