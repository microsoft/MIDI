// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "MainWindow.xaml.h"

#include "DialogParts.h"
#include "StringResources.h"
#include "ThemeBrushes.h"

using namespace winrt::Microsoft::UI::Xaml;

namespace patchbay = ::midipatchbay;
namespace resources = ::midipatchbay::resources;

// The message mask filter's picture of the message: every bit of every word, the bits each
// condition looks at, and the bits of a message typed in to try it. Dragging across the bits
// picks where the selected condition looks.
namespace winrt::midipatchbay::implementation
{
    namespace
    {
        using patchbay::parts::Heading;
        using patchbay::parts::HeadingHint;

        constexpr double LabelWidth = 64.0;
        constexpr double CellWidth = 16.0;
        constexpr double CellHeight = 22.0;
        constexpr double CellPitch = 18.0;
        constexpr double NibbleGap = 6.0;
        constexpr double GuideHeight = 20.0;
        constexpr double NumberRowHeight = 14.0;
        constexpr uint8_t BitsPerWord = 32;

        // From the left, so bit 31 is position 0.
        constexpr double CellLeft(_In_ uint8_t bit) noexcept
        {
            auto const position = static_cast<double>(31 - bit);
            return LabelWidth + position * CellPitch + static_cast<double>((31 - bit) / 4) * NibbleGap;
        }

        constexpr double GridWidth() noexcept
        {
            return CellLeft(0) + CellWidth;
        }

        // The bit under a point, or the nearest one to it.
        uint8_t BitAt(_In_ double x) noexcept
        {
            uint8_t nearest{ 31 };
            double best{ std::numeric_limits<double>::max() };

            for (int bit = 31; bit >= 0; bit--)
            {
                auto const middle = CellLeft(static_cast<uint8_t>(bit)) + CellWidth / 2.0;
                auto const distance = std::abs(x - middle);

                if (distance < best)
                {
                    best = distance;
                    nearest = static_cast<uint8_t>(bit);
                }
            }

            return nearest;
        }

        // Names drawn under the bits, for the message types people use the most. Only a guide:
        // messages of one size can be of several types.
        struct BitGuide
        {
            wchar_t const* NameKey;
            uint8_t Word;
            uint8_t HighBit;
            uint8_t LowBit;
            uint8_t MinimumWords;
            uint8_t MaximumWords;
        };

        constexpr BitGuide BitGuides[]
        {
            { L"MaskGuideType", 0, 31, 28, 1, 4 },
            { L"MaskGuideGroup", 0, 27, 24, 1, 4 },
            { L"MaskGuideStatus", 0, 23, 20, 1, 2 },
            { L"MaskGuideChannel", 0, 19, 16, 1, 2 },
            { L"MaskGuideIndex", 0, 14, 8, 1, 2 },
            { L"MaskGuideMidi1Value", 0, 6, 0, 1, 1 },
            { L"MaskGuideMidi2Velocity", 1, 31, 16, 2, 2 },
        };

        bool GuideShown(_In_ BitGuide const& guide, _In_ uint8_t wordCount) noexcept
        {
            return wordCount >= guide.MinimumWords && wordCount <= guide.MaximumWords && guide.Word < wordCount;
        }

        bool WordHasGuides(_In_ uint8_t word, _In_ uint8_t wordCount) noexcept
        {
            return std::any_of(std::begin(BitGuides), std::end(BitGuides),
                [word, wordCount](BitGuide const& guide) { return guide.Word == word && GuideShown(guide, wordCount); });
        }

        // How many words a message has, from its type.
        constexpr uint8_t WordsForType(_In_ uint32_t firstWord) noexcept
        {
            constexpr uint8_t sizes[16] = { 1, 1, 1, 2, 2, 4, 1, 1, 2, 2, 2, 3, 3, 4, 4, 4 };
            return sizes[(firstWord >> 28) & 0x0F];
        }

