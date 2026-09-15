// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#include <windows.h>

#include <atlbase.h>
#include <atlcom.h>
#include <atlctl.h>
#include <atlcoll.h>
#include <atlsync.h>

#include <atlconv.h>
#include <string>

#include <setupapi.h>
#include <initguid.h>
#include <Devpkey.h>

#include "MidiDefs.h"
#include "hstring_util.h"
#include "json_defs.h"

#include "MidiEndpointCustomizationProcessor.h"

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>

#include <Feature_Servicing_MIDI2PortNamingRework.h>

// windows.h defines this to GetObjectW, which collides with the JSON accessor
#undef GetObject

namespace json = ::winrt::Windows::Data::Json;
namespace internal = ::WindowsMidiServicesInternal;

namespace WindowsMidiServicesPluginConfigurationLib
{
    namespace
    {
        std::shared_ptr<MidiEndpointCustomProperties> ReadCustomProperties(
            _In_ json::JsonObject const& updateObject,
            _In_ bool const rejectImagePaths) noexcept
        {
            try
            {
                if (!updateObject.HasKey(MidiEndpointCustomProperties::PropertyKey))
                {
                    return nullptr;
                }

                auto const customPropsJson = updateObject.GetNamedObject(MidiEndpointCustomProperties::PropertyKey, nullptr);

                if (customPropsJson == nullptr)
                {
                    return nullptr;
                }

                if (rejectImagePaths)
                {
                    return MidiEndpointCustomProperties::FromJsonRejectingImagePath(customPropsJson);
                }

                return MidiEndpointCustomProperties::FromJson(customPropsJson);
            }
            catch (...)
            {
                return nullptr;
            }
        }

        std::shared_ptr<MidiEndpointCustomizationProvenance> ReadProvenance(
            _In_ json::JsonObject const& updateObject) noexcept
        {
            try
            {
                if (!updateObject.HasKey(MidiEndpointCustomizationProvenance::PropertyKey))
                {
                    return nullptr;
                }

                auto const provenanceJson = updateObject.GetNamedObject(MidiEndpointCustomizationProvenance::PropertyKey, nullptr);

                if (provenanceJson == nullptr)
                {
                    return nullptr;
                }

                return MidiEndpointCustomizationProvenance::FromJson(provenanceJson);
            }
            catch (...)
            {
                return nullptr;
            }
        }

        // Folds the customization's MIDI 1.0 port names into the endpoint's name table and adds
        // the resulting properties. The table is returned so it outlives the property write.
        void BuildProperties(
            _In_ MidiEndpointCustomizationApplyResult& result) noexcept
        {
            try
            {
                if (result.Properties == nullptr || result.ResolvedEndpointDeviceId.empty())
                {
                    return;
                }

                if (!result.Properties->WriteAllProperties(result.EndpointProperties))
                {
                    return;
                }

                if (result.Properties->Midi1Sources.empty() && result.Properties->Midi1Destinations.empty())
                {
                    return;
                }

                result.NameTable = WindowsMidiServicesNamingLib::MidiEndpointNameTable::FromEndpointDeviceId(
                    result.ResolvedEndpointDeviceId);

                if (result.NameTable == nullptr)
                {
                    return;
                }

                for (auto const& source : result.Properties->Midi1Sources)
                {
                    result.NameTable->UpdateSourceEntryCustomName(source.second.GroupIndex, source.second.Name);
                }

                for (auto const& destination : result.Properties->Midi1Destinations)
                {
                    result.NameTable->UpdateDestinationEntryCustomName(destination.second.GroupIndex, destination.second.Name);
                }

                result.NameTable->WriteProperties(result.EndpointProperties);

                if (Feature_Servicing_MIDI2PortNamingRework::IsEnabled())
                {
                    LOG_IF_FAILED(result.NameTable->WriteGroupTerminalBlockProperties(
                        result.ResolvedEndpointDeviceId, result.EndpointProperties));
                }
            }
            catch (...)
            {
            }
        }
    }


    _Use_decl_annotations_
    HRESULT MidiEndpointCustomizationProcessor::ProcessUpdates(
        json::JsonObject const& transportSection,
        EndpointResolver const& resolver,
        bool const rejectImagePaths,
        std::vector<MidiEndpointCustomizationApplyResult>& results) noexcept
    {
        RETURN_HR_IF_NULL(E_INVALIDARG, transportSection);
        RETURN_HR_IF_NULL(E_POINTER, m_cache);

        try
        {
            auto const updateArray = transportSection.GetNamedArray(MIDI_CONFIG_JSON_ENDPOINT_COMMON_UPDATE_KEY, nullptr);

            if (updateArray == nullptr)
            {
                return S_OK;
            }

            for (auto const& updateValue : updateArray)
            {
                if (updateValue == nullptr || updateValue.ValueType() != json::JsonValueType::Object)
                {
                    // a comment or some other stray value in the array
                    continue;
                }

                auto const updateObject = updateValue.GetObject();

                auto const matchObject = updateObject.GetNamedObject(MidiEndpointMatchCriteria::PropertyKey, nullptr);

                if (matchObject == nullptr)
                {
                    // nothing identifies which endpoint this is for
                    continue;
                }

                MidiEndpointCustomizationApplyResult result{};

                result.Match = MidiEndpointMatchCriteria::FromJson(matchObject);

                if (result.Match == nullptr)
                {
                    continue;
                }

                result.Properties = ReadCustomProperties(updateObject, rejectImagePaths);
                result.Provenance = ReadProvenance(updateObject);

                if (resolver)
                {
                    result.ResolvedEndpointDeviceId = resolver(*result.Match);
                }

                if (result.Properties != nullptr)
                {
                    // Cached whether or not it resolved. An unresolved entry is applied the moment
                    // a matching endpoint appears, which is what makes a customization survive the
                    // device being unplugged.
                    LOG_HR_IF(E_FAIL, !m_cache->AddWithProvenance(
                        result.Match,
                        result.Properties,
                        result.Provenance,
                        result.ResolvedEndpointDeviceId));

                    BuildProperties(result);
                }

                results.push_back(std::move(result));
            }

            return S_OK;
        }
        catch (...)
        {
            RETURN_HR(E_FAIL);
        }
    }


