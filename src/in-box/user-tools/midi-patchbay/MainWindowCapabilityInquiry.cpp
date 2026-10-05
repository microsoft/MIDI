// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

// The MIDI-CI responder's and filter's settings, right in the inspector, and what a responder has
// been answering.

#include "pch.h"
#include "MainWindow.xaml.h"

#include "CiFileStore.h"
#include "PatchLibrary.h"
#include "PatchStore.h"
#include "RouteEngine.h"
#include "StringResources.h"
#include "ThemeBrushes.h"

using namespace winrt::Microsoft::UI::Xaml;

namespace patchbay = ::midipatchbay;
namespace resources = ::midipatchbay::resources;

namespace winrt::midipatchbay::implementation
{
    namespace
    {
        constexpr wchar_t NewLine = 0x0A;

        // Lines of recent activity under a responder.
        constexpr size_t ShownActivity = 6;

        // Problems listed under a file that has them.
        constexpr size_t ShownProblems = 3;

        controls::TextBlock Hint(_In_ winrt::hstring const& text) noexcept
        {
            controls::TextBlock block{};

            block.Text(text);
            block.FontSize(11);
            block.TextWrapping(xaml::TextWrapping::Wrap);
            block.Foreground(patchbay::ThemeBrushes::Current().Get(L"TextFillColorTertiaryBrush"));

            return block;
        }

        controls::TextBlock Caption() noexcept
        {
            controls::TextBlock block{};

            block.FontSize(12);
            block.TextWrapping(xaml::TextWrapping::Wrap);

            return block;
        }

        controls::TextBlock Heading(_In_ winrt::hstring const& text) noexcept
        {
            controls::TextBlock block{};

            block.Text(text);
            block.FontSize(13);
            block.FontWeight(winrt::Microsoft::UI::Text::FontWeights::SemiBold());
            block.Margin(xaml::ThicknessHelper::FromLengths(0, 6, 0, 0));

            return block;
        }

        controls::CheckBox Check(_In_ winrt::hstring const& text, _In_ bool value) noexcept
        {
            controls::CheckBox check{};

            check.Content(winrt::box_value(text));
            check.IsChecked(value);

            return check;
        }

        controls::TextBox TextField(_In_ winrt::hstring const& header, _In_ std::wstring const& text, _In_ int32_t maximumLength) noexcept
        {
            controls::TextBox box{};

            box.Header(winrt::box_value(header));
            box.Text(winrt::hstring{ text });
            box.MaxLength(maximumLength);
            box.HorizontalAlignment(xaml::HorizontalAlignment::Stretch);

            return box;
        }

        controls::NumberBox Number(
            _In_ winrt::hstring const& header,
            _In_ double minimum,
            _In_ double maximum,
            _In_ double value) noexcept
        {
            controls::NumberBox box{};

            box.Header(winrt::box_value(header));
            box.Minimum(minimum);
            box.Maximum(maximum);
            box.SmallChange(1);
            box.LargeChange(16);
            box.HorizontalAlignment(xaml::HorizontalAlignment::Stretch);
            box.SpinButtonPlacementMode(controls::NumberBoxSpinButtonPlacementMode::Inline);
            box.ValidationMode(controls::NumberBoxValidationMode::InvalidInputOverwritten);
            box.Value(value);

            return box;
        }

        // Kept when the box is left, or on Enter, never on every key. The box comes from the event,
        // so its own handlers never hold on to it.
        void OnCommit(_In_ controls::TextBox const& box, _In_ std::function<void(controls::TextBox const&)> const& commit) noexcept
        {
            try
            {
                box.LostFocus([commit](foundation::IInspectable const& sender, auto&&)
                    {
                        if (auto const source = sender.try_as<controls::TextBox>())
                        {
                            commit(source);
                        }
                    });

                box.KeyDown([commit](foundation::IInspectable const& sender, input::KeyRoutedEventArgs const& args)
                    {
                        if (args.Key() != winrt::Windows::System::VirtualKey::Enter)
                        {
                            return;
                        }

                        args.Handled(true);

                        if (auto const source = sender.try_as<controls::TextBox>())
                        {
                            commit(source);
                        }
                    });
            }
            catch (...)
            {
            }
        }

