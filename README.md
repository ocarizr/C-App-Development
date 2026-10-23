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

## Baixar uma release

Os pacotes estão em [Releases](https://github.com/ocarizr/C-App-Development/releases).
No Windows x64, baixe `workforce-<versao>-windows-x64.zip`, extraia tudo e execute
`workforce.exe` dentro da pasta extraída. Pelo terminal, entre nessa pasta e use
`.\workforce.exe`. Não execute dentro do ZIP. O pacote inclui `app.yaml`, `ui/`,
documentação e licenças; o runtime C do MSVC é estático, sem exigir o Visual C++
Redistributable. O banco `workforce.db` é criado no primeiro uso. Use uma pasta
gravável e preserve seu banco e configuração nas atualizações. O pacote Linux
x64 é `.tar.gz`, compilado no Ubuntu 24.04 e requer uma distribuição compatível
com sua libc; execute `./workforce` na pasta extraída.

`SHA256SUMS.txt` permite conferir a integridade dos dois arquivos. No PowerShell:
`Get-FileHash .\workforce-<versao>-windows-x64.zip -Algorithm SHA256`.

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

## Preparar um pacote local

O CMake instala somente os recursos de execução. No Windows, com CMake no PATH:

```powershell
cmake -S . -B build-package -A x64 -DBUILD_TESTING=ON -DWORKFORCE_STATIC_RUNTIME=ON
cmake --build build-package --config Release --parallel
ctest --test-dir build-package -C Release --output-on-failure --no-tests=error
cmake --install build-package --config Release --prefix release --component Runtime
cpack --config build-package/CPackConfig.cmake -C Release
```

A pasta `release/` pode ser usada diretamente; os arquivos compactados ficam em
`build-package/packages/`. Ambos ficam fora do versionamento. Para gerar no Linux,
omita `-A x64` e adicione `-DCMAKE_BUILD_TYPE=Release` na configuração. O runtime
estático é uma opção para MSVC; Linux utiliza o runtime do sistema. Antes de
instalar novamente, escolha uma pasta nova se quiser preservar uma configuração
personalizada: a instalação copia o `app.yaml` padrão. Bancos não fazem parte do
pacote. `-DWORKFORCE_PACKAGE_VERSION=1.1` define a versão no nome dos arquivos;
sem essa opção, o empacotamento usa a versão do projeto CMake.

## Publicar por tag

O workflow [Publish Release](.github/workflows/publish-release.yml) é acionado
somente pelo envio de tags com duas ou três partes, como `V1.1`, `V1.2` e `v1.2.0`.
Ele valida o formato e exige que o commit marcado pertença ao histórico da
`master`. Merge de PR e push comum na branch não publicam releases. O commit
pode ser o topo da `master` ou um commit anterior que contenha esse workflow.
Tags anotadas e leves são aceitas.

Após integrar o workflow na `master`, publique uma versão assim:

```sh
git switch master
git pull --ff-only
git tag -a V1.1 -m "Workforce V1.1"
git push origin V1.1
```

A pipeline compila exatamente o commit marcado, executa CTest em Release para
Windows/Linux e testa os mesmos fluxos no aplicativo instalado. Somente depois
dos dois pacotes passarem, cria um draft, anexa os binários e checksums e publica
a release. Usa o `GITHUB_TOKEN` com escrita apenas no job de publicação, sem
PAT adicional. O workflow roda em runners hospedados pelo GitHub. Não mova uma
tag publicada; use um novo número para outra versão. Reexecutar uma publicação
concluída preserva seus assets; um draft incompleto pode ser concluído pela
reexecução do workflow.

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

Formatação usa `.clang-format`, com LLVM 18 no CI: quatro espaços, chaves Allman,
includes agrupados, uma linha vazia entre funções/definições e no máximo uma
linha vazia consecutiva. `scripts/check_spacing.py` complementa as regras com
uma linha vazia após blocos e entre includes e declarações. `else`, `do/while`,
fechamentos de blocos externos e diretivas condicionais preservam sua associação.
O script requer Python 3, somente com a biblioteca padrão. No PowerShell:

```powershell
$sourceFiles = @(git ls-files -- '*.c' '*.h')
clang-format --dry-run --Werror -- $sourceFiles
python scripts/check_spacing.py
```

Para aplicar, execute clang-format com `-i` e depois
`python scripts/check_spacing.py --fix`. Para testar o verificador de espaçamento,
use `python scripts/check_spacing.py --self-test`. Para análise com
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

`clang-analyzer-*` verifica caminhos com vazamentos, double free, uso após liberação,
desreferência nula e valores não inicializados. Os checks `bugprone` também cobrem
sobrescrita do ponteiro em `realloc`, aritmética/tamanho incorretos na alocação,
comparações de memória e terminadores de strings. ASan/UBSan exercitam os acessos
e a aritmética durante os testes; o CI Linux ativa também LeakSanitizer. Essas
verificações se complementam e não constituem uma prova de segurança de memória
para todos os caminhos possíveis. Contratos de ownership estão nos headers e
nos pontos de transferência de recursos.

Os workflows de qualidade rodam em pull requests para `master`, pushes na `master`
e manualmente. A publicação por tag é independente. As badges são independentes:

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
