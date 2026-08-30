@echo off
setlocal
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat" x64 >nul 2>&1
set CUDA_PATH=C:\Program Files\NVIDIA GPU Computing Toolkit\CUDA\v13.1
cmake --build build-cuda --config Release
if %errorlevel% neq 0 exit /b %errorlevel%
build-cuda\src\cuda\cuda_residency_proof.exe
echo --- EXIT %errorlevel% ---
