# Design do Workforce

## Dependências e responsabilidades

```text
main -> config
     -> application -> mvvm
                    -> infra -> domain + SQLite
                    -> export -> domain
     -> ui -> mvvm + Expat

config -> libyaml
app/ui/*.xml -> componentes + propriedades/comandos do ViewModel
```

Todos os módulos são bibliotecas estáticas em C11. `app/main.c` é o ponto de
entrada do aplicativo e compõe configuração, repositório, ViewModel e UI.
Infraestrutura, configuração, MVVM e UI são módulos técnicos; `libs/domain`
é o domínio de negócio. Cada biblioteca mantém seus próprios testes.

`Worker` é o agregado: possui perfil, contratos e vigências salariais. Funções
que recebem seu ponteiro protegem os invariantes. Cargo e departamento são
valores textuais do perfil. `Team` representa uma coleção para consultas.
`report_build` seleciona, calcula e agrega; `workforce_export` recebe esse
snapshot e formata texto/CSV. SQLite fica privado em `libs/infra`.

## MVVM e apresentação

`workforce_mvvm` é um registro genérico de propriedades textuais e comandos.
Propriedades são registradas como editáveis ou somente leitura. `mvvm_set`
representa uma edição da View; `mvvm_publish` publica estado do ViewModel,
inclusive propriedades de leitura. Escritas bem-sucedidas incrementam a revisão.
Comandos têm nomes estáveis, callback e contexto emprestado. O registro possui
seus valores; getters devolvem ponteiros emprestados, invalidados por escritas.
As APIs são de uso em uma única thread. Limites: 64 propriedades, 32 comandos,
nomes de até 63 bytes e valores de até 65535 bytes.

`workforce_application` possui o ViewModel específico. Ele converte formulários
textuais em parâmetros tipados e coordena o repositório e o core. Validação do
agregado, vigência e overflow permanecem no domínio. Cada comando busca dados
atuais do banco; o ViewModel não mantém uma cópia editável do agregado. Perfil
é salvo sobre um snapshot novo, preservando salário original e histórico.
Listas de apresentação são somente leitura, limitadas na prévia. Relatórios
reutilizam `report_build` e `report_write_text`; exportação usa `report_export_csv`.
Uma escrita invalida o relatório exibido. Se a escrita terminar mas a atualização
visual falhar, o status informa que a operação concluiu e a visualização falhou.

`workforce_ui` depende apenas de MVVM e do parser Expat. Deserializa e valida
a árvore modular de XML antes de expô-la, distribui componentes em células de
terminal, trata foco/edição e renderiza ANSI. A View lê propriedades a cada render
e escreve campos editáveis; botões executam comandos. Recursos de aplicação
(abas, rótulos, formulários, layouts e ações) ficam em XML. O motor não inclui
headers de negócio ou SQLite. [Guia de autoria](ui-authoring.md).

O backend Windows usa o console nativo para entrada e ANSI para saída. O backend
POSIX usa termios, poll e ANSI. O adaptador de scripts permite exercitar o mesmo
documento e ViewModel sem TTY. Os testes de layout usam snapshots sem sequências
ANSI; testes entre processos verificam persistência e os fluxos de comandos.

## Regras do agregado

- IDs de funcionários e contratos são estáveis; edição não muda identidade.
- Perfil representa nome, idade, cargo, departamento e nível atuais.
- O salário original vale antes da primeira mudança mensal.
- Uma vigência vale do seu ano/mês até a próxima; corrigir substitui o valor.
- Excluir uma vigência restaura a anterior até a próxima mudança.
- Salários zero e reduções são permitidos. Valores negativos não são aceitos.
- Contratos pertencem a um mês, com horas inteiras e valor/hora em centavos.
- Remuneração usa salário vigente mais contratos do mês, sem ponto flutuante.
- Mudanças retroativas validam todos os meses afetados com contratos existentes.
- Entradas inválidas ou overflow preservam o estado anterior.

O histórico representa valores vigentes, sem trilha de auditoria de cada correção
ou versões de campos do perfil. Não há rateio diário, impostos ou regras legais
de folha. Relatórios combinam departamento exato e nível com E, sobre o perfil
atual. Cada filtro pode ficar livre. Linhas são ordenadas por departamento/ID;
somente selecionados entram nos subtotais e total. Resultado vazio tem total zero.

## Persistência e memória

O repositório é opaco e recebe `StoreOptions` por injeção. Configuração não
depende de infraestrutura. SQLite usa parâmetros vinculados, transações e
exclusão em cascata. O esquema é identificado e validado; bancos desconhecidos
são recusados. A migração suportada de 1 para 2 é atômica e preserva identidades.
[Esquema e configuração](persistence.md).

`Worker` possui seus vetores de contratos e vigências; `Team` possui os funcionários.
Inicialize com `{0}`, não faça cópias rasas de valores proprietários e libere com
`worker_dispose`/`team_dispose`. `PayrollReport` possui seu snapshot e requer
`report_dispose`. Campos de structs do domínio devem ser tratados como leitura
pelos clientes; mutações passam pelas funções públicas.

`WorkforceViewModel` possui o registro MVVM e empresta o repositório. `UiDocument`
possui a árvore de componentes e empresta o registro. A ordem de destruição é
UI, ViewModel, repositório. O documento permanece válido durante a sessão e as
propriedades permanecem registradas até a destruição do ViewModel.

## Verificação

As suítes do domínio cobrem regras e aritmética; infraestrutura cobre persistência,
concorrência, constraints e rollback. Configuração cobre sintaxe e sobreposição;
exportação cobre CSV e preservação de arquivos. MVVM cobre acesso de leitura/escrita
e dispatch; UI cobre XML, referências, layouts, foco, edição UTF-8 e rolagem.
Application testa os casos de uso por bindings. Sessões entre processos carregam
os XMLs distribuídos e verificam cadastro, edição, contratos, histórico, relatórios,
CSV e exclusões. Debug, Release, clang-tidy, clang-format e sanitizers são checks
independentes no CI.
