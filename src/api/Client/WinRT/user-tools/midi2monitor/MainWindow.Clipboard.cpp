// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================
//
// Selection based clipboard commands. No protocol translation is performed: each command only
// emits messages that are already in the requested form.
//

#include "pch.h"
#include "MainWindow.xaml.h"

#include "MidiMessageDecoder.h"
#include "StringResources.h"

namespace native = ::midi2monitor;
namespace res = ::midi2monitor::resources;

namespace winrt::midi2monitor::implementation
{
    namespace
    {
        constexpr uint8_t Midi1StatusProgramChange = 0xC;
        constexpr uint8_t Midi1StatusChannelPressure = 0xD;

        constexpr uint8_t SysEx7StatusCompleteInOne = 0x0;
        constexpr uint8_t SysEx7StatusStart = 0x1;
        constexpr uint8_t SysEx7StatusContinue = 0x2;
        constexpr uint8_t SysEx7StatusEnd = 0x3;

        constexpr uint8_t SysExStartByte = 0xF0;
        constexpr uint8_t SysExEndByte = 0xF7;

        bool IsMidi1ByteConvertible(native::MessageRecord const& record) noexcept
        {
            if (record.Kind != native::RecordKind::MidiMessage || record.WordCount == 0)
            {
                return false;
            }

            auto const messageType = native::GetMessageTypeFromFirstWord(record.Words[0]);

            // MIDI 1.0 channel voice, plus system common and system real time
            return messageType == native::MessageTypeMidi1ChannelVoice32 ||
                   messageType == native::MessageTypeSystemCommon32;
        }

        bool IsSysEx7(native::MessageRecord const& record) noexcept
        {
            if (record.Kind != native::RecordKind::MidiMessage || record.WordCount < 2)
            {
                return false;
            }

            return native::GetMessageTypeFromFirstWord(record.Words[0]) == native::MessageTypeData64;
        }

        void AppendHexByte(std::wstring& target, uint8_t value)
        {
            if (!target.empty() && target.back() != L'\n')
            {
                target.append(L" ");
            }

            target.append(std::format(L"{:02X}", value));
        }

        // Number of bytes on the wire for a MIDI 1.0 message, from its status byte.
        uint32_t Midi1ByteCount(uint8_t statusByte) noexcept
        {
            if (statusByte < 0xF0)
            {
                auto const status = static_cast<uint8_t>((statusByte >> 4) & 0x0F);

                return (status == Midi1StatusProgramChange || status == Midi1StatusChannelPressure) ? 2u : 3u;
            }

            switch (statusByte)
            {
            case 0xF1:  // MIDI Time Code quarter frame
            case 0xF3:  // Song Select
                return 2;
            case 0xF2:  // Song Position Pointer
                return 3;
            default:    // Tune Request, and every System Real Time status
                return 1;
            }
        }

        std::wstring BuildUmpWordText(std::vector<native::MessageRecord> const& records)
        {
            std::wstring text{};

            for (auto const& record : records)
            {
                if (record.Kind != native::RecordKind::MidiMessage || record.WordCount == 0)
                {
                    continue;
                }

                for (uint8_t i = 0; i < record.WordCount && i < 4; i++)
                {
                    if (i > 0)
                    {
                        text.append(L" ");
                    }

                    text.append(std::format(L"{:08X}", record.Words[i]));
                }

                text.append(L"\r\n");
            }

            return text;
        }

        std::wstring BuildMidi1ByteText(std::vector<native::MessageRecord> const& records)
        {
            std::wstring text{};

            for (auto const& record : records)
            {
                if (!IsMidi1ByteConvertible(record))
                {
                    continue;
                }

                auto const word0 = record.Words[0];

                std::array<uint8_t, 3> const bytes
                {
                    static_cast<uint8_t>((word0 >> 16) & 0xFF),
                    static_cast<uint8_t>((word0 >> 8) & 0xFF),
                    static_cast<uint8_t>(word0 & 0xFF)
                };

                auto const count = Midi1ByteCount(bytes[0]);

                std::wstring line{};

                for (uint32_t i = 0; i < count; i++)
                {
                    AppendHexByte(line, bytes[i]);
                }

                text.append(line);
                text.append(L"\r\n");
            }

            return text;
        }