    _Use_decl_annotations_
    HRESULT MidiEndpointCustomizationProcessor::ProcessRemovals(
        json::JsonObject const& transportSection,
        EndpointResolver const& resolver,
        std::vector<MidiEndpointCustomizationApplyResult>& results) noexcept
    {
        RETURN_HR_IF_NULL(E_INVALIDARG, transportSection);
        RETURN_HR_IF_NULL(E_POINTER, m_cache);

        try
        {
            auto const removeObject = transportSection.GetNamedObject(MIDI_CONFIG_JSON_ENDPOINT_COMMON_REMOVE_KEY, nullptr);

            if (removeObject == nullptr)
            {
                return S_OK;
            }

            auto const removeArray = removeObject.GetNamedArray(MIDI_CONFIG_JSON_ENDPOINT_COMMON_UPDATE_KEY, nullptr);

            if (removeArray == nullptr)
            {
                return S_OK;
            }

            for (auto const& removeValue : removeArray)
            {
                if (removeValue == nullptr || removeValue.ValueType() != json::JsonValueType::Object)
                {
                    continue;
                }

                auto const entryObject = removeValue.GetObject();

                // Accepts the match object itself or an entry which wraps one, matching how a
                // removal is expressed in the configuration file.
                auto matchObject = entryObject.GetNamedObject(MidiEndpointMatchCriteria::PropertyKey, nullptr);

                if (matchObject == nullptr)
                {
                    matchObject = entryObject;
                }

                MidiEndpointCustomizationApplyResult result{};

                result.Match = MidiEndpointMatchCriteria::FromJson(matchObject);

                if (result.Match == nullptr)
                {
                    continue;
                }

                if (resolver)
                {
                    result.ResolvedEndpointDeviceId = resolver(*result.Match);
                }

                m_cache->Remove(*result.Match);

                // An empty set, so the endpoint goes back to what the transport supplied rather
                // than keeping values whose entry no longer exists.
                result.Properties = std::make_shared<MidiEndpointCustomProperties>();

                if (result.Properties != nullptr)
                {
                    BuildProperties(result);
                }

                results.push_back(std::move(result));
            }

            return S_OK;
        }
        catch (...)
        {
            RETURN_HR(E_FAIL);
        }
    }


    _Use_decl_annotations_
    HRESULT MidiEndpointCustomizationProcessor::WriteCustomizationsResponse(
        json::JsonObject& responseObject) noexcept
    {
        RETURN_HR_IF_NULL(E_POINTER, m_cache);

        try
        {
            json::JsonArray customizationsArray{};

            for (auto const& entry : m_cache->GetAllEntries())
            {
                if (entry == nullptr || entry->Match == nullptr || entry->Properties == nullptr)
                {
                    continue;
                }

                json::JsonObject entryObject{};

                json::JsonObject matchObject{};

                if (entry->Match->WriteJson(matchObject))
                {
                    entryObject.SetNamedValue(MidiEndpointMatchCriteria::PropertyKey, matchObject);
                }

                json::JsonObject propertiesObject{};

                if (entry->Properties->WriteJson(propertiesObject))
                {
                    entryObject.SetNamedValue(MidiEndpointCustomProperties::PropertyKey, propertiesObject);
                }

                if (entry->Provenance != nullptr)
                {
                    json::JsonObject provenanceObject{};

                    if (entry->Provenance->WriteJson(provenanceObject))
                    {
                        entryObject.SetNamedValue(MidiEndpointCustomizationProvenance::PropertyKey, provenanceObject);
                    }
                }

                entryObject.SetNamedValue(
                    MIDI_CONFIG_JSON_ENDPOINT_CUSTOMIZATION_RESOLVED_ENDPOINT_DEVICE_ID_KEY,
                    json::JsonValue::CreateStringValue(entry->ResolvedEndpointDeviceId));

                entryObject.SetNamedValue(
                    MIDI_CONFIG_JSON_ENDPOINT_CUSTOMIZATION_HAS_USER_CONTENT_KEY,
                    json::JsonValue::CreateBooleanValue(entry->Properties->HasUserContent()));

                customizationsArray.Append(entryObject);
            }

            responseObject.SetNamedValue(
                MIDI_CONFIG_JSON_ENDPOINT_CUSTOMIZATIONS_RESPONSE_ARRAY_KEY,
                customizationsArray);

            return S_OK;
        }
        catch (...)
        {
            RETURN_HR(E_FAIL);
        }
    }
}
