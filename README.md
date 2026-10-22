# Workforce — gerenciamento de equipe em C

Aplicativo de console em **C11** para cadastrar funcionários, organizar seus
cargos e departamentos e gerenciar contratos de trabalho por hora. O projeto
tem um único ponto de entrada da aplicação em [`app/main.c`](app/main.c).
Os executáveis de teste têm seus próprios pontos de entrada.

## Funcionalidades

- Cadastrar e listar funcionários com nome completo, idade, cargo, departamento,
  nível de experiência e salário mensal.
- Identificar funcionários por IDs estáveis durante a sessão, sem reutilizar IDs
  removidos.
- Adicionar e remover contratos por funcionário, com ano, mês, horas e valor/hora.
- Consultar a remuneração de cada funcionário e o total da equipe em um mês:
  **salário base + soma dos contratos daquele ano e mês**.
- Remover funcionários e liberar seus contratos.

Os dados ficam **somente em memória** e são descartados ao sair. Não há banco de
dados, cálculo de impostos, conversão de moeda ou regras legais de folha de pagamento.
O salário base informado vale para qualquer mês consultado; não há histórico salarial.

## Compilar e executar

Requisitos: CMake **3.20 ou superior** e compilador C11 (GCC, Clang ou MSVC com
suporte a C11). Não são necessárias bibliotecas externas ou downloads no build.
No Windows, instale as ferramentas de C++ do Visual Studio, que incluem o
compilador C, o Windows SDK e a opção de ferramentas CMake.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON
cmake --build build --config Debug --parallel
ctest --test-dir build -C Debug --output-on-failure --no-tests=error
```

Execute no Linux/macOS ou com um gerador de configuração única:

```sh
./build/workforce
```

No PowerShell, com o gerador do Visual Studio:

```powershell
.\build\Debug\workforce.exe
```

O CMake deve estar no `PATH`. Se estiver usando o compilador MSVC com Ninja,
abra um Developer PowerShell do Visual Studio. Para compilar em Release,
troque `Debug` por `Release` nos comandos; com geradores de configuração única,
use outro diretório, por exemplo `build-release`.

Para compilar apenas o aplicativo, configure com `-DBUILD_TESTING=OFF`.

## Usar o console

O menu permite cadastrar, listar, adicionar/remover contratos, consultar
remuneração e remover funcionários. Digite `0` para sair. EOF também encerra a
sessão e libera a memória, inclusive durante um cadastro incompleto.

Exemplo: cadastre Ana Silva, 30 anos, cargo Software Developer, departamento
Engineering, nível `Senior` e salário `3000.00`. Adicione um contrato ao ID
retornado, para outubro de 2026, com 10 horas a `25.50`. A consulta de outubro
retorna `3255.00`; em novembro, sem contratos, retorna `3000.00`.

- Níveis: `Trainee`, `Junior`, `MidLevel`, `Senior` (grafia exata).
- Valores monetários: `1250`, `1250.5` ou `1250.50`; use ponto decimal, sem
  separadores de milhar ou símbolo de moeda. Todos os valores usam a mesma moeda.
- Nomes, cargos e departamentos: até 99 bytes, com espaços internos permitidos.
  Espaços nas extremidades são removidos. O limite em bytes pode equivaler a menos
  caracteres quando há acentos em UTF-8.
- Idades: 1 a 150; anos: 1 a 9999; meses: 1 a 12; horas: inteiro positivo.
  Salário e valor/hora podem ser zero, mas nunca negativos.
- Para remover um contrato, consulte seu número na opção de listagem.
  Esses números começam em 1 e mudam após uma remoção.

Entradas inválidas são solicitadas novamente. Falhas do domínio são informadas
sem efetivar a operação. Valores que excederiam os limites de `int64_t` em
centavos são rejeitados, incluindo o total mensal da equipe.

## Arquitetura

```text
app/main.c          entrada do aplicativo
app/console.h       interface da sessão de console
src/console.c       menu, leitura, validação textual e apresentação
include/workforce.h modelos e operações de funcionários, contratos e equipe
src/workforce.c     regras do domínio e gerenciamento de memória
include/money.h     interface de conversão de valores monetários
src/money.c         conversão decimal exata para centavos
tests/              testes em C, executados pelo CTest
.github/workflows/  integração contínua
```

`workforce_domain` não depende de entrada/saída. `workforce_console` depende do
domínio e recebe `FILE *` para entrada e saída, permitindo testar o mesmo fluxo
usado pelo aplicativo. As implementações ficam em `.c`; os `.h` contêm apenas
declarações, tipos e contratos públicos, protegidos contra inclusão repetida.

O estilo mantém structs e funções que recebem ponteiros, sem simular herança.
`Team` é inicializado com `{0}` e possui a memória dos funcionários; cada
`Worker` possui seu vetor de contratos. As coleções crescem dinamicamente.
`team_dispose` libera toda a árvore de objetos. Não copie structs proprietárias
por atribuição no código cliente nem altere diretamente seus campos: use as
operações públicas. Os campos ficam visíveis para consulta nesta primeira versão.
Ponteiros retornados por `team_find` devem ser obtidos novamente após adicionar
ou remover funcionários, pois o vetor pode mudar de endereço ou posição.

Operações retornam `WorkforceStatus`. Erros de validação, alocação e aritmética
não alteram o estado lógico. Valores monetários usam `int64_t` em centavos, sem
arredondamento de ponto flutuante. Um contrato pertence a um único mês; seu valor
é a multiplicação das horas inteiras pelo valor/hora em centavos.

## Testes e integração contínua

Os testes são escritos em C e não precisam de framework externo. As verificações
continuam ativas em Release, independentemente de `NDEBUG`.

- **Domínio:** níveis, validação dos modelos, conversão monetária, contratos,
  filtragem por mês/ano, crescimento das coleções, remoções, IDs e overflow.
- **Console:** sessões completas com vários funcionários, remuneração, remoções,
  entradas inválidas, linhas longas, fim de entrada e overflow do total da equipe.

No GCC/Clang com suporte aos sanitizers, também é possível executar:

```sh
cmake -S . -B build-sanitized -DCMAKE_BUILD_TYPE=Debug -DWORKFORCE_SANITIZERS=ON
cmake --build build-sanitized --parallel
ctest --test-dir build-sanitized --output-on-failure --no-tests=error
```

A [pipeline](.github/workflows/ci.yml) executa em pull requests direcionados à
`master`, pushes na `master` e manualmente. Compila e testa em Linux e Windows,
em Debug e Release; um job Linux adicional usa Clang com AddressSanitizer,
UndefinedBehaviorSanitizer e detecção de vazamentos. Warnings de compilação
são tratados como erros. O workflow usa [`actions/checkout`](https://github.com/actions/checkout)
e os comandos oficiais do [CTest](https://cmake.org/cmake/help/latest/manual/ctest.1.html).

Para impedir merges com falhas, configure no GitHub uma regra de proteção da
`master` exigindo os checks desta pipeline; o arquivo de workflow por si só não
configura a proteção da branch.

## Próximas extensões

A primeira extensão implementada transforma o cadastro isolado em uma equipe
com múltiplos funcionários e remuneração mensal. A evolução proposta está em
[`docs/design.md`](docs/design.md): persistência, edição e histórico de salários,
mantendo regras de domínio independentes do console.

Os exercícios de HackerRank/VanHack e os binários antigos foram removidos da
árvore atual. Permanecem no histórico Git. Alocação dinâmica, structs, strings,
buscas e agregações agora são aplicadas no domínio do aplicativo e nos testes.
O `.gitignore` impede que novos artefatos de compilação sejam versionados.
