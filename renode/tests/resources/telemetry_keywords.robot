*** Settings ***
Library         ${CURDIR}/telemetry_validation.py

*** Variables ***
${UART}                 sysbus.usart2
${BOOT_TIMEOUT}         10

*** Keywords ***
Prepare Telemetry Firmware
    Setup
    Execute Command             include @${RENODE_SCRIPT}
    ${peripherals}=             Execute Command    peripherals
    Should Contain              ${peripherals}    cpu (CortexM)
    Should Contain              ${peripherals}    usart2 (STM32_UART)
    ${main_address}=            Execute Command    sysbus GetSymbolAddress "main"
    Should Match Regexp         ${main_address}    0x[0-9A-Fa-f]+
    Execute Command             ${UART} CreateFileBackend @${CAPTURE_PATH}
    Create Terminal Tester      ${UART}    timeout=${BOOT_TIMEOUT}
    Start Emulation
    Wait For Line On Uart       Booting Zephyr OS build    timeout=${BOOT_TIMEOUT}
    Wait For Line On Uart       TLFIRMWARE DONE    timeout=${BOOT_TIMEOUT}
    Execute Command             pause
    Execute Command             ${UART} CloseFileBackend @${CAPTURE_PATH}

Clear Telemetry Firmware
    Execute Command             Clear
    Teardown

Capture Should Contain A Zephyr Boot
    Validate Zephyr Boot        ${CAPTURE_PATH}

Capture Should Contain Exactly Eight Frames
    Validate Frame Count        ${CAPTURE_PATH}

Frames Should Have Valid Encoding And Headers
    Validate Frame Encoding And Headers    ${CAPTURE_PATH}

Frames Should Have Ordered Indices Sequences And CRC
    Validate Indices Sequences And CRC    ${CAPTURE_PATH}

Summary Should Be Valid
    Validate Firmware Summary   ${CAPTURE_PATH}

Done Should Be Unique And Final
    Validate Firmware Done      ${CAPTURE_PATH}

Capture Should Be Fault Free
    Validate Fault Free         ${CAPTURE_PATH}
