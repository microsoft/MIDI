// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

// History: the answers the customer copied, used or pinned. Laid out as
// src\prototypes\midi-calculator\design\8-history.html: search, Save as text and Clear beside the
// page title; Pinned and Today on the left, earlier days on the right.

#include "pch.h"
#include "MainWindow.xaml.h"

#include "CalcHistory.h"
#include "StringResources.h"

namespace native = ::midicalculator;
namespace res = ::midicalculator::resources;
namespace ui = ::midicalculator::ui;

namespace winrt::midicalculator::implementation
{
    namespace
    {
        bool Contains(std::wstring const& haystack, std::wstring const& needle) noexcept
        {
            if (needle.empty())
            {
                return true;
            }

            return ::FindNLSStringEx(LOCALE_NAME_USER_DEFAULT, FIND_FROMSTART | LINGUISTIC_IGNORECASE,
                haystack.c_str(), static_cast<int>(haystack.size()), needle.c_str(), static_cast<int>(needle.size()),
                nullptr, nullptr, nullptr, 0) >= 0;
        }

        bool LocalTime(int64_t fileTime, SYSTEMTIME& local) noexcept
        {
            FILETIME utc{ static_cast<DWORD>(static_cast<uint64_t>(fileTime) & 0xFFFFFFFF), static_cast<DWORD>(static_cast<uint64_t>(fileTime) >> 32) };
            SYSTEMTIME utcTime{};
            return ::FileTimeToSystemTime(&utc, &utcTime) && ::SystemTimeToTzSpecificLocalTimeEx(nullptr, &utcTime, &local);
        }

        std::wstring DateText(SYSTEMTIME const& local, DWORD flags)
        {
            wchar_t date[96]{};
            ::GetDateFormatEx(LOCALE_NAME_USER_DEFAULT, flags, &local, nullptr, date, ARRAYSIZE(date), nullptr);
            return date;
        }

        std::wstring TimeText(SYSTEMTIME const& local)
        {
            wchar_t time[64]{};
            ::GetTimeFormatEx(LOCALE_NAME_USER_DEFAULT, TIME_NOSECONDS, &local, nullptr, time, ARRAYSIZE(time));
            return time;
        }

        // days since 1601, local time, for grouping by day
        int64_t LocalDay(SYSTEMTIME const& local) noexcept
        {
            SYSTEMTIME day{ local };
            day.wHour = day.wMinute = day.wSecond = day.wMilliseconds = 0;
            FILETIME file{};
            ::SystemTimeToFileTime(&day, &file);
            return static_cast<int64_t>(((static_cast<uint64_t>(file.dwHighDateTime) << 32) | file.dwLowDateTime) / (10'000'000ull * 86400ull));
        }

        media::Brush Brush(wchar_t const* key)
        {
            return xaml::Application::Current().Resources().Lookup(winrt::box_value(key)).as<media::Brush>();
        }

        controls::Button IconButton(wchar_t const* glyph, winrt::hstring const& name)
        {
            controls::Button button{};
            button.Padding(xaml::Thickness{ 6, 4, 6, 4 });
            button.Background(media::SolidColorBrush{ winrt::Microsoft::UI::Colors::Transparent() });
            button.BorderThickness(xaml::Thickness{ 0 });
            button.VerticalAlignment(xaml::VerticalAlignment::Center);

            controls::FontIcon icon{};
            icon.Glyph(glyph);
            icon.FontSize(12);
            button.Content(icon);

            winrt::Microsoft::UI::Xaml::Automation::AutomationProperties::SetName(button, name);
            controls::ToolTipService::SetToolTip(button, winrt::box_value(name));
            return button;
        }
    }

