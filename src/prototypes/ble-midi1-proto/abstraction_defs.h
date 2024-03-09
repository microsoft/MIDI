// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#pragma once


#ifndef ABSTRACTION_DEFS_H
#define ABSTRACTION_DEFS_H

#define MIDI_BLE_SERVICE L"{03B80E5A-EDE8-4B33-A751-6CE34EC4C700}"
#define MIDI_BLE_DATA_IO_CHARACTERISTIC L"{7772E5DB-3868-4112-A1A9-F2669D106BF3}"


// Notes:
//      Write (encryption recommended, write without response is required)
//      Read (encryption recommended, respond with no payload)
//      Notify (encryption recommended)
// Max connection interval is 15ms. Lower is better.

#endif