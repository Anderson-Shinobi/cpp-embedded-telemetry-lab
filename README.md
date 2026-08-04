# Embedded Telemetry Lab

Laboratório C++20 para telemetria binária portátil, com pipeline host e
firmware Zephyr concorrentes, determinísticos e testáveis.

## Status

**Zephyr Firmware Telemetry Producer:** o Protocol Core v1 é compartilhado por
uma aplicação host e por um firmware Zephyr C++20. O firmware gera oito
amostras determinísticas, serializa frames de 34 bytes, usa uma `k_msgq`
estática e duas threads, transmite oito linhas `TLFRAME` e termina com métricas
validadas.

Renode, hardware físico, transportes reais e fault injection avançado ainda
não estão implementados.

## Arquitetura

```text
Protocol Core v1
  ├─ Host Pipeline
  │   SimulatedFrameSource -> FrameIngestor -> SafeConcurrentBuffer
  │   -> TelemetryProcessor -> TelemetryMetrics -> Text Report
  └─ Firmware Producer
      DeterministicSensorModel -> TelemetryProducer -> serialize
      -> Zephyr k_msgq -> TelemetryTransmitter -> TLFRAME
```

O frame v1 possui 34 bytes em big-endian e CRC-32. Consulte
[docs/PROTOCOL.md](docs/PROTOCOL.md) e
[docs/HOST_PIPELINE.md](docs/HOST_PIPELINE.md). A aplicação Zephyr está
documentada em [docs/FIRMWARE.md](docs/FIRMWARE.md).

## Requisitos

- CMake 3.20 ou mais recente;
- compilador C++20 compatível;
- `cpp-safe-concurrent-buffer` v0.3.0 instalado;
- Zephyr 4.4.99 e Zephyr SDK 1.0.1 para o firmware;
- `west` e uma plataforma `native_sim` disponível;
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

## Firmware Zephyr

Build e execute no `native_sim`:

```sh
west build -b native_sim firmware -d build-v030-zephyr-native --pristine
./build-v030-zephyr-native/zephyr/zephyr.exe -no-rt
```

Compile para a NUCLEO-F401RE sem gravar a placa:

```sh
west build -b nucleo_f401re firmware \
  -d build-v030-zephyr-nucleo --pristine
```

Checkouts com espaços no caminho exigem o procedimento de build externo
descrito em [docs/FIRMWARE.md](docs/FIRMWARE.md), devido a uma limitação do
Zephyr 4.4.99.

## Testes

```sh
# 97 testes host
ctest --test-dir build-gcc --output-on-failure

# 43 testes Ztest no native_sim
west twister -T tests/firmware -p native_sim \
  --outdir build-v030-twister
```

Plataformas validadas:

- `native_sim/native`: build, execução determinística e Twister;
- `nucleo_f401re/stm32f401xe`: build ARM e geração de artefatos, sem execução
  física.

## Demo

Execute a demonstração limpa ou a apresentação completa:

```sh
./tools/run-firmware-demo.sh
./tools/present-firmware-demo.sh
```

Consulte [docs/GIF_DEMO.md](docs/GIF_DEMO.md) para preparar uma gravação curta.

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

Etapas futuras poderão adicionar simulação Renode, transportes e fault
injection. Esses componentes não fazem parte do estado atual.


## License

This project is released under the MIT License. See the [`LICENSE`](LICENSE) file for details.
