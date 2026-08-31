// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#include "pch.h"

#include <bcrypt.h>

// See the header for the design constraints and the open questions. Nothing in this file
// resolves a real secret yet. https://github.com/microsoft/MIDI/issues/733

_Use_decl_annotations_
HRESULT
MidiNetworkCredentialResolver::ResolveSharedSecret(
    MidiNetworkCredentialIdentifier const& identifier,
    MidiNetworkSecret& secret)
{
    secret.Clear();

    // The identifier is attacker-influenced. Refusing malformed ones here, before any store is
    // touched, is what keeps this from becoming a way to read arbitrary credentials.
    RETURN_HR_IF(E_INVALIDARG, !identifier.IsWellFormed());

    TraceLoggingWrite(
        MidiNetworkMidiTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_WARNING,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_WARNING),
        TraceLoggingWideString(L"Shared secret resolution is not implemented. Authentication will be refused.", MIDI_TRACE_EVENT_MESSAGE_FIELD)
    );

    return E_NOTIMPL;
}

_Use_decl_annotations_
HRESULT
MidiNetworkCredentialResolver::ResolveUserCredential(
    MidiNetworkCredentialIdentifier const& identifier,
    std::wstring& userName,
    MidiNetworkSecret& password)
{
    userName.clear();
    password.Clear();

    RETURN_HR_IF(E_INVALIDARG, !identifier.IsWellFormed());

    TraceLoggingWrite(
        MidiNetworkMidiTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_WARNING,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_WARNING),
        TraceLoggingWideString(L"User credential resolution is not implemented. Authentication will be refused.", MIDI_TRACE_EVENT_MESSAGE_FIELD)
    );

    return E_NOTIMPL;
}

_Use_decl_annotations_
bool
MidiNetworkCredentialResolver::CredentialExists(
    MidiNetworkCredentialIdentifier const& identifier)
{
    if (!identifier.IsWellFormed())
    {
        return false;
    }

    return false;
}


_Use_decl_annotations_
HRESULT
MidiNetworkGenerateCryptoNonce(
    uint8_t* buffer,
    size_t const byteCount)
{
    RETURN_HR_IF_NULL(E_INVALIDARG, buffer);
    RETURN_HR_IF(E_INVALIDARG, byteCount == 0);
    RETURN_HR_IF(E_INVALIDARG, byteCount > ULONG_MAX);

    // A nonce must never come from a predictable source, so this uses the system CSPRNG rather
    // than anything based on a timestamp or a counter.
    auto status = BCryptGenRandom(
        nullptr,
        buffer,
        static_cast<ULONG>(byteCount),
        BCRYPT_USE_SYSTEM_PREFERRED_RNG);

    if (!BCRYPT_SUCCESS(status))
    {
        SecureZeroMemory(buffer, byteCount);

        RETURN_IF_FAILED(HRESULT_FROM_NT(status));
    }

    return S_OK;
}
