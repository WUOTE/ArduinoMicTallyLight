@echo off
setlocal
pushd "%~dp0.."
where cl >nul 2>&1
if errorlevel 1 (
  for /f "usebackq tokens=*" %%i in (`"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do call "%%i\Common7\Tools\VsDevCmd.bat" -no_logo -arch=x64
)
where cl >nul 2>&1
if errorlevel 1 (
  echo Install Visual Studio C++ Build Tools or run from a Developer Command Prompt.
  exit /b 1
)
if not exist build\tests mkdir build\tests
set "TALLY_TEST_FLAGS="
if /i "%~1"=="asan" set "TALLY_TEST_FLAGS=/fsanitize=address /Zi"
cl /nologo /std:c++17 /EHsc /W4 /WX %TALLY_TEST_FLAGS% /D_CRT_SECURE_NO_WARNINGS /I firmware\TallyLight\src tests\test_core.cpp firmware\TallyLight\src\controller.cpp firmware\TallyLight\src\protocol.cpp /Fe:build\tests\core-tests.exe /Fo:build\tests\ /Fd:build\tests\core-tests.pdb
if errorlevel 1 exit /b 1
build\tests\core-tests.exe
if errorlevel 1 exit /b 1
cl /nologo /std:c++17 /EHsc /W4 /WX %TALLY_TEST_FLAGS% /D_CRT_SECURE_NO_WARNINGS /FI test_config.h /I tests\stubs /I firmware\TallyLight\src tests\test_firmware.cpp tests\stubs\hardware.cpp firmware\TallyLight\src\controller.cpp firmware\TallyLight\src\protocol.cpp firmware\TallyLight\src\firmware.cpp /Fe:build\tests\firmware-tests.exe /Fo:build\tests\ /Fd:build\tests\firmware-tests.pdb
if errorlevel 1 exit /b 1
for %%s in (online offline missing-modem) do (
  build\tests\firmware-tests.exe %%s
  if errorlevel 1 exit /b 1
)
popd
exit /b 0
