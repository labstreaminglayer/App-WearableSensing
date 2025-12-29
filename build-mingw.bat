:: To run this .bat use .\build-mingw.bat
:: Make sure mingw64 is installed and the paths are set correctly (Very Important!!!)

@echo off
setlocal

:: Adjust path to your 64-bit MinGW-w64 bin directory
set PATH=C:\ProgramData\mingw64\mingw64\bin;%PATH%

:: Output folders - create mingwbuild folder
set OUT=mingwbuild
if not exist %OUT% mkdir %OUT%

:: LSL lib and include paths
set LSL_INC="YOUR_PATH_TO_LIBLSL_INCLUDE"
set LSL_LIB="YOUR_PATH_TO_LIBLSL_LIB"
set QT_INC="YOUR_PATH_TO_QT5_MINGW64_INCLUDE"
set QT_LIB="YOUR_PATH_TO_QT5_MINGW_LIB"
set QT_BIN="YOUR_PATH_TO_QT5_MINGW64_BIN"

:: Add Qt bin to PATH for tools
set PATH=%QT_BIN%;%PATH%

:: Build dsi2lsl (console app)
echo Building dsi2lsl...
gcc CLI\dsi2lsl.c ^
    DSI_API_v1.20.3_06202025\DSI_API_Loader.c ^
    -I DSI_API_v1.20.3_06202025 ^
    -I %LSL_INC% ^
    -L %LSL_LIB% -llsl ^
    -o %OUT%\dsi2lsl.exe

if %ERRORLEVEL% neq 0 (
    echo Failed to build dsi2lsl!
    pause
    exit /b 1
)

:: Create output directories for Qt tools
if not exist %OUT%\ui mkdir %OUT%\ui
if not exist %OUT%\moc mkdir %OUT%\moc

:: Generate Qt UIC file (converts .ui to .h)
echo Generating Qt UIC files...
uic.exe GUI\mainwindow.ui -o %OUT%\ui\ui_mainwindow.h

if %ERRORLEVEL% neq 0 (
    echo UIC failed! Check if Qt tools are in PATH.
    pause
    exit /b 1
)

:: Generate Qt MOC files
echo Generating Qt MOC files...
moc.exe GUI\mainwindow.h -o %OUT%\moc\moc_mainwindow.cpp

if %ERRORLEVEL% neq 0 (
    echo MOC failed! Check if Qt tools are in PATH.
    pause
    exit /b 1
)

:: Build GUI app
echo Building GUI...
g++ GUI\main.cpp GUI\mainwindow.cpp %OUT%\moc\moc_mainwindow.cpp ^
    -I GUI -I %OUT%\ui -I %LSL_INC% ^
    -I %QT_INC% -I %QT_INC%\QtCore -I %QT_INC%\QtGui -I %QT_INC%\QtWidgets ^
    -L %LSL_LIB% -llsl ^
    -L %QT_LIB% -lQt5Core -lQt5Gui -lQt5Widgets ^
    -mwindows ^
    -o %OUT%\dsi2lslGUI.exe

if %ERRORLEVEL% neq 0 (
    echo Failed to build GUI!
    pause
    exit /b 1
)

:: Copy MinGW runtime DLLs (needed for distribution)
echo Copying MinGW runtime DLLs...
copy "YOUR_PATH_TO_MINGW64_BIN_LIBGCC_S_SEH_1.DLL" "%OUT%\" >nul
copy "YOUR_PATH_TO_MINGW64_BIN_LIBSTDC++_6.DLL" "%OUT%\" >nul
copy "YOUR_PATH_TO_MINGW64_BIN_LIBWINPTHREAD_1.DLL" "%OUT%\" >nul

:: Copy required DLLs
echo Copying required DLLs...
copy "%QT_BIN%\Qt5Core.dll" "%OUT%\" >nul
copy "%QT_BIN%\Qt5Gui.dll" "%OUT%\" >nul
copy "%QT_BIN%\Qt5Widgets.dll" "%OUT%\" >nul
copy "YOUR_PATH_TO_LIBLSL__BIN_LSL.DLL" "%OUT%\" >nul 
copy "YOUR_PATH_TO_LIBDSI.DLL" "%OUT%\" >nul 

:: Copy Qt platform plugins (REQUIRED for Qt GUI apps!)
echo Copying Qt platform plugins...
if not exist %OUT%\platforms mkdir %OUT%\platforms
copy "YOUR_PATH_TO_QT5_MINGW64_PLUGINS_PLATFORMS_QWINDOWS.dll" "%OUT%\platforms\" >nul

echo.
echo Build completed successfully!
echo Executables are in the %OUT% folder:
echo - %OUT%\dsi2lsl.exe
echo - %OUT%\dsi2lslGUI.exe
echo.
pause