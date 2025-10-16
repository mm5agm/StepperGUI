@echo off
REM Download the latest code from GitHub for StepperGUI and MagLoop_Common_Files

REM Pull latest changes for main project (StepperGUI)
echo Pulling latest changes for StepperGUI...
git pull origin master

REM Change to submodule directory
cd MagLoop_Common_Files

REM Pull latest changes for submodule (MagLoop_Common_Files)
echo Pulling latest changes for MagLoop_Common_Files...
git pull origin main

REM Return to main project directory
cd ..

echo.
echo Done downloading latest code from both repositories.
pause