        // Up to four words in hex, split by spaces or commas. Nothing when it isn't that.
        std::optional<std::vector<uint32_t>> ReadTestWords(_In_ std::wstring_view text)
        {
            std::vector<uint32_t> words{};
            std::wstring token{};

            auto const flush = [&]() -> bool
                {
                    if (token.empty())
                    {
                        return true;
                    }

                    if (token.size() > 2 && token[0] == L'0' && (token[1] == L'x' || token[1] == L'X'))
                    {
                        token.erase(0, 2);
                    }

                    if (token.empty() || token.size() > 8 || words.size() >= patchbay::MaximumUmpWords ||
                        !std::all_of(token.begin(), token.end(), [](wchar_t c) { return std::iswxdigit(c) != 0; }))
                    {
                        return false;
                    }

                    words.push_back(static_cast<uint32_t>(std::wcstoul(token.c_str(), nullptr, 16)));
                    token.clear();
                    return true;
                };

            for (auto const c : text)
            {
                if (c == L',' || c == L';' || std::iswspace(c))
                {
                    if (!flush())
                    {
                        return std::nullopt;
                    }
                }
                else
                {
                    token.push_back(c);
                }
            }

            if (!flush())
            {
                return std::nullopt;
            }

            return words;
        }

        media::Brush BrushOr(_In_ std::wstring_view name, _In_ uint8_t alpha, _In_ uint8_t gray) noexcept
        {
            if (auto brush = patchbay::ThemeBrushes::Current().Get(name))
            {
                return brush;
            }

            try
            {
                return media::SolidColorBrush{ winrt::Windows::UI::Color{ alpha, gray, gray, gray } };
            }
            catch (...)
            {
            }

            return nullptr;
        }
    }

