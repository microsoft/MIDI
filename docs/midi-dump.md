---
layout: page
title: MIDI Dump Utility
parent: Windows Midi Services
---

# Windows Dump Utility

Windows MIDI Services comes with a simple command-line utility `mididmp.exe`. This has been designed for use by DAWs and support applications which need to shell out to a known executable to capture health information about the system.

The utility returns information only about MIDI. Here is example output:

```
C:\Users\peteb>mididmp
Microsoft Windows MIDI Services
2024-02-08 18:28:52


===============================================================================
Endpoints
===============================================================================

endpoint_device_id : \\?\swd#midisrv#midiu_diag_loopback_a#{e7cce071-3c03-423f-88d3-f1045d02552b}
name : Diagnostics Loopback A
transport_mnemonic : DIAG
name_user_supplied :
name_user_endpoint_supplied :
name_user_transport_supplied : Diagnostics Loopback A
description_transport_supplied :
description_user_supplied : Diagnostics loopback endpoint. For testing purposes.
parent_id : SWD\MIDISRV\MIDIU_DIAG_TRANSPORT
parent_name : MIDI 2.0 Diagnostics Devices
-------------------------------------------------------------------------------
endpoint_device_id : \\?\swd#midisrv#midiu_diag_loopback_b#{e7cce071-3c03-423f-88d3-f1045d02552b}
name : Diagnostics Loopback B
transport_mnemonic : DIAG
name_user_supplied :
name_user_endpoint_supplied :
name_user_transport_supplied : Diagnostics Loopback B
description_transport_supplied :
description_user_supplied : Diagnostics loopback endpoint. For testing purposes.
parent_id : SWD\MIDISRV\MIDIU_DIAG_TRANSPORT
parent_name : MIDI 2.0 Diagnostics Devices
-------------------------------------------------------------------------------
endpoint_device_id : \\?\swd#midisrv#midiu_diag_ping#{e7cce071-3c03-423f-88d3-f1045d02552b}
name : Diagnostics Ping (Internal)
transport_mnemonic : DIAG
name_user_supplied :
name_user_endpoint_supplied :
name_user_transport_supplied : Diagnostics Ping (Internal)
description_transport_supplied :
description_user_supplied : Internal UMP Ping endpoint. Do not send messages to this endpoint.
parent_id : SWD\MIDISRV\MIDIU_DIAG_TRANSPORT
parent_name : MIDI 2.0 Diagnostics Devices
-------------------------------------------------------------------------------
endpoint_device_id : \\?\swd#midisrv#midiu_ks_bidi_16024944077548273316_outpin.0_inpin.2#{e7cce071-3c03-423f-88d3-f1045d02552b}
name : Unnamed Awesome MIDI 2.0 Synth
transport_mnemonic : KS
name_user_supplied : Unnamed Awesome MIDI 2.0 Synth
name_user_endpoint_supplied : Foo Synth Amazing
name_user_transport_supplied : Foo Synth
description_transport_supplied :
description_user_supplied : I love this synthesizer, but can't name it in public.
parent_id : USB\VID_6666&PID_7777&MI_03\9&1663efa2&0&0003
parent_name : Foo Synth

===============================================================================
Ping Test
===============================================================================

ping_attempt_count : 10
ping_return_count : 10
ping_time_round_trip_total_ticks : 7954
ping_time_round_trip_average_ticks : 795

===============================================================================
Clock
===============================================================================

clock_frequency : 10000000
clock_now : 912988511667
```