    void MainWindow::BuildHistoryPage()
    {
        // ---- beside the page title ----
        m_historyTools = controls::StackPanel{};
        m_historyTools.Orientation(controls::Orientation::Horizontal);
        m_historyTools.Spacing(8);
        m_historyTools.HorizontalAlignment(xaml::HorizontalAlignment::Right);
        m_historyTools.VerticalAlignment(xaml::VerticalAlignment::Center);
        m_historyTools.Margin(xaml::Thickness{ 0, 10, 24, 6 });
        m_historyTools.Visibility(xaml::Visibility::Collapsed);

        m_historySearch = controls::TextBox{};
        m_historySearch.Width(260);
        m_historySearch.PlaceholderText(res::GetString(L"HistorySearchPlaceholder"));
        m_historySearch.IsSpellCheckEnabled(false);
        winrt::Microsoft::UI::Xaml::Automation::AutomationProperties::SetName(m_historySearch, res::GetString(L"HistorySearchLabel"));
        m_historySearch.TextChanged([weak = get_weak()](auto&&, auto&&) { if (auto s = weak.get()) s->RefreshHistory(); });

        auto save = ui::MakeButton(res::GetString(L"HistorySave"), L"\uEDE1", false);
        save.Click([weak = get_weak()](auto&&, auto&&) { if (auto s = weak.get()) s->SaveHistoryAsText(); });

        auto clear = ui::MakeButton(res::GetString(L"HistoryClear"), L"\uE74D", false);
        clear.Background(media::SolidColorBrush{ winrt::Microsoft::UI::Colors::Transparent() });
        clear.BorderThickness(xaml::Thickness{ 0 });
        controls::ToolTipService::SetToolTip(clear, winrt::box_value(res::GetString(L"HistoryClearTip")));
        clear.Click([](auto&&, auto&&) { native::History::Current().Clear(); });

        m_historyTools.Children().Append(m_historySearch);
        m_historyTools.Children().Append(save);
        m_historyTools.Children().Append(clear);
        controls::Grid::SetRow(m_historyTools, 0);
        ContentGrid().Children().Append(m_historyTools);

        // ---- the lists ----
        controls::StackPanel layout{};
        layout.Spacing(12);

        m_historyEmpty = ui::MakeHelp(res::GetString(L"HistoryEmpty"));
        layout.Children().Append(m_historyEmpty);

        m_historyList = controls::StackPanel{};
        layout.Children().Append(m_historyList);

        HistoryPage().Children().Append(layout);
        RefreshHistory();
    }

