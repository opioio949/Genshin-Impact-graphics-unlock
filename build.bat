@echo off
chcp 65001 >nul

:: 检查当前环境是否已有 MSVC 编译器
where cl >nul 2>&1
if %ERRORLEVEL%==0 goto BUILD

echo 查找 Visual Studio 编译环境...

:: 查找 vswhere.exe 路径
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" set "VSWHERE=%ProgramFiles%\Microsoft Visual Studio\Installer\vswhere.exe"

if not exist "%VSWHERE%" goto NO_VS

:: 获取 VS 安装目录
for /f "usebackq tokens=*" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do (
    set "VS_INSTALL_DIR=%%i"
)

if not defined VS_INSTALL_DIR goto NO_VS

set "VCVARS=%VS_INSTALL_DIR%\VC\Auxiliary\Build\vcvars64.bat"
if not exist "%VCVARS%" goto NO_VS

:: 加载环境
call "%VCVARS%" >nul 2>&1
if %ERRORLEVEL%==0 goto BUILD

:NO_VS
echo 未能找到 vcvars64.bat，请在 Visual Studio 命令提示符中运行此批处理。
pause
exit /b 1

:BUILD
echo 开始编译...

:: 1. 编译 DLL
cl /utf-8 /LD /O2 /nologo dxgi_hook.c /link /OUT:dxgi_hook.dll

:: 2. 编译资源
rc /nologo resource.rc

:: 3. 编译主程序
cl /utf-8 /O2 /nologo launcher_gui.c resource.res /link /SUBSYSTEM:WINDOWS user32.lib shell32.lib comdlg32.lib advapi32.lib gdi32.lib /OUT:launcher.exe

:: 4. 清理中间文件
del *.obj *.lib *.exp *.res dxgi_hook.dll >nul 2>&1

echo.
echo 编译完成：launcher.exe
pause