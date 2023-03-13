# Client SDK

## SDK vs API

The base API is the bare-bones required for communicating with the MIDI Service and sending/receiving messages, opening/closing communications channels, etc. This is intended to have minimal revision over time.

The SDK includes all the additional functionality a typical app will need. In general, these provide more easily used constructs for MIDI CI and Function Blocks, Endpoints, properties, etc. as well as strongly-typed message classes. This is planned to change to keep up with new messages, features, and transports and to provide maximum features with minimal overhead.

**It is highly recommended that all applications and Windows and cross-platform middleware code against the SDK and not the API.** However, those prototyping transports, new MIDI messages, low-level features, SDK add-ons, and more can use the API directly.

