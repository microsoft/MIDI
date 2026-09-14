// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"

#include <thread>

#include "MidiMessageForwarder.h"

#include "cmd_forward.h"
#include "console_output.h"
#include "console_table.h"
#include "endpoint_picker.h"
#include "endpoint_utility.h"
#include "midi_formatting.h"
#include "pickers.h"
#include "return_codes.h"
#include "strings.h"

namespace midi2console
{
    namespace
    {
        constexpr int KeyEscape = 27;
        constexpr uint8_t LowestGroupNumber = 1;
        constexpr uint8_t HighestGroupNumber = 16;

        struct ForwardEnd
        {
            std::string EndpointDeviceId;
            std::string EndpointName;
            uint8_t GroupIndex{ 0 };
        };

        // The id may come from the command line or the picker; the group follows whichever the
        // endpoint turned out to be, so the two cannot be resolved independently.
        bool ResolveEnd(
            _In_ std::string const& suppliedEndpointDeviceId,
            _In_ int suppliedGroupNumber,
            _In_ bool isSource,
            _Out_ ForwardEnd& end)
        {
            end = {};

            auto endpointDeviceId = suppliedEndpointDeviceId;

            // Anything not supplied has to come from a picker, so a redirected run has to be
            // told off rather than left waiting for a keypress nobody can send.
            if (endpointDeviceId.empty() || suppliedGroupNumber == 0)
            {
                if (!CanShowInteractiveUI())
                {
                    WriteErrorLine(ResourceString(IDS_FWD_ERROR_NO_INTERACTIVE));
                    return false;
                }
            }

            if (endpointDeviceId.empty())
            {
                auto const picked = PickEndpoint(ResourceString(
                    isSource ? IDS_FWD_PROMPT_SELECT_SOURCE_ENDPOINT : IDS_FWD_PROMPT_SELECT_DESTINATION_ENDPOINT));

                if (picked.Canceled)
                {
                    WriteWarningLine(ResourceString(IDS_STATUS_CANCELED));
                    return false;
                }

                endpointDeviceId = picked.EndpointDeviceId;
                end.EndpointName = picked.EndpointName;
            }
            else
            {
                end.EndpointName = GetEndpointNameFromEndpointDeviceId(endpointDeviceId);
            }

            if (midi2enum::MidiEndpointDeviceInformation::CreateFromEndpointDeviceId(
                winrt::hstring{ FromUtf8(endpointDeviceId) }) == nullptr)
            {
                WriteErrorLine(ResourceString(IDS_ERROR_ENDPOINT_NOT_FOUND));
                return false;
            }

            end.EndpointDeviceId = endpointDeviceId;

            if (suppliedGroupNumber != 0)
            {
                if (suppliedGroupNumber < LowestGroupNumber || suppliedGroupNumber > HighestGroupNumber)
                {
                    WriteErrorLine(ResourceString(IDS_FWD_ERROR_INVALID_GROUP));
                    return false;
                }

                end.GroupIndex = static_cast<uint8_t>(suppliedGroupNumber - 1);
                return true;
            }

            auto const pickedGroup = PickGroup(
                ResourceString(isSource ? IDS_FWD_PROMPT_SELECT_SOURCE_GROUP : IDS_FWD_PROMPT_SELECT_DESTINATION_GROUP),
                end.EndpointDeviceId,
                isSource);

            if (pickedGroup.Canceled)
            {
                WriteWarningLine(ResourceString(IDS_STATUS_CANCELED));
                return false;
            }

            end.GroupIndex = pickedGroup.GroupIndex;

            return true;
        }

        void WriteEndSummary(_In_ UINT labelId, _In_ ForwardEnd const& end)
        {
            WriteLine(fmt::format("{} {} {} {}",
                Styled(ResourceString(labelId), inlineLabelTextStyle),
                Styled(end.EndpointName, endpointNameTextStyle),
                Styled(ToUtf8(midi2::MidiGroup::LongLabel()), inlineLabelTextStyle),
                Styled(end.GroupIndex + 1, portNumberTextStyle)));

            WriteLine(fmt::format("  {}", Styled(end.EndpointDeviceId, endpointIdTextStyle)));
        }
    }

