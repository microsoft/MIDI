// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

//#define BASE_MESSAGE_SCHEDULER_TICK_WINDOW 10               // this is in ticks of the MIDI timestamp clock, which is typically 10Mhz or 10 microseconds per tick
//#define MIDI_MESSAGE_SCHEDULE_ENQUEUE_RETRY_COUNT 10

// complete guess. We can fine tune this
#define MIDI_SCHEDULER_LOCK_AND_SEND_FUNCTION_LATENCY_TICKS     100     // roughly 10 microseconds
#define MIDI_SCHEDULER_ENQUEUE_OVERHEAD_LATENCY_TICKS           10
#define MIDI_SCHEDULER_MAX_MESSAGES_TO_PROCESS_AT_ONCE          500


#define MAXIMUM_UMP_DATASIZE 16
#define MINIMUM_UMP_DATASIZE 4


#define MIDI_SCHEDULER_MINIMUM_EVENT_SLEEP_TIME_MS 2000
#define MIDI_OUTBOUND_EMPTY_QUEUE_SLEEP_DURATION_MS 60000


// ---------------------------------------------------------------------------------------------
// Scheduler V2 only. Values in microseconds; converted to clock ticks at run time because
// QueryPerformanceFrequency is not guaranteed to be any particular value.

// Largest busy wait we will ever do, immediately before a message is due. Chosen from measured
// high-resolution timer behavior: it wakes a consistent ~500us late, so a 1ms budget leaves a
// spin of roughly half a millisecond.
#define MIDI_SCHEDULER_V2_MAX_SPIN_MICROSECONDS                 1000

// The spin is also capped at this fraction of the gap between consecutive due messages, so a
// dense stream cannot turn the guard band into a continuous spin.
#define MIDI_SCHEDULER_V2_SPIN_GAP_DIVISOR                      8

// Measured overshoot of CREATE_WAITABLE_TIMER_HIGH_RESOLUTION on current hardware. Only used to
// aim the timer; the loop re-checks the clock afterwards, so an inaccurate value here costs at
// most one extra short wait and never correctness.
#define MIDI_SCHEDULER_V2_TIMER_OVERSHOOT_MICROSECONDS          500

// Below this, arming a timer is not worth the round trip, so the remaining time is spun instead.
#define MIDI_SCHEDULER_V2_MINIMUM_TIMER_WAIT_MICROSECONDS       250

// Ceiling on the per-device compensation read from the endpoint properties. A value beyond this is
// not a plausible device latency and would start eating into the forward scheduling window.
#define MIDI_SCHEDULER_V2_MAX_DEVICE_LATENCY_MICROSECONDS       1000000


