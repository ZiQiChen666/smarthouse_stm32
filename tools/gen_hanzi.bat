@echo off
chcp 65001 >nul
setlocal
cd /d "%~dp0"
python "%~dp0gen_hanzi.py"
echo.
echo ---- exit code: %errorlevel% ----
pause
