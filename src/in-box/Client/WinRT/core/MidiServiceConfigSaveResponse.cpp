// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services WinRT API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================


#include "pch.h"
#include "MidiServiceConfigSaveResponse.h"
#include "ServiceConfig.MidiServiceConfigSaveResponse.g.cpp"

namespace winrt::Windows::Devices::Midi2::ServiceConfig::implementation
{
    namespace
    {
        UINT ResourceIdForResult(_In_ svc::MidiServiceConfigSaveResult const result) noexcept
        {
            switch (result)
            {
            case svc::MidiServiceConfigSaveResult::ErrorNotPersistable:
                return IDS_CONFIG_SAVE_ERROR_NOT_PERSISTABLE;

            case svc::MidiServiceConfigSaveResult::ErrorConfigJsonNullOrEmpty:
                return IDS_CONFIG_SAVE_ERROR_JSON_NULL_OR_EMPTY;

            case svc::MidiServiceConfigSaveResult::ErrorProcessingConfigJson:
                return IDS_CONFIG_SAVE_ERROR_PROCESSING_JSON;

            case svc::MidiServiceConfigSaveResult::ErrorNoConfigFileRegistered:
                return IDS_CONFIG_SAVE_ERROR_NO_CONFIG_FILE_REGISTERED;

            case svc::MidiServiceConfigSaveResult::ErrorConfigFileNotValidJson:
                return IDS_CONFIG_SAVE_ERROR_FILE_NOT_VALID_JSON;

            case svc::MidiServiceConfigSaveResult::ErrorAccessDenied:
                return IDS_CONFIG_SAVE_ERROR_ACCESS_DENIED;

            case svc::MidiServiceConfigSaveResult::ErrorConfigFileBusy:
                return IDS_CONFIG_SAVE_ERROR_FILE_BUSY;

            case svc::MidiServiceConfigSaveResult::ErrorWritingConfigFile:
                return IDS_CONFIG_SAVE_ERROR_WRITING_FILE;

            case svc::MidiServiceConfigSaveResult::ErrorVerificationFailed:
                return IDS_CONFIG_SAVE_ERROR_VERIFICATION_FAILED;

            default:
                return IDS_CONFIG_SAVE_ERROR_UNEXPECTED;
            }
        }
    }

    _Use_decl_annotations_
    void MidiServiceConfigSaveResponse::InternalSetResult(
        svc::MidiServiceConfigSaveResult const result)
    {
        m_result = result;
        m_errorMessage = result == svc::MidiServiceConfigSaveResult::Success ?
            winrt::hstring{} :
            internal::ResourceGetHString(ResourceIdForResult(result));
    }

    void MidiServiceConfigSaveResponse::InternalSetSuccess() noexcept
    {
        m_result = svc::MidiServiceConfigSaveResult::Success;
        m_errorMessage = winrt::hstring{};
    }
}