    int RunForwardCommand(_In_ ForwardOptions const& options)
    {
        ForwardEnd source;
        ForwardEnd destination;

        if (!ResolveEnd(options.SourceEndpointDeviceId, options.SourceGroupNumber, true, source))
        {
            return AsExitCode(ReturnCode::ErrorGeneralFailure);
        }

        if (!ResolveEnd(options.DestinationEndpointDeviceId, options.DestinationGroupNumber, false, destination))
        {
            return AsExitCode(ReturnCode::ErrorGeneralFailure);
        }

        // Forwarding a group back onto itself on the same endpoint feeds anything that loops
        // internally straight back into the source, which never settles.
        if (EqualsIgnoreCase(source.EndpointDeviceId, destination.EndpointDeviceId) &&
            source.GroupIndex == destination.GroupIndex)
        {
            WriteErrorLine(ResourceString(IDS_FWD_ERROR_SAME_ENDPOINT_AND_GROUP));
            return AsExitCode(ReturnCode::ErrorGeneralFailure);
        }

        WriteBlankLine();
        WriteEndSummary(IDS_FWD_LABEL_FROM, source);
        WriteEndSummary(IDS_FWD_LABEL_TO, destination);

        SetConsoleTitleText(FormatResourceString(IDS_FWD_CONSOLE_TITLE,
            source.EndpointName, destination.EndpointName));

        auto session = midi2::MidiSession::Create(L"MIDI Console - Forward");

        if (session == nullptr)
        {
            WriteErrorLine(ResourceString(IDS_ERROR_CREATING_SESSION));
            return AsExitCode(ReturnCode::ErrorGeneralFailure);
        }

        auto sourceConnection = session.CreateEndpointConnection(
            winrt::hstring{ FromUtf8(source.EndpointDeviceId) });

        auto destinationConnection = session.CreateEndpointConnection(
            winrt::hstring{ FromUtf8(destination.EndpointDeviceId) });

        if (sourceConnection == nullptr || destinationConnection == nullptr)
        {
            WriteErrorLine(ResourceString(IDS_ERROR_CREATING_CONNECTION));
            return AsExitCode(ReturnCode::ErrorGeneralFailure);
        }

        midiapp::MidiMessageForwarder forwarder;

        // The callback has to be registered before the source is opened, so nothing that
        // arrives on connect is missed.
        auto const startResult = forwarder.Start(sourceConnection, destinationConnection,
            { source.GroupIndex, destination.GroupIndex });

        if (FAILED(startResult))
        {
            WriteErrorLine(ResourceString(startResult == E_NOINTERFACE
                ? IDS_FWD_ERROR_NO_COM_EXTENSIONS
                : IDS_FWD_ERROR_START_FAILED));

            return AsExitCode(ReturnCode::ErrorGeneralFailure);
        }

        // Destination first, so the first message forwarded has somewhere to land.
        if (!destinationConnection.Open() || !sourceConnection.Open())
        {
            WriteErrorLine(ResourceString(IDS_ERROR_OPENING_CONNECTION));
            return AsExitCode(ReturnCode::ErrorGeneralFailure);
        }

        WriteBlankLine();
        WriteInfoLine(ResourceString(IDS_FWD_KEYS));
        WriteBlankLine();

        auto const liveStatus = StylingEnabled();

        uint64_t lastForwarded{ UINT64_MAX };

        bool keepGoing{ true };

        while (keepGoing)
        {
            // Drained rather than read one per pass, so a burst of keystrokes does not take a
            // second to clear before the Escape in it is seen.
            while (_kbhit())
            {
                if (_getch() == KeyEscape)
                {
                    keepGoing = false;
                    break;
                }
            }

            if (!keepGoing)
            {
                break;
            }

            if (liveStatus)
            {
                auto const forwarded = forwarder.MessagesForwarded();

                if (forwarded != lastForwarded)
                {
                    lastForwarded = forwarded;

                    fmt::print("\r{} {}  ",
                        Styled(ResourceString(IDS_FWD_LABEL_FORWARDED), inlineLabelTextStyle),
                        Styled(forwarded, successTextStyle));

                    std::fflush(stdout);
                }
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }

        forwarder.Stop();

        if (liveStatus)
        {
            WriteBlankLine();
        }

        WriteBlankLine();

        WriteField(ResourceString(IDS_FWD_LABEL_RECEIVED), fmt::format("{}", forwarder.MessagesReceived()), numberTextStyle);
        WriteField(ResourceString(IDS_FWD_LABEL_FORWARDED), fmt::format("{}", forwarder.MessagesForwarded()), successTextStyle);

        auto const failures = forwarder.SendFailureCount();

        if (failures > 0)
        {
            WriteField(ResourceString(IDS_FWD_LABEL_SEND_FAILURES), fmt::format("{}", failures), warningTextStyle);
        }

        return AsExitCode(ReturnCode::Success);
    }
}
