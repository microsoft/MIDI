# sfpcopy.ps1 - System File Protection Copy replacement
# This script copies a file to a system-protected location by taking ownership

param(
    [Parameter(Mandatory=$true)]
    [string]$Source,
    
    [Parameter(Mandatory=$true)]
    [string]$Destination
)

# Check if running as administrator
if (-not ([Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) {
    Write-Error "This script must be run as Administrator"
    exit 1
}

# Check if source file exists
if (-not (Test-Path $Source)) {
    Write-Error "Source file not found: $Source"
    exit 1
}

Write-Host "Copying $Source to $Destination..."

# If destination exists, take ownership and modify permissions
if (Test-Path $Destination) {
    Write-Host "Taking ownership of $Destination..."
    takeown /F "$Destination" | Out-Null
    icacls "$Destination" /grant administrators:F | Out-Null
}

# Copy the file
try {
    Copy-Item -Path $Source -Destination $Destination -Force
    Write-Host "File copied successfully!"
} catch {
    Write-Error "Failed to copy file: $_"
    exit 1
}
