# PowerShell script to update MagLoop_Common_Files submodule to latest version

Write-Host "Updating MagLoop_Common_Files submodule..." -ForegroundColor Green

# Update the submodule to latest remote commit
git submodule update --remote MagLoop_Common_Files

# Check if there are changes
$changes = git diff --quiet HEAD -- MagLoop_Common_Files
if ($LASTEXITCODE -eq 0) {
    Write-Host "No updates available for MagLoop_Common_Files" -ForegroundColor Yellow
} else {
    Write-Host "MagLoop_Common_Files updated to latest version" -ForegroundColor Green
    
    # Add and commit the submodule update
    git add MagLoop_Common_Files
    git commit -m "Update MagLoop_Common_Files submodule to latest version"
    
    Write-Host "Changes committed. You can now push with: git push origin master" -ForegroundColor Cyan
}

Write-Host "Done!" -ForegroundColor Green