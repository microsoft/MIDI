---
layout: kb
title: Threading, apartments, and COM / WinRT initialization
audience: developers
description: How to initialize COM / WinRT on your threads when using the WinRT SDK
---

COM developers are used to initializing COM with `CoInitialize` or `CoInitializeEx`. With WinRT APIs, these still work, but are not exactly equivalent, and the WinRT versions are preferred.

This article helps clarify the concurrency model for the Windows MIDI Services SDK, and WinRT in general, as needed by native code developers (C++ etc.) Managed code developers generally do not need to worry about initializing WinRT/COM.

## Initializing the apartment

The recommended way to initialize the thread that will make WinRT API/SDK calls is to use the C++/WinRT `winrt::init_apartment` call. If you are not using C++/WinRT, you should call `RoInitialize` from roapi.h. [More information on RoInitialize available here](https://learn.microsoft.com/windows/win32/api/roapi/nf-roapi-roinitialize).

When you are done with the SDK, and no longer holding any SDK or other WinRT or COM references, call `RoUnitialize`. This is typically something you would call on thread or app shutdown.

## Single or multi-threaded apartment

We recommend initializing the thread using the multi-threaded apartment concurrency model. This will help avoid cross-thread marshalling for time-sensitive tasks like the callback when a MIDI message has been received.

By default, WinRT thread initialization calls specify a multi-threaded apartment. This is different from the default behavior of `CoInitialize`.

> The Windows MIDI Services WinRT SDK has been designed to be thread-safe for most use-cases, with multi-threaded apartment as the recommended concurrency model to avoid marshalling calls. If you find something that runs counter to this, please let us know [by filing a bug with a repro](https://aka.ms/midirepoissues).

## RoInitialize vs CoInitialize(Ex)

It is safe to use `RoInitialize` instead of `CoInitialize` or `CoInitializeEx` on a thread which uses both COM and WinRT. The former (`RoInitialize`) is a superset of CoInitialize, with a different default concurrency model, as mentioned. For WinRT APIs/SDKs, to maximize compatibility and stability, we recommend always using `RoInitialize` (or `winrt::init_apartment`) instead of `CoInitialize` or `CoInitializeEx`.

## Shutdown

When shutting down the thread or app, you may call `RoUninitialize` or `winrt::uninit_apartment` to clean up the apartment, after you have voided all references to WinRT and COM objects. However, this call is typically not needed. 

The rules here have not really changed from those in COM, so follow the practices you [have found to work with COM shutdown in the past](https://devblogs.microsoft.com/oldnewthing/20120105-00/?p=8683).

## A note on UI threads

UI threads in most UI toolkits should be initialized as single-threaded apartments, if they require COM/WinRT initialization. One example that was brought up during testing is the "Print to PDF" COM API. This was designed to run on an STA-initialized UI thread.

The above information has been provided on the assumption that your MIDI calls are not happening on the UI thread.
