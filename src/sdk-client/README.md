# Client SDK

C++ client SDK.

WinRT and other client SDKs coming. C++ is first.

Everything in here is subject to change. As of August 31, 2022, it does not yet include the new Function Block changes (which are significant) or other upcoming spec changes. Consider anything else work-in-progress or prototype.

Current "shape" of API can be found in the WindowsMidiServicesApi.h and the other WindowsMidi*.h files it references.

> NOTE: These have not yet gone through any internal API reviews, nor have they been code reviewed internally. But they are good for getting an idea of where we're headed. This will all get more solid in September/October.

What this is good for is to validate that we're taking an approach you find usable. We're hitting features at the right levels, etc. We're trying to strike a balance between exposing raw protocol information (which can be easily misinterpreted and can be challenging for apps to handle, especially in a multi-client setup) and too much handholding.

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

