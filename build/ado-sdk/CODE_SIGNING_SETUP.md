# Code Signing Setup for Azure DevOps Pipeline

This document provides instructions for enabling code signing in the Azure DevOps pipeline for Windows MIDI Services SDK.

## Overview

The pipeline includes commented-out signing tasks for all executable files, DLLs, PowerShell modules, and installers. This document explains how to configure and enable these signing tasks.

## Prerequisites

1. **Code Signing Certificate**: You need a valid code signing certificate from a trusted Certificate Authority (CA)
2. **Azure Key Vault**: Store your certificate in Azure Key Vault for secure access
3. **Azure DevOps Service Connection**: Configure a service connection to access your Key Vault
4. **Azure Sign Tool Extension**: Install in your Azure DevOps organization (or use alternative signing tools)

## Setup Steps

### 1. Store Certificate in Azure Key Vault

```powershell
# Upload your certificate to Azure Key Vault
az keyvault certificate import \
  --vault-name YourKeyVaultName \
  --name YourCertificateName \
  --file /path/to/certificate.pfx \
  --password YourCertificatePassword
```

### 2. Create Azure Service Connection

1. In Azure DevOps, go to **Project Settings** → **Service connections**
2. Click **New service connection**
3. Select **Azure Resource Manager**
4. Choose **Service principal (automatic)** or **Service principal (manual)**
5. Select your subscription and authorize access to your Key Vault
6. Name the connection (e.g., `AzureKeyVaultConnection`)

### 3. Grant Key Vault Permissions

Ensure the service principal has the following permissions on your Key Vault:
- **Certificate permissions**: Get
- **Key permissions**: Sign
- **Secret permissions**: Get (if needed)

```powershell
# Grant permissions to the service principal
az keyvault set-policy \
  --name YourKeyVaultName \
  --spn <service-principal-id> \
  --certificate-permissions get \
  --key-permissions sign \
  --secret-permissions get
```

### 4. Configure Pipeline Variables

Add the following variables to your Azure DevOps pipeline (either in the YAML or as Pipeline Variables):

| Variable Name | Description | Example Value |
|---------------|-------------|---------------|
| `SigningCertificateName` | Name of the certificate in Key Vault | `WindowsMIDICodeSigningCert` |
| `SigningTimeStampServer` | RFC 3161 timestamp server URL | `http://timestamp.digicert.com` |
| `SigningDescription` | Description embedded in signed files | `Windows MIDI Services` |

**To add variables:**
1. Go to your pipeline in Azure DevOps
2. Click **Edit** → **Variables**
3. Click **New variable**
4. Add each variable and mark sensitive ones as secret

### 5. Install Azure Sign Tool Extension

**Option A: Install from Azure DevOps Marketplace**
1. Go to https://marketplace.visualstudio.com/
2. Search for "Azure Sign Tool" or similar code signing extensions
3. Install to your organization

**Option B: Use Command-Line Azure Sign Tool**
The pipeline includes tasks that can use the `azuresigntool` CLI directly:

```powershell
dotnet tool install --global AzureSignTool
```

### 6. Enable Signing Tasks in Pipeline

Search for all `# TODO: Configure code signing` comments in `azure-pipelines.yml` and uncomment the signing tasks. Update the following placeholders:

- `YourAzureServiceConnection` → Replace with your actual service connection name
- `https://yourkeyvault.vault.azure.net/` → Replace with your Key Vault URL
- `YourCertSubject` (for PowerShell signing) → Replace with your certificate subject

## Files That Will Be Signed

The pipeline includes signing tasks for:

### 1. SDK Binaries
- `Microsoft.Windows.Devices.Midi2.dll` (x64, Arm64EC)

### 2. SDK Tool Executables
- `mididiag.exe`
- `midiksinfo.exe`
- `midimdnsinfo.exe`
- `midi1monitor.exe`
- `midi1enum.exe`
- `midifixreg.exe`
- `midicheckservice.exe`

### 3. User Applications
- `midi.exe` (Console application)
- `MidiSettings.exe` (Settings application)

### 4. PowerShell Module
- `WindowsMidiServices.dll`
- `WindowsMidiServices.psd1` (manifest)
- All `.ps1`, `.psd1`, `.psm1` script files

### 5. Installers
- `WindowsMidiServicesSdkRuntimeSetup.exe` (x64, Arm64)

### 6. NuGet Package (Optional)
- `Microsoft.Windows.Devices.Midi2.{version}.nupkg`

## Signing Task Examples

### Example 1: Azure Sign Tool Task

```yaml
- task: AzureSignTool@3
  displayName: 'Sign Executable'
  inputs:
    ConnectedServiceName: 'AzureKeyVaultConnection'
    KeyVaultUrl: 'https://yourkeyvault.vault.azure.net/'
    CertificateName: '$(SigningCertificateName)'
    SignedFilePath: |
      $(Build.SourcesDirectory)\path\to\file.exe
    TimeStampServer: '$(SigningTimeStampServer)'
    FileDigest: 'SHA256'
    TimeStampDigest: 'SHA256'
    Description: '$(SigningDescription)'
```

