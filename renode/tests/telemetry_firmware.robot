*** Settings ***
Documentation       Headless validation of the real NUCLEO-F401RE Zephyr ELF.
Resource            resources/telemetry_keywords.robot
Suite Setup         Prepare Telemetry Firmware
Suite Teardown      Clear Telemetry Firmware

*** Test Cases ***
Platform ELF And Zephyr Should Load
    Capture Should Contain A Zephyr Boot

Exactly Eight Frames Should Be Captured
    Capture Should Contain Exactly Eight Frames

Frame Encoding And Headers Should Match Protocol V1
    Frames Should Have Valid Encoding And Headers

Indices Sequences And CRC Should Be Valid
    Frames Should Have Ordered Indices Sequences And CRC

Firmware Summary Should Report Complete Transmission
    Summary Should Be Valid

Firmware Done Should Be Unique And Final
    Done Should Be Unique And Final

Firmware Execution Should Be Fault Free
    Capture Should Be Fault Free
