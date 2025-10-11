# PowerShell script to create a Windows shortcut that will definitely run the batch file

$WshShell = New-Object -comObject WScript.Shell
$Shortcut = $WshShell.CreateShortcut("$PWD\Update Submodule.lnk")
$Shortcut.TargetPath = "cmd.exe"
$Shortcut.Arguments = "/c `"cd /d `"$PWD`" && update_submodule.bat`""
$Shortcut.WorkingDirectory = $PWD
$Shortcut.IconLocation = "cmd.exe,0"
$Shortcut.Description = "Update MagLoop_Common_Files submodule"
$Shortcut.Save()

Write-Host "Created shortcut: Update Submodule.lnk" -ForegroundColor Green
Write-Host "Double-click the shortcut to run the submodule update script." -ForegroundColor Cyan