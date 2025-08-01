---
layout: kb
title: SDK versioning and compatibility for developers
audience: developers
description: How do we version the SDK, and what compatibility is expected?
---

The Windows MIDI Services App SDK ships in two pieces
- The NuGet package with the metadata (winmd) file, and the C# projection
- The Runtime implementation package

We do not enforce updates, but customers will prompted to update their runtime installation through the MIDI Settings app. Applications also have access to the APIs to prompt the customers in the same way.

The NuGet package is a compile-time artifact. 

As a result, the two could get out of sync. For example, a customer could have version 1.2.3 installed, and the app was compiled against 1.0.2. How will this work?

# App SDK Versioning

Except for during the preview period before the first production release, the App SDK generally follows SemVer versioning rules. The version is represented by `Major.Minor.Patch` with some additional build number and preview data optionally at the end.

Here's what you should expect when looking at version numbers between the NuGet package and the SDK Runtime.

| Version Component Change | Expectation |
| ------------------------ | ----------- |
| Major | When this number changes, compatibility has been broken. We'll install side-by-side with the previous compatibility version to ensure both old and new apps will work. Apps using the new SDK version may have a different bootstrapper COM component to activate.|
| Minor | New features have been added which typically only light up for apps using a matching NuGet package. Older apps will continue to work as expected. |
| Patch | Bug fixes. No SDK compatibility changes, but could have limited behavioral changes as a result of the bug fix. |
| Build metadata | Internal. Not factored into compatibility. Resets with each patch. |

# Service Versioning

The MIDI Service itself may require changes over time, and is delivered with Windows. Our plan is to not break compatibility here, but instead to add on to existing interfaces or create new interfaces as-needed, just like we do for other COM APIs in Windows. We also have a good extensibility mechanism based on json which makes the creation of new COM interfaces needed only in limited circumstances. For example, adding a new type of Transport does not require a new interface, because we handle transport configurations through a common JSON endpoint.

