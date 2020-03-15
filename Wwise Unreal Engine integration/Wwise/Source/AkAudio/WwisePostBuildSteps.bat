@echo off
if "%~f1"=="" exit /B 0

if %2=="" exit /B 0

if %2=="$(ProjectFile)" exit /B 0

if "%SKIP_PLUGIN_ACTIVATION%"=="true" exit /B 0

if "%3"=="Editor" exit /B 0

if not exist "%~f1" goto Error_EditorCmdNotFound
call %*
exit /B %ERRORLEVEL%

:Error_EditorCmdNotFound
echo WwisePostBuildSteps ERROR: Can't find "%~nx1"
pause
exit /B 1
