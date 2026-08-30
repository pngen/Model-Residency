@echo off
setlocal
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat" x64 >nul 2>&1
set CUDA_PATH=C:\Program Files\NVIDIA GPU Computing Toolkit\CUDA\v13.1
"C:\Program Files\NVIDIA GPU Computing Toolkit\CUDA\v13.1\bin\nvcc.exe" -std=c++20 --compiler-options=/std:c++20 -gencode arch=compute_120,code=sm_120 -o build-debug\_cuprobe.exe build-debug\_cuprobe.cu -lcudart
if %errorlevel% neq 0 exit /b %errorlevel%
build-debug\_cuprobe.exe
echo --- EXIT %errorlevel% ---
