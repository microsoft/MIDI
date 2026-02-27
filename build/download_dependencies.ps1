# Download required dependencies for WiX installer builds

# Create directories if they don't exist
New-Item -ItemType Directory -Force -Path ".\dependencies\x64"
New-Item -ItemType Directory -Force -Path ".\dependencies\arm64"

# Download VC Redist
Write-Host "Downloading VC Redist..."
curl -L "https://aka.ms/vs/17/release/vc_redist.x64.exe" -o ".\dependencies\vc_redist.x64.exe"

# Download Windows App Runtime
# Note: This URL may need to be updated. Check https://learn.microsoft.com/en-us/windows/apps/windows-app-sdk/downloads
Write-Host "Downloading Windows App Runtime for x64..."
curl -L "https://aka.ms/windowsappsdk/1.6/1.6.250205002/windowsappruntimeinstall-x64.exe" -o ".\dependencies\x64\WindowsAppRuntimeInstall-x64.exe"

Write-Host "Downloading Windows App Runtime for arm64..."
curl -L "https://aka.ms/windowsappsdk/1.6/1.6.250205002/windowsappruntimeinstall-arm64.exe" -o ".\dependencies\arm64\WindowsAppRuntimeInstall-arm64.exe"

# Download .NET 10 Desktop Runtime
Write-Host "Downloading .NET 10 Desktop Runtime for x64..."
curl -L "https://download.visualstudio.microsoft.com/download/pr/7a1e5e0b-8c64-4f7c-9d7e-5e3f5e6e6e6e/windowsdesktop-runtime-10.0.3-win-x64.exe" -o ".\dependencies\x64\windowsdesktop-runtime-10.0.3-win-x64.exe"

Write-Host "Downloading .NET 10 Desktop Runtime for arm64..."
curl -L "https://download.visualstudio.microsoft.com/download/pr/7a1e5e0b-8c64-4f7c-9d7e-5e3f5e6e6e6e/windowsdesktop-runtime-10.0.3-win-arm64.exe" -o ".\dependencies\arm64\windowsdesktop-runtime-10.0.3-win-arm64.exe"

Write-Host "All dependencies downloaded successfully!"
