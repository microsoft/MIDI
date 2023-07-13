# Notes on WinRT

The Windows MIDI Services SDK is built using C++/WinRT. WinRT, a requirement for modern APIs on Windows, enables desktop applications, regardless of language, to be able to use APIs, SDKs, etc. that we create. The older tools, C++/CX, are arguably simpler to implement in, but because they include proprietary extensions to C++, we decided to go with standards-based C++/WinRT instead.

When running inside Visual Studio there's a lot that is forgiven by the tools.

* You can have C++/WinRT to C++/WinRT project references
* Type activation works without a manifest file entry even when not a packaged application

## Type Activation

When run outside the tools, you'll need to be prepared to either use a packaged application (UWP or a desktop MSIX with an identity and a .appxmanifest file) or for those with new or existing non-packaged desktop apps, add an `<appname.exe>.manifest` file which lists the classes you need to activate. **We will provide one here on GitHub and keep it updated with the full list of types for all the SDK assemblies.** But, for example, the `.exe.manifest` file will look something like this:

```xml
<?xml version="1.0" encoding="utf-8"?>
<assembly manifestVersion="1.0" xmlns="urn:schemas-microsoft-com:asm.v1">
    <assemblyIdentity version="1.0.0.0" name="MyArbitraryButUniqueApplicationName.app"/>
    <file name="Windows.Devices.Midi2.dll">		
		<activatableClass name="Windows.Devices.Midi2.MidiSession" 
            threadingModel="both" xmlns="urn:schemas-microsoft-com:winrt.v1" />
		<activatableClass name="Windows.Devices.Midi2.MidiUmp32"
            threadingModel="both" xmlns="urn:schemas-microsoft-com:winrt.v1" />
		<activatableClass name="Windows.Devices.Midi2.MidiUmp64"
            threadingModel="both" xmlns="urn:schemas-microsoft-com:winrt.v1" />
		<activatableClass name="Windows.Devices.Midi2.MidiUmp96"
            threadingModel="both" xmlns="urn:schemas-microsoft-com:winrt.v1" />
		<activatableClass name="Windows.Devices.Midi2.MidiUmp128"
            threadingModel="both" xmlns="urn:schemas-microsoft-com:winrt.v1" />
        ...
	</file>
</assembly>
```

For more information, please refer to [this blog post](https://blogs.windows.com/windowsdeveloper/2019/04/30/enhancing-non-packaged-desktop-apps-using-windows-runtime-components/). Note that some of the details listed in that blog post, like the VC++ dependencies, have since changed. Setting the Desktop-Compatible project setting for C++/WinRT projects handles that. [This documentation article](https://learn.microsoft.com/windows/win32/sbscs/application-manifests#activatableClass) has specific information on activatableClass in the app manifest.

Note that when the API is distributed with Windows, you won't need to declare those classes. But during development, they are no different from the SDK classes, and must be declared.

## References

| Scenario | VS Project Reference | NuGet Projection DLL Reference | winmd Reference | App manifest entries needed |
| -------- | ------------------| -------------------- | --------------- | ------------------- |
| C++/WinRT app in Visual Studio | Yes | No | Yes | No |
| C#/WinRT app in Visual Studio | No | Yes | No | No |
| Unpackaged Desktop C++ / WinRT App | No | No | Yes | Yes |
| Appx/MSIX Packaged Desktop C++ / WinRT App | No | No | Yes | No |
| Appx/MSIX Packaged Desktop .NET / WinRT App | No | Yes | No | No |

## Problem locating header files

**First, REBuild your project (not just Build) after adding or updating the SDK NuGet package.** I'm working on the correct targets file to enable normal incremental build to generate the header file based on the winmd, but it's not functioning at the moment. Of course, I'm assuming you have the C++/WinRT NuGet package already installed in your project.

If your project cannot resolve the header files, you may be running into [this bug](https://github.com/microsoft/cppwinrt/issues/593). That bug should be fixed, but there are instances, it seems, where it still crops up. If that happens, and you've verified that it's not a problem with the WinMD missing from your project (it's more typically an issue with a reference), you can add `$(GeneratedFilesDir)` to the include path for your project.

CPPWinRT.exe creates the winrt/namespace-name.h files in the Generated Files/winrt folder based upon the referenced winmd.

## Problem with E_CLASSNOTREG hresult

Hresult errors including this, and related errors around type initialization, are typically the result of one or both:

* Missing implementation DLL in your exe folder. You need any projection DLL (if used), as well as the platform-specific implementation DLL, in the same folder as your executable file.
* Missing type activation entries in your app's manifest. (see above)