        std::wstring HexBytes(_In_reads_(count) uint8_t const* bytes, _In_ size_t count)
        {
            std::wstring text{};

            for (size_t i = 0; i < count; i++)
            {
                if (i > 0)
                {
                    text += L' ';
                }

                text += std::format(L"{:02X}", bytes[i]);
            }

            return text;
        }

        int HexDigit(_In_ wchar_t c) noexcept
        {
            if (c >= L'0' && c <= L'9') { return c - L'0'; }
            if (c >= L'a' && c <= L'f') { return c - L'a' + 10; }
            if (c >= L'A' && c <= L'F') { return c - L'A' + 10; }

            return -1;
        }

        // One byte or three, in hex: "41" is 41 00 00. Each byte is 7 bits.
        std::optional<std::array<uint8_t, 3>> ReadManufacturer(_In_ std::wstring_view text)
        {
            std::vector<int> digits{};

            for (auto const c : text)
            {
                if (c == L' ')
                {
                    continue;
                }

                auto const digit = HexDigit(c);

                if (digit < 0)
                {
                    return std::nullopt;
                }

                digits.push_back(digit);
            }

            if (digits.size() != 2 && digits.size() != 6)
            {
                return std::nullopt;
            }

            std::array<uint8_t, 3> bytes{};

            for (size_t i = 0; i < digits.size() / 2; i++)
            {
                auto const value = digits[i * 2] * 16 + digits[i * 2 + 1];

                if (value > 0x7F)
                {
                    return std::nullopt;
                }

                bytes[i] = static_cast<uint8_t>(value);
            }

            return bytes;
        }

        std::wstring VersionText(_In_ std::array<uint8_t, 4> const& version)
        {
            return std::format(L"{}.{}.{}.{}", version[0], version[1], version[2], version[3]);
        }

        // "1", "1.2" or "1.2.3.4", each part 0 to 127. Parts left off are 0.
        std::optional<std::array<uint8_t, 4>> ReadVersion(_In_ std::wstring_view text)
        {
            std::array<uint8_t, 4> version{};
            size_t part{ 0 };
            int value{ -1 };

            for (auto const c : text)
            {
                if (c == L' ')
                {
                    continue;
                }

                if (c == L'.')
                {
                    if (value < 0 || part >= version.size() - 1)
                    {
                        return std::nullopt;
                    }

                    version[part++] = static_cast<uint8_t>(value);
                    value = -1;
                    continue;
                }

                if (c < L'0' || c > L'9')
                {
                    return std::nullopt;
                }

                value = (value < 0 ? 0 : value) * 10 + (c - L'0');

                if (value > 127)
                {
                    return std::nullopt;
                }
            }

            if (value < 0)
            {
                return std::nullopt;
            }

            version[part] = static_cast<uint8_t>(value);

            return version;
        }

        std::wstring MuidText(_In_ uint32_t muid)
        {
            return std::format(L"0x{:07X}", muid);
        }

        // In the customer's own time format.
        std::wstring TimeText(_In_ std::chrono::system_clock::time_point time)
        {
            auto const ticks = std::chrono::duration_cast<std::chrono::duration<int64_t, std::ratio<1, 10'000'000>>>(
                time.time_since_epoch()).count() + 116444736000000000LL;

            FILETIME utc{};
            utc.dwLowDateTime = static_cast<DWORD>(ticks & 0xFFFFFFFF);
            utc.dwHighDateTime = static_cast<DWORD>(static_cast<uint64_t>(ticks) >> 32);

            SYSTEMTIME universal{};
            SYSTEMTIME local{};

            if (!::FileTimeToSystemTime(&utc, &universal) || !::SystemTimeToTzSpecificLocalTime(nullptr, &universal, &local))
            {
                return {};
            }

            wchar_t buffer[64]{};

            if (::GetTimeFormatEx(LOCALE_NAME_USER_DEFAULT, 0, &local, nullptr, buffer, ARRAYSIZE(buffer)) == 0)
            {
                return {};
            }

            return buffer;
        }

