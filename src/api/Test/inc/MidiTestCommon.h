// Copyright (c) Microsoft Corporation. All rights reserved.
#pragma once

typedef struct
{
    BYTE mt_group;
    BYTE status;
    WORD data;
} UMP32;

typedef struct
{
    BYTE mt_group;
    BYTE status;
    WORD data;
    DWORD data2;
} UMP64;

typedef struct
{
    BYTE mt_group;
    BYTE status;
    WORD data;
    DWORD data2;
    DWORD data3;
} UMP96;

typedef struct
{
    BYTE mt_group;
    BYTE status;
    WORD data;
    DWORD data2;
    DWORD data3;
    DWORD data4;
} UMP128;

extern UMP32 g_MidiTestData_32;
extern UMP64 g_MidiTestData_64;
extern UMP96 g_MidiTestData_96;
extern UMP128 g_MidiTestData_128;

void PrintMidiMessage(_In_ PVOID, _In_ UINT32, _In_ UINT32, _In_ LONGLONG);

#ifndef LOG_OUTPUT
#define LOG_OUTPUT(fmt, ...)  WEX::Logging::Log::Comment(WEX::Common::String().Format(fmt, __VA_ARGS__))
#endif

