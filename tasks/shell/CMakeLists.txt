# ============================================================
# MICROSHELL LIBRARY TARGET
# This can be linked by external projects
# microshell Copyright (c) 2021 Marcin Borowicz <marcinbor85@gmail.com>
# Huge thanks to Marcin for this amazing project
# ============================================================

add_library(microshell INTERFACE)

target_sources(microshell INTERFACE
        shellminator/src/Shellminator-Browser-Response.hpp
        shellminator/src/Shellminator-VT100-Commands.hpp
        shellminator/src/Shellminator-Screen.hpp
        shellminator/src/Shellminator-BufferedPrinter.cpp
        shellminator/src/Shellminator-BufferedPrinter.hpp
        shellminator/src/Shellminator-Neofetch.cpp
        shellminator/src/Shellminator-Neofetch.hpp
        shellminator/src/Shellminator-GUI.hpp
        shellminator/src/GUI/Shellminator-PlotModule.cpp
        shellminator/src/GUI/Shellminator-PlotModule.hpp
        shellminator/src/GUI/Shellminator-Buttons.cpp
        shellminator/src/GUI/Shellminator-Buttons.hpp
        shellminator/src/GUI/Shellminator-Progress.hpp
        shellminator/src/GUI/Shellminator-Progress.cpp
        shellminator/src/GUI/Shellminator-List.hpp
        shellminator/src/GUI/Shellminator-List.cpp
        shellminator/src/GUI/Shellminator-ScreenGrid.hpp
        shellminator/src/GUI/Shellminator-ScreenGrid.cpp
        shellminator/src/GUI/Shellminator-List-Detailed.hpp
        shellminator/src/GUI/Shellminator-List-Detailed.cpp
        shellminator/src/GUI/Shellminator-Level-Meter.hpp
        shellminator/src/GUI/Shellminator-Level-Meter.cpp
        shellminator/src/GUI/Shellminator-Notification.hpp
        shellminator/src/GUI/Shellminator-Notification.cpp
        shellminator/src/Shellminator-DefaultSettings.hpp
        shellminator/src/Shellminator.hpp
        shellminator/src/Shellminator.cpp
        shellminator/src/Shellminator-SpecialKeys.cpp
        shellminator/src/Shellminator-Styles.cpp
        shellminator/src/Shellminator-SearchModule.cpp
        shellminator/src/Shellminator-MouseInterface.cpp
        shellminator/src/Shellminator-Crypto.hpp
        shellminator/src/Shellminator-Crypto.cpp
        shellminator/src/Shellminator-Websocket.hpp
        shellminator/src/Shellminator-Websocket.cpp
        shellminator/src/Shellminator-WebServer.hpp
        shellminator/src/Shellminator-WebServer.cpp
        shellminator/src/Shellminator-Input.cpp
        shellminator/src/Shellminator-Events.cpp
        shellminator/src/Shellminator-QR-Code-Module.hpp
        shellminator/src/Shellminator-QR-Code-Module.cpp
        shellminator/src/Shellminator-Colorizer.hpp
        shellminator/src/Shellminator-Colorizer.cpp
        shellminator/src/Shellminator-Commander-Interface.hpp
        shellminator/src/Shellminator-Commander-Interface.cpp
        shellminator/src/Shellminator-TcpSocket.hpp
        shellminator/src/Shellminator-TcpSocket.cpp
        shellminator/src/Shellminator-Helpers.hpp
        shellminator/src/Shellminator-Helpers.cpp
)

target_include_directories(microshell INTERFACE
        ${CMAKE_CURRENT_LIST_DIR}/shellminator/src/
)

target_link_libraries(microshell INTERFACE
        pico_stdlib
)