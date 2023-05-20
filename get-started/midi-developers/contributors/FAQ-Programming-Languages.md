# Programming Languages and App Models FAQ

This is a developer-focused FAQ.

## WinRT

**Q: Why is the API and SDK WinRT instead of a set of C headers like classic APIs? Why not use classic COM?**
A: New APIs for Windows are required to be WinRT, unless there are really good reasons not to be. WinRT is enhanced COM with a richer type system with better support for use by most of the languages and frameworks used to develop Windows applications. 

**Q: Does the fact that the API and SDK are WinRT mean they are sandboxed?**
A: No. WinRT is modern COM. The term has been overloaded in the past to also include an app model, Store requirements, and more. In this project, WinRT simply means the implementation flavor with support for projections. It does not impose any sandbox or other restrictions on consuming applications.

**Q: Why does the service plugin model use COM instead of WinRT?**
A: For our runtime discovery-based plugin model, "Classic" COM makes more sense. WinRT components need to be known at compile time.

**Q: Why do MIDI namespaces sometimes start with Microsoft instead of Windows?**
A: Anything targeted for delivery in-box can use Windows.Devices. Anything which is an additional component download for applications, like the SDK, use Microsoft as the top-level namespace as per our conventions.

## Projections

**Q: Which projections will this project deliver?**
A: We will start with the basic projections: C++, C# (current .net versions), and JavaScript. We will add more (Rust, for example) as we proceed in development. We want to be as inclusive here as we can reasonably be.

## App Models

**Q: What is the primary app model the API and SDK are targeting?**
A: Windows desktop apps of all types including C++, C#, Electron, and more.

**Q: Do the API and SDK support UWP Applications?**
A: During the initial testing rollout, the API is not built into Windows, and so may not be completely compatible with UWP apps. TBD which functions are usable from the UWP sandbox in the future, but we want to support as much as is possible.

## Project Implementation Languages

**Q: Which languages are used in the project?**
A: Primarily, the project is C++ and C#.

**Q: Why is the API and SDK C++ instead of Rust?**
A: Rust supports WinRT, including authoring, through the rs/WinRT project. However, Rust does not currently support Arm64EC, which means apps on Arm64 devices which need to load x64 plugins (that is, most DAWs) would not be able to load the SDK into their process. Additionally, modern C++ can be used quite safely, it's just not "safe by default" like Rust is.

**Q: Why is the API and SDK C++ instead of C#/.net?**
A: The majority of DAWs are written in C++ or similar languages. Although one can create WinRT components from C#, they carry along a runtime and garbage collection which most DAW developers do not want in their process. Additionally, C# does not support Arm64EC.

**Q: Why are the apps in C# /.net?**
A: C# is a great language for applications. Additionally, we want to encourage contributions from our enormous C#/.net development community.

**Q: Why is the Windows Service C++ instead of C#, Rust, or something else?**
A: Early prototypes of the service were in C#, which worked fine for most things, until you got into the kernel data transfer, integration with the PnP stack, and more. The implementation team already knows how to use those features and APIs, with great performance, in C++ based on their work with the audio services in Windows today, so the implementation is in C++.

**Q: Why does the driver have reimplementations of features we see in the standard library?**
A: In kernel mode drivers, the standard library is largely unavailable.
