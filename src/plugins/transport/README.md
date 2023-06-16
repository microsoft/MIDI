# Pluggable Transports / Devices

Prior to this project, new transports required custom drivers. The new model is to create transports as user mode components which plug into the API. This makes it easier/faster to adopt new transports in the future, as they are defined.

BLE 1.0 and RTP 1.0 transport specifications are defined (as well as USB MIDI 1.0) for MIDI 1.0. For MIDI 2.0, only USB MIDI 2.0 is defined. However, network MIDI 2.0 is in progress and will be available in the future.

All pluggable transports need to follow the contract defined by the services abstractions. Additionally, transports need to provide a separate settings app plugin for end-user configuration and visibility.

Likely transports over time

* USB MIDI (plug-in speaks to new USB MIDI class driver for MIDI 1.0 and MIDI 2.0)
* BLE MIDI (repackaging of WinRT MIDI interface to BLE)
* RTP MIDI 1.0 (support existing RTP MIDI 1.0 devices)
* Network MIDI 2.0 (support upcoming network MIDI standard)
* Virtual MIDI (support app to app, routing, and more)
* In-box Roland GS Synth
* Additional controllers which are not MIDI-enabled today. Example: surface dial, gamepad, proximity devices, accelerometers, remote controls, etc.

Others may also create "playground" types of transports to enable experimentation. For example, an interface to a custom PCIe or serial-connected device, or using a microphone for pitch-to-MIDI, or a webcam as a controller. We want implementation here to be easy enough to encourage this type of experimentation on Windows.

## Settings UI sections

TODO