### Example 2: PowerShell Script Signing

```yaml
- task: PowerShell@2
  displayName: 'Sign PowerShell Scripts'
  inputs:
    targetType: 'inline'
    script: |
      # Load certificate from Key Vault or local store
      $cert = Get-ChildItem -Path Cert:\CurrentUser\My -CodeSigningCert | 
        Where-Object { $_.Subject -like "*$(SigningCertificateName)*" }

      # Sign all PowerShell files
      $filesToSign = Get-ChildItem "$(Build.SourcesDirectory)\path" `
        -Include "*.ps1", "*.psd1", "*.psm1" -Recurse

      foreach ($file in $filesToSign) {
        Set-AuthenticodeSignature `
          -FilePath $file.FullName `
          -Certificate $cert `
          -TimestampServer "$(SigningTimeStampServer)"
        Write-Host "Signed: $($file.Name)"
      }
```

### Example 3: NuGet Package Signing

```yaml
- task: NuGetCommand@2
  displayName: 'Sign NuGet Package'
  inputs:
    command: 'custom'
    arguments: >
      sign $(Build.SourcesDirectory)\path\to\package.nupkg
      -CertificateFingerprint $(SigningCertificateFingerprint)
      -Timestamper $(SigningTimeStampServer)
      -NonInteractive
```

## Alternative Signing Methods

### Using SignTool.exe (Windows SDK)

If you prefer to use `signtool.exe` from the Windows SDK:

```yaml
- task: PowerShell@2
  displayName: 'Sign with SignTool'
  inputs:
    targetType: 'inline'
    script: |
      $signtool = "C:\Program Files (x86)\Windows Kits\10\bin\10.0.22621.0\x64\signtool.exe"

      & $signtool sign `
        /fd SHA256 `
        /tr "$(SigningTimeStampServer)" `
        /td SHA256 `
        /d "$(SigningDescription)" `
        /du "https://github.com/microsoft/MIDI" `
        /sha1 "$(SigningCertificateThumbprint)" `
        "$(Build.SourcesDirectory)\path\to\file.exe"
```

### Using Azure Sign Tool CLI

```yaml
- task: PowerShell@2
  displayName: 'Sign with Azure Sign Tool CLI'
  inputs:
    targetType: 'inline'
    script: |
      # Install if needed
      dotnet tool install --global AzureSignTool --version 4.0.1

      azuresigntool sign `
        --file-digest sha256 `
        --timestamp-rfc3161 "$(SigningTimeStampServer)" `
        --azure-key-vault-url "https://yourkeyvault.vault.azure.net/" `
        --azure-key-vault-certificate "$(SigningCertificateName)" `
        --azure-key-vault-client-id "$(AzureClientId)" `
        --azure-key-vault-client-secret "$(AzureClientSecret)" `
        --azure-key-vault-tenant-id "$(AzureTenantId)" `
        --verbose `
        "$(Build.SourcesDirectory)\path\to\file.exe"
```

## Verification

After enabling signing, verify that files are properly signed:

```powershell
# Verify authenticode signature
Get-AuthenticodeSignature -FilePath "path\to\signed\file.exe" | Format-List

# Check signature details
signtool verify /pa /v "path\to\signed\file.exe"
```

## Troubleshooting

### Common Issues

1. **Certificate not found**
   - Verify the certificate name matches exactly in Key Vault
   - Check service principal permissions

2. **Timestamp server timeout**
   - Try alternative timestamp servers:
     - `http://timestamp.digicert.com`
     - `http://timestamp.globalsign.com/tsa/r6advanced1`
     - `http://tsa.starfieldtech.com`

3. **Access denied to Key Vault**
   - Verify service connection is configured correctly
   - Check service principal has required permissions
   - Ensure Key Vault firewall allows Azure DevOps IPs

4. **PowerShell script signing fails**
   - Ensure certificate is imported to the agent's certificate store
   - Verify certificate has the "Code Signing" enhanced key usage

## Security Best Practices

1. **Store sensitive values as secret variables** in Azure DevOps
2. **Use managed identities** when possible instead of service principals
3. **Rotate certificates** before expiration
4. **Audit access** to your Key Vault regularly
5. **Use separate certificates** for different environments (dev, staging, production)
6. **Enable Key Vault logging** to track certificate usage

## Additional Resources

- [Azure Key Vault Documentation](https://docs.microsoft.com/azure/key-vault/)
- [Azure Sign Tool GitHub](https://github.com/vcsjones/AzureSignTool)
- [Code Signing Best Practices](https://docs.microsoft.com/windows/win32/seccrypto/cryptography-tools)
- [PowerShell Script Signing](https://docs.microsoft.com/powershell/module/microsoft.powershell.security/set-authenticodesignature)
- [NuGet Package Signing](https://docs.microsoft.com/nuget/create-packages/sign-a-package)

## Support

For issues related to:
- **Azure DevOps**: Contact your DevOps administrator
- **Key Vault**: Check Azure support documentation
- **Certificate issues**: Contact your Certificate Authority
- **Pipeline configuration**: Review this document and Azure Pipelines documentation
