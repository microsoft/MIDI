// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#pragma once

//#define BASE_MESSAGE_SCHEDULER_TICK_WINDOW 10               // this is in ticks of the MIDI timestamp clock, which is typically 10Mhz or 10 microseconds per tick
//#define MIDI_MESSAGE_SCHEDULE_ENQUEUE_RETRY_COUNT 10

// complete guess. We can fine tune this
#define MIDI_SCHEDULER_LOCK_AND_SEND_FUNCTION_LATENCY_TICKS     5
#define MIDI_SCHEDULER_ENQUEUE_OVERHEAD_LATENCY_TICKS           10
#define MIDI_SCHEDULER_MAX_MESSAGES_TO_PROCESS_AT_ONCE          100


#define MAXIMUM_UMP_DATASIZE 16
#define MINIMUM_UMP_DATASIZE 4


