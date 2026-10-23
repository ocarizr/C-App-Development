# Persistência SQLite e configuração

## Bibliotecas estáticas

| Target CMake | Responsabilidade | Dependências |
| --- | --- | --- |
| `workforce_domain` | Structs, validação, contratos e remuneração | Biblioteca C padrão |
| `workforce_config` | Arquivos YAML e sobreposição tipada | `workforce_yaml` (libyaml) |
| `workforce_infra` | Conexão, esquema, consultas e gravações | `workforce_domain`, `workforce_sqlite` |
| `workforce_mvvm` | Registro de propriedades e comandos | Biblioteca C padrão |
| `workforce_application` | ViewModel e casos de uso | `workforce_mvvm`, `workforce_infra`, `workforce_export` |
| `workforce_ui` | XML, componentes, layouts e terminal | `workforce_mvvm`, Expat |
| `workforce_export` | Apresentação de relatórios em texto/CSV | `workforce_domain` |
| `workforce` | Inicialização, configuração e composição | Configuração, application e UI |

Todos os targets de biblioteca são declarados explicitamente como `STATIC`,
inclusive SQLite, libyaml e Expat. Linux produz arquivos `.a`; MSVC produz `.lib`.
Não há dependência das DLLs dessas três bibliotecas em execução. Nos pacotes Windows,
`WORKFORCE_STATIC_RUNTIME=ON` também vincula estaticamente o runtime C do MSVC,
incluindo as dependências. Builds normais seguem o padrão do compilador e Linux
mantém a dependência do runtime do sistema.

As APIs públicas estão em `libs/<componente>/include`, implementações em `src` e
testes em `tests`, com um `CMakeLists.txt` por componente. A infraestrutura não
depende da biblioteca de configuração: `main.c` converte `AppConfig` em
`StoreOptions` e injeta as opções em `store_open`. A UI recebe bindings do ViewModel e não depende de YAML ou SQLite.

## Configuração

O parser usa libyaml, sem implementar um dialeto YAML manual. O esquema atual
aceita uma raiz de mapeamento com a seção `database` e as opções `path` e
`busy_timeout_ms`. Arquivos devem conter um único documento. Chaves desconhecidas,
duplicadas e tipos incompatíveis são erros. Arquivos vazios são erros; um mapa
vazio é permitido como sobreposição sem alterações.

`config_defaults` fornece valores locais iniciais. `config_load_file` aplica um
arquivo sobre a configuração recebida e só atualiza a saída se a leitura e a
validação terminarem com sucesso. Caminhos relativos são associados ao arquivo
que informou `path`; sobrepor apenas o timeout preserva o caminho já resolvido.
Não há busca implícita por configurações de cada biblioteca: a aplicação fornece
a ordem com `--config`, tornando a precedência explícita e testável.

O banco padrão é `workforce.db` ao lado do `app.yaml`. O programa não cria
diretórios intermediários. Não existem endpoint de rede, senha, expansão de
variáveis de ambiente ou interpolação no YAML nesta versão.

## Repositório e esquema

`WorkforceStore` é opaco e possui uma conexão SQLite. O chamador fecha a conexão
com `store_close`. Cada operação de leitura abre uma transação para montar um
snapshot consistente, inclusive seus contratos. O domínio valida novamente os
dados carregados. Os snapshots pertencem ao chamador e são liberados com
`worker_dispose` ou `team_dispose`.

O banco contém:

- `workers`: ID persistente, nome, cargo, departamento, idade, nível e salário
  em centavos inteiros.
- `contracts`: ID persistente interno, funcionário, ano, mês, horas e valor/hora
  em centavos inteiros.
- `salary_changes`: funcionário, ano, mês e salário em centavos. A chave composta
  `(worker_id, year, month)` identifica uma única vigência.

Tabelas `STRICT`, `NOT NULL` e `CHECK` preservam os tipos e limites básicos.
Contratos e vigências têm chaves estrangeiras com exclusão em cascata. O índice
por funcionário/ano/mês atende à associação de contratos. IDs de funcionários
mantêm o limite público `uint32_t` e não são reutilizados após exclusão, inclusive
depois de reiniciar. IDs de contratos são persistentes e aparecem no modelo e na interface. Edição e remoção usam o ID do funcionário e do contrato,
sem offsets ou posições de listas. O ID persistente é a chave `contracts.id`.

A primeira abertura de um banco vazio cria o esquema dentro de uma transação.
`PRAGMA application_id=1464226371` e `user_version=2` identificam o formato atual.
Bancos de outra aplicação, arquivos inválidos e versões desconhecidas são
recusados. Bancos existentes não são apagados nem recriados após erro.
A versão 1 é migrada automaticamente para a 2: a tabela de vigências é criada,
mantendo o salário original em `workers.salary_cents` como base anterior à primeira
mudança. IDs, contratos e sequências não são recriados. A versão só é atualizada
na mesma transação que cria e valida o esquema novo. Uma falha faz rollback,
preservando os registros e a versão anterior. Versões futuras desconhecidas são recusadas.

