// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App SDK and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================
// Portions of this code adapted from the Windows UsbView Sample application
// Licenses and restrictions for that code apply

#include "pch.h"



void OutputBlankLine()
{
    std::wcout
        << std::endl;
}

#define DESCRIPTOR_NAME_UNDERLINE_WIDTH 42

void OutputDescriptorName(_In_ std::wstring const& descriptorName, _In_ std::wstring annotation)
{
    // blank line
    std::wcout << std::endl;

    // descriptor name
    std::wcout
        << descriptorName
        << std::endl;

    // annotation if any
    if (!annotation.empty())
    {
        std::wcout
            << L" <-- "
            << annotation
            << std::endl;
    }

    std::wcout
        << std::setw(DESCRIPTOR_NAME_UNDERLINE_WIDTH)
        << std::left
        << std::setfill('-')
        << L""
        << std::endl;

}

#define DESCRIPTOR_FIELD_NAME_WIDTH 35

void OutputDescriptorFieldName(_In_ std::wstring const& fieldName)
{
    std::wstring label = fieldName + L":";

    std::wcout
        << std::setw(DESCRIPTOR_FIELD_NAME_WIDTH)
        << std::left
        << std::setfill(' ')
        << label;
}

void OutputHexField(_In_ std::wstring const& fieldName, _In_ uint16_t const value, _In_ uint8_t width, _In_ std::wstring const& annotation)
{
    OutputDescriptorFieldName(fieldName);

    // spacing to keep byte and word right align
    std::wcout
        << std::setw(max(0, 4 - width))
        << std::right
        << std::setfill(' ')
        << L"";

    std::wcout
        << L"0x"
        << std::hex
        << std::setw(width)
        << std::setfill('0')
        << value;

    if (!annotation.empty())
    {
        std::wcout
            << L" <-- "
            << annotation;
    }

    std::wcout << std::endl;
}

void OutputHexByteField(_In_ std::wstring const& fieldName, _In_ uint8_t const value, _In_ std::wstring const& annotation)
{
    OutputHexField(fieldName, value, 2, annotation);
}

void OutputHexWordField(_In_ std::wstring const& fieldName, _In_ uint16_t const value, _In_ std::wstring const& annotation)
{
    OutputHexField(fieldName, value, 4, annotation);
}



#define RETURN_SUCCESS return 0
#define RETURN_FAIL return 1




void BuildDescriptorTree(_In_ uint16_t vid, _In_ uint16_t pid)
{
    // find the device with the vid and pid, and store its parent hub and the port # it is in

    // Start reading descriptors.



    // If this has a MIDI 2.0 interface, request the group terminal blocks



}


int __cdecl main()
{
    // TODO: Command-line parsing

    // command line parameter is vid and pid




    RETURN_SUCCESS;
}

