# General Developer on-boarding

NOTE: In the future, we may replace this with on-boarding scripts. For now, please use the information in this document. Find something missing or incorrect? Please open an issue.

Information below is for working directly on the projects in this repo, not for creating your own applications using these components.

A single developer workstation can be set up with the superset of everything required here, if desired.

## Basics

### Visual Studio

Install the latest version of Visual Studio. Any edition will work (Enterprise, Community, etc.). Many use the Community edition. Preview versions are acceptable and sometimes preferable.

### Windows SDK

These projects currently support the latest supported version of Windows 10 as well as all supported versions of Windows 11.

Install (through Visual Studio) the **latest Windows 10 SDK** and the **Windows 11 SDK(s) required by the projects**. If in doubt, install the latest 2 or 3 Windows 11 SDKs in addition to that Windows 10 SDK. If you do not have the correct SDK version, the projects will not build.

Note: Do not change the minimum or target Windows SDK version in a project you intend to PR.

#### How to verify the SDK version used by a project

Open the project file and look for min and target SDK versions. Versions below are for illustration purposes only.

C++ Project (.vcxproj)

```xml
<WindowsTargetPlatformVersion Condition=" '$(WindowsTargetPlatformVersion)' == '' ">10.0.22621.0</WindowsTargetPlatformVersion>
<WindowsTargetPlatformMinVersion>10.0.17134.0</WindowsTargetPlatformMinVersion>
```

C# Project File (.csproj)

```xml
<TargetFramework>net8.0-windows10.0.20348.0</TargetFramework>
<TargetPlatformMinVersion>10.0.20348.0</TargetPlatformMinVersion>
<SupportedOSPlatformVersion>10.0.20348.0</SupportedOSPlatformVersion>
```

You'll want to install the SDK versions from the minimum through to the target.

## Service/API/SDK Development

The Service, API, and SDK are all written using C++. The API and SDK also use C++/WinRT.

### Visual Studio Workloads for C++

* Desktop development with C++

Additional Installs

* [C++ / WinRT (Restore through NuGet)](https://github.com/microsoft/CsWinRT)

### Additional Help

* [C++/WinRT Docs](https://learn.microsoft.com/windows/uwp/cpp-and-winrt-apis/)

## Tools

The tools are developed in C# and use WinUI for the user interface. They use C#/WinRT for consuming the WinRT components.

### Visual Studio Workloads for CSharp

* .NET desktop Development Workload

Additional installs

* [.NET 8.0 Preview SDK](https://dotnet.microsoft.com/download/visual-studio-sdks)

These packages can be restored from within Visual Studio using the NuGet package manager

* [C# / WinRT](https://github.com/microsoft/CsWinRT)
* [WinUI 3](https://github.com/microsoft/microsoft-ui-xaml)
* [Community Toolkit](https://github.com/CommunityToolkit)

(installer extension?)

## USB MIDI Driver

The USB MIDI Driver is a kernel-mode driver written in C++ using Visual Studio.

### Visual Studio Workloads for C++ Driver Development

Additional Installs

* [Windows Driver Development Kit](https://learn.microsoft.com/windows-hardware/drivers/download-the-wdk)

## Docs

We use Visual Studio Code with the Learn Authoring Pack extension. That includes learn-article-templates, learn-images, learn-linting, learn-markdown, learn-metadata, learn-preview, learn-scaffolding, learn-validation, and learn-yaml.

* [Marketplace Link for the Learn Authoring Pack](https://marketplace.visualstudio.com/items?itemName=docsmsft.docs-authoring-pack)
