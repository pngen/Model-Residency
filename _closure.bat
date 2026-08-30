@echo off
setlocal
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat" x64 >nul 2>&1
echo ============ RELEASE CLOSURE ============
cmake --build build-release --config Release >nul
echo -- release tests --
build-release\test\mr_tests.exe | findstr /R "checks:"
echo -- multiprocess proof (release) --
build-release\distributed\multiprocess_proof.exe "E:\The Journey\Coding\GitHub\production\Model-Residency\build-release\distributed\mr_coordinator.exe" "E:\The Journey\Coding\GitHub\production\Model-Residency\build-release\distributed\mr_worker.exe" | findstr /R "PASSED"
echo ============ DEBUG CLOSURE ============
cmake --build build-debug --config Debug >nul
echo -- debug tests --
build-debug\test\mr_tests.exe | findstr /R "checks:"
echo ============ CUDA CLOSURE (release) ============
build-cuda\src\cuda\cuda_residency_proof.exe | findstr /R "PASSED"
echo --- EXIT %errorlevel% ---
