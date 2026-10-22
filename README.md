# Workforce — gerenciamento de equipe em C

[![Build Debug](https://github.com/ocarizr/C-App-Development/actions/workflows/build-debug.yml/badge.svg?branch=master&event=push)](https://github.com/ocarizr/C-App-Development/actions/workflows/build-debug.yml)
[![Build Release](https://github.com/ocarizr/C-App-Development/actions/workflows/build-release.yml/badge.svg?branch=master&event=push)](https://github.com/ocarizr/C-App-Development/actions/workflows/build-release.yml)
[![Testes](https://github.com/ocarizr/C-App-Development/actions/workflows/tests.yml/badge.svg?branch=master&event=push)](https://github.com/ocarizr/C-App-Development/actions/workflows/tests.yml)
[![Formatação](https://github.com/ocarizr/C-App-Development/actions/workflows/format.yml/badge.svg?branch=master&event=push)](https://github.com/ocarizr/C-App-Development/actions/workflows/format.yml)
[![Análise estática](https://github.com/ocarizr/C-App-Development/actions/workflows/static-analysis.yml/badge.svg?branch=master&event=push)](https://github.com/ocarizr/C-App-Development/actions/workflows/static-analysis.yml)

Aplicativo de terminal em **C11** para gerenciar funcionários, contratos por hora,
perfis e histórico salarial por competência. Os dados persistem em SQLite.
A interface tem abas, formulários, botões e tabelas, com telas modulares escritas
em XML e bindings MVVM. Relatórios mensais filtram por departamento e nível,
mostram remuneração e subtotais e exportam CSV.

## Compilar e executar

Requisitos: CMake 3.20+, compilador C11 e acesso à rede na primeira configuração.
No Windows, instale o workload de desenvolvimento C++ do Visual Studio, incluindo
Windows SDK e ferramentas CMake. SQLite 3.54.1, libyaml 0.2.5 e Expat 2.9.0 são
baixados de fontes oficiais com SHA-256 fixado e compilados como bibliotecas
estáticas. Não é necessário instalar um servidor de banco ou essas bibliotecas.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON
cmake --build build --config Debug --parallel
ctest --test-dir build -C Debug --output-on-failure --no-tests=error
```

No PowerShell, com o gerador do Visual Studio:

```powershell
.\build\app\Debug\workforce.exe
```

No Linux, com um gerador de configuração única:

```sh
./build/app/workforce
```

Execute a partir da raiz do repositório ou do build, onde está `app.yaml`.
Para Release, troque `Debug` por `Release`; com gerador de configuração única,
use outro diretório de build. `-DBUILD_TESTING=OFF` compila somente a aplicação
e suas bibliotecas. Com MSVC e Ninja, use um Developer PowerShell.

O executável carrega `ui/application.xml` ao seu lado. Distribua a pasta `ui/`
completa junto dele. A cópia dos XMLs é atualizada em cada build. Para executar
diretamente com os XMLs de autoria:

```powershell
.\build\app\Debug\workforce.exe --ui app/ui/application.xml
```

## Usar a interface

Use um terminal com ANSI e UTF-8, preferencialmente com pelo menos 100×35 células.
Tab/Shift+Tab ou setas acima/abaixo mudam o foco; Enter executa o botão ou avança
no formulário. F1/F2 trocam abas; Esc encerra. Ctrl+U limpa o campo e Backspace
apaga no final. Tabelas usam PgUp/PgDn e setas esquerda/direita para rolagem.
A barra inferior apresenta o resultado da operação. Não há uso de mouse.

| Aba | Operações |
| --- | --- |
| Equipe | Listar, cadastrar, carregar por ID, editar perfil e excluir funcionário |
| Contratos | Consultar, adicionar, carregar, editar e excluir por IDs persistentes |
| Histórico | Consultar, cadastrar/corrigir e excluir salários por ano e mês |
| Relatórios | Consultar remuneração mensal filtrada e exportar CSV |

Para editar, informe o ID mostrado na tabela e use **Carregar funcionário**.
Edite os campos e salve o perfil. O campo de salário inicial serve ao cadastro;
alterações salariais são informadas na aba Histórico. Em Contratos, carregue
também o ID do contrato antes de editar. Os comandos atuam nos IDs informados
nos campos, sem usar posições das tabelas. Exclusões são executadas pelo botão
e removem os dados associados ao funcionário.

Salários e valores por hora usam ponto decimal, sem símbolo ou separador de
milhar: `1250.50`. Níveis: `Trainee`, `Junior`, `MidLevel`, `Senior`. Nome, cargo
e departamento aceitam até 99 bytes; idade de 1 a 150, ano de 1 a 9999, mês de
1 a 12 e horas inteiras positivas. Valores monetários podem ser zero, sem valores
negativos. Operações inválidas ou com overflow são rejeitadas.

Uma vigência substitui o salário a partir de seu ano/mês até a próxima. Corrigir
a mesma competência substitui o valor; excluir restaura a vigência anterior.
Antes da primeira mudança vale o salário inicial. O histórico representa valores
vigentes, sem log de auditoria de cada correção. A remuneração mensal soma o
salário vigente aos contratos daquele mês, usando centavos inteiros.

Nos relatórios, vazio ou `*` libera cada filtro. Departamento é uma correspondência
exata, incluindo maiúsculas/minúsculas; departamento e nível são combinados com
**E** e usam o perfil atual. O resultado inclui funcionários, valores de salário
e contratos, subtotais por departamento e total geral. Seleção vazia gera zero.
A prévia na tela é limitada a aproximadamente 65 KB; o CSV contém todos os dados
selecionados. O arquivo de exportação deve ser novo, em um diretório existente
e gravável. Arquivos existentes não são sobrescritos.

## Configuração e persistência

```yaml
database:
  path: workforce.db
  busy_timeout_ms: 5000
```

Sem `--config`, a aplicação lê `app.yaml` do diretório atual. Caminhos de banco
relativos são resolvidos a partir do YAML que os define. O diretório pai precisa
existir. O aplicativo cria o banco se estiver ausente e informa a criação ou a
conexão bem-sucedida no console e na barra de status.

```powershell
.\build\app\Debug\workforce.exe --config app.yaml --config infra.local.yaml
```

Sobreposições alteram somente as opções informadas. `--config` substitui a busca
automática; a ordem dos arquivos define a precedência. YAML inválido, chaves
desconhecidas/duplicadas e múltiplos documentos são recusados. Timeout aceita
0–60000 ms. O caminho deve ser persistente; `:memory:` e URIs SQLite não são
aceitos pela configuração do aplicativo. SQLite utiliza um arquivo local,
sem host, usuário ou senha. Bancos, `*.local.yaml`, CSVs e artefatos de build
são ignorados pelo Git.

## Arquitetura e documentação

```text
app/                 ponto de entrada e testes entre processos
app/ui/              aplicação, janelas e fragmentos XML
libs/domain/         agregado Worker, dinheiro, vigências e relatórios
libs/config/         carregamento e sobreposição de YAML
libs/infra/          repositório SQLite e transações
libs/export/         adaptadores de relatório em texto e CSV
libs/mvvm/           propriedades, comandos e revisão de estado
libs/application/    ViewModel e coordenação dos casos de uso
libs/ui/             deserialização XML, componentes, layout e terminal
tests/support/       verificações compartilhadas dos testes C
cmake/               dependências fixadas e validação da árvore de fontes
.github/workflows/   build, testes, formatação e análise estática
```

As bibliotecas são `STATIC`: `.a` com toolchains Unix e `.lib` com MSVC. O domínio
protege o agregado `Worker`; a infraestrutura chama suas regras dentro de
transações. `report_build` concentra seleção, cálculo e agregação no core.
O ViewModel coordena os comandos e fornece estado de apresentação; a UI opera
somente propriedades e comandos. XML define telas e composição. UI e XML não
calculam remuneração, repetem filtros de negócio ou escrevem SQL.

- [Autoria de UI](docs/ui-authoring.md): componentes, layout, XML modular, bindings,
  comandos, teclas, limites e automação sem terminal.
- [Design e contratos](docs/design.md): dependências, DDD, memória e regras do agregado.
- [Persistência](docs/persistence.md): YAML, esquema, transações, migrações e build offline.
- Headers em `libs/*/include/`: contratos públicos, ownership e ciclo de vida.

## Testes e qualidade

Cada biblioteca possui testes próprios, com verificações ativas também em Release.
As suítes cobrem domínio, histórico, contratos, relatórios, configuração, SQLite,
CSV, MVVM, XML inválido, includes, layout horizontal/grid, foco, edição UTF-8,
rolagem e fluxos da aplicação entre processos. O adaptador `--script` permite
executar os mesmos comandos e carregar os mesmos XMLs sem terminal interativo;
o formato está no guia de autoria.

```sh
ctest --test-dir build -C Debug -L ui --output-on-failure
ctest --test-dir build -C Debug -L infra --output-on-failure
```

Com GCC/Clang e suporte aos sanitizers:

```sh
cmake -S . -B build-sanitized -DCMAKE_BUILD_TYPE=Debug -DWORKFORCE_SANITIZERS=ON
cmake --build build-sanitized --parallel
ctest --test-dir build-sanitized --output-on-failure --no-tests=error
```

Formatação usa `.clang-format`, com LLVM 18 no CI. No PowerShell:

```powershell
$sourceFiles = @(git ls-files -- '*.c' '*.h')
clang-format --dry-run --Werror -- $sourceFiles
```

Para aplicar o estilo, substitua `--dry-run --Werror` por `-i`. Para análise com
Clang/Ninja (em Developer PowerShell quando usar o SDK do Windows):

```sh
CC=clang-18 cmake -S . -B build-tidy -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON \
  -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -DCMAKE_C_CLANG_TIDY=clang-tidy-18
cmake --build build-tidy --parallel
```

O exemplo acima usa sintaxe Bash. No PowerShell, defina `$env:CC = 'clang'` e
informe `clang-tidy` conforme o nome instalado. O CMake transmite definições e
includes dos targets ao analyzer; dependências externas ficam fora do clang-tidy.
Warnings do projeto e diagnósticos habilitados no `.clang-tidy` são erros.

Os workflows rodam em pull requests para `master`, pushes na `master` e manualmente.
As badges são independentes:

| Badge | Verificação |
| --- | --- |
| Build Debug | Compilação Debug em Linux/Windows |
| Build Release | Compilação Release em Linux/Windows |
| Testes | CTest Debug/Release em Linux/Windows; ASan/UBSan e leaks no Linux |
| Formatação | clang-format de todos os `.c` e `.h` versionados |
| Análise estática | clang-tidy das bibliotecas, aplicativo e testes |

## Licença

[MIT](LICENSE), com software fornecido “AS IS”, sem garantias. Os avisos e licenças
das dependências estão em [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).
