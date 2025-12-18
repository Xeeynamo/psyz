@echo off
cd tests
if not exist "build\clang" mkdir build\clang
cd build\clang
cmake ../.. -T ClangCl -DCMAKE_BUILD_TYPE=Debug || (cd ..\..\.. && exit /b 1)
cmake --build . --config Debug || (cd ..\..\.. && exit /b 1)
copy /Y build\sdl\Debug\SDL3.dll Debug\
cd ..
clang\Debug\psyz_tests.exe
set TEST_RESULT=%errorlevel%
cd ..\..
exit /b %TEST_RESULT%
