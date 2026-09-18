// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App WinRT API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "MidiDeviceInfo.h"

// Consuming the projected identity type needs its implementation header, or its property getters
// are still auto-returning and unusable here.
#include "MidiDeclaredDeviceIdentity.h"

#include "CapabilityInquiry.MidiDeviceInfo.g.cpp"

namespace winrt::Windows::Devices::Midi2::CapabilityInquiry::implementation
{
    namespace
    {
        constexpr std::wstring_view FieldManufacturerId{ L"manufacturerId" };
        constexpr std::wstring_view FieldManufacturer{ L"manufacturer" };
        constexpr std::wstring_view FieldFamilyId{ L"familyId" };
        constexpr std::wstring_view FieldFamily{ L"family" };
        constexpr std::wstring_view FieldModelId{ L"modelId" };
        constexpr std::wstring_view FieldModel{ L"model" };
        constexpr std::wstring_view FieldVersionId{ L"versionId" };
        constexpr std::wstring_view FieldVersion{ L"version" };

        winrt::hstring ReadString(
            _In_ json::JsonObject const& jsonObject,
            _In_ std::wstring_view const& field) noexcept
        {
            try
            {
                winrt::hstring const name{ field };

                if (jsonObject.HasKey(name) &&
                    jsonObject.Lookup(name).ValueType() == json::JsonValueType::String)
                {
                    return jsonObject.Lookup(name).GetString();
                }
            }
            catch (...)
            {
                LOG_CAUGHT_EXCEPTION();
            }

            return {};
        }

        // Reads up to four seven-bit bytes from a JSON array into a fixed buffer. Anything the
        // device did not send, or sent out of range, is left as zero.
        void ReadByteArray(
            _In_ json::JsonObject const& jsonObject,
            _In_ std::wstring_view const& field,
            _Out_writes_(count) uint8_t* destination,
            _In_ uint32_t const count) noexcept
        {
            for (uint32_t i = 0; i < count; i++)
            {
                destination[i] = 0;
            }

            try
            {
                winrt::hstring const name{ field };

                if (!jsonObject.HasKey(name) ||
                    jsonObject.Lookup(name).ValueType() != json::JsonValueType::Array)
                {
                    return;
                }

                auto const array = jsonObject.Lookup(name).GetArray();

                for (uint32_t i = 0; i < count && i < array.Size(); i++)
                {
                    if (array.GetAt(i).ValueType() != json::JsonValueType::Number)
                    {
                        continue;
                    }

                    auto const value = array.GetAt(i).GetNumber();

                    if (std::isfinite(value) && value >= 0.0 && value <= 127.0)
                    {
                        destination[i] = static_cast<uint8_t>(value);
                    }
                }
            }
            catch (...)
            {
                LOG_CAUGHT_EXCEPTION();
            }
        }

        void WriteByteArray(
            _In_ json::JsonObject const& jsonObject,
            _In_ std::wstring_view const& field,
            _In_reads_(count) uint8_t const* source,
            _In_ uint32_t const count) noexcept
        {
            try
            {
                json::JsonArray array{};

                for (uint32_t i = 0; i < count; i++)
                {
                    array.Append(json::JsonValue::CreateNumberValue(source[i] & 0x7F));
                }

                jsonObject.SetNamedValue(winrt::hstring{ field }, array);
            }
            catch (...)
            {
                LOG_CAUGHT_EXCEPTION();
            }
        }

        void WriteStringIfAny(
            _In_ json::JsonObject const& jsonObject,
            _In_ std::wstring_view const& field,
            _In_ winrt::hstring const& value) noexcept
        {
            try
            {
                if (!value.empty())
                {
                    jsonObject.SetNamedValue(winrt::hstring{ field },
                        json::JsonValue::CreateStringValue(value));
                }
            }
            catch (...)
            {
                LOG_CAUGHT_EXCEPTION();
            }
        }
    }

