# Autoria da interface XML

As telas do Workforce são arquivos XML UTF-8 em `app/ui/`. O executável compõe
o ViewModel e carrega `ui/application.xml` ao lado do próprio executável. O build
copia a pasta a cada execução, inclusive quando somente XMLs mudam. Para editar
sem recompilar C, use a pasta de fontes diretamente:

```powershell
.\build\app\Debug\workforce.exe --ui app/ui/application.xml
```

Reabra a aplicação após alterar o XML; não há recarga durante uma sessão.
Os caminhos de `source` são relativos ao arquivo XML que os declara. Distribua
o executável com a pasta `ui/` completa; `app.yaml` é procurado no diretório atual,
ou pode ser informado com `--config`.

## Composição de arquivos

```xml
<?xml version="1.0" encoding="UTF-8"?>
<Application text="Minha aplicação">
  <Tabs>
    <Tab id="team" text="Equipe" source="team.xml"/>
    <Tab id="reports" text="Relatórios" source="reports.xml"/>
  </Tabs>
  <StatusBar bind="status"/>
  <StatusBar text="Tab: foco | Enter: ação | F1/F2: abas | Esc: sair"/>
</Application>
```

O arquivo de uma aba contém uma raiz `Window`:

```xml
<Window text="Equipe">
  <VStack gap="1">
    <Include source="shared/worker.xml"/>
    <Grid columns="2" gap="2">
      <Input text="Nome" bind="worker.name" maxLength="99"/>
      <Input text="Idade" bind="worker.age" maxLength="3"/>
    </Grid>
    <HStack>
      <Button text="Cadastrar" command="worker.create"/>
      <Button text="Salvar perfil" command="worker.save"/>
    </HStack>
    <Table text="Equipe" bind="workers" height="10"/>
  </VStack>
</Window>
```

Um fragmento tem raiz `Fragment`. `Include` insere seus componentes na árvore;
não instancia outro ViewModel. As várias inclusões de `shared/worker.xml`
compartilham `worker.id` e o perfil. Não coloque IDs em um fragmento reutilizado
mais de uma vez: IDs são únicos em toda a aplicação expandida.

`Application` é a única raiz principal e deve conter exatamente um `Tabs`.
Cada filho de `Tabs` deve ser um `Tab`, com `id` e `source`. `Tab` e `Include`
não aceitam filhos inline. `Window` pertence a `Tab`; `Fragment` pertence a
`Include`. Componentes visuais finais não aceitam filhos. Containers podem ser
aninhados; a ordem do XML define a ordem de foco e de distribuição.

## Componentes e propriedades

| Componente | Responsabilidade | Atributos específicos |
| --- | --- | --- |
| `Application` | Raiz e título | `text` |
| `Tabs` | Navegação entre abas | — |
| `Tab` | Referência a uma janela | `id` obrigatório, `text`, `source` obrigatório |
| `Window` | Título e conteúdo da aba | `text` |
| `Fragment` | Raiz de um arquivo incluído | — |
| `Include` | Composição de fragmentos | `source` obrigatório |
| `VStack` | Lista vertical de componentes | `gap` |
| `HStack` | Lista horizontal em colunas iguais | `gap` |
| `Grid` | Grade com preenchimento por linha | `columns`, `gap` |
| `Label` | Texto estático ou propriedade de leitura | `text`, `bind` |
| `Input` | Campo de texto com binding de duas vias | `text` (rótulo), `bind` obrigatório, `maxLength` |
| `Button` | Executar um comando | `text`, `command` obrigatório |
| `Table` | Lista de linhas de leitura, com rolagem | `text` (cabeçalho), `bind` obrigatório, `height` |
| `Separator` | Linha horizontal | — |
| `StatusBar` | Texto fixo na parte inferior | `text`, `bind` |

Todos aceitam `id`, `width` e `height`. Tamanhos são limites em células de terminal
(1–300), não pixels. `width` limita a largura oferecida pelo container; o espaço
restante da coluna não é redistribuído. Containers cortam o conteúdo que excede
seus limites. `VStack` empilha alturas naturais; `HStack` divide a largura entre
os filhos e usa a maior altura; `Grid` divide em `columns` (1–12, padrão 2) e usa
a maior altura de cada linha. `gap` varia de 0 a 8, padrão 1 em layouts e 0 nos
demais containers. A tela acompanha o componente em foco quando precisa rolar.

