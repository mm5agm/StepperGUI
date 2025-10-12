@echo off
title Push MagLoop Common Files Changes
echo ============================================
echo Push MagLoop Common Files Changes to GitHub
echo ============================================
echo.

cd MagLoop_Common_Files

REM Check if there are changes
git diff --quiet
if %errorlevel% equ 0 (
    git diff --cached --quiet
    if %errorlevel% equ 0 (
        echo No changes found in MagLoop_Common_Files submodule
        echo.
        goto :main_project
    )
)

echo Found changes in submodule:
git status --short
echo.

REM Add all changes
git add .

REM Get commit message from user
set /p "commitmsg=Enter commit message (or press Enter for default): "
if "%commitmsg%"=="" (
    for /f "tokens=2 delims==" %%I in ('wmic os get localdatetime /value') do set datetime=%%I
    set datetime=%datetime:~0,4%-%datetime:~4,2%-%datetime:~6,2% %datetime:~8,2%:%datetime:~10,2%
    set commitmsg=Update shared files from StepperGUI - %datetime%
)

REM Commit changes
git commit -m "%commitmsg%"
if %errorlevel% neq 0 (
    echo ERROR: Failed to commit changes
    goto :error
)

REM Push to remote repository
echo.
echo Pushing to MagLoop_Common_Files repository...
git push origin main
if %errorlevel% neq 0 (
    echo ERROR: Failed to push to MagLoop_Common_Files repository
    goto :error
)

echo.
echo ✅ Successfully pushed to MagLoop_Common_Files repository!

:main_project
REM Go back to main project
cd ..

REM Update main project submodule reference
echo.
echo Updating main project submodule reference...
git add MagLoop_Common_Files
git commit -m "Update MagLoop_Common_Files submodule to latest version"

echo Pushing main project changes...
git push origin master
if %errorlevel% neq 0 (
    echo ERROR: Failed to push main project changes
    goto :error
)

echo.
echo ✅ All changes successfully pushed to both repositories!
echo Your changes are now visible on GitHub
echo.
goto :end

:error
echo.
echo ❌ An error occurred during the push process
echo.

:end
echo ============================================
echo Press any key to close this window...
pause > nul