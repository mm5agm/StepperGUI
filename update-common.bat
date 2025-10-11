@echo off
echo Updating MagLoop_Common_Files submodule...
git submodule update --remote --merge

echo Checking for changes...
git status --porcelain MagLoop_Common_Files > nul
if %errorlevel% == 0 (
    echo Committing and pushing submodule update...
    git add MagLoop_Common_Files
    git commit -m "Update MagLoop_Common_Files submodule to latest version"
    git push
    echo Submodule update complete!
) else (
    echo Submodule already up to date.
)
pause