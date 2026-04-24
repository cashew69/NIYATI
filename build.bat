@echo off

if not exist build mkdir build
pushd build

cl ^
/Zi /EHsc /MD ^
/I..\ ^
/I..\engine ^
/I..\engine\dependancies\glew\include ^
/I..\engine\dependancies\assimp\include ^
..\platforms\windows\winmain.cpp ^
/link ^
/LIBPATH:..\engine\dependancies\glew\lib\Release\x64 ^
/LIBPATH:..\engine\dependancies\assimp\lib ^
glew32.lib opengl32.lib user32.lib gdi32.lib kernel32.lib assimp-vc143-mt.lib ^
/OUT:window_win32.exe

copy /Y "..\engine\dependancies\assimp\bin\assimp-vc143-mt.dll" .
:: copy /Y "..\engine\dependancies\glew\bin\Release\x64\glew32.dll" .

popd