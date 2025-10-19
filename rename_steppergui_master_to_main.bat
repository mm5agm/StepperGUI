@echo off
REM Rename 'master' branch to 'main' locally and on GitHub for StepperGUI

REM Step 1: Checkout master branch

git checkout master

REM Step 2: Rename master to main locally

git branch -m master main

REM Step 3: Push main branch to GitHub

git push origin main

git push --set-upstream origin main

REM Step 4: Instruct user to change default branch on GitHub

echo Please go to your StepperGUI repository on GitHub, then:
echo 1. Click Settings > Branches

echo 2. Change the default branch to 'main'

echo 3. After changing, press any key to continue...
pause

REM Step 5: Delete master branch on GitHub

git push origin --delete master

echo Branch 'master' renamed to 'main' locally and on GitHub.
pause