    controls::StackPanel MainWindow::BuildMaskBitsSection() noexcept
    {
        controls::StackPanel section{};

        try
        {
            auto weak = get_weak();

            section.Spacing(6);
            section.Margin(xaml::ThicknessHelper::FromLengths(0, 6, 0, 0));

            section.Children().Append(Heading(resources::GetString(L"MaskBitsHeading")));
            section.Children().Append(HeadingHint(resources::GetString(L"MaskBitsHint")));

            m_maskBitsHost = controls::StackPanel{};
            m_maskBitsHost.Spacing(4);

            controls::ScrollViewer scroller{};

            scroller.HorizontalScrollBarVisibility(controls::ScrollBarVisibility::Auto);
            scroller.VerticalScrollBarVisibility(controls::ScrollBarVisibility::Disabled);
            scroller.HorizontalScrollMode(controls::ScrollMode::Auto);
            scroller.VerticalScrollMode(controls::ScrollMode::Disabled);
            scroller.Content(m_maskBitsHost);

            section.Children().Append(scroller);

            // ---- try a message
            controls::TextBox tryBox{};

            tryBox.Header(winrt::box_value(resources::GetString(L"MaskTryHeader")));
            tryBox.PlaceholderText(resources::GetString(L"MaskTryPlaceholder"));
            tryBox.MaxWidth(360);
            tryBox.HorizontalAlignment(xaml::HorizontalAlignment::Left);
            tryBox.FontFamily(media::FontFamily{ L"Cascadia Mono, Consolas" });

            std::wstring initial{};

            for (auto const word : m_maskTestWords)
            {
                initial += (initial.empty() ? L"" : L" ") + std::format(L"{:08X}", word);
            }

            tryBox.Text(winrt::hstring{ initial });

            tryBox.TextChanged([weak](foundation::IInspectable const& sender, auto&&)
                {
                    auto s = weak.get();
                    auto const box = sender.try_as<controls::TextBox>();

                    if (s == nullptr || box == nullptr)
                    {
                        return;
                    }

                    auto const read = ReadTestWords(std::wstring_view{ box.Text() });

                    s->m_maskTestWords = read.value_or(std::vector<uint32_t>{});
                    s->RefreshMaskBits();

                    // After the refresh, which only knows the words and not that the text was wrong.
                    if (!read.has_value() && s->m_maskVerdictText != nullptr)
                    {
                        s->m_maskVerdictText.Text(resources::GetString(L"MaskTryInvalid"));
                    }
                });

            section.Children().Append(tryBox);

            m_maskVerdictText = controls::TextBlock{};
            m_maskVerdictText.FontSize(12);
            m_maskVerdictText.TextWrapping(xaml::TextWrapping::Wrap);
            m_maskVerdictText.Foreground(patchbay::ThemeBrushes::Current().Get(L"TextFillColorSecondaryBrush"));
            xaml::Automation::AutomationProperties::SetLiveSetting(
                m_maskVerdictText, xaml::Automation::Peers::AutomationLiveSetting::Polite);

            section.Children().Append(m_maskVerdictText);

            RebuildMaskBits();
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to build the mask's bits.")

        return section;
    }

    void MainWindow::RebuildMaskBits() noexcept
    {
        try
        {
            if (m_maskBitsHost == nullptr)
            {
                return;
            }

            m_maskBitsHost.Children().Clear();
            m_maskBitCells.clear();
            m_maskDragging = false;

            auto weak = get_weak();
            auto const wordCount = static_cast<uint8_t>(std::clamp<int>(m_editingSettings.Mask.WordCount, 1, patchbay::MaximumUmpWords));
            auto const tertiary = BrushOr(L"TextFillColorTertiaryBrush", 0x8A, 0x80);
            auto const line = BrushOr(L"ControlStrokeColorDefaultBrush", 0x24, 0x80);

            // ---- bit numbers over the first word
            controls::Canvas numbers{};

            numbers.Width(GridWidth());
            numbers.Height(NumberRowHeight);
            numbers.IsHitTestVisible(false);

            for (uint8_t const bit : { uint8_t{ 31 }, uint8_t{ 24 }, uint8_t{ 16 }, uint8_t{ 8 }, uint8_t{ 0 } })
            {
                controls::TextBlock number{};

                number.Text(winrt::to_hstring(static_cast<int>(bit)));
                number.FontSize(10);
                number.Width(CellWidth + 8);
                number.TextAlignment(xaml::TextAlignment::Center);
                number.Foreground(tertiary);

                controls::Canvas::SetLeft(number, CellLeft(bit) - 4);
                numbers.Children().Append(number);
            }

            m_maskBitsHost.Children().Append(numbers);

            for (uint8_t word = 0; word < wordCount; word++)
            {
                auto const guided = WordHasGuides(word, wordCount);

                controls::Canvas row{};

                row.Width(GridWidth());
                row.Height(CellHeight + (guided ? GuideHeight + 2 : 0));

                // Transparent rather than null, so the gaps between cells still take a press.
                row.Background(media::SolidColorBrush{ winrt::Windows::UI::Color{ 0, 0, 0, 0 } });

                xaml::Automation::AutomationProperties::SetName(row,
                    resources::FormatString(L"MaskWordFormat", static_cast<int>(word) + 1));

                controls::TextBlock label{};

                label.Text(resources::FormatString(L"MaskWordFormat", static_cast<int>(word) + 1));
                label.FontSize(12);
                label.Width(LabelWidth - 6);
                label.Foreground(patchbay::ThemeBrushes::Current().Get(L"TextFillColorSecondaryBrush"));
                label.IsHitTestVisible(false);

                controls::Canvas::SetTop(label, 3);
                row.Children().Append(label);

                for (int bit = 31; bit >= 0; bit--)
                {
                    controls::Border cell{};

                    cell.Width(CellWidth);
                    cell.Height(CellHeight);
                    cell.CornerRadius(xaml::CornerRadiusHelper::FromUniformRadius(3));
                    cell.BorderThickness(xaml::ThicknessHelper::FromUniformLength(1));
                    cell.IsHitTestVisible(false);

                    controls::TextBlock value{};

                    value.FontSize(11);
                    value.FontFamily(media::FontFamily{ L"Cascadia Mono, Consolas" });
                    value.HorizontalAlignment(xaml::HorizontalAlignment::Center);
                    value.VerticalAlignment(xaml::VerticalAlignment::Center);

                    cell.Child(value);

                    controls::Canvas::SetLeft(cell, CellLeft(static_cast<uint8_t>(bit)));
                    row.Children().Append(cell);

                    m_maskBitCells.push_back(cell);
                }

                if (guided)
                {
                    for (auto const& guide : BitGuides)
                    {
                        if (guide.Word != word || !GuideShown(guide, wordCount))
                        {
                            continue;
                        }

                        auto const left = CellLeft(guide.HighBit);
                        auto const width = CellLeft(guide.LowBit) + CellWidth - left;

                        controls::Border span{};

                        span.Width(width);
                        span.Height(GuideHeight);
                        span.BorderThickness(xaml::ThicknessHelper::FromLengths(0, 1, 0, 0));
                        span.BorderBrush(line);
                        span.IsHitTestVisible(false);

                        controls::TextBlock name{};

                        name.Text(resources::GetString(guide.NameKey));
                        name.FontSize(11);
                        name.Foreground(tertiary);
                        name.TextTrimming(xaml::TextTrimming::CharacterEllipsis);
                        name.HorizontalAlignment(xaml::HorizontalAlignment::Center);
                        name.VerticalAlignment(xaml::VerticalAlignment::Center);

                        span.Child(name);

                        controls::Canvas::SetLeft(span, left);
                        controls::Canvas::SetTop(span, CellHeight + 2);
                        row.Children().Append(span);
                    }
                }

                // Everything is placed by arithmetic, so the row works out what was pressed from
                // where. The cells and names take no input of their own.
                row.PointerPressed([weak, word, wordCount](foundation::IInspectable const& sender, input::PointerRoutedEventArgs const& args)
                    {
                        auto s = weak.get();
                        auto const element = sender.try_as<xaml::UIElement>();

                        if (s == nullptr || element == nullptr)
                        {
                            return;
                        }

                        auto const point = args.GetCurrentPoint(element);

                        if (!point.Properties().IsLeftButtonPressed())
                        {
                            return;
                        }

                        args.Handled(true);

                        auto const position = point.Position();
                        auto const bit = BitAt(position.X);

                        // Under the bits: the field name there, if there is one.
                        if (position.Y > CellHeight + 1)
                        {
                            for (auto const& guide : BitGuides)
                            {
                                if (guide.Word == word && GuideShown(guide, wordCount) &&
                                    bit <= guide.HighBit && bit >= guide.LowBit)
                                {
                                    s->PickMaskField(guide.Word, guide.HighBit, guide.LowBit);
                                    return;
                                }
                            }

                            return;
                        }

                        if (position.X < LabelWidth - 4)
                        {
                            return;
                        }

                        element.CapturePointer(args.Pointer());
                        s->OnMaskBitPressed(word, bit);
                    });

                row.PointerMoved([weak](foundation::IInspectable const& sender, input::PointerRoutedEventArgs const& args)
                    {
                        auto s = weak.get();
                        auto const element = sender.try_as<xaml::UIElement>();

                        if (s == nullptr || element == nullptr || !s->m_maskDragging)
                        {
                            return;
                        }

                        s->OnMaskBitDragged(BitAt(args.GetCurrentPoint(element).Position().X));
                    });

                row.PointerReleased([weak](foundation::IInspectable const& sender, input::PointerRoutedEventArgs const& args)
                    {
                        if (auto const element = sender.try_as<xaml::UIElement>())
                        {
                            element.ReleasePointerCapture(args.Pointer());
                        }

                        if (auto s = weak.get())
                        {
                            s->EndMaskBitDrag();
                        }
                    });

                row.PointerCaptureLost([weak](auto&&, auto&&)
                    {
                        if (auto s = weak.get())
                        {
                            s->EndMaskBitDrag();
                        }
                    });

                m_maskBitsHost.Children().Append(row);
            }

            RefreshMaskBits();
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to draw the mask's bits.")
    }

    void MainWindow::RefreshMaskBits() noexcept
    {
        try
        {
            if (m_maskBitsHost == nullptr)
            {
                return;
            }

            auto const& mask = m_editingSettings.Mask;

            if (!mask.Conditions.empty())
            {
                m_maskActiveCondition = (std::min)(m_maskActiveCondition, mask.Conditions.size() - 1);
            }

            auto const accent = BrushOr(L"AccentFillColorDefaultBrush", 0xFF, 0x60);
            auto const onAccent = BrushOr(L"TextOnAccentFillColorPrimaryBrush", 0xFF, 0xFF);
            auto const plain = BrushOr(L"ControlAltFillColorSecondaryBrush", 0x10, 0x80);
            auto const other = BrushOr(L"SubtleFillColorSecondaryBrush", 0x18, 0x80);
            auto const stroke = BrushOr(L"ControlStrokeColorDefaultBrush", 0x24, 0x80);
            auto const primary = BrushOr(L"TextFillColorPrimaryBrush", 0xFF, 0xE0);
            auto const tertiary = BrushOr(L"TextFillColorTertiaryBrush", 0x8A, 0x80);

            for (size_t index = 0; index < m_maskBitCells.size(); index++)
            {
                auto const word = static_cast<uint8_t>(index / BitsPerWord);
                auto const bit = static_cast<uint8_t>(31 - index % BitsPerWord);

                // The selected condition first, so it shows where conditions overlap.
                std::optional<size_t> owner{};

                for (size_t position = 0; position < mask.Conditions.size(); position++)
                {
                    auto const& condition = mask.Conditions[position];

                    if (condition.Word == word && bit <= condition.HighBit && bit >= condition.LowBit &&
                        (!owner.has_value() || position == m_maskActiveCondition))
                    {
                        owner = position;
                    }
                }

                auto const active = owner.has_value() && owner.value() == m_maskActiveCondition;
                auto const& cell = m_maskBitCells[index];

                cell.Background(active ? accent : (owner.has_value() ? other : plain));
                cell.BorderBrush(owner.has_value() && !active ? accent : stroke);

                if (auto const text = cell.Child().try_as<controls::TextBlock>())
                {
                    std::optional<bool> set{};

                    if (word < m_maskTestWords.size())
                    {
                        set = ((m_maskTestWords[word] >> bit) & 1u) != 0;
                    }

                    text.Text(set.has_value() ? (set.value() ? L"1" : L"0") : L"");
                    text.Foreground(active ? onAccent : (set.value_or(false) ? primary : tertiary));
                }
            }

            if (m_maskVerdictText == nullptr)
            {
                return;
            }

            // What the step does with the message typed in.
            winrt::hstring verdict{};

            if (!m_maskTestWords.empty())
            {
                auto const size = WordsForType(m_maskTestWords.front());

                if (m_maskTestWords.size() != size)
                {
                    verdict = resources::FormatString(L"MaskTryLengthFormat", static_cast<int>(size) * 32);
                }
                else if (size != mask.WordCount)
                {
                    verdict = resources::FormatString(L"MaskTryOtherSizeFormat",
                        static_cast<int>(size) * 32, static_cast<int>(mask.WordCount) * 32);
                }
                else if (mask.Conditions.empty())
                {
                    verdict = resources::GetString(L"MaskTryNoConditions");
                }
                else
                {
                    auto const matches = std::all_of(mask.Conditions.begin(), mask.Conditions.end(),
                        [this, size](patchbay::MaskCondition const& condition)
                        {
                            return condition.Matches(m_maskTestWords.data(), size);
                        });

                    auto const passes = mask.Passes(m_maskTestWords.data(), size);

                    verdict = resources::GetString(matches
                        ? (passes ? L"MaskTryMatchPasses" : L"MaskTryMatchKeptOut")
                        : (passes ? L"MaskTryNoMatchPasses" : L"MaskTryNoMatchKeptOut"));
                }
            }

            m_maskVerdictText.Text(verdict);
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to show the mask's bits.")
    }

    _Use_decl_annotations_
    void MainWindow::OnMaskBitPressed(uint8_t word, uint8_t bit) noexcept
    {
        try
        {
            auto& mask = m_editingSettings.Mask;

            // A press inside a condition's bits picks that condition. Dragging from there
            // changes it.
            std::optional<size_t> hit{};

            for (size_t position = 0; position < mask.Conditions.size(); position++)
            {
                auto const& condition = mask.Conditions[position];

                if (condition.Word == word && bit <= condition.HighBit && bit >= condition.LowBit &&
                    (!hit.has_value() || position == m_maskActiveCondition))
                {
                    hit = position;
                }
            }

            m_maskDragging = true;
            m_maskDragWord = word;
            m_maskDragAnchor = bit;
            m_maskDragChanges = !hit.has_value();

            if (hit.has_value())
            {
                SetActiveMaskCondition(hit.value());
                return;
            }

            // Anywhere else moves the selected condition, or starts the first one.
            if (mask.Conditions.empty())
            {
                mask.Conditions.push_back(patchbay::MaskCondition{});
                m_maskActiveCondition = 0;
            }

            auto& target = mask.Conditions[(std::min)(m_maskActiveCondition, mask.Conditions.size() - 1)];

            target.Word = word;
            target.HighBit = bit;
            target.LowBit = bit;

            UpdateBlockSummary();
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to pick a bit.")
    }

    _Use_decl_annotations_
    void MainWindow::OnMaskBitDragged(uint8_t bit) noexcept
    {
        try
        {
            auto& mask = m_editingSettings.Mask;

            if (!m_maskDragging || mask.Conditions.empty() || (!m_maskDragChanges && bit == m_maskDragAnchor))
            {
                return;
            }

            m_maskDragChanges = true;

            auto& target = mask.Conditions[(std::min)(m_maskActiveCondition, mask.Conditions.size() - 1)];
            auto const high = (std::max)(bit, m_maskDragAnchor);
            auto const low = (std::min)(bit, m_maskDragAnchor);

            if (target.Word == m_maskDragWord && target.HighBit == high && target.LowBit == low)
            {
                return;
            }

            target.Word = m_maskDragWord;
            target.HighBit = high;
            target.LowBit = low;

            UpdateBlockSummary();
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to pick bits.")
    }

    void MainWindow::EndMaskBitDrag() noexcept
    {
        if (!m_maskDragging)
        {
            return;
        }

        m_maskDragging = false;

        // The boxes under each condition are drawn again once, now that the drag is over.
        if (m_maskDragChanges)
        {
            m_maskDragChanges = false;
            RebuildMaskConditions();
            UpdateBlockSummary();
        }
    }

    _Use_decl_annotations_
    void MainWindow::PickMaskField(uint8_t word, uint8_t highBit, uint8_t lowBit) noexcept
    {
        try
        {
            auto& mask = m_editingSettings.Mask;

            if (mask.Conditions.empty())
            {
                mask.Conditions.push_back(patchbay::MaskCondition{});
                m_maskActiveCondition = 0;
            }

            auto& target = mask.Conditions[(std::min)(m_maskActiveCondition, mask.Conditions.size() - 1)];

            target.Word = word;
            target.HighBit = highBit;
            target.LowBit = lowBit;

            RebuildMaskConditions();
            UpdateBlockSummary();
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to pick a field.")
    }

    _Use_decl_annotations_
    void MainWindow::SetActiveMaskCondition(size_t position) noexcept
    {
        if (m_maskActiveCondition == position)
        {
            return;
        }

        m_maskActiveCondition = position;
        RefreshMaskBits();
    }
}