        winrt::hstring OutcomeText(_In_ patchbay::CiOutcome outcome) noexcept
        {
            switch (outcome)
            {
            case patchbay::CiOutcome::Refused: return resources::GetString(L"CiOutcomeRefused");
            case patchbay::CiOutcome::NewMuid: return resources::GetString(L"CiOutcomeNewMuid");
            default:                           return resources::GetString(L"CiOutcomeAnswered");
            }
        }
    }

    _Use_decl_annotations_
    void MainWindow::BuildCiResponderSettings(patchbay::PatchBlock const& block, controls::StackPanel const& body) noexcept
    {
        try
        {
            auto const blockId = block.Id;
            auto const& responder = block.Settings.CiResponder;
            auto weak = get_weak();

            auto const change = [weak, blockId](std::function<void(patchbay::BlockSettings&)> const& apply)
                -> std::optional<patchbay::BlockSettings>
                {
                    auto strong = weak.get();

                    if (strong == nullptr || strong->m_updatingStepSettings)
                    {
                        return std::nullopt;
                    }

                    return strong->ChangeStepSettings(blockId, apply);
                };

            // ------------------------------------------------- who it says it is
            body.Children().Append(Heading(resources::GetString(L"CiIdentityHeading")));
            body.Children().Append(Hint(resources::GetString(L"CiIdentityHint")));

            auto manufacturer = TextField(resources::GetString(L"CiManufacturer"),
                HexBytes(responder.Manufacturer.data(), responder.Manufacturer.size()), 16);

            auto manufacturerText = Caption();
            manufacturerText.Text(resources::GetString(L"CiManufacturerHint"));

            OnCommit(manufacturer, [weak, change, manufacturerText](controls::TextBox const& box)
                {
                    auto const read = ReadManufacturer(std::wstring_view{ box.Text() });

                    if (!read.has_value())
                    {
                        manufacturerText.Text(resources::GetString(L"CiManufacturerNotUnderstood"));
                        return;
                    }

                    manufacturerText.Text(resources::GetString(L"CiManufacturerHint"));

                    auto const bytes = *read;

                    change([bytes](patchbay::BlockSettings& s) { s.CiResponder.Manufacturer = bytes; });

                    if (auto strong = weak.get())
                    {
                        strong->m_updatingStepSettings = true;
                        box.Text(winrt::hstring{ HexBytes(bytes.data(), bytes.size()) });
                        strong->m_updatingStepSettings = false;
                    }
                });

            body.Children().Append(manufacturer);
            body.Children().Append(manufacturerText);

            controls::Grid numbers{};
            numbers.ColumnSpacing(8);
            numbers.ColumnDefinitions().Append(controls::ColumnDefinition{});
            numbers.ColumnDefinitions().Append(controls::ColumnDefinition{});

            auto family = Number(resources::GetString(L"CiFamily"), 0, 16383, responder.Family);
            auto model = Number(resources::GetString(L"CiModel"), 0, 16383, responder.Model);
            controls::Grid::SetColumn(model, 1);

            for (auto const isFamily : { true, false })
            {
                (isFamily ? family : model).ValueChanged([change, isFamily](auto&&, controls::NumberBoxValueChangedEventArgs const& args)
                    {
                        if (std::isnan(args.NewValue()))
                        {
                            return;
                        }

                        auto const value = static_cast<uint16_t>(std::lround(std::clamp(args.NewValue(), 0.0, 16383.0)));

                        change([isFamily, value](patchbay::BlockSettings& s)
                            {
                                (isFamily ? s.CiResponder.Family : s.CiResponder.Model) = value;
                            });
                    });
            }

            numbers.Children().Append(family);
            numbers.Children().Append(model);
            body.Children().Append(numbers);

            auto version = TextField(resources::GetString(L"CiVersion"), VersionText(responder.Version), 16);
            version.PlaceholderText(L"1.0.0.0");

            auto versionText = Caption();
            versionText.Visibility(xaml::Visibility::Collapsed);

            OnCommit(version, [weak, change, versionText](controls::TextBox const& box)
                {
                    auto const read = ReadVersion(std::wstring_view{ box.Text() });

                    if (!read.has_value())
                    {
                        versionText.Text(resources::GetString(L"CiVersionNotUnderstood"));
                        versionText.Visibility(xaml::Visibility::Visible);
                        return;
                    }

                    versionText.Text({});
                    versionText.Visibility(xaml::Visibility::Collapsed);

                    auto const value = *read;

                    change([value](patchbay::BlockSettings& s) { s.CiResponder.Version = value; });

                    if (auto strong = weak.get())
                    {
                        strong->m_updatingStepSettings = true;
                        box.Text(winrt::hstring{ VersionText(value) });
                        strong->m_updatingStepSettings = false;
                    }
                });

            body.Children().Append(version);
            body.Children().Append(versionText);

            auto instance = TextField(resources::GetString(L"CiProductInstanceId"), responder.ProductInstanceId, 42);

            OnCommit(instance, [weak, change](controls::TextBox const& box)
                {
                    auto const value = patchbay::CiProductInstanceIdFrom(std::wstring_view{ box.Text() });

                    change([value](patchbay::BlockSettings& s) { s.CiResponder.ProductInstanceId = value; });

                    if (auto strong = weak.get())
                    {
                        strong->m_updatingStepSettings = true;
                        box.Text(winrt::hstring{ value });
                        strong->m_updatingStepSettings = false;
                    }
                });

            body.Children().Append(instance);
            body.Children().Append(Hint(resources::GetString(L"CiProductInstanceIdHint")));

            // ------------------------------------------------- what it answers
            body.Children().Append(Heading(resources::GetString(L"CiAnswersHeading")));

            auto report = Check(resources::GetString(L"CiProcessInquiry"), responder.ProcessInquiry);

            auto const setReport = [change](bool on)
                {
                    change([on](patchbay::BlockSettings& s) { s.CiResponder.ProcessInquiry = on; });
                };

            report.Checked([setReport](auto&&, auto&&) { setReport(true); });
            report.Unchecked([setReport](auto&&, auto&&) { setReport(false); });

            body.Children().Append(report);
            body.Children().Append(Hint(resources::GetString(L"CiProcessInquiryHint")));

            auto pass = Check(resources::GetString(L"CiPassMidiCi"), responder.PassMidiCi);

            auto const setPass = [change](bool on)
                {
                    change([on](patchbay::BlockSettings& s) { s.CiResponder.PassMidiCi = on; });
                };

            pass.Checked([setPass](auto&&, auto&&) { setPass(true); });
            pass.Unchecked([setPass](auto&&, auto&&) { setPass(false); });

            body.Children().Append(pass);
            body.Children().Append(Hint(resources::GetString(L"CiPassMidiCiHint")));

            // ------------------------------------------------- profiles and properties
            body.Children().Append(Heading(resources::GetString(L"CiFileHeading")));

            auto file = TextField(resources::GetString(L"CiFileName"), responder.FileName, 200);
            file.PlaceholderText(resources::GetString(L"CiFileNamePlaceholder"));

            auto fileText = Caption();
            fileText.Visibility(xaml::Visibility::Collapsed);

            OnCommit(file, [weak, change, fileText](controls::TextBox const& box)
                {
                    auto name = std::wstring{ box.Text() };

                    // Spaces around a name are never part of it.
                    auto const first = name.find_first_not_of(L' ');
                    auto const last = name.find_last_not_of(L' ');
                    name = first == std::wstring::npos ? std::wstring{} : name.substr(first, last - first + 1);

                    if (!name.empty() && !patchbay::IsCiFileName(name))
                    {
                        fileText.Text(resources::GetString(L"CiFileNameNotPlain"));
                        fileText.Visibility(xaml::Visibility::Visible);
                        return;
                    }

                    fileText.Text({});
                    fileText.Visibility(xaml::Visibility::Collapsed);

                    change([name](patchbay::BlockSettings& s) { s.CiResponder.FileName = name; });

                    if (auto strong = weak.get())
                    {
                        strong->UpdateInspectorActivity();
                    }
                });

            body.Children().Append(file);
            body.Children().Append(fileText);

            controls::StackPanel buttons{};
            buttons.Orientation(controls::Orientation::Horizontal);
            buttons.Spacing(8);

            controls::Button choose{};
            choose.Content(winrt::box_value(resources::GetString(L"CiChooseFile")));

            choose.Click([weak, blockId, file](auto&&, auto&&)
                {
                    if (auto strong = weak.get())
                    {
                        strong->ChooseCiFile(blockId, file);
                    }
                });

            controls::Button folder{};
            folder.Content(winrt::box_value(resources::GetString(L"CiOpenFolder")));

            folder.Click([](auto&&, auto&&)
                {
                    patchbay::PatchStore::Current().ShowFolder();
                });

            buttons.Children().Append(choose);
            buttons.Children().Append(folder);

            body.Children().Append(buttons);
            body.Children().Append(Hint(resources::GetString(L"CiFileHint")));

            m_stepSettingsFocus = manufacturer;
        }
        catch (...)
        {
            m_updatingStepSettings = false;
            MIDI_PATCHBAY_LOG_GENERAL_EXCEPTION(L"Unable to build the MIDI-CI responder's settings.");
        }
    }

