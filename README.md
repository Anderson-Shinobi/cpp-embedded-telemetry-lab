# Embedded Telemetry Lab

Núcleo portátil de um protocolo binário para telemetria embarcada. O componente
é independente de sistema operacional, RTOS, hardware e transporte, permitindo
que os mesmos frames sejam usados futuramente no firmware e na aplicação host.

## Status

**Protocol Core:** formato binário v1, CRC-32, serialização big-endian,
desserialização defensiva e testes host.

Zephyr, Renode, transportes e testes de integração serão adicionados em etapas
futuras e ainda não estão implementados.

## Requisitos

- CMake 3.20 ou mais recente;
- compilador C++20 compatível (GCC ou Clang);
- Git e acesso à rede apenas para obter a versão fixada do GoogleTest quando
  `BUILD_TESTING=ON`.

O núcleo do protocolo não possui dependências externas.

## Build e testes

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

Build somente da biblioteca, sem obter ou compilar GoogleTest:

```sh
cmake -S . -B build-minimal -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=OFF
cmake --build build-minimal --parallel
```

## Frame v1

Cada frame possui exatamente 34 bytes:

- magic `TL` (2 bytes);
- versão e tipo da mensagem (1 byte cada);
- tamanho do payload (2 bytes);
- sequência (4 bytes);
- timestamp em microssegundos (8 bytes);
- payload `telemetry_sample` (12 bytes);
- CRC-32 (4 bytes).

Todos os campos multibyte são serializados em big-endian. A especificação
completa está em [docs/PROTOCOL.md](docs/PROTOCOL.md).
