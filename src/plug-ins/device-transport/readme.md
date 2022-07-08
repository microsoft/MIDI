# Pluggable Transports

Prior to this project, new transports required custom drivers. The new model
is to create transports as user mode components which plug into the API. This
makes it easier/faster to adopt new transports in the future, as they are defined.

Today (June 2022), BLE 1.0 and RTP 1.0 transports are defined (as well as USB MIDI 1.0) for
MIDI 1.0. For MIDI 2.0, only USB MIDI 2.0 is defined. However, RTP MIDI 2.0 is in
progress and will be available soon.

All pluggable transports need to follow the contract defined in the API.