    _Use_decl_annotations_
    void MainWindow::BuildCiFilterSettings(patchbay::PatchBlock const& block, controls::StackPanel const& body) noexcept
    {
        try
        {
            auto const blockId = block.Id;
            auto const& filter = block.Settings.CiFilter;
            auto weak = get_weak();

            auto const change = [weak, blockId](std::function<void(patchbay::BlockSettings&)> const& apply)
                -> std::optional<patchbay::BlockSettings>
                {
                    auto strong = weak.get();

                    if (strong == nullptr || strong->m_updatingStepSettings)
                    {
                        return std::nullopt;
                    }

                    return strong->ChangeStepSettings(blockId, apply);
                };

            controls::RadioButtons action{};

            action.Header(winrt::box_value(resources::GetString(L"FilterActionHeader")));
            action.Items().Append(winrt::box_value(resources::GetString(L"CiFilterKeepOut")));
            action.Items().Append(winrt::box_value(resources::GetString(L"CiFilterLetThrough")));
            action.SelectedIndex(filter.Action == patchbay::FilterAction::LetThrough ? 1 : 0);

            action.SelectionChanged([change](foundation::IInspectable const& sender, auto&&)
                {
                    auto const list = sender.try_as<controls::RadioButtons>();

                    if (list == nullptr || list.SelectedIndex() < 0)
                    {
                        return;
                    }

                    auto const letThrough = list.SelectedIndex() == 1;

                    change([letThrough](patchbay::BlockSettings& s)
                        {
                            s.CiFilter.Action = letThrough ? patchbay::FilterAction::LetThrough : patchbay::FilterAction::KeepOut;
                        });
                });

            body.Children().Append(action);

            controls::TextBlock which{};
            which.Text(resources::GetString(L"CiFilterWhich"));
            which.FontSize(12);
            body.Children().Append(which);

            struct Category
            {
                wchar_t const* Key;
                uint8_t Bit;
            };

            for (auto const& category : {
                Category{ L"CiCategoryManagement", patchbay::CiCategoryManagement },
                Category{ L"CiCategoryProfiles", patchbay::CiCategoryProfiles },
                Category{ L"CiCategoryPropertyExchange", patchbay::CiCategoryPropertyExchange },
                Category{ L"CiCategoryProcessInquiry", patchbay::CiCategoryProcessInquiry } })
            {
                auto check = Check(resources::GetString(category.Key), (filter.Categories & category.Bit) != 0);
                auto const bit = category.Bit;

                auto const set = [change, bit](bool on)
                    {
                        change([bit, on](patchbay::BlockSettings& s)
                            {
                                s.CiFilter.Categories = static_cast<uint8_t>(on ? (s.CiFilter.Categories | bit) : (s.CiFilter.Categories & ~bit));
                            });
                    };

                check.Checked([set](auto&&, auto&&) { set(true); });
                check.Unchecked([set](auto&&, auto&&) { set(false); });

                body.Children().Append(check);
            }

            body.Children().Append(Hint(resources::GetString(L"CiFilterHint")));

            m_stepSettingsFocus = action;
        }
        catch (...)
        {
            m_updatingStepSettings = false;
            MIDI_PATCHBAY_LOG_GENERAL_EXCEPTION(L"Unable to build the MIDI-CI filter's settings.");
        }
    }

