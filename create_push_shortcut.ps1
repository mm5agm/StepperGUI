# PowerShell script to create a Windows shortcut for pushing submodule changes

$WshShell = New-Object -comObject WScript.Shell
$Shortcut = $WshShell.CreateShortcut("$PWD\Push Submodule Changes.lnk")
$Shortcut.TargetPath = "cmd.exe"
$Shortcut.Arguments = "/c `"cd /d `"$PWD`" && push_submodule_changes.bat`""
$Shortcut.WorkingDirectory = $PWD
$Shortcut.IconLocation = "cmd.exe,0"
$Shortcut.Description = "Push MagLoop_Common_Files changes to GitHub"
$Shortcut.Save()

Write-Host "Created shortcut: Push Submodule Changes.lnk" -ForegroundColor Green
Write-Host "Double-click the shortcut to push your submodule changes to GitHub." -ForegroundColor Cyan