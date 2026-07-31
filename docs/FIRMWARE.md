# Zephyr Firmware Telemetry Producer

## Objetivo

Esta aplicação C++20 executa no Zephyr RTOS, produz oito amostras simuladas e
determinísticas, reutiliza diretamente o Protocol Core v1 e transmite cada
frame binário de 34 bytes como uma linha hexadecimal. A implementação foi
validada com Zephyr 4.4.99 (`v4.4.0-10039-g746eb4060b3e`) e Zephyr SDK 1.0.1.

## Arquitetura

```text
DeterministicSensorModel
  -> TelemetryProducer
  -> telemetry::protocol::serialize
  -> k_msgq estática
  -> TelemetryTransmitter
  -> HexFormatter
  -> TLFRAME
  -> TLFIRMWARE SUMMARY
  -> TLFIRMWARE DONE
```

O firmware inclui diretamente os fontes do Protocol Core. Não existem cópias
dos tipos, do serializer ou do CRC. Os mesmos `TelemetryFrame`,
`TelemetrySample`, `SerializedFrame`, `StatusFlag`, `serialize` e `deserialize`
usados pelo host são usados pelo firmware e por seus testes.

## Modelo determinístico

O timestamp inicial é 1.000.000 microssegundos e o incremento fixo é 250.000
microssegundos. Nenhum relógio real, uptime, RTC ou periférico participa da
geração.

| Sequência | Timestamp (µs) | Temperatura (m°C) | Pressão (Pa) | Tensão (mV) | Flags |
|---:|---:|---:|---:|---:|---|
| 1000 | 1000000 | -5500 | 98500 | 2900 | none |
| 1001 | 1250000 | 0 | 100000 | 3300 | sensor_warning |
| 1002 | 1500000 | 12500 | 100800 | 3150 | voltage_warning |
| 1003 | 1750000 | 23000 | 101325 | 3300 | communication_warning |
| 1004 | 2000000 | 31500 | 102100 | 3250 | watchdog_event |
| 1005 | 2250000 | 45000 | 99500 | 2800 | sensor_warning, voltage_warning |
| 1006 | 2500000 | 18500 | 100500 | 3000 | communication_warning, watchdog_event |
| 1007 | 2750000 | 27000 | 101800 | 3350 | none |

## Fila e concorrência

`TelemetryQueue` encapsula uma `k_msgq` com armazenamento estático, capacidade
de quatro itens e operações bloqueantes com `K_FOREVER`. Cada
`TelemetryQueueItem` é trivialmente copiável e contém um discriminador
`frame`/`end_of_stream` e um `SerializedFrame` fixo.

Duas threads estáticas executam o pipeline:

| Thread | Prioridade | Stack |
|---|---:|---:|
| `telemetry_tx` | 4 | 2048 bytes |
| `telemetry_producer` | 5 | 2048 bytes |

O transmissor tem prioridade maior e bloqueia na fila até receber dados. O
produtor envia oito frames e exatamente um marcador `end_of_stream`. O `main`
aguarda as duas conclusões com semáforos, sem polling e sem sleeps. No
`native_sim`, `nsi_exit` encerra oficialmente o simulador após a validação das
métricas.

## Métricas

O snapshot final esperado é:

```text
samples_generated=8
frames_serialized=8
frames_enqueued=8
frames_transmitted=8
queue_push_failures=0
queue_pop_failures=0
end_markers_sent=1
end_markers_received=1
```

Cada frame usa 68 caracteres hexadecimais maiúsculos:

```text
TLFRAME 0001 <68 caracteres>
```

Após o oitavo frame:

```text
TLFIRMWARE SUMMARY produced=8 transmitted=8 queue_errors=0
TLFIRMWARE DONE
```

## Build e execução no native_sim

Em um checkout cujo caminho não contenha espaços:

```sh
west build -b native_sim firmware -d build-v030-zephyr-native --pristine
./build-v030-zephyr-native/zephyr/zephyr.exe -no-rt
```

O Zephyr 4.4.99 não processa corretamente alguns caminhos de Kconfig, arquivos
gerados e seções estáticas quando o checkout contém espaços. Para esse caso,
foi validado um build externo sem espaços, mantendo os fontes no checkout
original:

```sh
install -m 0644 firmware/prj.conf /tmp/telemetry-firmware-prj.conf
west build -b native_sim firmware -d /tmp/build-v030-zephyr-native \
  --pristine -- -DCONF_FILE=/tmp/telemetry-firmware-prj.conf
/tmp/build-v030-zephyr-native/zephyr/zephyr.exe -no-rt
```

Três execuções produziram saída controlada byte a byte idêntica.

## Ztest e Twister

A suíte possui 43 casos e cobre o modelo, protocolo, CRC, round-trip, item de
fila, formatter, métricas, ordenação, marcador final e cenário concorrente.

```sh
install -m 0644 tests/firmware/prj.conf \
  /tmp/telemetry-firmware-test-prj.conf
west twister -T tests/firmware -p native_sim \
  --outdir /tmp/build-v030-twister \
  --short-build-path \
  --extra-args CONF_FILE=/tmp/telemetry-firmware-test-prj.conf
```

Resultado validado: uma configuração, 43 casos aprovados, zero falhas, zero
erros, zero ignorados, plataforma `native_sim/native`.

## Build para NUCLEO-F401RE

```sh
west build -b nucleo_f401re firmware -d /tmp/build-v030-zephyr-nucleo \
  --pristine -- -DCONF_FILE=/tmp/telemetry-firmware-prj.conf
```

O build usou `arm-zephyr-eabi` GCC 14.3 e gerou `zephyr.elf`, `zephyr.bin` e
`zephyr.hex`. Uso reportado:

- flash: 30.624 bytes de 512 KiB (5,84%);
- RAM: 12.428 bytes de 96 KiB (12,64%).

Nenhum `west flash`, debug, attach ou reset foi executado. A placa foi apenas
alvo de compilação; não houve validação física.

## Limitações e roadmap

Esta missão não inclui sensores físicos, UART adicional, rede, sockets, MQTT,
Bluetooth, filesystem, interface gráfica, Renode, Robot Framework,
hardware-in-the-loop ou fault injection avançado. A Missão 4 poderá adicionar
simulação de sistema e cenários de falha sem alterar o protocolo v1.
