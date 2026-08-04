# Demonstração em GIF — v0.3.0

## Preparação

- Ative previamente o ambiente Zephyr e defina `ZEPHYR_BASE`.
- Faça uma execução de aquecimento com `./tools/run-firmware-demo.sh` para que
  a gravação não inclua a configuração da build.
- Maximize o terminal, use fonte monoespaçada grande e reserve largura
  suficiente para que nenhuma linha `TLFRAME` seja quebrada.
- Oculte a barra lateral do editor e desative notificações durante a gravação.
- Enquadre somente o terminal e não mostre o nome completo de diretórios.
- Verifique que a tela não contém tokens, e-mails, credenciais ou outros dados
  privados.

## Sequência recomendada

A gravação deve durar aproximadamente de 12 a 18 segundos, sem áudio.

### Cena 1 — 2 segundos

Mostre o título:

```text
cpp-embedded-telemetry-lab
Zephyr Firmware Telemetry Producer — v0.3.0
```

O cabeçalho completo pode ser preparado com:

```sh
./tools/present-firmware-demo.sh --recording-mode
```

### Cena 2 — 3 segundos

Execute:

```sh
./tools/run-firmware-demo.sh
```

Não use `--rebuild` durante a gravação. A build deve ter sido preparada antes.

### Cena 3 — 7 a 10 segundos

Mantenha visíveis as oito linhas `TLFRAME` produzidas pela execução real e o
encerramento:

```text
TLFIRMWARE SUMMARY produced=8 transmitted=8 queue_errors=0
TLFIRMWARE DONE
```

Não edite, substitua ou simule a saída. Se a ferramenta de gravação permitir,
apenas prolongue o último quadro para dar tempo de leitura.

### Cena 4 — 2 segundos

Mostre o estado validado da release:

```text
Zephyr native_sim: PASS
Firmware tests: 43/43 PASS
Host regression: 97/97 PASS
NUCLEO-F401RE build: PASS
```

## Exportação

- Limite a duração total a aproximadamente 18 segundos.
- Exporte o GIF sem áudio.
- Revise o arquivo final para confirmar que não há caminhos locais, dados
  privados ou notificações.
- Caso o GIF perca legibilidade ou qualidade no LinkedIn, publique a mesma
  sequência em MP4.

Este documento descreve somente o fluxo de gravação. Ele não exige nem instala
programas de captura de tela.
