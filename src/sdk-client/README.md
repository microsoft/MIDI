# Client SDK

C++ client SDK.

## Timer Utility

Scheduled MIDI message sending uses the high-resolution system counters provided by QueryPerformanceCounter. The resolution of this counter varies by system, as does the call latency. In modern Windows 10 and 11 systems, this is generally not an issue. However, to be sure, you can run the TimerInfoUtility command-line tool to inspect your own system. (We'll likely put similar functionality into the settings app in the future.)

![Timer Information](img/timer-info-utility.png)

## Building and Tools

Use latest Visual Studio 2022.

The C++ code in this solution uses boost (currently boost 1.80.0) for a number of types . My BOOST_ROOT is C:\Program Files\boost\boost_1_80_0. I set a system environment variable so I could use it in Visual Studio in the include and lib locations.

![BOOST_ROOT Environment Variable](img/boost-path.png)

![BOOST_ROOT Environment Variable in Visual Studio](img/boost-include-and-lib-in-visual-studio.png)

The C++ code also uses the latest accepted language spec (C++ 20) and latest standard library functions.

### Building

TODO

### Boost

TODO

### Visual Studio Add-in

If you end up editing MIDL at all, be sure to download this extension from the Visual Studio marketplace. MIDL 3.0 editing is pretty frustrating without it: [WinRT Tools for C++ by Mads Kristensen](https://marketplace.visualstudio.com/items?itemName=MadsKristensen.MIDL)

While you're at it, if you are updating any of the readme files in Visual Studio, grab the [Markdown Editor v2](https://marketplace.visualstudio.com/items?itemName=MadsKristensen.MarkdownEditor2) VSIX.