    json::JsonObject MidiDeviceInfo::GetJson() noexcept
    {
        try
        {
            json::JsonObject jsonObject{};

            // The identifier arrays are always written, even when the identity was never set. A
            // reader is entitled to find them, and three zero bytes is a well formed way of saying
            // the device did not identify itself.
            uint8_t manufacturer[3]{};
            uint8_t family[2]{};
            uint8_t model[2]{};
            uint8_t version[4]{};

            if (m_identity != nullptr)
            {
                auto const sysExId = m_identity.SystemExclusiveId();

                for (uint32_t i = 0; i < 3 && i < sysExId.size(); i++)
                {
                    manufacturer[i] = sysExId[i];
                }

                family[0] = m_identity.DeviceFamilyLsb();
                family[1] = m_identity.DeviceFamilyMsb();

                model[0] = m_identity.DeviceFamilyModelNumberLsb();
                model[1] = m_identity.DeviceFamilyModelNumberMsb();

                auto const revision = m_identity.SoftwareRevisionLevel();

                for (uint32_t i = 0; i < 4 && i < revision.size(); i++)
                {
                    version[i] = revision[i];
                }
            }

            WriteByteArray(jsonObject, FieldManufacturerId, manufacturer, 3);
            WriteStringIfAny(jsonObject, FieldManufacturer, m_manufacturer);

            WriteByteArray(jsonObject, FieldFamilyId, family, 2);
            WriteStringIfAny(jsonObject, FieldFamily, m_family);

            WriteByteArray(jsonObject, FieldModelId, model, 2);
            WriteStringIfAny(jsonObject, FieldModel, m_model);

            WriteByteArray(jsonObject, FieldVersionId, version, 4);
            WriteStringIfAny(jsonObject, FieldVersion, m_version);

            return jsonObject;
        }
        catch (...)
        {
            LOG_CAUGHT_EXCEPTION();
            return nullptr;
        }
    }

    _Use_decl_annotations_
    ci::MidiDeviceInfo MidiDeviceInfo::FromJson(json::JsonObject const& jsonObject) noexcept
    {
        auto info = winrt::make_self<implementation::MidiDeviceInfo>();

        try
        {
            if (jsonObject == nullptr)
            {
                return *info;
            }

            info->Manufacturer(ReadString(jsonObject, FieldManufacturer));
            info->Family(ReadString(jsonObject, FieldFamily));
            info->Model(ReadString(jsonObject, FieldModel));
            info->Version(ReadString(jsonObject, FieldVersion));

            uint8_t manufacturer[3]{};
            uint8_t family[2]{};
            uint8_t model[2]{};
            uint8_t version[4]{};

            ReadByteArray(jsonObject, FieldManufacturerId, manufacturer, 3);
            ReadByteArray(jsonObject, FieldFamilyId, family, 2);
            ReadByteArray(jsonObject, FieldModelId, model, 2);
            ReadByteArray(jsonObject, FieldVersionId, version, 4);

            midi2enum::MidiDeclaredDeviceIdentity identity{};

            identity.SetSystemExclusiveId(manufacturer[0], manufacturer[1], manufacturer[2]);
            identity.SetDeviceFamily(family[0], family[1]);
            identity.SetDeviceFamilyModelNumber(model[0], model[1]);
            identity.SetSoftwareRevisionLevel(version[0], version[1], version[2], version[3]);

            info->Identity(identity);
        }
        catch (...)
        {
            LOG_CAUGHT_EXCEPTION();
        }

        return *info;
    }

    winrt::hstring MidiDeviceInfo::ToString()
    {
        try
        {
            // Falls back to whichever names the device did send, so this stays useful for a device
            // that names itself only partly.
            std::wstring result{};

            for (auto const& part : { m_manufacturer, m_family, m_model })
            {
                if (part.empty()) continue;

                if (!result.empty()) result += L" ";

                result += part;
            }

            return winrt::hstring{ result };
        }
        catch (...)
        {
            LOG_CAUGHT_EXCEPTION();
            return {};
        }
    }
}
