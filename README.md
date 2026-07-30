# Embedded Telemetry Lab

Laboratório C++20 para um protocolo binário portátil e um pipeline host
concorrente, determinístico e testável.

## Status

**Host Telemetry Pipeline:** o Protocol Core v1 é usado por uma aplicação host
que valida entradas simuladas, rejeita frames inválidos, drena frames válidos
por uma fila limitada e produz métricas e relatório textual.

Firmware, Zephyr, Renode, transportes reais e fault injection avançado ainda
não estão implementados.

## Arquitetura

```text
SimulatedFrameSource
  -> FrameIngestor
  -> telemetry::protocol::deserialize
  -> SafeConcurrentBuffer
  -> TelemetryProcessor
  -> TelemetryMetrics
  -> Text Report
```

O frame v1 possui 34 bytes em big-endian e CRC-32. Consulte
[docs/PROTOCOL.md](docs/PROTOCOL.md) e
[docs/HOST_PIPELINE.md](docs/HOST_PIPELINE.md).

## Requisitos

- CMake 3.20 ou mais recente;
- compilador C++20 compatível;
- `cpp-safe-concurrent-buffer` v0.3.0 instalado;
- Git e acesso à rede quando GoogleTest ainda não estiver disponível.

## Dependência externa

Instale a fila em um prefixo isolado:

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

O projeto consome somente o pacote instalado por
`find_package(cpp_safe_concurrent_buffer 0.3 REQUIRED)` e pelo target
`cpp_safe_concurrent_buffer::concurrent_buffer`.

## Build com testes

```sh
cmake -S . -B build-gcc \
  -DCMAKE_BUILD_TYPE=Debug \
  -DBUILD_TESTING=ON \
  -DCMAKE_PREFIX_PATH="${DEPENDENCY_PREFIX}"
cmake --build build-gcc --parallel
ctest --test-dir build-gcc --output-on-failure
```

## Build mínima

Esta configuração não obtém nem compila GoogleTest:

```sh
cmake -S . -B build-minimal \
  -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_TESTING=OFF \
  -DCMAKE_PREFIX_PATH="${DEPENDENCY_PREFIX}"
cmake --build build-minimal --parallel
```

## Execução

```sh
./build-gcc/host/telemetry_host_demo
```

Frames inválidos fazem parte do cenário normal e não causam código de saída
de erro. Falhas operacionais inesperadas retornam valor diferente de zero.

## Sanitizers

Use builds separadas e acrescente, conforme o diagnóstico desejado:

```sh
-DCMAKE_CXX_FLAGS="-fsanitize=address,undefined -fno-omit-frame-pointer"
-DCMAKE_EXE_LINKER_FLAGS="-fsanitize=address,undefined"
```

ou:

```sh
-DCMAKE_CXX_FLAGS="-fsanitize=thread -fno-omit-frame-pointer"
-DCMAKE_EXE_LINKER_FLAGS="-fsanitize=thread"
```

## Roadmap

Etapas futuras poderão adicionar firmware Zephyr, simulação Renode,
transportes e fault injection. Esses componentes não fazem parte do estado
atual.