    void MainWindow::RefreshHistory() noexcept
    {
        try
        {
            if (!m_historyList)
            {
                return;
            }

            m_historyTools.Visibility(m_page == native::CalculatorPage::History ? xaml::Visibility::Visible : xaml::Visibility::Collapsed);

            if (m_page != native::CalculatorPage::History)
            {
                return;
            }

            m_historyList.Children().Clear();

            // a fresh set of columns each time, so cards for days that are gone go with them
            auto columns = std::make_shared<ui::CardColumns>();

            std::wstring const search{ m_historySearch.Text() };
            auto const& entries = native::History::Current().Entries();

            SYSTEMTIME nowLocal{};
            ::GetLocalTime(&nowLocal);
            auto const today = LocalDay(nowLocal);

            // the cards, in order: Pinned, Today, then one for each earlier day
            struct Group
            {
                winrt::hstring Title{};
                int32_t Column{ 0 };
                std::vector<size_t> Items{};
            };

            std::vector<Group> groups{};
            groups.push_back({ res::GetString(L"HistoryPinned"), 0, {} });
            groups.push_back({ res::GetString(L"HistoryToday"), 0, {} });
            std::vector<int64_t> days{};

            size_t shown{ 0 };

            for (size_t index = 0; index < entries.size(); ++index)
            {
                auto const& entry = entries[index];

                if (!Contains(entry.Answer, search) && !Contains(entry.Question, search) && !Contains(entry.Page, search))
                {
                    continue;
                }

                ++shown;

                if (entry.Pinned)
                {
                    groups[0].Items.push_back(index);
                    continue;
                }

                SYSTEMTIME local{};

                if (!LocalTime(entry.FileTime, local) || LocalDay(local) >= today)
                {
                    groups[1].Items.push_back(index);
                    continue;
                }

                auto const day = LocalDay(local);
                auto const found = std::find(days.begin(), days.end(), day);

                if (found == days.end())
                {
                    days.push_back(day);
                    groups.push_back({ day == today - 1 ? res::GetString(L"HistoryYesterday") : winrt::hstring{ DateText(local, DATE_LONGDATE) }, 1, {} });
                    groups.back().Items.push_back(index);
                }
                else
                {
                    groups[2 + static_cast<size_t>(found - days.begin())].Items.push_back(index);
                }
            }

            for (auto const& group : groups)
            {
                if (group.Items.empty())
                {
                    continue;
                }

                auto card = ui::MakeCard(group.Title, nullptr);
                card.Body.Spacing(0);

                for (auto const index : group.Items)
                {
                    auto const& entry = entries[index];

                    controls::Grid row{};
                    row.Padding(xaml::Thickness{ 0, 8, 0, 8 });
                    row.BorderBrush(Brush(L"DividerStrokeColorDefaultBrush"));
                    row.BorderThickness(xaml::Thickness{ 0, 1, 0, 0 });
                    row.ColumnSpacing(4);

                    controls::ColumnDefinition words{};
                    words.Width(xaml::GridLengthHelper::FromValueAndType(1, xaml::GridUnitType::Star));
                    row.ColumnDefinitions().Append(words);

                    for (int i = 0; i < 2; ++i)
                    {
                        controls::ColumnDefinition button{};
                        button.Width(xaml::GridLengthHelper::Auto());
                        row.ColumnDefinitions().Append(button);
                    }

                    SYSTEMTIME local{};
                    auto const when = LocalTime(entry.FileTime, local) ? TimeText(local) : std::wstring{};

                    controls::StackPanel text{};
                    text.Spacing(2);

                    controls::TextBlock where{};
                    where.Text(winrt::hstring{ when.empty() ? entry.Page : std::format(L"{} \u00B7 {}", entry.Page, when) });
                    where.FontSize(11);
                    where.Foreground(Brush(L"AccentTextFillColorPrimaryBrush"));

                    controls::TextBlock question{};
                    question.Text(entry.Question);
                    question.FontSize(12);
                    question.TextWrapping(xaml::TextWrapping::Wrap);
                    question.Foreground(Brush(L"TextFillColorTertiaryBrush"));

                    controls::TextBlock answer{};
                    answer.Text(entry.Answer);
                    answer.IsTextSelectionEnabled(true);
                    answer.TextWrapping(xaml::TextWrapping::Wrap);
                    answer.FontWeight(winrt::Windows::UI::Text::FontWeights::SemiBold());

                    text.Children().Append(where);
                    text.Children().Append(question);
                    text.Children().Append(answer);
                    row.Children().Append(text);

                    auto const answerText = winrt::hstring{ entry.Answer };
                    auto copy = IconButton(L"\uE8C8", res::FormatString(L"CopyNameFormat", std::wstring_view{ answerText }));
                    copy.Click([weak = get_weak(), answerText](auto&&, auto&&)
                        {
                            if (auto s = weak.get())
                            {
                                ui::CopyText(answerText);
                                s->ShowStatus(res::FormatString(L"StatusCopied", std::wstring_view{ answerText }));
                            }
                        });
                    controls::Grid::SetColumn(copy, 1);
                    row.Children().Append(copy);

                    auto pin = IconButton(entry.Pinned ? L"\uE840" : L"\uE718", res::GetString(entry.Pinned ? L"HistoryUnpin" : L"HistoryPin"));
                    pin.Click([index](auto&&, auto&&) { native::History::Current().TogglePin(index); });
                    controls::Grid::SetColumn(pin, 2);
                    row.Children().Append(pin);

                    card.Body.Children().Append(row);
                }

                columns->Add(card, group.Column);
            }

            // the note about what's kept goes under the right column, as in the comp
            auto note = ui::MakeCard(L"", nullptr);
            note.Title.Visibility(xaml::Visibility::Collapsed);
            note.Body.Children().Append(ui::MakeHelp(res::FormatString(L"HistoryHelp", ::midicalculator::History::MaximumEntries)));
            note.Root.Background(media::SolidColorBrush{ winrt::Microsoft::UI::Colors::Transparent() });
            note.Root.BorderThickness(xaml::Thickness{ 0 });
            note.Root.Padding(xaml::Thickness{ 2, 0, 2, 0 });

            if (shown > 0)
            {
                columns->Add(note, 1);
            }

            columns->Layout(HistoryPage().ActualWidth());
            m_historyList.Children().Append(columns->Root());
            m_historyColumns = columns;

            m_historyEmpty.Visibility(shown == 0 ? xaml::Visibility::Visible : xaml::Visibility::Collapsed);
            m_historyEmpty.Text(entries.empty() ? res::GetString(L"HistoryEmpty") : res::GetString(L"HistoryNoMatch"));
        }
        MIDI_CALCULATOR_CATCH_AND_LOG(L"Unable to show the history.")
    }

