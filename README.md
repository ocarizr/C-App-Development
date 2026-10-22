# Workforce — gerenciamento de equipe em C

[![Build Debug](https://github.com/ocarizr/C-App-Development/actions/workflows/build-debug.yml/badge.svg?branch=master&event=push)](https://github.com/ocarizr/C-App-Development/actions/workflows/build-debug.yml)
[![Build Release](https://github.com/ocarizr/C-App-Development/actions/workflows/build-release.yml/badge.svg?branch=master&event=push)](https://github.com/ocarizr/C-App-Development/actions/workflows/build-release.yml)
[![Testes](https://github.com/ocarizr/C-App-Development/actions/workflows/tests.yml/badge.svg?branch=master&event=push)](https://github.com/ocarizr/C-App-Development/actions/workflows/tests.yml)
[![Formatação](https://github.com/ocarizr/C-App-Development/actions/workflows/format.yml/badge.svg?branch=master&event=push)](https://github.com/ocarizr/C-App-Development/actions/workflows/format.yml)
[![Análise estática](https://github.com/ocarizr/C-App-Development/actions/workflows/static-analysis.yml/badge.svg?branch=master&event=push)](https://github.com/ocarizr/C-App-Development/actions/workflows/static-analysis.yml)

Aplicativo de console em **C11** para cadastrar funcionários, organizar seus
cargos e departamentos e gerenciar contratos de trabalho por hora. O projeto
tem um único ponto de entrada da aplicação em [`app/main.c`](app/main.c).
Os executáveis de teste têm seus próprios pontos de entrada.

## Funcionalidades

- Cadastrar e listar funcionários com nome completo, idade, cargo, departamento,
  nível de experiência e salário mensal.
- Identificar funcionários por IDs persistentes, sem reutilizar IDs
  removidos.
- Adicionar e remover contratos por funcionário, com ano, mês, horas e valor/hora.
- Consultar a remuneração de cada funcionário e o total da equipe em um mês:
  **salário base + soma dos contratos daquele ano e mês**.
- Remover funcionários e liberar seus contratos.
- Consultar um funcionário por ID, editar seu perfil e visualizar seu histórico salarial.
- Cadastrar, corrigir e remover salários com competência de vigência.

Os dados ficam em um **banco SQLite local**, com gravação imediata de funcionários
e contratos. Ao reiniciar, a aplicação consulta os registros persistidos.
Não há cálculo de impostos, conversão de moeda ou regras legais de folha de pagamento.
O salário inicial vale até a primeira mudança cadastrada. A remuneração de cada
mês usa o salário vigente naquela competência, somado aos contratos do período.

## Compilar e executar

Requisitos: CMake **3.20 ou superior** e compilador C11 (GCC, Clang ou MSVC com
suporte a C11). Na primeira configuração, o CMake baixa os fontes de SQLite
**3.54.1** e libyaml **0.2.5**, com versões e hashes SHA-256 fixados.
Ambas as dependências são compiladas como bibliotecas estáticas; não é necessário
instalar SQLite, libyaml ou um servidor de banco na máquina.
No Windows, instale as ferramentas de C++ do Visual Studio, que incluem o
compilador C, o Windows SDK e a opção de ferramentas CMake.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON
cmake --build build --config Debug --parallel
ctest --test-dir build -C Debug --output-on-failure --no-tests=error
```

Execute no Linux/macOS ou com um gerador de configuração única:

```sh
./build/app/workforce
```

No PowerShell, com o gerador do Visual Studio:

```powershell
.\build\app\Debug\workforce.exe
```

O CMake deve estar no `PATH`. Se estiver usando o compilador MSVC com Ninja,
abra um Developer PowerShell do Visual Studio. Para compilar em Release,
troque `Debug` por `Release` nos comandos; com geradores de configuração única,
use outro diretório, por exemplo `build-release`.

Para compilar apenas o aplicativo, configure com `-DBUILD_TESTING=OFF`.

## Configuração e banco de dados

Execute a partir da raiz do repositório, onde está o [`app.yaml`](app.yaml).
O CMake também copia esse arquivo para a raiz do diretório de build, permitindo
executar a partir dele. Sem argumentos, a aplicação procura `app.yaml` no diretório
atual. Um arquivo ausente ou inválido encerra a inicialização com uma mensagem de erro.

```yaml
database:
  path: workforce.db
  busy_timeout_ms: 5000
```

SQLite usa um arquivo local, sem `localhost`, usuário ou senha. O caminho relativo
do banco é resolvido a partir do diretório do YAML que definiu `database.path`.
O diretório pai deve existir; o arquivo do banco e as tabelas são criados se necessário.
O console avisa quando um banco foi criado e quando conectou a um banco existente.

É possível carregar configurações próprias ou sobreposições de bibliotecas:

```powershell
.\build\app\Debug\workforce.exe --config app.yaml --config infra.local.yaml
```

Cada arquivo sobrescreve apenas as opções que informa. Por exemplo,
`infra.local.yaml` pode conter somente `database: {busy_timeout_ms: 10000}`.
Os arquivos informados com `--config` substituem a busca automática pelo `app.yaml`;
use o primeiro argumento para carregá-lo explicitamente quando quiser sobrepor valores.
Chaves desconhecidas, duplicadas e mais de um documento YAML são rejeitados.
`busy_timeout_ms` aceita 0 a 60000. A configuração do aplicativo exige um caminho
persistente e rejeita `:memory:` e URIs SQLite.

Arquivos `*.local.yaml`, bancos e seus arquivos auxiliares são ignorados pelo Git.
O arquivo versionado contém somente valores locais de exemplo. As opções são
injetadas na biblioteca de infraestrutura; ela não lê YAML nem imprime no console.
Consulte [persistência e arquitetura](docs/persistence.md) para detalhes do esquema,
das bibliotecas estáticas e do build sem acesso à rede.

## Usar o console

O menu permite cadastrar, listar, adicionar/remover contratos, consultar
remuneração e remover funcionários. Digite `0` para sair. EOF também encerra a
sessão e libera a memória, inclusive durante um cadastro incompleto.

Exemplo: cadastre Ana Silva, 30 anos, cargo Software Developer, departamento
Engineering, nível `Senior` e salário `3000.00`. Adicione um contrato ao ID
retornado, para outubro de 2026, com 10 horas a `25.50`. A consulta de outubro
retorna `3255.00`; em novembro, sem contratos, retorna `3000.00`.

### Perfil e histórico salarial

| Opção | Ação |
| --- | --- |
| 7 | Consultar um funcionário pelo ID, com contratos e histórico salarial |
| 8 | Editar nome, idade, cargo, departamento ou nível, um campo por vez |
| 9 | Cadastrar ou corrigir o salário de uma competência (ano e mês) |
| 10 | Remover uma vigência salarial pelo ano e mês |

Na opção 8, `0` cancela a escolha do campo. EOF encerra a sessão sem gravar uma
edição incompleta. O salário não é alterado pela edição de perfil: use a opção 9
para informar sua vigência.

Exemplo: com salário inicial `3000.00`, cadastre `4000.00` a partir de novembro
de 2026 e `4500.00` a partir de janeiro de 2027. Outubro continua usando `3000.00`,
novembro e dezembro usam `4000.00`, e janeiro de 2027 em diante usa `4500.00`.
O histórico pode ser informado fora de ordem e é exibido cronologicamente.
Informar novembro de 2026 novamente, com `4200.00`, corrige aquela vigência sem
criar duplicata nem alterar o salário de outubro ou a vigência de janeiro.

Ao remover uma vigência, o salário anterior volta a valer até a mudança seguinte.
Salário zero e reduções são permitidos. Mudanças retroativas, correções e remoções
são rejeitadas se provocarem overflow em algum mês com contratos existentes.
O histórico é de **valores vigentes**, e não uma trilha de auditoria de cada
correção. Nome, cargo e demais campos do perfil representam o cadastro atual.

Na primeira abertura, bancos da versão anterior são migrados automaticamente
do esquema 1 para o 2 em uma transação, preservando os dados existentes.

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
app/                main.c, console e testes do aplicativo
libs/domain/        modelos, regras, dinheiro e testes do domínio
libs/config/        leitura e sobreposição de YAML, com testes próprios
libs/infra/         repositório SQLite e testes próprios
tests/support/      utilitários compartilhados dos testes
cmake/              dependências estáticas com versões e hashes fixos
app.yaml            configuração local de exemplo
.github/workflows/  integração contínua
```

`workforce_domain` não depende de entrada/saída. `workforce_config` lê YAML com
libyaml. `workforce_infra` implementa o repositório usando SQLite e recebe as
opções por injeção. `main.c` compõe as bibliotecas e notifica o usuário sobre a conexão.
`workforce_console` recebe `FILE *` para entrada/saída e uma conexão de repositório.
As implementações ficam em `.c`; os `.h` contêm apenas
declarações, tipos e contratos públicos, protegidos contra inclusão repetida.

O estilo mantém structs e funções que recebem ponteiros, sem simular herança.
`Team` é inicializado com `{0}` e possui a memória dos funcionários; cada
`Worker` possui seus contratos e seu histórico salarial. As coleções crescem dinamicamente.
`team_dispose` libera toda a árvore de objetos. Não copie structs proprietárias
por atribuição no código cliente nem altere diretamente seus campos: use as
operações públicas. Os campos ficam visíveis para consulta nesta primeira versão.
O console não mantém uma equipe global em memória: listagens, buscas e consultas
mensais recebem snapshots novos do banco, descartados após o uso. Mutação ocorre
por operações do repositório com parâmetros vinculados e transações. A remoção de
um funcionário exclui seus contratos por chave estrangeira com `ON DELETE CASCADE`.

Operações retornam `WorkforceStatus`. Erros de validação, alocação e aritmética
não alteram o estado lógico. Valores monetários usam `int64_t` em centavos, sem
arredondamento de ponto flutuante. Um contrato pertence a um único mês; seu valor
é a multiplicação das horas inteiras pelo valor/hora em centavos.

No DDD (Domain-Driven Design), `Worker` é o agregado que protege as regras de
perfil, contratos e vigências salariais. `SalaryChange` representa um valor com
competência. Essas regras ficam em `libs/domain`, o core da aplicação;
infraestrutura e configuração são módulos técnicos separados. O repositório
SQLite coordena transações e chama o domínio antes de gravar. O console coleta
comandos e apresenta os resultados. Veja o [design](docs/design.md).

## Testes e integração contínua

Os testes são escritos em C e não precisam de framework externo. As verificações
continuam ativas em Release, independentemente de `NDEBUG`.

- **Domínio:** níveis, validação dos modelos, conversão monetária, contratos,
  filtragem por mês/ano, crescimento das coleções, remoções, IDs e overflow.
- **Console:** sessões completas com vários funcionários, remuneração, remoções,
  entradas inválidas, linhas longas, fim de entrada e overflow do total da equipe.
- **Configuração:** YAML, sobreposição, caminhos relativos/absolutos, chaves
  duplicadas/desconhecidas, arquivos ausentes e valores inválidos.
- **Infraestrutura:** reabertura, IDs persistentes, visibilidade entre conexões,
  exclusão em cascata, overflow, rollback, bloqueios e bancos incompatíveis.
- **Integração:** executa o aplicativo em processos separados, verifica os avisos
  de criação/conexão e confirma persistência de cadastros, contratos e exclusões.
- **Histórico no domínio:** consultas antes/durante/depois de cada vigência,
  correções, remoções, edição do perfil e rejeição atômica de overflow.
- **Histórico na infraestrutura:** persistência de edições, migração de bancos
  antigos, rollback de migração/escrita, bloqueios e exclusão em cascata.
  O fluxo entre processos também verifica edição e correção/remoção do histórico.

A evolução do histórico começou pelo teste de comportamento falhando, seguido
da implementação e validação das regras, mantendo os testes de regressão existentes.

Cada biblioteca guarda seus testes na própria pasta. Para executar apenas uma:

```sh
ctest --test-dir build -C Debug -L config --output-on-failure
ctest --test-dir build -C Debug -L infra --output-on-failure
```

No GCC/Clang com suporte aos sanitizers, também é possível executar:

```sh
cmake -S . -B build-sanitized -DCMAKE_BUILD_TYPE=Debug -DWORKFORCE_SANITIZERS=ON
cmake --build build-sanitized --parallel
ctest --test-dir build-sanitized --output-on-failure --no-tests=error
```

Os workflows executam em pull requests direcionados à `master`, pushes na
`master` e manualmente. Cada badge consulta seu próprio workflow na `master`,
considerando o evento de push, para que os resultados sejam independentes:

| Badge | Workflow | Verificação |
| --- | --- | --- |
| Build Debug | [build-debug.yml](.github/workflows/build-debug.yml) | Aplicativo em Debug no Linux e Windows |
| Build Release | [build-release.yml](.github/workflows/build-release.yml) | Aplicativo em Release no Linux e Windows |
| Testes | [tests.yml](.github/workflows/tests.yml) | CTest em Debug/Release, Linux/Windows e sanitizers no Linux |
| Formatação | [format.yml](.github/workflows/format.yml) | clang-format em todos os `.c` e `.h` versionados |
| Análise estática | [static-analysis.yml](.github/workflows/static-analysis.yml) | clang-tidy no aplicativo e nos testes |

O workflow de testes inclui Clang com AddressSanitizer, UndefinedBehaviorSanitizer
e detecção de vazamentos. Warnings de compilação são tratados como erros.
Não há configuração de pré-release: Debug e Release são modos de compilação,
independentes do versionamento ou da publicação de releases.

As badges só terão resultado depois que os novos workflows forem enviados e
executados na `master`. São usadas as [badges oficiais do GitHub Actions](https://docs.github.com/en/actions/how-tos/monitor-workflows/add-a-status-badge),
sem publicar arquivos de status ou usar tokens adicionais.

## Formatação e análise estática

O estilo está em [`.clang-format`](.clang-format): indentação de quatro espaços,
chaves em linhas separadas e limite de 100 colunas. O clang-format usa seu parser
de C/C++ (`Language: Cpp`) também para arquivos C; o aplicativo continua sendo C11.
O CI usa LLVM **18** no Ubuntu 24.04 para manter a versão das ferramentas estável.
Instale `clang-format-18`, `clang-tidy-18`, `clang-18` e Ninja para repetir os checks
nesse ambiente.

Para verificar a formatação no Bash, incluindo caminhos com espaços:

```bash
mapfile -d '' files < <(git ls-files -z -- '*.c' '*.h')
clang-format-18 --dry-run --Werror "${files[@]}"
```

Para aplicar o estilo, troque `--dry-run --Werror` por `-i`.
No PowerShell, com as ferramentas LLVM no `PATH`:

```powershell
$sourceFiles = @(git ls-files -- '*.c' '*.h')
clang-format --dry-run --Werror -- $sourceFiles
# Para aplicar: clang-format -i -- $sourceFiles
```

A configuração [`.clang-tidy`](.clang-tidy) habilita o Clang Static Analyzer,
checks de operações suspeitas, conversões numéricas e uso de chaves.
Todo diagnóstico habilitado é tratado como erro. O check
`clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling` fica
excluído por recomendar substituições de funções C padrão que não são portáveis
entre os compiladores suportados; as verificações de acesso e tamanho de memória
do analyzer permanecem habilitadas.

Para analisar todas as unidades de compilação, incluindo os testes, no Bash:

```bash
CC=clang-18 cmake -S . -B build-tidy -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON \
  -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -DCMAKE_C_CLANG_TIDY=clang-tidy-18
cmake --build build-tidy --parallel
```

O CMake passa ao clang-tidy as mesmas definições e caminhos de inclusão usados
na compilação. Aplicativo, bibliotecas próprias e testes são analisados;
o código baixado de SQLite/libyaml fica fora do clang-tidy. Use um diretório novo para a primeira análise completa; builds
incrementais só analisam novamente as fontes que precisarem ser recompiladas.
No Windows, execute em um Developer PowerShell do Visual Studio e substitua
`clang-18`/`clang-tidy-18` pelos nomes locais `clang`/`clang-tidy`.

Para impedir merges com falhas, configure no GitHub uma regra de proteção da
`master` exigindo os checks destes workflows; os arquivos de workflow por si só não
configuram a proteção da branch.

## Próximas extensões

A aplicação já gerencia múltiplos funcionários, remuneração mensal e persistência
SQLite. A separação em bibliotecas permite adicionar uma UI de terminal depois,
mantendo o domínio, a configuração e o armazenamento independentes da apresentação.
Edição de perfis e histórico salarial por competência já estão implementados.
Relatórios, edição de contratos e a futura UI de terminal continuam como extensões.
As decisões desta etapa estão em [`docs/persistence.md`](docs/persistence.md).

Os exercícios de HackerRank/VanHack e os binários antigos foram removidos da
árvore atual. Permanecem no histórico Git. Alocação dinâmica, structs, strings,
buscas e agregações agora são aplicadas no domínio do aplicativo e nos testes.
O `.gitignore` impede que novos artefatos de compilação sejam versionados.
