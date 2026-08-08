@echo off
cd tests
if not exist "build\msvc" mkdir build\msvc
cd build\msvc
cmake ../.. -DCMAKE_BUILD_TYPE=Debug || (cd ..\..\.. && exit /b 1)
cmake --build . --config Debug || (cd ..\..\.. && exit /b 1)
copy /Y build\sdl\Debug\SDL3.dll Debug\
cd ..\..
build\msvc\Debug\psyz_tests.exe
set TEST_RESULT=%errorlevel%
cd ..
exit /b %TEST_RESULT%
