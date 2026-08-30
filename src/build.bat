@echo off
rem MemoryCleaner Windows build script (MinGW-w64)
setlocal
if "%1"=="" (
    set MINGW=%LOCALAPPDATA%\Microsoft\WinGet\Packages\BrechtSanders.WinLibs.POSIX.UCRT_Microsoft.Winget.Source_8wekyb3d8bbwe\mingw64\bin
) else (
    set MINGW=%1
)
set OUT=..\dist\Castling.exe
echo [1/2] windres resources...
"%MINGW%\windres.exe" memclean.rc -O coff -o memclean_res.o || exit /b 1
echo [2/2] gcc build...
"%MINGW%\gcc.exe" -mwindows -O2 -static -specs=gcc.specs memclean.c memclean_res.o -o "%OUT%" -lcomctl32 -lpsapi -lgdiplus -lole32 || exit /b 1
echo Done: %OUT%
endlocal
