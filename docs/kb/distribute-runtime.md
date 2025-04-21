---
layout: kb
title: How to distribute the Windows MIDI Services runtime and tools
audience: developers
description: How to distribute the Windows MIDI Services runtime and tools with your app
---

Windows MIDI Services installs in two pieces
- The in-box service, MIDI 2 driver, and API which handle WinMM and WinRT MIDI 1.0 compatibility
- The out-of-band delivered SDK Runtime and Tools, which provides a richer customer experience due to the tools, and also provides apps with the required MIDI 2.0 features and capabilities, programmatic creation of endpoints (like Virtual Device endpoints), better enumeration support, and much more.

The second part is not shipped with Windows, and there are no short to medium-term plans to do so. We will evaluate in the future based upon frequency of changes, but this will not change how you distribute and use the runtime today.

If you only use the older WinMM or WinRT MIDI 1.0 APIs, providing a runtime and tools install link from your app is optional. The user will still benefit from this as it allows for renaming ports, and much more. 

If you use the Windows MIDI Services App SDK, you must provide a way to install the required runtime if not present. The rest of this article covers the options.

## Preferred way: Link to our installer page

The Windows MIDI Services bootstrapper code, available in the [GitHub samples](https://aka.ms/midisamples) shows how to check for the presence of a compatible runtime as one of the first startup steps in your application. If not present, you may open up the user's default browser with the download page for the runtime. The link to the download page is provided from the bootstrapper code.

This is preferred because we will always have the latest compatible version available for the customer to install.

The bootstrapping check is required during your app's run. You may optionally link to the installer page as a step during the install if you feel that is a good experience for your customers.

## Including with your own install media

In some cases, you may want to provide the most recent version with your app's installer.

If you include the Windows MIDI Services runtime and tools with your own install media, you must use our full installation package. Do not separately distribute individual components from within the install package. We want customers to have all the tools and features available to them.

The installer sets the required registry entries, and ensures that dependencies (like the VC and .NET runtime versions) are met. It also sets appropriate path and other variables.

- Please select the correct installer package (Arm64 or x64) based upon the architecture of your application.
- You may run the installer in silent mode if that fits your installation flow better.
- We do not provide a merge module, just the full installer .exe bundle.

## Do not restrict versions

Your app will require a minimum version, but do not restrict the customer to a maximum version of the runtime and tools installer. We want the customer to always have the latest and greatest, with the most recent bug fixes.

For non-preview releases, we will ensure that subsequent versions are backwards compatible with the version your built on. And, in the future, if we need to break compatibility, we will do so in a way that supports both existing and new applications.

## Do not distribute a release app which depends on preview versions

You are free to distribute beta/preview versions of your apps which rely on experimental or preview versions of the SDK runtime and tools. You must caveat this somewhere on your download page/post or in the app itself so the user knows they will have to install a preview version of the SDK and Tools runtime, which will overwrite the one they have already installed, and may possibly break other applications.
