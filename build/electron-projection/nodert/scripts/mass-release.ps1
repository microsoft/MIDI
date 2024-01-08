Function Get-Folder($initialDirectory) {
  [System.Reflection.Assembly]::LoadWithPartialName("System.windows.forms") | Out-Null

  $foldername = New-Object System.Windows.Forms.FolderBrowserDialog
  $foldername.Description = "Select a folder to relase from"
  $foldername.rootfolder = "MyComputer"

  if ($foldername.ShowDialog() -eq "OK") {
    $folder += $foldername.SelectedPath
  }
  return $folder
}

$dir = Get-Folder

if ($dir -eq $null) {
  return
}

$dirs = dir $dir

foreach ($d in $dirs) {
  Write-Host "Publishing $d"

  cd $d.FullName
  npm publish . --access public
  cd $dir
}