`Input` ocupa duas linhas (rótulo e valor). `maxLength` limita **bytes UTF-8**,
de 1 a 1023, padrão 99. A edição ocorre no final do texto: Backspace apaga o último
caractere e Ctrl+U limpa o valor. Enter avança ao próximo controle. Não há cursor
de edição intermediário. Valores monetários usam ponto decimal (`1250.50`).

`Label`, `Button` e `Separator` ocupam uma linha. `Table` tem altura padrão 10;
o título ocupa a primeira linha e o restante exibe a propriedade vinculada,
separada em linhas por `\n`. Ela é uma lista de leitura, sem seleção de registro:
use os IDs expostos e os campos de ID para as operações. PgUp/PgDn rolam linhas;
setas esquerda/direita rolam colunas em passos de oito células. Cabeçalhos dos
dados rolam junto das linhas; o título do componente permanece na primeira linha.

`StatusBar` é fixo e ocupa uma linha inferior, independente da rolagem. Vários
formam uma pilha na ordem declarada. Seu tamanho é definido pelo viewport;
`width`/`height` não alteram sua posição ou altura. O viewport aceita 10–300
colunas e 4–100 linhas. A aplicação funciona melhor com pelo menos 100×35.
As colunas dos grids permanecem lado a lado em janelas estreitas; não há reflow
automático. O terminal precisa aceitar sequências ANSI e UTF-8. A renderização
usa uma célula por caractere; caracteres de largura dupla, emojis e combinações
de acentos não têm medição especializada. Caracteres de controle vindos de dados
são substituídos ao renderizar, sem executar sequências de terminal.

Use `text="A &amp; B"`, `&lt;`, `&gt;`, `&quot;` e `&apos;` para os escapes XML.
Texto dentro dos elementos, além de espaços/quebras para indentação, é rejeitado.
Comentários e a declaração XML são permitidos. Quando `bind` está presente em
`Label`, `Table` ou `StatusBar`, o valor da propriedade é exibido literalmente;
não há interpolação, expressões, avaliação de código ou bindings calculados no XML.

## Navegação

Tab/seta abaixo avançam; Shift+Tab/seta acima voltam. Enter executa o botão,
avança em um input ou troca para a próxima aba quando `Tabs` está em foco.
Setas esquerda/direita também trocam abas quando `Tabs` está em foco. F1/F2
trocam para a aba anterior/próxima em qualquer componente. Esc, Ctrl+C e Ctrl+D
encerram a sessão. O terminal restaura seu modo de entrada, cursor e tela ao sair.
Redimensionamento é percebido durante o loop de eventos, mesmo sem digitação.
Não há interação por mouse.

## Contrato do ViewModel do Workforce

| Propriedades editáveis | Uso |
| --- | --- |
| `worker.id` | ID persistente do funcionário alvo |
| `worker.name`, `worker.age`, `worker.profession`, `worker.department`, `worker.level` | Cadastro e perfil |
| `worker.salary` | Salário inicial, utilizado apenas em `worker.create` |
| `contract.id` | ID persistente do contrato dentro do funcionário informado |
| `contract.year`, `contract.month`, `contract.hours`, `contract.rate` | Dados do contrato |
| `salary.year`, `salary.month`, `salary.amount` | Vigência salarial a cadastrar/corrigir |
| `report.year`, `report.month` | Competência da consulta |
| `report.department`, `report.level` | Filtros; vazio ou `*` libera cada filtro |
| `report.path` | Caminho do novo CSV, relativo ao diretório atual do processo |

| Propriedades somente leitura | Conteúdo |
| --- | --- |
| `workers` | Equipe com IDs e perfil atual |
| `contracts` | Contratos do funcionário consultado |
| `history` | Salário inicial e vigências em ordem cronológica |
| `selection` | Identificação do funcionário consultado |
| `report` | Resultado calculado pelo core e formatado pelo adaptador de exportação |
| `status` | Resultado da última operação ou erro de entrada |

