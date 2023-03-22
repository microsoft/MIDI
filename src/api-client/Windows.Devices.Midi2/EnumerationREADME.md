If we can just use the built-in Windows.Devices.Midi, that would be better than a whole set of custom classes here.

Considerations
* Do we need to register everything in PnP?
* How will that work with virtual devices, devices added at runtime like network MIDI, etc.
* Can we provide an easy-to-use API for server plugins to use to register 