    _Use_decl_annotations_
    winrt::hstring MainWindow::CiStatusText(std::wstring const& blockId) noexcept
    {
        try
        {
            auto const* patch = CurrentPatch();
            auto const* block = patch == nullptr ? nullptr : patch->FindBlock(blockId);

            if (block == nullptr || block->Kind != patchbay::BlockKind::CiResponder)
            {
                return {};
            }

            auto const& responder = block->Settings.CiResponder;

            std::wstring text{};

            // ------------------------------------------------- the file
            if (responder.FileName.empty())
            {
                text = resources::GetString(L"CiFileNone");
            }
            else
            {
                auto const state = patchbay::CiFileStore::Current().Read(responder.FileName);

                if (!state.Found)
                {
                    text = resources::FormatString(L"CiFileMissingFormat", responder.FileName);
                }
                else if (state.Description == nullptr)
                {
                    text = resources::FormatString(L"CiFileUnreadableFormat", responder.FileName);
                }
                else
                {
                    auto const& description = *state.Description;

                    auto const profiles = static_cast<int>(description.Profiles.size());
                    auto const properties = static_cast<int>(description.Resources.size() + (description.HasDeviceInfo ? 1 : 0));

                    text = resources::FormatString(L"CiFileReadFormat", responder.FileName, profiles, properties);
                }

                if (!state.Problems.empty())
                {
                    text += NewLine;
                    text += resources::FormatString(L"CiFileProblemsFormat", static_cast<int>(state.Problems.size()));

                    for (size_t i = 0; i < state.Problems.size() && i < ShownProblems; i++)
                    {
                        text += NewLine;
                        text += patchbay::DescribeCiFileProblem(state.Problems[i]);
                    }
                }
            }

            // ------------------------------------------------- what it has been answering
            if (block->Bypassed || !patchbay::PatchLibrary::Current().IsRouting(m_patchKey))
            {
                return winrt::hstring{ text };
            }

            auto const status = patchbay::RouteEngine::Current().CiResponderStatus(m_patchKey + L'|' + blockId);

            if (!status.has_value())
            {
                return winrt::hstring{ text };
            }

            text += NewLine;
            text += NewLine;
            text += resources::FormatString(L"CiMuidFormat", MuidText(status->Muid));

            if (status->Recent.empty())
            {
                text += NewLine;
                text += resources::GetString(L"CiNothingAskedYet");
            }

            for (size_t i = 0; i < status->Recent.size() && i < ShownActivity; i++)
            {
                auto const& entry = status->Recent[i];

                text += NewLine;
                text += resources::FormatString(L"CiActivityFormat",
                    TimeText(entry.Time),
                    patchbay::DescribeCiMessage(entry.MessageType),
                    MuidText(entry.InitiatorMuid),
                    OutcomeText(entry.Outcome));
            }

            return winrt::hstring{ text };
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to describe the MIDI-CI step.")

        return {};
    }

    _Use_decl_annotations_
    void MainWindow::ChooseCiFile(std::wstring const& blockId, controls::TextBox const& nameBox) noexcept
    {
        try
        {
            auto& store = patchbay::PatchStore::Current();

            if (!store.EnsureFolder())
            {
                return;
            }

            // The Win32 common item dialog, never Windows.Storage.Pickers, which needs a package
            // identity this unpackaged app does not have.
            auto dialog = wil::CoCreateInstance<IFileOpenDialog>(CLSID_FileOpenDialog);

            auto const ciFilter = resources::GetString(L"CiFileFilter");
            auto const jsonFilter = resources::GetString(L"CiFileFilterJson");

            COMDLG_FILTERSPEC const filters[]
            {
                { ciFilter.c_str(), L"*.midici" },
                { jsonFilter.c_str(), L"*.json" },
            };

            dialog->SetFileTypes(ARRAYSIZE(filters), filters);
            dialog->SetTitle(resources::GetString(L"CiChooseFileTitle").c_str());

            winrt::com_ptr<IShellItem> folder{};

            if (SUCCEEDED(::SHCreateItemFromParsingName(store.FolderPath().c_str(), nullptr, IID_PPV_ARGS(folder.put()))))
            {
                dialog->SetFolder(folder.get());
            }

            if (FAILED(dialog->Show(m_chrome.WindowHandle())))
            {
                return;
            }

            winrt::com_ptr<IShellItem> item{};

            if (FAILED(dialog->GetResult(item.put())) || item == nullptr)
            {
                return;
            }

            wil::unique_cotaskmem_string path{};

            if (FAILED(item->GetDisplayName(SIGDN_FILESYSPATH, &path)) || path.get() == nullptr)
            {
                return;
            }

            std::filesystem::path const picked{ path.get() };
            std::filesystem::path const patches{ store.FolderPath() };

            std::error_code ec{};
            auto name = picked.filename().wstring();

            // A file from somewhere else is copied in, under a name nothing there has.
            if (!std::filesystem::equivalent(picked.parent_path(), patches, ec))
            {
                ec.clear();

                auto const size = std::filesystem::file_size(picked, ec);

                if (ec || size > patchbay::MaximumCiFileBytes)
                {
                    ShowStatus(resources::GetString(L"CiFileTooLargeToCopy"), controls::InfoBarSeverity::Error);
                    return;
                }

                auto target = patches / name;

                for (int i = 2; std::filesystem::exists(target, ec) && i < 100; i++)
                {
                    target = patches / (picked.stem().wstring() + L" (" + std::to_wstring(i) + L")" + picked.extension().wstring());
                }

                if (std::filesystem::exists(target, ec) || !std::filesystem::copy_file(picked, target, ec) || ec)
                {
                    ShowStatus(resources::GetString(L"CiFileCopyFailed"), controls::InfoBarSeverity::Error);
                    return;
                }

                name = target.filename().wstring();
            }

            if (!patchbay::IsCiFileName(name))
            {
                ShowStatus(resources::GetString(L"CiFileNameNotPlain"), controls::InfoBarSeverity::Error);
                return;
            }

            ChangeStepSettings(blockId, [name](patchbay::BlockSettings& s) { s.CiResponder.FileName = name; });

            m_updatingStepSettings = true;
            nameBox.Text(winrt::hstring{ name });
            m_updatingStepSettings = false;

            UpdateInspectorActivity();
        }
        catch (...)
        {
            m_updatingStepSettings = false;
            MIDI_PATCHBAY_LOG_GENERAL_EXCEPTION(L"Unable to choose a MIDI-CI file.");
        }
    }
}
