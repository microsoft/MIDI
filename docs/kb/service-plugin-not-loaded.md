---
layout: kb
title: Why a Service Plugin may not load correctly
audience: everyone
description: Explains how to get a third-party service plugin to load in Windows MIDI Services.
---

Windows MIDI Services supports the creation of Transport (and in the future, Transform) plugins in the service, as a way to create or prototype new MIDI standards and communications methods without having to write kernel drivers.

## Unsigned and not running in Developer Mode

The Windows MIDI Service does a signing check for all loaded service plugins, including our own. If the plugin has not been signed using a certificate recognized by this PC, then the plugin will not load.

To get around this, developers, and those testing out unsigned plugins, should turn on Developer Mode in Windows Settings. This bypasses the signing check.

> <h4>NOTE<h4>
> Bypassing the signing check could put your PC at risk by loading malicious code into the MIDI Service. Do this only for plugins you know and trust.

## Failed Initialize call

If the plugin fails in the `Initialize` method, it will not be loaded in the service and will not be available. This failure could be an unexpected exception (your plugin should catch all exceptions and not do this), or a failure `HRESULT`.
