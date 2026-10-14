@echo off
setlocal
"%~dp0deps\python\python.exe" "%~dp0steamvr\launch_plus.py" --mode vr %*
exit /b %errorlevel%
