@echo off
setlocal
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat" x64 >nul 2>&1
cd /d "E:\The Journey\Coding\GitHub\production\Model-Residency"
"C:\Program Files\NVIDIA GPU Computing Toolkit\CUDA\v13.1\bin\nvcc.exe" -std=c++20 -gencode arch=compute_120,code=sm_120 -I include -c src/cuda/cuda_backend.cu -o build-cuda/manual_test.obj -Xcompiler=/W4 -Xcompiler=/WX -Xcompiler=/permissive-
echo --- EXIT %errorlevel% ---
