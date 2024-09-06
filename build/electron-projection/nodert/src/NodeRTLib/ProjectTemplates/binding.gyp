{
  "variables": {
    "WIN_VER": "{WinVer}",
    "USE_ADDITIONAL_WINMD": "{UseAdditionalWinmd}",
  },
  "includes": ["common.gypi"],
  "targets": [{
    "target_name": "binding",
    "cflags_cc":[ "-std=c++17" ],
    "include_dirs": [
      "<!(node -e \"require('nan')\")"
    ],
    "libraries": [],
    "conditions": [
      ["OS=='win'", {
        "libraries": ["-lruntimeobject.lib"],
        "sources": [
          "_nodert_generated.cpp",
          "NodeRtUtils.cpp",
          "OpaqueWrapper.cpp",
          "CollectionsConverterUtils.cpp"
        ]
      }],
      ["WIN_VER==\"v8.0\"", {
          "msvs_settings": {
            "VCCLCompilerTool": {
              "AdditionalUsingDirectories": [
                "%ProgramFiles(x86)%/Microsoft SDKs/Windows/v8.0/ExtensionSDKs/Microsoft.VCLibs/11.0/References/CommonConfiguration/neutral",
                "%ProgramFiles(x86)%/Windows Kits/8.0/References/CommonConfiguration/Neutral",
                "%ProgramFiles%/Microsoft SDKs/Windows/v8.0/ExtensionSDKs/Microsoft.VCLibs/11.0/References/CommonConfiguration/neutral",
                "%ProgramFiles%/Windows Kits/8.0/References/CommonConfiguration/Neutral"
              ]
            }
          }
        }
      ],
      ["WIN_VER==\"v8.1\"", {
        "msvs_settings": {
          "VCCLCompilerTool": {
            "AdditionalUsingDirectories": [
              "%ProgramFiles(x86)%/Microsoft SDKs/Windows/v8.1/ExtensionSDKs/Microsoft.VCLibs/12.0/References/CommonConfiguration/neutral",
              "%ProgramFiles(x86)%/Windows Kits/8.1/References/CommonConfiguration/Neutral",
              "%ProgramFiles%/Microsoft SDKs/Windows/v8.1/ExtensionSDKs/Microsoft.VCLibs/12.0/References/CommonConfiguration/neutral",
              "%ProgramFiles%/Windows Kits/8.1/References/CommonConfiguration/Neutral"
            ]
          }
        }
      }],
      ["WIN_VER==\"v10\"", {
        "msvs_settings": {
          "VCCLCompilerTool": {
            "AdditionalUsingDirectories": [
              "%ProgramFiles(x86)%/Microsoft Visual Studio 14.0/VC/lib/store/references",
              "%ProgramFiles%/Microsoft Visual Studio 14.0/VC/lib/store/references",
              "%ProgramFiles(x86)%/Windows Kits/10/UnionMetadata/10.0.20348.0",
              "$(VCToolsInstallDir)/lib/x86/store/references",
              "%midi_repo_root%/build/staging/app-sdk/x64",
              "%midi_repo_root%/build/staging/app-sdk/Arm64",
              "%midi_repo_root%/build/staging/app-sdk/Arm64EC"
            ]
          }
        }
      }],
      ["USE_ADDITIONAL_WINMD==\"true\"", {
        "msvs_settings": {
          "VCCLCompilerTool": {
            "AdditionalUsingDirectories": [
              {AdditionalWinmdPaths}
            ]
          }
        }
      }]
    ],
    "msvs_settings": {
      "VCCLCompilerTool": {
        "AdditionalOptions": ["/ZW", "-std:c++17"],
        "DisableSpecificWarnings": [4609],
      }
    },
    "msbuild_settings":{
        "ClCompile":{
            "LanguageStandard": "stdcpp17",
            "AdditionalOptions": "/ZW",
            "AdditionalUsingDirectories": [
              "%ProgramFiles(x86)%/Microsoft Visual Studio 14.0/VC/lib/store/references",
              "%ProgramFiles%/Microsoft Visual Studio 14.0/VC/lib/store/references",
              "%ProgramFiles(x86)%/Windows Kits/10/UnionMetadata/10.0.20348.0",
              "$(VCToolsInstallDir)/lib/x86/store/references",
              "../../",
            ]

        }
    }
  }]
}