| Comando | Operação |
| --- | --- |
| `worker.list` | Atualizar a equipe do banco |
| `worker.create` | Cadastrar e preencher `worker.id` com o ID atribuído |
| `worker.load` | Buscar o ID e preencher perfil, contratos e histórico |
| `worker.save` | Salvar o perfil do ID preservando salário original e histórico |
| `worker.delete` | Excluir o ID com seus contratos e salários |
| `contract.load` | Buscar `contract.id` do `worker.id` e preencher o formulário |
| `contract.add` | Adicionar e preencher `contract.id` com o ID atribuído |
| `contract.save`, `contract.delete` | Editar/excluir o par de IDs informado |
| `salary.save`, `salary.delete` | Cadastrar/corrigir ou excluir a competência informada |
| `report.build` | Consultar snapshot novo e gerar relatório |
| `report.export` | Consultar e gerar relatório novo, exportando para um arquivo novo |
| `quit` | Comando reservado do motor para sair |

Informe o ID e carregue o funcionário antes de editar. Alterar um ID no campo
não carrega automaticamente outro registro. Os comandos usam o ID informado
no momento da ação; tabelas e identificação refletem a última consulta concluída.
Botões de exclusão executam a operação diretamente. O resultado aparece no status.
Mutações invalidam a prévia do relatório; gere novamente para ver os dados atuais.
Pré-visualizações são limitadas a aproximadamente 65 KB por propriedade; uma
mensagem indica o corte. Consultas e exportações completas usam todos os dados.

## Validação e diagnóstico

O carregamento usa Expat 2.9.0, compilado estaticamente. XML malformado, componentes
ou atributos desconhecidos, IDs duplicados, bindings ausentes, inputs ligados a
propriedades somente leitura e comandos inexistentes impedem a abertura.
Erros de sintaxe incluem a linha; falhas em referências identificam o caminho.
O carregamento não apresenta uma árvore parcial ao usuário.

Cada arquivo tem limite de 64 KiB, com até 512 nós na árvore expandida, 32 níveis
por XML e 8 níveis de referências. Ciclos são recusados pelo limite de referências.
`source` exige caminho relativo, sem `..`, `.`, barras invertidas, unidades ou
URI. DTDs e entidades personalizadas são rejeitados; entidades XML predefinidas
e referências numéricas são aceitas pelo parser. Os arquivos de UI são recursos
locais confiáveis: links simbólicos do sistema de arquivos não são uma sandbox.

## Automação e testes sem terminal

O adaptador `--script` carrega os **mesmos XMLs e o mesmo ViewModel** e permite
testar sem TTY. Exemplo de `consulta.txt`:

```text
set worker.id=1
invoke worker.load
get history
tab reports
set report.year=2026
set report.month=10
set report.department=Engineering
set report.level=Senior
invoke report.build
get report
render
quit
```

```powershell
.\build\app\Debug\workforce.exe --script consulta.txt
```

`set NOME=VALOR` edita uma propriedade; `invoke COMANDO` exige sucesso;
`reject COMANDO` exige uma falha de negócio; `get NOME` escreve seu valor;
`tab ID` troca a aba; `render` escreve um snapshot 120×40 sem ANSI; `quit` ou
EOF encerram. Linhas vazias e iniciadas por `#` são ignoradas. Instruções
desconhecidas, valores em propriedades somente leitura, comandos inexistentes,
falhas inesperadas e linhas maiores que 2046 bytes encerram com status não zero.
O limite de `maxLength` é do componente interativo; scripts passam pela validação
de propriedades e comandos do ViewModel. Nunca use dados de produção nos testes.

## Adicionar componentes ou operações

Para reorganizar telas, editar rótulos ou compor layouts, altere somente XML.
Para um novo controle, adicione seu tipo e atributos em `libs/ui/src/document.c`,
estado em `ui_internal.h`, comportamento/layout/renderização em `render.c` e,
se necessário, uma tecla em `terminal.c`. Escreva testes do parser e do comportamento
em `libs/ui/tests`. O controle deve operar propriedades e comandos genéricos,
sem incluir cabeçalhos do domínio ou do SQLite.

Uma nova operação de aplicação é registrada em `libs/application/src/viewmodel.c`,
com teste de comando em `libs/application/tests`; depois pode ser referenciada no
XML. Regras de cálculo, vigência, validade do agregado e filtragem permanecem em
`libs/domain`. A infraestrutura fornece snapshots e operações transacionais.
O XML e a UI não escrevem SQL nem calculam remuneração.
