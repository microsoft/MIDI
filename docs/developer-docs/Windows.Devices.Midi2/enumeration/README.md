# Enumeration

Enumeration is how you discover endpoints and get notified of endpoints when they are added, updated, or removed. For the best user experience, keep a `MidiEndpointDeviceWatcher` running in a background thread so you can monitor device removal, and property updates (name, function blocks, etc.)

* [MidiEndpointDeviceInformation](./MidiEndpointDeviceInformation/)
* [MidiEndpointDeviceWatcher](./MidiEndpointDeviceWatcher/)
* [MidiEndpointDeviceInformationUpdateEventArgs](./MidiEndpointDeviceInformationUpdateEventArgs/)