    void MainWindow::SaveHistoryAsText() noexcept
    {
        try
        {
            // the Win32 dialog: Windows.Storage.Pickers don't work in these unpackaged tools
            auto dialog = wil::CoCreateInstance<IFileSaveDialog>(CLSID_FileSaveDialog);

            COMDLG_FILTERSPEC const types[]{ { L"Text", L"*.txt" } };
            THROW_IF_FAILED(dialog->SetFileTypes(ARRAYSIZE(types), types));
            THROW_IF_FAILED(dialog->SetDefaultExtension(L"txt"));

            auto const fileName = res::GetString(L"HistoryFileName");
            THROW_IF_FAILED(dialog->SetFileName(fileName.c_str()));

            HWND window{};
            THROW_IF_FAILED(this->try_as<::IWindowNative>()->get_WindowHandle(&window));

            if (FAILED(dialog->Show(window)))
            {
                // canceled
                return;
            }

            wil::com_ptr<IShellItem> item{};
            THROW_IF_FAILED(dialog->GetResult(&item));

            wil::unique_cotaskmem_string path{};
            THROW_IF_FAILED(item->GetDisplayName(SIGDN_FILESYSPATH, &path));

            std::wstring all{};

            for (auto const& entry : native::History::Current().Entries())
            {
                SYSTEMTIME local{};
                auto const when = LocalTime(entry.FileTime, local) ? std::format(L"{} {}", DateText(local, DATE_SHORTDATE), TimeText(local)) : std::wstring{};
                all += std::format(L"{}\t{}\t{}\t{}{}\r\n", when, entry.Page, entry.Question, entry.Answer, entry.Pinned ? L"\t*" : L"");
            }

            auto const bytes = winrt::to_string(all);

            wil::unique_hfile file{ ::CreateFileW(path.get(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr) };
            THROW_LAST_ERROR_IF(!file);

            // UTF-8 with its mark, so Notepad and spreadsheets read the note names right
            static constexpr char Mark[3]{ '\xEF', '\xBB', '\xBF' };
            DWORD written{ 0 };
            THROW_IF_WIN32_BOOL_FALSE(::WriteFile(file.get(), Mark, 3, &written, nullptr));
            THROW_IF_WIN32_BOOL_FALSE(::WriteFile(file.get(), bytes.data(), static_cast<DWORD>(bytes.size()), &written, nullptr));

            ShowStatus(res::GetString(L"HistorySaved"));
        }
        MIDI_CALCULATOR_CATCH_AND_LOG(L"Unable to save the history.")
    }
}
