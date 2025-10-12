# PowerShell script to push MagLoop_Common_Files changes to GitHub

Write-Host "Pushing MagLoop_Common_Files changes to GitHub..." -ForegroundColor Green

# Navigate to submodule directory
Push-Location MagLoop_Common_Files

try {
    # Check if there are any changes to commit
    $status = git status --porcelain
    
    if ($status) {
        Write-Host "Found changes in submodule:" -ForegroundColor Yellow
        git status --short
        Write-Host ""
        
        # Add all changes
        git add .
        
        # Prompt for commit message
        $commitMessage = Read-Host "Enter commit message (or press Enter for default)"
        if ([string]::IsNullOrWhiteSpace($commitMessage)) {
            $commitMessage = "Update shared files from StepperGUI - $(Get-Date -Format 'yyyy-MM-dd HH:mm')"
        }
        
        # Commit changes
        git commit -m $commitMessage
        
        # Push to remote repository
        Write-Host "Pushing to MagLoop_Common_Files repository..." -ForegroundColor Cyan
        git push origin main
        
        if ($LASTEXITCODE -eq 0) {
            Write-Host "✅ Successfully pushed to MagLoop_Common_Files repository!" -ForegroundColor Green
        } else {
            Write-Host "❌ Failed to push to MagLoop_Common_Files repository" -ForegroundColor Red
            Pop-Location
            Read-Host "Press any key to close..."
            exit 1
        }
    } else {
        Write-Host "No changes found in MagLoop_Common_Files submodule" -ForegroundColor Yellow
        Pop-Location
        Read-Host "Press any key to close..."
        exit 0
    }
} catch {
    Write-Host "Error: $($_.Exception.Message)" -ForegroundColor Red
    Pop-Location
    Read-Host "Press any key to close..."
    exit 1
}

# Go back to main project directory
Pop-Location

# Update the main project's submodule reference
Write-Host "Updating main project submodule reference..." -ForegroundColor Cyan
git add MagLoop_Common_Files

$mainCommitMessage = "Update MagLoop_Common_Files submodule to latest version"
git commit -m $mainCommitMessage

Write-Host "Pushing main project changes..." -ForegroundColor Cyan
git push origin master

if ($LASTEXITCODE -eq 0) {
    Write-Host "✅ All changes successfully pushed to both repositories!" -ForegroundColor Green
    Write-Host "Your changes are now visible on GitHub" -ForegroundColor Cyan
} else {
    Write-Host "❌ Failed to push main project changes" -ForegroundColor Red
}

Write-Host ""
Write-Host "Done! Press any key to close..." -ForegroundColor Gray
$null = $Host.UI.RawUI.ReadKey("NoEcho,IncludeKeyDown")