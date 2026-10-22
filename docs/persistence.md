# Persistência SQLite e configuração

## Bibliotecas estáticas

| Target CMake | Responsabilidade | Dependências |
| --- | --- | --- |
| `workforce_domain` | Structs, validação, contratos e remuneração | Biblioteca C padrão |
| `workforce_config` | Arquivos YAML e sobreposição tipada | `workforce_yaml` (libyaml) |
| `workforce_infra` | Conexão, esquema, consultas e gravações | `workforce_domain`, `workforce_sqlite` |
| `workforce_console` | Menu e apresentação atual | `workforce_infra` |
| `workforce` | Inicialização, configuração e composição | Console e configuração |

Todos os targets de biblioteca são declarados explicitamente como `STATIC`,
inclusive SQLite e libyaml. Linux produz arquivos `.a`; MSVC produz `.lib`.
Não há dependência das DLLs de SQLite/libyaml em execução. O runtime C do sistema
segue o padrão do compilador; não se trata de um executável totalmente estático.

As APIs públicas estão em `libs/<componente>/include`, implementações em `src` e
testes em `tests`, com um `CMakeLists.txt` por componente. A infraestrutura não
depende da biblioteca de configuração: `main.c` converte `AppConfig` em
`StoreOptions` e injeta as opções em `store_open`. A configuração também pode
ser usada por uma futura biblioteca de UI, sem depender de SQLite.

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

Tabelas `STRICT`, `NOT NULL` e `CHECK` preservam os tipos e limites básicos.
`contracts.worker_id` é uma chave estrangeira com exclusão em cascata. O índice
por funcionário/ano/mês atende à associação de contratos. IDs de funcionários
mantêm o limite público `uint32_t` e não são reutilizados após exclusão, inclusive
depois de reiniciar. A interface ainda exibe contratos por posição ordenada por
ID; seus números visíveis podem mudar após uma remoção.

A primeira abertura de um banco vazio cria o esquema dentro de uma transação.
`PRAGMA application_id=1464226371` e `user_version=1` identificam o formato.
Bancos de outra aplicação, arquivos inválidos e versões desconhecidas são
recusados. Bancos existentes não são apagados nem recriados após erro.
Migrações futuras devem ser implementadas explicitamente por versão.

O acesso usa prepared statements com parâmetros vinculados. Inclusões de
contratos usam `BEGIN IMMEDIATE` para validar os dados atuais e gravar na mesma
transação, preservando limites monetários mesmo entre conexões concorrentes.
Operações compostas usam rollback em caso de erro; exclusões simples são
statements atômicos do SQLite. O aplicativo só anuncia sucesso após a gravação.
O timeout configurado controla a espera por bloqueios; falhas são mostradas ao
usuário, sem fallback silencioso para armazenamento em memória.

Os dados antigos existiam apenas em memória: não há arquivos anteriores a migrar.
Não foi adicionada uma UI visual nesta etapa.

## Dependências e build sem rede

O CMake usa [SQLite 3.54.1](https://www.sqlite.org/download.html) e
[libyaml 0.2.5](https://github.com/yaml/libyaml/releases/tag/0.2.5), fixados em
[`cmake/Dependencies.cmake`](../cmake/Dependencies.cmake) com hashes SHA-256.
SQLite é compilado a partir da amalgamation oficial; extensão dinâmica está
desabilitada. Os arquivos baixados ficam no build, fora do controle de versão.
Os avisos de licença estão em [`THIRD_PARTY_NOTICES.md`](../THIRD_PARTY_NOTICES.md).

Para compilar sem rede, extraia previamente os dois arquivos oficiais e informe
os diretórios que contêm `sqlite3.c` e os fontes do libyaml:

```sh
cmake -S . -B build \
  -DFETCHCONTENT_SOURCE_DIR_SQLITE_SOURCE=/caminho/sqlite-amalgamation-3540100 \
  -DFETCHCONTENT_SOURCE_DIR_YAML_SOURCE=/caminho/libyaml-0.2.5
```

Os overrides usam fontes locais fornecidos pelo usuário, portanto não passam pela
verificação de hash do download. Conserve as versões fixadas e verifique os
arquivos ao prepará-los. Builds normais e os workflows fazem essa verificação
automaticamente.

## Recuperação de uma cópia de pastas

A árvore de fontes atual tem `app/`, `libs/` e `tests/support/`. Os antigos
`src/`, `include/` e testes na raiz de `tests/` foram movidos; não devem ser
copiados de volta. O CMake recusa as versões antigas conhecidas para identificar
esse problema na configuração, antes de executar os demais checks.

Após mover/copiar o checkout, use um diretório novo para evitar caminhos e
arquivos gerados do checkout anterior:

```sh
cmake -S . -B build-recovery -DBUILD_TESTING=ON -DCMAKE_BUILD_TYPE=Debug
cmake --build build-recovery --config Debug --parallel
ctest --test-dir build-recovery -C Debug --output-on-failure --no-tests=error
```

Preserve o banco e os arquivos de configuração locais. Copiar arquivos pelo
Explorer não aplica as exclusões e renomeações do Git.

No Linux, o target privado de libyaml define `_POSIX_C_SOURCE=200809L` para
expor `strdup` em C11 estrito. GCC e Clang também tratam declarações implícitas
de funções nessa dependência como erros: uma configuração incorreta deve falhar
no build, em vez de gerar um ponteiro truncado e causar um crash ao ler YAML.
