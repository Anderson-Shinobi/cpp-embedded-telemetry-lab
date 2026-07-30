# Host Telemetry Pipeline

## Objetivo

A aplicação host exercita o Protocol Core sem hardware ou transporte real. Ela
gera entradas binárias determinísticas, valida cada frame, encaminha somente
frames válidos por uma fila concorrente limitada, agrega métricas e imprime um
relatório final em stdout.

## Arquitetura e fluxo

```text
SimulatedFrameSource
        |
        v
FrameIngestor -> telemetry::protocol::deserialize
        |
        v
elite::concurrency::SafeConcurrentBuffer
        |
        v
TelemetryProcessor -> TelemetryMetrics -> telemetry report
```

- `SimulatedFrameSource` gera bytes válidos com
  `telemetry::protocol::serialize` e cria casos inválidos alterando offsets
  nomeados do protocolo.
- `FrameIngestor` registra a entrada, chama `deserialize`, registra o erro ou
  converte os bytes válidos para o tipo aceito pela fila.
- O adaptador consumidor reconverte o payload binário canônico, reutiliza
  `deserialize` e entrega somente `TelemetryFrame` validado ao processor.
- `TelemetryProcessor` atualiza exclusivamente as métricas de processamento.
- `TelemetryMetricsSnapshot` separa o estado interno da apresentação.

## Dependência concorrente

O projeto usa `cpp-safe-concurrent-buffer` v0.3.0 como pacote CMake instalado:

```cmake
find_package(cpp_safe_concurrent_buffer 0.3 REQUIRED)
```

O target consumido é:

```cmake
cpp_safe_concurrent_buffer::concurrent_buffer
```

A API real fornece `elite::concurrency::SafeConcurrentBuffer` com payload fixo
`std::vector<std::byte>`. `push()` bloqueia enquanto a fila está cheia e
retorna `false` somente quando a fila foi fechada. Como não existe operação
não bloqueante, o host não define um resultado artificial `queue_full`.

O link da dependência é `PUBLIC` em `telemetry_host_core` porque tipos da
biblioteca aparecem nos headers públicos do ingestor e do consumidor.

## Instalação e descoberta

```sh
DEPENDENCY_SOURCE=/path/to/cpp-safe-concurrent-buffer
DEPENDENCY_BUILD=/tmp/cpp-safe-concurrent-buffer-build
DEPENDENCY_PREFIX=/tmp/cpp-safe-concurrent-buffer-v030

cmake -S "${DEPENDENCY_SOURCE}" -B "${DEPENDENCY_BUILD}" \
  -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_TESTING=OFF \
  -DBUILD_BENCHMARKS=OFF \
  -DCMAKE_INSTALL_PREFIX="${DEPENDENCY_PREFIX}"
cmake --build "${DEPENDENCY_BUILD}" --parallel
cmake --install "${DEPENDENCY_BUILD}"
```

O consumidor recebe o prefixo somente na configuração:

```sh
cmake -S . -B build \
  -DBUILD_TESTING=ON \
  -DCMAKE_PREFIX_PATH="${DEPENDENCY_PREFIX}"
```

Não há `add_subdirectory`, `FetchContent`, header copiado ou path da árvore da
dependência gravado no projeto.

## Encerramento

A thread principal produz e ingere entradas. Uma `std::jthread` consome a fila.
Após a última entrada, a thread principal chama `close()` e faz join. A
semântica da dependência mantém os itens já aceitos disponíveis; `pop()` retorna
`std::nullopt` somente quando a fila está fechada e vazia. Assim, todos os itens
aceitos são drenados sem polling, sleeps ou timeouts.

Uma alocação ou falha de sincronização excepcional é tratada como falha
operacional. Erros de parsing previstos usam `IngestResult` e não exceções.

## Política de sequência

- o primeiro frame define primeira e última sequência;
- valor igual à última sequência é duplicado;
- valor crescente atualiza a última sequência;
- distância maior que um soma exatamente os números ausentes;
- valor menor é regressivo e não reduz a última sequência;
- a distância é calculada somente após confirmar a ordem, evitando
  `last + 1` e overflow no limite de `std::uint32_t`;
- wraparound não é interpretado nesta versão.

## Métricas

O snapshot contém entradas, aceitos, rejeitados, processados, erros individuais,
sequências, warnings, extremos e somas de temperatura e tensão. Temperatura usa
acumulador `std::int64_t`; tensão e contadores usam `std::uint64_t`.

Médias exatas são formatadas como inteiro quando divisíveis. Caso contrário,
são apresentadas como razão `soma/quantidade`, evitando truncamento silencioso
e ponto flutuante em decisões.

## Cenário determinístico

São geradas 15 entradas: oito válidas e sete inválidas. Os casos inválidos
incluem tamanho, magic, versão, tipo, payload size, payload corrompido e CRC
corrompido.

As sequências válidas são:

```text
100, 101, 103, 103, 102, 104, 105, 106
```

O resultado esperado é:

- 15 entradas, 8 aceitas, 7 rejeitadas e 8 processadas;
- erros: tamanho 1, magic 1, versão 1, tipo 1, payload size 1, checksum 2;
- uma sequência ausente, uma duplicada e uma regressiva;
- três ocorrências de cada warning;
- temperatura mínima -10000, máxima 30000 e média 8125 millicelsius;
- tensão mínima 2900, máxima 3600 e média 3250 millivolts.

Frames inválidos não entram na fila e não chegam ao processor.

## Build, teste e execução

```sh
cmake -S . -B build \
  -DCMAKE_BUILD_TYPE=Debug \
  -DBUILD_TESTING=ON \
  -DCMAKE_PREFIX_PATH="${DEPENDENCY_PREFIX}"
cmake --build build --parallel
ctest --test-dir build --output-on-failure
./build/host/telemetry_host_demo
```

Build sem testes:

```sh
cmake -S . -B build-minimal \
  -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_TESTING=OFF \
  -DCMAKE_PREFIX_PATH="${DEPENDENCY_PREFIX}"
cmake --build build-minimal --parallel
```

## Sanitizers

ASan/UBSan e TSan devem ser executados em diretórios de build separados. As
flags são fornecidas por `CMAKE_CXX_FLAGS` e `CMAKE_EXE_LINKER_FLAGS`, sem
alterar os targets públicos. O demo deve ser executado sob cada runtime além
do CTest.

## Limitações atuais

- entrada exclusivamente simulada e em memória;
- um produtor e um consumidor no demo;
- nenhuma política de wraparound de sequência;
- nenhuma operação não bloqueante ou timeout na fila externa;
- sem firmware, Zephyr, Renode, UART, sockets, persistência, GUI ou fault
  injection avançado.
