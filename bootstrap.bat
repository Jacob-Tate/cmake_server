@echo off

:: If they have previously run this script
if exist ./initialized (
    :: Delete the directory and fresh clone it
    rd /s /q "vcpkg"
)

:: Download vcpkg assuming it isnt installed
git clone https://github.com/Microsoft/vcpkg.git

:: cd to vcpkg
pushd vcpkg
call .\bootstrap-vcpkg.bat

:: Install the packages
.\vcpkg install nlohmann-json:x86-windows
.\vcpkg install nlohmann-json:x64-windows

:: Return the cwd
popd

:: Create a touch file to identify the build
echo.> initialized
