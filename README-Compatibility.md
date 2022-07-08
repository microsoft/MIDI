# Compatibility

Although this API will coexist with the existing MIDI 1.0 WinMM API, for backwards-compatibility reasons, the intent is to have everyone converted to this new API going forward and to eventually deprecate the WinMM API, as it doesn't support many of the features in demand today.

## Client code compatibility

The APIs will be created as components usable by as many different languages as possible. Any language which can talk to WinRT ("modern COM") interfaces will be able to use the API. That includes but is not limited to
C++/WinRT, .NET and others.

As this approaches maturity, we'll also work with browser teams to enable WebMIDI to use the new APIs.

## Older APIs

What happens to the older APIs like WinMM, DirectMusic, WinRT MIDI, and more? Our intention is to deprecate those when this API meets the requirements for use and a critical mass of adoption. Initially, we didn't want to create yet another API to add to the list, but existing APIs were heavily tied to models which don't move well into MIDI 2.0 and a more open ecosystem of transports.

Deprecate doesn't mean those APIs go away immediately, but it does tell developers that we have an intention to eventually remove or otherwise stop supporting them. An example is DirectMusic. That was deprecated many years ago but is still shipped with Windows, but does not receive enhancements or other changes. We'll make our intentions clear to developers and encourage them to adopt the new APIs.

### Can older APIs use new features?

The new class driver replaces the existing MIDI 1.0 class driver delivered with Windows. As a result, existing APIs may gain some minor features, such as multi-client. However, the older APIs will not gain new transports or MIDI 2.0 functionality.

### What about additions like the GS Synth in Windows?

We're evaluating functionality for this. Intent is to make sure it works for as long as it is supported in Windows, at least with the existing APIs today, but that will depend on a number of factors in driver and API design. Additionally, going forward, in-box Virtual MIDI will provide more options for MIDI playback under the new API.

We know some folks love the GS synth, but it has high latency, and very little flexibility. The sounds are
also quite old at this point. But it's still useful for MIDI file playback.

### What about third-party MIDI 1.0 drivers?

We're evaluating how to best support these drivers, which include device-specific drivers for enhanced devices, or non-class-compliant devices, as well as drivers for features like BLE MIDI and others. The intent here is to maintain compatibility for apps using the old WinMM API, but also to find a way to enable the use of these devices alongside the class driver in the new MIDI 2.0 API.

There's currently no plan to support devices with custom USB MIDI 2.0 drivers. These devices should all
use the new class driver.

## Taking a dependency on this API in your own apps

Windows MIDI Services will not be installed on all PCs but instead will need to be installed by customers who want the functionality it provides. Apps which require this API and driver should indicate as much in the product itself, and include any minimum version. Example:

> Requires Windows MIDI Services 1.1 available here (link to release).

### Checking for Windows MIDI Services on a PC

TODO: Guidance on how to tell if Windows MIDI Services is currently installed, and which version it is

### Linking to the installer

TODO: Guidance on linking to the installer from your own apps and/or app installers.
