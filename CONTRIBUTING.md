# Contributing

This project welcomes contributions and suggestions. Most contributions require you to agree to a Contributor License Agreement (CLA) declaring that you have the right to, and actually do, grant us the rights to use your contribution. For details, visit https://cla.opensource.microsoft.com.

When you submit a pull request, a CLA bot will automatically determine whether you need to provide a CLA and decorate the PR appropriately (e.g., status check, comment). Simply follow the instructions provided by the bot. You will only need to do this once across all repos using our CLA.

This project has adopted the [Microsoft Open Source Code of Conduct](https://opensource.microsoft.com/codeofconduct/). For more information see the [Code of Conduct FAQ](https://opensource.microsoft.com/codeofconduct/faq/) or contact [opencode@microsoft.com](mailto:opencode@microsoft.com) with any additional questions or comments.

## Pull Requests and Forks

We're excited to have others contribute to the APIs, tools, plug-ins, drivers and more! We generally recommend that contributors fork the project they are interested in, and then submit pull requests from their own forks. We'll review the PRs on a regular basis, and accept/decline or request additional information.

> If you prefer to work with C# and .NET, the MIDI Settings app and MIDI Console will likely be of interest to you. The rest is in C++. The MIDI Console is a powerful console app using Spectre.Console for UI. The MIDI Settings app is a desktop WinUI3 / WinAppSDK app designed for end-user configuration.

This is an open source project with a permissive license, but sometimes it helps for OSS owners to declare intent. This is our intent: You may fork these projects as per the license, without any further restriction. However, we highly recommend you consider contributing to the main project rather than spin off a separate and potentially incompatible or competing project targeting Windows. In the end, we want to do what's right for musicians, music creation app / plug-in developers, and music hardware developers.

**Forking to support other operating systems (including hardware devices) is also allowed and encouraged.**
However, in that case, we again encourage you to consider what we can do together in the primary repo to support the requirements for these other platforms so that we can keep feature level, tool, and API parity, especially as the specifications evolve, or we fix bugs.

## Contributing image assets

Any images submitted must have clear license for use that is compatible with the Windows MIDI Services project. We cannot accept icons generated from other work without clear license, including images and fonts. Preference is given to icons/images created specifically for use in this project. Please adhere to the Contributor License Agreement when contributing. If there's any ambiguity, we will not be able to accept the contribution.

# What do I need to get started?

Projects generally use the most recent recent versions of [Visual Studio](https://visualstudio.microsoft.com/), [C++/WinRT](https://docs.microsoft.com/windows/uwp/cpp-and-winrt-apis/), [Windows Implementation Library](https://github.com/microsoft/wil), Windows 10/11 SDKs, [Windows App SDK](https://github.com/microsoft/WindowsAppSDK), [Windows Community Toolkit](https://github.com/CommunityToolkit/WindowsCommunityToolkit) and [.NET 8](https://dotnet.microsoft.com/) available at the time of authoring. Specific build requirements will be available in the readme for each sub project.

Projects are intended to be compiled on Windows, typically the latest release of Windows 11. Cross-compiling from another platform may be appropriate for some forks, but not necessarily for the main API, driver, and other components. Additionally, using toolchains other than Visual Studio are currently not supported for targeting Windows. When in doubt, please ask.

We're happy to accept contributions which add support for other compilers and which do not break the internal and Visual Studio support. We're unlikely to maintain those ourselves, and would require on-going community support for those.

# What should I contribute?

We'd love to have your help on this project, no matter what kind it is.

Types of contributions we're especially interested in:

* Bug fixes
* Features you want to see implemented
* Transport plug-ins for new or even experimental transports
* Processing plug-ins for MIDI Mapper and similar types of message processing
* Testing and bug reports (especially with different MIDI 1.0 and MIDI 2.0 hardware and software)
* API and user Documentation
* Samples
* Localization to a language you are fluent in

Testing and bug reports are the most critical, as everyone's setup is different, and this is a complete rewrite of an important part of Windows. We want to ensure the project is as bug free and as performant as possible, with a large variety of PCs and peripherals.

For code, one place to start is by looking at the open issues here in Github, or discussions in the dedicated Discord server. When in doubt, ask the core team.

> All contributions must be compatible with the MIT license we use. 

## Specific Considerations

### External OSS Library Dependencies

For the main API, service, and in-box applications, other OSS libraries may only be used if they have a .vcpkg for that library. This is a requirement of our internal build system, which is required for code signing and shipping in-box.

New external and OSS dependencies must be explicitely declared in the PR, because we need to clear them and register their use before we can ingest the code.

### Specific Libraries for Internal Pieces (Service, Plugins, API, etc.)

Our internal build system generally supports the latest versions of WIL and C++/WinRT. When in doubt, look to see the versions we're referencing in the core code.

For JSON manipulation, the only library we support is the WinRT JSON platform library Windows.Data.Json

### API Changes

We cannot accept any breaking API changes once the project ships in-box. We must maintain backwards compatibility. This includes renaming parameters, changing types, etc. There are mechanisms for us to *add* functionality, however. When in doubt, talk with us.

> Note that any app-visible API changes need to go through an internal central API review process, which does add time to that process.

