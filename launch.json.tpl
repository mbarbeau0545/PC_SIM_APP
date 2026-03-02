// AUTOMATICALLY GENERATED FILE. PLEASE DO NOT MODIFY IT MANUALLY
//
// PlatformIO Debugging Solution
//
// Documentation: https://docs.platformio.org/en/latest/plus/debugging.html
// Configuration: https://docs.platformio.org/en/latest/projectconf/sections/env/options/debug/index.html

{
    "version": "0.2.0",
    "configurations": [
        {
            "name": "C/C++ Runner: Debug Session",
            "type": "cppdbg",
            "request": "launch",
            "args": [],
            "stopAtEntry": false,
            "externalConsole": true,
            "cwd": "d:/Project/Software/STM32/Gamma_fMotionPlanner",
            "program": "d:/Project/Software/STM32/Gamma_fMotionPlanner/build/Debug/outDebug",
            "MIMode": "gdb",
            "miDebuggerPath": "C:/Users/mbarb/.platformio/packages/toolchain-gdbarmnoneeabi@1.70201.0/bin/arm-none-eabi-gcc.exe",
            "setupCommands": [
                {
                    "description": "Enable pretty-printing for gdb",
                    "text": "-enable-pretty-printing",
                    "ignoreFailures": true
                },
                {
                    "description": "Allow pending breakpoints",
                    "text": "-gdb-set breakpoint pending on",
                    "ignoreFailures": true
                },
                {
                    "description": "Enable async mode",
                    "text": "-gdb-set mi-async on",
                    "ignoreFailures": true
                }
            ]
        },
        {
            "name": "Debug PythonTool",
            "type": "debugpy",
            "request": "launch",
            "program": "${file}",
            "args": [
                "Doc/ConfigPrj/GammaCfg/FirmwareSafetyCfg/Safety_STM32G474RE_HwCfg.xlsx",
                "Doc/ConfigPrj/GammaCfg/FirmwareSafetyCfg/SafetyProject_SoftwareCfg.xlsm"
            ]
        },
        {
            "name": "PC_SIM Debug (default)",
            "type": "cppdbg",
            "request": "launch",
            "program": "${workspaceFolder}/.pio/build/pc_sim_debug/program.exe",
            "args": [
                "--udp-port",
                "19090",
                "--sleep-ms",
                "1"
            ],
            "cwd": "${workspaceFolder}",
            "MIMode": "gdb",
            "miDebuggerPath": "C:/msys64/ucrt64/bin/gdb.exe",
            "externalConsole": true,
            "stopAtEntry": false,
            "preLaunchTask": "pc_sim_debug_prepare",
            "postDebugTask": "pc_sim_debug_kill",
            "setupCommands": [
                {
                    "description": "Enable pretty-printing for gdb",
                    "text": "-enable-pretty-printing",
                    "ignoreFailures": true
                },
                {
                    "description": "Allow pending breakpoints",
                    "text": "-gdb-set breakpoint pending on",
                    "ignoreFailures": true
                },
                {
                    "description": "Enable async mode",
                    "text": "-gdb-set mi-async on",
                    "ignoreFailures": true
                }
            ]
        },
        {
            "name": "PC_SIM Debug (CAN scenario)",
            "type": "cppdbg",
            "request": "launch",
            "program": "${workspaceFolder}/.pio/build/pc_sim_debug/program.exe",
            "args": [
                "--udp-port",
                "19090",
                "--sleep-ms",
                "1",
                "--ana",
                "1",
                "3000"
            ],
            "cwd": "${workspaceFolder}",
            "MIMode": "gdb",
            "miDebuggerPath": "C:/msys64/ucrt64/bin/gdb.exe",
            "externalConsole": true,
            "stopAtEntry": true,
            "preLaunchTask": "pc_sim_debug_build",
            "postDebugTask": "pc_sim_debug_kill",
            "setupCommands": [
                {
                    "description": "Enable pretty-printing for gdb",
                    "text": "-enable-pretty-printing",
                    "ignoreFailures": true
                }
            ]
        },
        {
            "type": "platformio-debug",
            "request": "launch",
            "name": "PIO Debug",
            "executable": "D:/Project/Software/STM32/Gamma/Gamma_Safety_AddCfg/.pio/build/nucleo_g474re/firmware.elf",
            "projectEnvName": "pc_sim_debug",
            "toolchainBinDir": "C:/Users/mbarb/.platformio/packages/toolchain-gccarmnoneeabi@1.70201.0/bin",
            "internalConsoleOptions": "openOnSessionStart",
            "svdPath": "C:/Users/mbarb/.platformio/platforms/ststm32/misc/svd/STM32G474xx.svd",
            "preLaunchTask": {
                "type": "PlatformIO",
                "task": "Pre-Debug"
            }
        },
        {
            "type": "platformio-debug",
            "request": "launch",
            "name": "PIO Debug (skip Pre-Debug)",
            "executable": "D:/Project/Software/STM32/Gamma/Gamma_Safety_AddCfg/.pio/build/nucleo_g474re/firmware.elf",
            "projectEnvName": "pc_sim_debug",
            "toolchainBinDir": "C:/Users/mbarb/.platformio/packages/toolchain-gccarmnoneeabi@1.70201.0/bin",
            "internalConsoleOptions": "openOnSessionStart",
            "svdPath": "C:/Users/mbarb/.platformio/platforms/ststm32/misc/svd/STM32G474xx.svd"
        },
        {
            "type": "platformio-debug",
            "request": "launch",
            "name": "PIO Debug (without uploading)",
            "executable": "D:/Project/Software/STM32/Gamma/Gamma_Safety_AddCfg/.pio/build/nucleo_g474re/firmware.elf",
            "projectEnvName": "pc_sim_debug",
            "toolchainBinDir": "C:/Users/mbarb/.platformio/packages/toolchain-gccarmnoneeabi@1.70201.0/bin",
            "internalConsoleOptions": "openOnSessionStart",
            "svdPath": "C:/Users/mbarb/.platformio/platforms/ststm32/misc/svd/STM32G474xx.svd",
            "loadMode": "manual"
        }
    ]
}