        // SysEx7 payload lives in the third and fourth bytes of word 0 and all four bytes of
        // word 1. A transmission split across several UMPs is joined back into one line.
        std::wstring BuildSysExByteText(std::vector<native::MessageRecord> const& records)
        {
            std::wstring text{};
            std::wstring current{};
            bool inTransmission{ false };

            for (auto const& record : records)
            {
                if (!IsSysEx7(record))
                {
                    continue;
                }

                auto const word0 = record.Words[0];
                auto const word1 = record.Words[1];

                auto const status = static_cast<uint8_t>((word0 >> 20) & 0x0F);
                auto const byteCount = std::min<uint8_t>(static_cast<uint8_t>((word0 >> 16) & 0x0F), 6);

                std::array<uint8_t, 6> const payload
                {
                    static_cast<uint8_t>((word0 >> 8) & 0xFF),
                    static_cast<uint8_t>(word0 & 0xFF),
                    static_cast<uint8_t>((word1 >> 24) & 0xFF),
                    static_cast<uint8_t>((word1 >> 16) & 0xFF),
                    static_cast<uint8_t>((word1 >> 8) & 0xFF),
                    static_cast<uint8_t>(word1 & 0xFF)
                };

                if (status == SysEx7StatusCompleteInOne || status == SysEx7StatusStart)
                {
                    current.clear();
                    AppendHexByte(current, SysExStartByte);
                    inTransmission = true;
                }

                if (!inTransmission)
                {
                    // a continue or end without its start, most likely a partial selection
                    continue;
                }

                for (uint8_t i = 0; i < byteCount; i++)
                {
                    AppendHexByte(current, payload[i]);
                }

                if (status == SysEx7StatusCompleteInOne || status == SysEx7StatusEnd)
                {
                    AppendHexByte(current, SysExEndByte);

                    text.append(current);
                    text.append(L"\r\n");

                    current.clear();
                    inTransmission = false;
                }
            }

            // an unterminated transmission is still worth copying
            if (!current.empty())
            {
                text.append(current);
                text.append(L"\r\n");
            }

            return text;
        }

        void CopyToClipboard(std::wstring const& text) noexcept
        {
            try
            {
                if (text.empty())
                {
                    return;
                }

                winrt::Windows::ApplicationModel::DataTransfer::DataPackage package{};
                package.RequestedOperation(winrt::Windows::ApplicationModel::DataTransfer::DataPackageOperation::Copy);
                package.SetText(winrt::hstring{ text });

                winrt::Windows::ApplicationModel::DataTransfer::Clipboard::SetContent(package);
                winrt::Windows::ApplicationModel::DataTransfer::Clipboard::Flush();
            }
            MIDI_MONITOR_CATCH_AND_LOG(L"Unable to place the selection on the clipboard.")
        }
    }

    std::vector<native::MessageRecord> MainWindow::SelectedRecords() noexcept
    {
        std::vector<native::MessageRecord> records{};

        try
        {
            auto const selected = MessagesListView().SelectedItems();

            records.reserve(selected.Size());

            for (auto const& item : selected)
            {
                auto const viewModel = item.try_as<midi2monitor::MidiMessageViewModel>();

                if (viewModel == nullptr)
                {
                    continue;
                }

                native::MessageRecord record{};

                if (m_pipeline.TryGetRecord(viewModel.Sequence(), record))
                {
                    records.push_back(std::move(record));
                }
            }

            // selection order follows clicks, but the clipboard should read in capture order
            std::sort(records.begin(), records.end(),
                [](native::MessageRecord const& left, native::MessageRecord const& right)
                {
                    return left.Sequence < right.Sequence;
                });
        }
        MIDI_MONITOR_CATCH_AND_LOG(L"Unable to read the selected messages.")

        return records;
    }

    _Use_decl_annotations_
    void MainWindow::OnMessagesContextFlyoutOpening(foundation::IInspectable const&, foundation::IInspectable const&)
    {
        try
        {
            auto const records = SelectedRecords();

            auto const hasAny = !records.empty();

            auto const hasMidi1 = std::any_of(records.begin(), records.end(), IsMidi1ByteConvertible);
            auto const hasSysEx = std::any_of(records.begin(), records.end(), IsSysEx7);

            CopyUmpWordsMenuItem().IsEnabled(hasAny);
            CopyMidi1BytesMenuItem().IsEnabled(hasMidi1);
            CopySysExBytesMenuItem().IsEnabled(hasSysEx);

            auto const rowCount = MessagesListView().Items().Size();

            SelectAllMenuItem().IsEnabled(rowCount > 0 && MessagesListView().SelectedItems().Size() < rowCount);
            DeselectAllMenuItem().IsEnabled(MessagesListView().SelectedItems().Size() > 0);
            ClearCaptureMenuItem().IsEnabled(rowCount > 0);
        }
        MIDI_MONITOR_CATCH_AND_LOG(L"Unable to prepare the message context menu.")
    }