O acesso usa prepared statements com parâmetros vinculados. Inclusões de
contratos usam `BEGIN IMMEDIATE` para validar os dados atuais e gravar na mesma
transação, preservando limites monetários mesmo entre conexões concorrentes.
Operações compostas usam rollback em caso de erro; exclusões simples são
statements atômicos do SQLite. O aplicativo só anuncia sucesso após a gravação.
O timeout configurado controla a espera por bloqueios; falhas são mostradas ao
usuário, sem fallback silencioso para armazenamento em memória.

## Edição e vigências

`worker_update_profile` valida o cadastro e preserva ID, contratos, salário inicial
e histórico. `worker_set_salary` insere ou corrige uma competência;
`worker_remove_salary` remove sua vigência. `worker_salary_at` encontra o valor
mais recente com competência menor ou igual à consulta, usando o salário inicial
quando não há nenhuma mudança anterior.

O agregado valida a linha do tempo candidata e todos os meses com contratos antes
de substituir seu estado. Alterações inválidas ou com overflow não mudam seus dados.
O cálculo mensal (`worker_income`) usa o salário daquela competência e os contratos
daquele mês, sem depender do relógio do sistema.

As operações equivalentes de `WorkforceStore` abrem `BEGIN IMMEDIATE`, carregam o
agregado completo, chamam o domínio e gravam apenas a alteração solicitada. Correções
usam UPSERT na chave da competência. A consulta carrega primeiro as vigências e
depois os contratos, para validar os valores com o salário correto, inclusive
quando um salário posterior é menor que o inicial. Falhas de domínio, bloqueio,
constraints ou triggers não geram sucesso nem gravam parcialmente.

O histórico informa os salários vigentes, com correções editáveis. Não armazena
versões antigas de nomes/cargos ou um log imutável de cada correção salarial.
A UI de terminal usa o ViewModel para estas operações.

## Contratos e relatórios

`worker_update_contract` preserva o ID e verifica os novos valores e a soma do mês
de destino antes de alterar o agregado. `store_update_contract` aplica essa regra
dentro de `BEGIN IMMEDIATE` e grava com `WHERE worker_id=? AND id=?`. Exclusões
usam o mesmo par de IDs. O SQLite atribui os IDs novos e o adaptador devolve o ID
somente após o commit. Os snapshots preservam os IDs reais, inclusive os de bancos
anteriores.

O repositório fornece um snapshot consistente da equipe ao core. `report_build`
aplica os filtros de departamento/nível sobre o perfil atual, calcula os valores
vigentes no ano/mês e gera linhas e subtotais independentes da memória da equipe.
O total é validado antes de apresentar ou exportar qualquer resultado. Os filtros
são aplicados antes das somas: registros excluídos não entram nos totais.

`workforce_export` recebe esse resultado pronto. Os adaptadores de texto e CSV não
dependem de configuração ou SQLite. A exportação abre o arquivo em modo exclusivo
de criação, verifica erros de escrita/fechamento e tenta remover um arquivo parcial
em caso de falha. Nunca substitui um arquivo existente. Os testes verificam CSV
com vírgulas, aspas, quebras de linha e textos semelhantes a fórmulas.

## Dependências e build sem rede

O CMake usa [SQLite 3.54.1](https://www.sqlite.org/download.html),
[libyaml 0.2.5](https://github.com/yaml/libyaml/releases/tag/0.2.5) e
[Expat 2.9.0](https://github.com/libexpat/libexpat/releases/tag/R_2_9_0), fixados em
[`cmake/Dependencies.cmake`](../cmake/Dependencies.cmake) com hashes SHA-256.
SQLite é compilado a partir da amalgamation oficial; extensão dinâmica está
desabilitada. Os arquivos baixados ficam no build, fora do controle de versão.
Os avisos de licença estão em [`THIRD_PARTY_NOTICES.md`](../THIRD_PARTY_NOTICES.md).

Para compilar sem rede, extraia previamente os três arquivos oficiais e informe
os diretórios que contêm `sqlite3.c`, os fontes do libyaml e o CMakeLists do Expat:

```sh
cmake -S . -B build \
  -DFETCHCONTENT_SOURCE_DIR_SQLITE_SOURCE=/caminho/sqlite-amalgamation-3540100 \
  -DFETCHCONTENT_SOURCE_DIR_YAML_SOURCE=/caminho/libyaml-0.2.5 \
  -DFETCHCONTENT_SOURCE_DIR_EXPAT_SOURCE=/caminho/expat-2.9.0
```

Os overrides usam fontes locais fornecidos pelo usuário, portanto não passam pela
verificação de hash do download. Conserve as versões fixadas e verifique os
arquivos ao prepará-los. Builds normais e os workflows fazem essa verificação
automaticamente.

No Linux, o target privado de libyaml define `_POSIX_C_SOURCE=200809L` para
expor `strdup` em C11 estrito. GCC e Clang também tratam declarações implícitas
de funções nessa dependência como erros: uma configuração incorreta deve falhar
no build, em vez de gerar um ponteiro truncado e causar um crash ao ler YAML.
