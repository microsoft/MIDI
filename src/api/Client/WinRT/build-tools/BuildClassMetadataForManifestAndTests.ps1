# This takes the winmd file from the midi\src\api\vsfiles-sdk\out\Windows.Devices.Midi2\x64\Release\Windows.Devices.Midi2.winmd
# and pulls class information out of it to build the manifest section for Windows and also the 
# RegistrationValidation unit test data

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

function Get-WinmdTypeMetadata {
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

		$typeMetadata = New-Object System.Collections.Generic.List[object]
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

			$isActivatable = $false
			$isStatic = $false
			$isComposable = $false
			$clsid = [Guid]::Empty
			$threading = "Both"

			foreach ($customAttributeHandle in $typeDefinition.GetCustomAttributes()) {
				$customAttribute = $metadataReader.GetCustomAttribute($customAttributeHandle)
				$attributeTypeFullName = $null

				if ($customAttribute.Constructor.Kind -eq [System.Reflection.Metadata.HandleKind]::MemberReference) {
					$memberRefHandle = [System.Reflection.Metadata.MemberReferenceHandle]$customAttribute.Constructor
					$memberRef = $metadataReader.GetMemberReference($memberRefHandle)
					$parentHandle = $memberRef.Parent

					if ($parentHandle.Kind -eq [System.Reflection.Metadata.HandleKind]::TypeReference) {
						$typeRef = $metadataReader.GetTypeReference([System.Reflection.Metadata.TypeReferenceHandle]$parentHandle)
						$attributeTypeFullName = "{0}.{1}" -f $metadataReader.GetString($typeRef.Namespace), $metadataReader.GetString($typeRef.Name)
					}
					elseif ($parentHandle.Kind -eq [System.Reflection.Metadata.HandleKind]::TypeDefinition) {
						$attributeTypeDef = $metadataReader.GetTypeDefinition([System.Reflection.Metadata.TypeDefinitionHandle]$parentHandle)
						$attributeTypeFullName = "{0}.{1}" -f $metadataReader.GetString($attributeTypeDef.Namespace), $metadataReader.GetString($attributeTypeDef.Name)
					}
				}
				elseif ($customAttribute.Constructor.Kind -eq [System.Reflection.Metadata.HandleKind]::MethodDefinition) {
					$methodDef = $metadataReader.GetMethodDefinition([System.Reflection.Metadata.MethodDefinitionHandle]$customAttribute.Constructor)
					$declaringType = $metadataReader.GetTypeDefinition($methodDef.GetDeclaringType())
					$attributeTypeFullName = "{0}.{1}" -f $metadataReader.GetString($declaringType.Namespace), $metadataReader.GetString($declaringType.Name)
				}

				if ($attributeTypeFullName -eq "Windows.Foundation.Metadata.ActivatableAttribute") {
					$isActivatable = $true
				}

				if ($attributeTypeFullName -eq "Windows.Foundation.Metadata.StaticAttribute") {
					$isStatic = $true
				}

				if ($attributeTypeFullName -eq "Windows.Foundation.Metadata.ComposableAttribute") {
					$isComposable = $true
				}

				if ($attributeTypeFullName -eq "Windows.Foundation.Metadata.GuidAttribute") {
					$blob = $metadataReader.GetBlobBytes($customAttribute.Value)

					if ($blob.Length -ge 20 -and $blob[0] -eq 1 -and $blob[1] -eq 0) {
						$a = [System.BitConverter]::ToUInt32($blob, 2)
						$b = [System.BitConverter]::ToUInt16($blob, 6)
						$c = [System.BitConverter]::ToUInt16($blob, 8)
						$d0 = $blob[10]
						$d1 = $blob[11]
						$d2 = $blob[12]
						$d3 = $blob[13]
						$d4 = $blob[14]
						$d5 = $blob[15]
						$d6 = $blob[16]
						$d7 = $blob[17]

						$guidText = "{0:x8}-{1:x4}-{2:x4}-{3:x2}{4:x2}-{5:x2}{6:x2}{7:x2}{8:x2}{9:x2}{10:x2}" -f $a, $b, $c, $d0, $d1, $d2, $d3, $d4, $d5, $d6, $d7
						$clsid = [Guid]::Parse($guidText)
					}
				}

				if ($attributeTypeFullName -eq "Windows.Foundation.Metadata.ThreadingAttribute") {
					$blob = $metadataReader.GetBlobBytes($customAttribute.Value)

					if ($blob.Length -ge 6 -and $blob[0] -eq 1 -and $blob[1] -eq 0) {
						$threadingValue = [System.BitConverter]::ToInt32($blob, 2)

						switch ($threadingValue) {
							1 { $threading = "STA"; break }
							2 { $threading = "MTA"; break }
							3 { $threading = "Both"; break }
							default { }
						}
					}
				}
			}

			[void]$typeMetadata.Add([pscustomobject]@{
				FullName = [string]$fullName
				IsActivatable = [bool]$isActivatable
				IsStatic = [bool]$isStatic
				IsComposable = [bool]$isComposable
				Threading = [string]$threading
				CLSID = [Guid]$clsid
			})
		}

		return $typeMetadata | Sort-Object -Property FullName
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

$typeMetadata = Get-WinmdTypeMetadata -WinmdPath $winmdPath

Write-Host "Found $($typeMetadata.Count) type definitions in: $winmdPath"
Write-Host ""

$runtimeClasses = $typeMetadata |
	Where-Object { $_.IsActivatable -or $_.IsStatic -or $_.IsComposable } |
	Sort-Object -Property FullName

Write-Host "Found $($runtimeClasses.Count) runtime classes (activatable/static/composable)."
Write-Host ""

foreach ($type in $runtimeClasses) {
	Write-Output "        <!-- $($type.FullName) -->"
	Write-Output "        <class"
	Write-Output ('          activatableClassId="{0}"' -f $type.FullName)
	Write-Output ('          threading="{0}"' -f $type.Threading)
	Write-Output '          trustLevel="Base"'
	Write-Output "          />"
	Write-Output ""
}



