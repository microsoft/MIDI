---
layout: kb
title: Consuming the MIDI SDK from Various Languages and Tools
audience: developers
description: Developer-focused information on using the Windows MIDI Services SDK from various languages and products.
---

(TODO: This page requires updating)

The Windows MIDI Services SDK is built using C++/WinRT. WinRT, a requirement for modern APIs/SDKs on Windows, enables desktop applications regardless of language to be able to use APIs, SDKs, etc. that we create. The older tools, C++/CX, are arguably simpler to implement in, but because they include proprietary extensions to C++, we decided to go with standards-based C++/WinRT instead.

## Developer Prerequisites

To use the SDK, your application language and tools must be able to work with WinRT metadata and libraries, or the generated projection header file.

* Visual Studio 2022+ if you are using Visual Studio
* Latest Windows 11 SDKs and Windows SDK 10.0.20348 (Install with Visual Studio)
* The latest version of Windows 11.
* C++ 20
* The NuGet package(s) from the release

There are somewhat hacky ways to get traditional C to work with the COM interfaces, but it is a ton of work for you, and is not a scenario we support. If you find yourself in that situation, I recommend factoring out the MIDI code into its own lib and encapsulating all the C++ calls in there.

> NOTE: In the period of time before Windows MIDI Services ships in retail Windows, you will also need to run the latest Windows 11 Insider Canary build of Windows in order to be able to use the USB MIDI 2.0 driver. [Click here to learn more and join the Windows Insider Program](https://www.microsoft.com/windowsinsider/).

> CPU Architecture: There is no planned support for Arm(32) or x86. We only support 64 bit operating systems. The SDK and service also support only 64 bit applications. We have basic 32-bit support for 32-bit apps on Windows 64bit using the older WinMM API.

## Consuming from C++ with Visual Studio

Add the C++/WinRT Nuget package to your C++ project in Visual Studio. This installs the required tools and build process. See the C++/WinRT FAQ link below for using LLVM/Clang. Note that the Windows MIDI Services team does not provide any support for LLVM/Clang, but we will take PRs as required if we need to change something reasonable to ensure you are successful with those tools, within what C++/WinRT can support.

In your project, set your target and minimum SDK versions to 10.0.20348.0

Download the NuGet package for the Core SDK

* Until this is in the in-box SDK, you'll need to set up a local NuGet package repository. This is easy to do inside the NuGet Package Manager in Visual Studio -- you simply point to a folder. The structure I use in the local clone of the repo is a subfolder of the release folder for all NuGet packages. Specifically `G:\GitHub\microsoft\midi\build\release\NuGet\` on my Dev Drive.

If needed, modify the project file as required (info in the C++/WinRT docs, and you can also look at the sample application code). If you are not using Visual Studio as your toolchain for your project, you may want to pull out the MIDI code into a library in your project which does. It's not strictly required, but it's much easier to use C++/WinRT. (If you do not want to do this, you'll need to manually set up the cppwinrt tools as part of your build process to generate the required `Microsoft.Windows.Devices.Midi2.h` projection header. After that, you can develop using your normal flow.).

[Read through this page](https://learn.microsoft.com/windows/uwp/cpp-and-winrt-apis/consume-apis), specifically the "If the API is implemented in a Windows Runtime component".

After that, you reference the types as you would anything else in C++. Only the toolchain is an extra step. What it produces is standard C++. We're considering what we can do here to possibly eliminate even that step in the future, but it's required for now.

> When in doubt, REbuild your project. C++/WinRT does a lot of code generation for the projections.

* [C++ Windows MIDI Services Example Code](https://github.com/microsoft/MIDI/tree/main/samples/cpp-winrt)
* [Introduction to C++/WinRT](https://learn.microsoft.com/windows/uwp/cpp-and-winrt-apis/)
* [C++/WinRT on GitHub](https://github.com/microsoft/cppwinrt)
* [C++/WinRT FAQ](https://learn.microsoft.com/windows/uwp/cpp-and-winrt-apis/faq)
* [C++/WinRT Troubleshooting](https://learn.microsoft.com/windows/uwp/cpp-and-winrt-apis/troubleshooting)

## Consuming from C# Desktop App

Your project will currently need to target .NET 7 or above. We prefer .NET 8.

Releases will eventually be in the official NuGet.org package source. For now, you can create a local package source and place the NuGet package in there. Then add it to your package sources in the NuGet Package Manager in Visual Studio.

The package contains the .NET (C#) projection for .NET 7 and .NET 8. You will still need to install the C#/WinRT NuGet package in your project because we use other Windows SDK types from Windows.Foundation and more.

**Note that other .NET languages (like Visual Basic) may work, but have not been tested.**

* [C# Windows MIDI Services Example Code](https://github.com/microsoft/MIDI/tree/main/samples/csharp-net)
* [C#/WinRT on GitHub](https://github.com/microsoft/cswinrt)

## Consuming from C# UWP

Support for this is not yet in place. We are evaluating the need for UWP support. Our top priority is desktop application support.

## Consuming from Rust / RS WinRT

You will follow a similar approach to C++ using windows-rs instead of C++/WinRT. Note that the Rust WinRT tools are newer and are still in active development. Supporting non-Windows SDK winmd files is or will be supported, but is not intuitive at the moment. **There is no existing crate for Windows MIDI Services right now.**

* [Getting Started with windows-rs](https://kennykerr.ca/rust-getting-started/)
* [Rust for Windows and the windows crate](https://learn.microsoft.com/windows/dev-environment/rust/rust-for-windows)
* [Set up your Rust Development Environment](https://learn.microsoft.com/windows/dev-environment/rust/setup)
* [windows-rs on GitHub](https://github.com/microsoft/windows-rs)

## Consuming from C++ without Visual Studio (using cmake or other tools)

The C++/WinRT tool `cppwinrt.exe` will generate a set of standard C++ header files including `Microsoft.Windows.Devices.Midi2.h` which you can pull in and include in your project. The header file projections for WinRT types outside of `Microsoft::Windows::Devices::Midi2` are generated from the Windows SDK and included in a subfolder. 

First, install the latest Windows SDK. You can get the SDK from the [Windows Dev Center](https://developer.microsoft.com/windows/downloads/windows-sdk/)

The SDK install includes a version  of the `cppwinrt.exe` tool. However, the latest version of this tool is [available via NuGet](https://www.nuget.org/packages/Microsoft.Windows.CppWinRT/), and is preferable to the older versions shipping in the Windows SDK. You can end up with version mismatches if you use an older version.

Normally, all SDK header files, on my PC with the 10.0.22621.0 version of the SDK installed, are located here
`C:\Program Files (x86)\Windows Kits\10\Include\10.0.22621.0\cppwinrt\winrt`

### Generating the Projection Headers

The tool produces the header files from from the .winmd file. This file can be found with the developer release of Windows MIDI Services either as a separate download in the release, or by opening the NuGet package (it's just a zip file) and pulling it from there. The `.winmd` file is just metadata about the implementation dll.

> NOTE: The Windows SDK Includes a version of cppwinrt.exe, but it is older. To generate compatible projections with the latest bug fixes, you want the version that is included in the C++/WinRT NuGet package.

```
C:\demos\cppwinrt>dir
 Volume in drive C has no label.
 Volume Serial Number is 0AEC-1038

 Directory of C:\demos\cppwinrt

11/09/2023  02:21 PM    <DIR>          .
11/09/2023  02:20 PM    <DIR>          ..
11/06/2023  08:48 PM            55,808 Microsoft.Windows.Devices.Midi2.winmd
               1 File(s)         55,808 bytes
               2 Dir(s)  32,987,725,824 bytes free

C:\demos\cppwinrt>

C:\demos\cppwinrt>set cppwinrt="path_to_downloaded_nuget_cppwinrt_exe\cppwinrt.exe"

C:\demos\cppwinrt>%cppwinrt% -input Microsoft.Windows.Devices.Midi2.winmd -reference 10.0.20348.0+ -output .\projection

C:\demos\cppwinrt>dir /s
 Volume in drive C has no label.
 Volume Serial Number is 0AEC-1038

 Directory of C:\demos\cppwinrt

11/09/2023  02:26 PM    <DIR>          .
11/09/2023  02:20 PM    <DIR>          ..
11/09/2023  02:26 PM    <DIR>          projection
11/06/2023  08:48 PM            55,808 Microsoft.Windows.Devices.Midi2.winmd
               1 File(s)         55,808 bytes

 Directory of C:\demos\cppwinrt\projection

11/09/2023  02:26 PM    <DIR>          .
11/09/2023  02:26 PM    <DIR>          ..
11/09/2023  02:26 PM    <DIR>          winrt
               0 File(s)              0 bytes

 Directory of C:\demos\cppwinrt\projection\winrt

11/09/2023  02:26 PM    <DIR>          .
11/09/2023  02:26 PM    <DIR>          ..
11/09/2023  02:26 PM    <DIR>          impl
11/09/2023  02:26 PM           349,214 Microsoft.Windows.Devices.Midi2.h
               1 File(s)        349,214 bytes

 Directory of C:\demos\cppwinrt\projection\winrt\impl

11/09/2023  02:26 PM    <DIR>          .
11/09/2023  02:26 PM    <DIR>          ..
11/09/2023  02:26 PM           155,054 Microsoft.Windows.Devices.Midi2.0.h
11/09/2023  02:26 PM            22,170 Microsoft.Windows.Devices.Midi2.1.h
11/09/2023  02:26 PM            26,886 Microsoft.Windows.Devices.Midi2.2.h
               3 File(s)        204,110 bytes

     Total Files Listed:
               5 File(s)        609,132 bytes
              11 Dir(s)  32,988,221,440 bytes free

C:\demos\cppwinrt>
```

> Note: there are quite a few additional header files generated. The above documentation is older so only shows a few. You want all files that start with Microsoft.Windows.Deivces.Midi2*.h . You may also need header files from the OS SDK. Normally, C++/WinRT will take care of this for you if you are using Visual Studio 2022 or later.

Build againt the latest SDK, but the minimum SDK to build against is `10.0.20348.0`, to support Windows 10. If you get a ["Mismatched C++/WinRT headers" message](https://github.com/microsoft/cppwinrt/pull/683), double-check that you are using the `cppwinrt.exe` from the nuget package and you have the latest SDK installed.

### Using the Projection

Once you have the header file referenced, you can use the same sample code used in the C++/WinRT examples. Note that the generated projection header takes care of referencing dependencies from the generated files and from the SDK. You will need to ensure that referenced tree of files is part of your build process by having the correct include path for the generated files and the SDK headers. **NOTE: don't use my example below. That is subject to change. Use what is actually generated by the version of cppwinrt.exe from the NuGet package.**

```cpp
// WARNING: Please don't edit this file. It was generated by C++/WinRT v2.0.220110.5

#pragma once
#ifndef WINRT_Microsoft_Windows_Devices_Midi2_H
#define WINRT_Microsoft_Windows_Devices_Midi2_H
#include "winrt/base.h"
static_assert(winrt::check_version(CPPWINRT_VERSION, "2.0.220110.5"), "Mismatched C++/WinRT headers.");
#define CPPWINRT_VERSION "2.0.220110.5"
#include "winrt/Windows.Devices.h"
#include "winrt/impl/Windows.Data.Json.2.h"
#include "winrt/impl/Windows.Devices.Enumeration.2.h"
#include "winrt/impl/Windows.Devices.Midi.2.h"
#include "winrt/impl/Windows.Foundation.2.h"
#include "winrt/impl/Windows.Foundation.Collections.2.h"
#include "winrt/impl/Microsoft.Windows.Devices.Midi2.2.h"
...
```

### GCC Support

We haven't tried it ourselves, but C++/WinRT does appear to be compatible with GCC. See this [pull request from 2022](https://github.com/microsoft/cppwinrt/pull/1245).

## Consuming from NodeJS / Electron

We are investigating projection support for node.js / Electron. We have a prelimary version working. In that version, the code to enumerate endpoints and then send messages in a loop looks like this:

```js
function createWindow () {
    const mainWindow = new BrowserWindow({
      width: 800,
      height: 600,
      webPreferences: {
        preload: path.join(__dirname, 'preload.js')
      }      
    })
  
    mainWindow.loadFile('index.html')

    // Enumerate endpoints
    const endpoints = midi2.MidiEndpointDeviceInformation.findAll(
        midi2.MidiEndpointDeviceInformationSortOrder.name, 
        midi2.MidiEndpointDeviceInformationFilters.diagnosticLoopback +
            midi2.MidiEndpointDeviceInformationFilters.standardNativeUniversalMidiPacketFormat +
            midi2.MidiEndpointDeviceInformationFilters.standardNativeMidi1ByteFormat);


    console.log(endpoints);

    for (var i = 0; i < endpoints.size; i++)
    {
        var endpoint = endpoints.getAt(i);

        console.log(endpoint.id);
        console.log(endpoint.deviceInstanceId);
        console.log(endpoint.name);
        console.log("------------------------------------------------");
        console.log("");

    }

    const loopbackAId = midi2.MidiDiagnostics.diagnosticsLoopbackAEndpointDeviceId;
    const loopbackBId = midi2.MidiDiagnostics.diagnosticsLoopbackBEndpointDeviceId;

    // create a new session
    var session = midi2.MidiSession.create("Electron Test Session");

    // connect to loopback A
    var sendConnection = session.createEndpointConnection(loopbackAId);

    // connection needs to be opened before it is used
    sendConnection.open();

    // send messages out to that endpoint
    for (var j = 0; j < 1000; j++)
    {
        sendConnection.sendSingleMessageWords(midi2.MidiClock.now, 0x48675309, 0xDEADBEEF);
    }

    session.close();
  }
```

You can see it's very similar to the code for other languages like C# and C++.

Here's what the output looked like in the initial test. I also had a `midi.exe` console running and all 1000 messages were received.

```
C:\demos\node-midi\electron-midi>npm start

> electron-midi@1.0.0 start
> electron .


Windows::Foundation::Collections:IVectorView {
  __winRtInstance__: true
}
\\?\SWD#MIDISRV#MIDIU_DIAG_LOOPBACK_A#{e7cce071-3c03-423f-88d3-f1045d02552b}
SWD\MIDISRV\MIDIU_DIAG_LOOPBACK_A
Diagnostics Loopback A
------------------------------------------------

\\?\SWD#MIDISRV#MIDIU_DIAG_LOOPBACK_B#{e7cce071-3c03-423f-88d3-f1045d02552b}
SWD\MIDISRV\MIDIU_DIAG_LOOPBACK_B
Diagnostics Loopback B
------------------------------------------------

\\?\SWD#MIDISRV#MIDIU_KS_BIDI_14488056966904779946_OUTPIN.0_INPIN.1#{e7cce071-3c03-423f-88d3-f1045d02552b}
SWD\MIDISRV\MIDIU_KS_BIDI_14488056966904779946_OUTPIN.0_INPIN.1
UM-ONE
------------------------------------------------


C:\demos\node-midi\electron-midi>
```

## Webview2 Hosted / PWA

We are investigating.

## Python

Our SDK ships out-of-band with Windows, however, tools like [PyWinRT](https://github.com/pywinrt/pywinrt) could potentially be used to create projections.
