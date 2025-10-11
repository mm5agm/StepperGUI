# PowerShell script to update MagLoop_Common_Files submodule
# Usage: .\update-submodules.ps1

param(
    [switch]$Force,
    [switch]$Quiet
)

function Write-ColorOutput {
    param([string]$Message, [string]$Color = "White")
    if (-not $Quiet) {
        Write-Host $Message -ForegroundColor $Color
    }
}

try {
    Write-ColorOutput "=== MagLoop Common Files Submodule Updater ===" "Cyan"
    Write-ColorOutput "Updating MagLoop_Common_Files submodule..." "Green"
    
    # Check if we're in a git repository
    if (-not (Test-Path ".git")) {
        Write-ColorOutput "Error: Not in a Git repository!" "Red"
        exit 1
    }
    
    # Update submodule to latest
    $updateResult = git submodule update --remote --merge 2>&1
    if ($LASTEXITCODE -ne 0) {
        Write-ColorOutput "Error updating submodule: $updateResult" "Red"
        exit 1
    }
    
    # Check if there are changes
    $status = git status --porcelain MagLoop_Common_Files
    if ($status -or $Force) {
        Write-ColorOutput "Submodule updated! Committing changes..." "Yellow"
        
        git add MagLoop_Common_Files
        if ($LASTEXITCODE -ne 0) {
            Write-ColorOutput "Error adding submodule changes!" "Red"
            exit 1
        }
        
        $commitMsg = "Update MagLoop_Common_Files submodule to latest version - $(Get-Date -Format 'yyyy-MM-dd HH:mm')"
        git commit -m $commitMsg
        if ($LASTEXITCODE -ne 0) {
            Write-ColorOutput "Error committing changes!" "Red"
            exit 1
        }
        
        git push
        if ($LASTEXITCODE -ne 0) {
            Write-ColorOutput "Error pushing changes!" "Red"
            exit 1
        }
        
        Write-ColorOutput "✓ Submodule update committed and pushed successfully!" "Green"
    } else {
        Write-ColorOutput "✓ Submodule already up to date." "Blue"
    }
    
    Write-ColorOutput "=== Update Complete ===" "Cyan"
    
} catch {
    Write-ColorOutput "Unexpected error: $($_.Exception.Message)" "Red"
    exit 1
}

if (-not $Quiet) {
    Write-Host "Press any key to continue..." -ForegroundColor Gray
    $null = $Host.UI.RawUI.ReadKey("NoEcho,IncludeKeyDown")
}