    _Use_decl_annotations_
    void MainWindow::OnSelectAllClick(foundation::IInspectable const&, xaml::RoutedEventArgs const&)
    {
        // queued so the menu closes before the walk starts
        m_dispatcherQueue.TryEnqueue(
            winrt::Microsoft::UI::Dispatching::DispatcherQueuePriority::Low,
            [weak = get_weak()]()
            {
                auto strong = weak.get();

                if (strong == nullptr)
                {
                    return;
                }

                try
                {
                    strong->MessagesListView().SelectAll();
                }
                MIDI_MONITOR_CATCH_AND_LOG(L"Unable to select every message.")
            });
    }

    _Use_decl_annotations_
    void MainWindow::OnDeselectAllClick(foundation::IInspectable const&, xaml::RoutedEventArgs const&)
    {
        try
        {
            MessagesListView().SelectedItems().Clear();
        }
        MIDI_MONITOR_CATCH_AND_LOG(L"Unable to clear the selection.")
    }

    _Use_decl_annotations_
    void MainWindow::OnSelectAllSysEx7Click(foundation::IInspectable const&, xaml::RoutedEventArgs const&)
    {
        m_dispatcherQueue.TryEnqueue(
            winrt::Microsoft::UI::Dispatching::DispatcherQueuePriority::Low,
            [weak = get_weak()]()
            {
                auto strong = weak.get();

                if (strong == nullptr)
                {
                    return;
                }

                strong->SelectAllSysEx7();
            });
    }

    void MainWindow::SelectAllSysEx7() noexcept
    {
        try
        {
            if (m_listSource == nullptr)
            {
                return;
            }

            auto list = MessagesListView();
            auto const count = m_listSource->Size();

            list.SelectedItems().Clear();

            // Selected in contiguous runs. Appending item by item forces a view model per row and
            // raises a selection change each time, which took seconds on a full capture.
            uint32_t runStart{ 0 };
            uint32_t runLength{ 0 };

            for (uint32_t i = 0; i < count; i++)
            {
                native::MessageRecord record{};

                if (m_pipeline.TryGetVisibleRecord(i, record) && IsSysEx7(record))
                {
                    if (runLength == 0)
                    {
                        runStart = i;
                    }

                    runLength++;
                }
                else if (runLength > 0)
                {
                    list.SelectRange(xaml::Data::ItemIndexRange(static_cast<int32_t>(runStart), runLength));
                    runLength = 0;
                }
            }

            if (runLength > 0)
            {
                list.SelectRange(xaml::Data::ItemIndexRange(static_cast<int32_t>(runStart), runLength));
            }
        }
        MIDI_MONITOR_CATCH_AND_LOG(L"Unable to select the SysEx messages.")
    }

    _Use_decl_annotations_
    void MainWindow::OnCopyUmpWordsClick(foundation::IInspectable const&, xaml::RoutedEventArgs const&)
    {
        CopyToClipboard(BuildUmpWordText(SelectedRecords()));
    }

    _Use_decl_annotations_
    void MainWindow::OnCopyMidi1BytesClick(foundation::IInspectable const&, xaml::RoutedEventArgs const&)
    {
        CopyToClipboard(BuildMidi1ByteText(SelectedRecords()));
    }

    _Use_decl_annotations_
    void MainWindow::OnCopySysExBytesClick(foundation::IInspectable const&, xaml::RoutedEventArgs const&)
    {
        CopyToClipboard(BuildSysExByteText(SelectedRecords()));
    }

    _Use_decl_annotations_
    void MainWindow::OnMessagesContextRequested(
        xaml::UIElement const&,
        xaml::Input::ContextRequestedEventArgs const& args)
    {
        try
        {
            // right clicking a row that is not part of the selection should act on that row,
            // which is what every other Windows list does
            auto element = args.OriginalSource().try_as<xaml::DependencyObject>();

            while (element != nullptr)
            {
                if (auto const container = element.try_as<controls::ListViewItem>())
                {
                    auto const item = container.Content();
                    auto const selected = MessagesListView().SelectedItems();

                    uint32_t index{ 0 };

                    if (!selected.IndexOf(item, index))
                    {
                        selected.Clear();
                        selected.Append(item);
                    }

                    break;
                }

                element = media::VisualTreeHelper::GetParent(element);
            }
        }
        MIDI_MONITOR_CATCH_AND_LOG(L"Unable to update the selection for the context menu.")
    }
}
