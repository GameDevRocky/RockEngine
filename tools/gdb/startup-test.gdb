set pagination off
set confirm off

# This script assumes GDB is launched with the repo root as the working directory.
file build/qt-mingw-debug/bin/RockEngineLauncher.exe

set environment QT_PLUGIN_PATH=C:/Qt/6.10.0/mingw_64/plugins
set environment QT_QPA_PLATFORM_PLUGIN_PATH=C:/Qt/6.10.0/mingw_64/plugins/platforms
set environment QT_DEBUG_PLUGINS=1
set environment PYTHONHOME=C:/Users/rockl/AppData/Local/Programs/Python/Python314
set environment PYTHONPATH=.
set environment PATH=C:/Users/rockl/AppData/Local/Programs/Python/Python314;C:/Users/rockl/AppData/Local/Programs/Python/Python314/DLLs;C:/Qt/Tools/mingw1310_64/bin;C:/Qt/6.10.0/mingw_64/bin;C:/Windows/System32;C:/Windows;C:/Windows/System32/Wbem

break main
run
quit
