# USB MIDI 2.0 Driver Specs

## Driver high-level requirements

* Perform as the MIDI 2.0 class & device driver for MIDI 2.0 devices.
* Completely replace the existing MIDI 1.0 class driver and act as the MIDI 1.0 class & device driver for MIDI 1.0 devices.

### Compatibility with WinMM MIDI (Win32 MIDI)

At a minimum, by acting as the MIDI 1.0 class driver, it will be available, in a MIDI 1.0 capacity, to all existing MIDI 1.0 APIs.

### Driver communication requirements

Communicate back to Windows, ideally through an interface already available in Windows 10 and Windows 11. To support in-market Windows, this same interface must be a public API available to user code (Windows Service running as Local System) to be able to communicate with the driver, get buffers of data, etc.

TODO: Communications and buffer exchange recommendations
