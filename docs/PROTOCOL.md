# Protocolo de Telemetria

## Objetivo

O protocolo define uma representação binária pequena, determinística e
independente de arquitetura para transportar amostras de telemetria entre
componentes embarcados e aplicações host. A versão 1 não define transporte,
framing de stream, retransmissão ou concorrência.

## Formato do frame v1

Um frame v1 possui tamanho fixo de 34 bytes. O cabeçalho ocupa 18 bytes, o
payload ocupa 12 bytes e o CRC ocupa 4 bytes.

| Offset | Tamanho | Campo | Codificação |
|---:|---:|---|---|
| 0 | 2 | Magic | bytes `54 4C` (`TL`) |
| 2 | 1 | Protocol version | `01` |
| 3 | 1 | Message type | `01` (`telemetry_sample`) |
| 4 | 2 | Payload size | inteiro sem sinal, valor 12 |
| 6 | 4 | Sequence | inteiro sem sinal |
| 10 | 8 | Timestamp | microssegundos, inteiro sem sinal |
| 18 | 4 | Temperature | milésimos de grau Celsius, inteiro com sinal |
| 22 | 4 | Pressure | pascal, inteiro sem sinal |
| 26 | 2 | Supply voltage | milivolts, inteiro sem sinal |
| 28 | 2 | Status flags | máscara de bits sem sinal |
| 30 | 4 | CRC-32 | inteiro sem sinal |

## Endianness

Todos os campos multibyte são codificados em big-endian (network byte order).
A implementação escreve e lê cada campo explicitamente; não serializa a
representação em memória de structs.

## CRC-32

O CRC cobre os bytes nos offsets 0 a 29. Os quatro bytes finais reservados ao
próprio CRC não participam do cálculo.

- polinômio refletido: `0xEDB88320`;
- valor inicial: `0xFFFFFFFF`;
- XOR final: `0xFFFFFFFF`;
- entrada refletida;
- saída refletida;
- vetor de verificação `123456789`: `0xCBF43926`.

O CRC calculado é armazenado em big-endian nos offsets 30 a 33.

## Limites da versão 1

A versão 1 aceita somente:

- versão `1`;
- tipo de mensagem `1` (`telemetry_sample`);
- payload de exatamente 12 bytes;
- frame de exatamente 34 bytes;
- quatro flags conhecidas: aviso de sensor, aviso de tensão, aviso de
  comunicação e evento de watchdog.

O protocolo preserva os 16 bits do campo de flags. A interpretação de bits
reservados fica a cargo de uma futura política de compatibilidade.

## Erros de parsing

A validação ocorre antes da extração do payload e retorna um resultado
explícito, sem exceções:

- `invalid_frame_size`: tamanho diferente de 34 bytes;
- `invalid_magic`: magic diferente de `TL`;
- `unsupported_version`: versão diferente de 1;
- `unsupported_message_type`: tipo diferente de `telemetry_sample`;
- `invalid_payload_size`: payload size diferente de 12;
- `checksum_mismatch`: CRC recebido diferente do CRC calculado.

As validações seguem exatamente essa ordem. Em qualquer erro, nenhum frame é
retornado.

## Exemplo válido

Frame com sequência 1, timestamp 2, temperatura 1000 m°C, pressão 100000 Pa,
tensão 3300 mV e flag de aviso de sensor:

```text
54 4C 01 01 00 0C 00 00 00 01 00 00 00 00 00 00 00 02
00 00 03 E8 00 01 86 A0 0C E4 00 01 B6 FA E4 FA
```

O CRC deste exemplo é `0xB6FAE4FA`.

## Compatibilidade futura

Novas versões devem receber um novo valor de versão e uma especificação
própria. Implementações v1 rejeitam versões, tipos e tamanhos desconhecidos em
vez de inferir layouts. Campos existentes não terão semântica alterada dentro
da mesma versão.
