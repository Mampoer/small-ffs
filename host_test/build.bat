@echo off
REM ---- locate vcvars64.bat ----------------------------------------------------
setlocal enabledelayedexpansion
set "VCVARS="
for %%P in (
  "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
  "C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvars64.bat"
  "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvars64.bat"
  "C:\Program Files\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
  "C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvars64.bat"
) do (
  if exist %%~P set "VCVARS=%%~P"
)

if "%VCVARS%"=="" (
  echo Could not find vcvars64.bat — install Visual Studio 2022 ^(or Build Tools^) and rerun.
  exit /b 1
)

call "%VCVARS%" >nul 2>&1

REM ---- paths -------------------------------------------------------------------
set "HERE=%~dp0"
if "%HERE:~-1%"=="\" set "HERE=%HERE:~0,-1%"
set "REPO=%HERE%\.."
set "OUT=%HERE%\build"
if not exist "%OUT%" mkdir "%OUT%"

REM ---- shadow fs.c -------------------------------------------------------------
REM Patches applied for host build only (committed fs.c is unchanged):
REM   1. Strip <stdio.h>/<stdarg.h> — they'd drag MSVC libc's FILE typedef.
REM   2. Inject the global declarations that the 2022 refactor commented out
REM      (open_file_list, current_fat, start_search_page, wear_threshold,
REM      next_file_num, open_file_list_size) — fs.c references them as bare
REM      names but the singleton fs_t was never wired up, so the committed
REM      file doesn't link even on Keil.
REM   3. Strip the file_printf function body — uses Keil's __va_list /
REM      putchar_func_pointer retargeting which isn't on host.
powershell -NoProfile -ExecutionPolicy Bypass -File "%HERE%\shadow_fs.ps1" "%REPO%\src\fs.c" "%OUT%\fs_host.c"
if errorlevel 1 (
  echo Shadow step failed.
  exit /b 1
)

REM ---- compile ----------------------------------------------------------------
set "CC=cl /nologo /W3 /Od /Zi /MD /D_CRT_SECURE_NO_WARNINGS /I "%REPO%\inc" /I "%HERE%" /FI "%HERE%\host_shim.h""

%CC% /DFS_HOST_FS_C /c /Fo"%OUT%\fs_host.obj" "%OUT%\fs_host.c"   || goto :fail
%CC%                /c /Fo"%OUT%\list.obj"    "%REPO%\src\list.c" || goto :fail
%CC%                /c /Fo"%OUT%\flash.obj"   "%HERE%\flash_driver_mock.c" || goto :fail
%CC%                /c /Fo"%OUT%\cb.obj"      "%HERE%\fs_callbacks.c"      || goto :fail
%CC%                /c /Fo"%OUT%\test.obj"    "%HERE%\test_main.c"         || goto :fail

cl /nologo /Fe"%HERE%\small-ffs-test.exe" "%OUT%\fs_host.obj" "%OUT%\list.obj" "%OUT%\flash.obj" "%OUT%\cb.obj" "%OUT%\test.obj" || goto :fail

echo.
echo Built %HERE%\small-ffs-test.exe
endlocal
exit /b 0

:fail
echo Build failed.
endlocal
exit /b 1
