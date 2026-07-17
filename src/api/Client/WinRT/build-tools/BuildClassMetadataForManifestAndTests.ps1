# This takes the winmd file from the midi\src\api\vsfiles-sdk\out\Windows.Devices.Midi2\x64\Release\Windows.Devices.Midi2.winmd
# and pulls class information out of it to build the manifest section for Windows and also the 
# RegistrationValidation unit test data

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

function Get-WinmdTypeFullNames {
	[CmdletBinding()]
	param(
		[Parameter(Mandatory = $true)]
		[ValidateNotNullOrEmpty()]
		[string]$WinmdPath
	)

	if (-not (Test-Path -LiteralPath $WinmdPath -PathType Leaf)) {
		throw "WinMD file not found: $WinmdPath"
	}

	$stream = $null
	$peReader = $null

	try {
		$stream = [System.IO.File]::OpenRead($WinmdPath)
		$peReader = [System.Reflection.PortableExecutable.PEReader]::new($stream)
		$metadataReader = [System.Reflection.Metadata.PEReaderExtensions]::GetMetadataReader($peReader)

		$typeByHandle = @{}
		foreach ($typeHandle in $metadataReader.TypeDefinitions) {
			$typeDefinition = $metadataReader.GetTypeDefinition($typeHandle)
			$typeByHandle[$typeHandle] = $typeDefinition
		}

		$fullNames = New-Object System.Collections.Generic.List[string]
		foreach ($typeHandle in $metadataReader.TypeDefinitions) {
			$typeDefinition = $typeByHandle[$typeHandle]
			$typeName = $metadataReader.GetString($typeDefinition.Name)

			if ([string]::IsNullOrEmpty($typeName) -or $typeName -eq "<Module>") {
				continue
			}

			$nameParts = New-Object System.Collections.Generic.List[string]
			$nameParts.Add($typeName)

			$currentDeclaringType = $typeDefinition.GetDeclaringType()
			while (-not $currentDeclaringType.IsNil) {
				$parentDefinition = $typeByHandle[$currentDeclaringType]
				$parentName = $metadataReader.GetString($parentDefinition.Name)

				if (-not [string]::IsNullOrEmpty($parentName)) {
					$nameParts.Insert(0, $parentName)
				}

				$currentDeclaringType = $parentDefinition.GetDeclaringType()
			}

			$combinedTypeName = ($nameParts -join "+")
			$namespaceName = $metadataReader.GetString($typeDefinition.Namespace)

			if ([string]::IsNullOrEmpty($namespaceName)) {
				$fullName = $combinedTypeName
			}
			else {
				$fullName = "$namespaceName.$combinedTypeName"
			}

			[void]$fullNames.Add($fullName)
		}

		return $fullNames | Sort-Object
	}
	finally {
		if ($peReader -ne $null) {
			$peReader.Dispose()
		}

		if ($stream -ne $null) {
			$stream.Dispose()
		}
	}
}

$scriptRoot = if ($PSScriptRoot) { $PSScriptRoot } else { Split-Path -Parent $MyInvocation.MyCommand.Path }
$winmdPath = [System.IO.Path]::GetFullPath((Join-Path $scriptRoot "..\..\..\vsfiles-sdk\out\Windows.Devices.Midi2\x64\Release\Windows.Devices.Midi2.winmd"))

$typeFullNames = Get-WinmdTypeFullNames -WinmdPath $winmdPath

Write-Host "Found $($typeFullNames.Count) type definitions in: $winmdPath"
# $typeFullNames now contains the full names of all types from the WinMD.

#Write-Host $($typeFullNames)

foreach ($typeFullName in $typeFullNames) {
    Write-Host $typeFullName
}



