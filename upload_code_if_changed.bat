
@echo off
REM Upload changes to StepperGUI and MagLoop_Common_Files

REM Always prompt for commit message before checking for changes
set /p commit_msg=Enter commit message for StepperGUI: 
git status --porcelain > temp_status.txt
findstr /r /c:"^.." temp_status.txt >nul
if %errorlevel%==0 (
    echo Changes detected in StepperGUI. Pushing to GitHub...
    echo Staged files:
    git status --short
    git add -A
    echo After staging:
    git status --short
    git commit -m "%commit_msg%"
    for /f "delims=* " %%b in ('git rev-parse --abbrev-ref HEAD') do set current_branch=%%b
    git push origin %current_branch%
) else (
    echo No changes to push in StepperGUI.
)
del temp_status.txt

REM Change to submodule directory
cd MagLoop_Common_Files

REM Check for changes in submodule (MagLoop_Common_Files)
git status --porcelain > temp_status.txt
findstr /r /c:"^.." temp_status.txt >nul
if %errorlevel%==0 (
    echo Changes detected in MagLoop_Common_Files. Pushing to GitHub...
    echo Staged files:
    git status --short
    git add .
    echo After staging:
    git status --short
    git commit -m "Update MagLoop_Common_Files"
    git push origin main
) else (
    echo No changes to push in MagLoop_Common_Files.
)
del temp_status.txt

REM Return to main project directory
cd ..

echo.
echo Upload script complete. All done.
pause