// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

// The memories and tags list: every name the patch's logic steps use, the steps that set it and
// the steps that read it, and what each memory holds now. Pointing at a name lights up its steps
// on the canvas, and selecting a step goes to it.

#include "pch.h"
#include "MainWindow.xaml.h"

#include "DialogParts.h"
#include "MessageText.h"
#include "PatchModel.h"
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
        using patchbay::parts::Card;
        using patchbay::parts::Heading;
        using patchbay::parts::Hint;

        // How a memory's value reads: the way the first step that sets it reads it, so a memory
        // that holds a program change shows a program number.
        void ValueLookOf(
            _In_ patchbay::PatchDocument const& patch,
            _In_ patchbay::LogicNameUse const& use,
            _Out_ patchbay::LogicUnit& unit,
            _Out_ patchbay::ValueScale& scale) noexcept
        {
            unit = patchbay::LogicUnit::Number;
            scale = patchbay::ValueScale::SevenBit;

            for (auto const& blockId : use.SetBy)
            {
                auto const* block = patch.FindBlock(blockId);

                if (block == nullptr || block->Kind != patchbay::BlockKind::SetMemory)
                {
                    continue;
                }

                auto const& memory = block->Settings.SetMemory;
                auto const& source = memory.Value;

                scale = memory.Scale;

                if (memory.Action != patchbay::MemoryAction::Set)
                {
                    unit = memory.Unit;
                }
                else if (source.Kind == patchbay::LogicSourceKind::Part)
                {
                    unit = patchbay::UnitOfPart(source.Place.Part);
                }
                else if (source.Kind == patchbay::LogicSourceKind::Number)
                {
                    unit = source.Unit;
                }

                return;
            }
        }
    }

    _Use_decl_annotations_
    void MainWindow::OnLogicNamesToggleChanged(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(args);

        try
        {
            auto const checked = LogicNamesToggle().IsChecked();
            auto const shown = checked != nullptr && checked.Value();

            // ShowLogicNames sets the button too, which comes back here.
            if (shown != m_showingLogicNames)
            {
                ShowLogicNames(shown);
            }
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to show or hide the memories and tags.")
    }

    _Use_decl_annotations_
    void MainWindow::OnLogicNamesCloseClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(args);

        ShowLogicNames(false);
    }

    _Use_decl_annotations_
    void MainWindow::ShowLogicNames(bool shown) noexcept
    {
        try
        {
            m_showingLogicNames = shown;

            LogicNamesToggle().IsChecked(shown);
            LogicNamesPanel().Visibility(shown ? xaml::Visibility::Visible : xaml::Visibility::Collapsed);

            if (shown)
            {
                RefreshLogicNames();
            }
            else
            {
                LogicNamesContent().Children().Clear();
                m_logicNameValueLines.clear();
                m_canvas.Highlight({});
            }
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to show or hide the memories and tags.")
    }

    void MainWindow::RefreshLogicNames() noexcept
    {
        try
        {
            if (m_closing || !m_showingLogicNames)
            {
                return;
            }

            auto const content = LogicNamesContent();

            content.Children().Clear();
            m_logicNameValueLines.clear();

            // Whatever the pointer was over may be gone.
            m_canvas.Highlight({});

            auto const* patch = CurrentPatch();

            if (patch == nullptr)
            {
                return;
            }

            auto const uses = patchbay::FindLogicNameUses(patch->Blocks);

            if (uses.empty())
            {
                content.Children().Append(Hint(resources::GetString(L"LogicNamesEmpty")));
                return;
            }

            content.Children().Append(Hint(resources::GetString(L"LogicNamesHint")));

            auto weak = get_weak();
            auto const caution = patchbay::ThemeBrushes::Current().Get(L"SystemFillColorCautionBrush");
            auto const secondary = patchbay::ThemeBrushes::Current().Get(L"TextFillColorSecondaryBrush");

            auto const addSteps = [&](controls::StackPanel const& body, winrt::hstring const& caption, std::vector<std::wstring> const& blockIds)
                {
                    if (blockIds.empty())
                    {
                        return;
                    }

                    controls::TextBlock label{};
                    label.Text(caption);
                    label.FontSize(12);
                    label.Foreground(secondary);
                    label.Margin(xaml::ThicknessHelper::FromLengths(0, 4, 0, 0));

                    body.Children().Append(label);

                    for (auto const& blockId : blockIds)
                    {
                        auto const* block = patch->FindBlock(blockId);

                        if (block == nullptr)
                        {
                            continue;
                        }

                        auto name = patchbay::BlockDisplayName(*block);

                        if (block->Bypassed)
                        {
                            name = resources::FormatString(L"LogicNamesBypassedFormat", name);
                        }

                        controls::TextBlock text{};
                        text.Text(name);
                        text.TextTrimming(xaml::TextTrimming::CharacterEllipsis);

                        controls::HyperlinkButton link{};
                        link.Content(text);
                        link.Padding(xaml::ThicknessHelper::FromLengths(4, 2, 4, 2));
                        link.HorizontalAlignment(xaml::HorizontalAlignment::Left);
                        controls::ToolTipService::SetToolTip(link, winrt::box_value(resources::GetString(L"LogicNamesGoToStepTip")));

                        link.Click([weak, blockId](auto&&, auto&&)
                            {
                                if (auto strong = weak.get())
                                {
                                    strong->GoToStep(blockId);
                                }
                            });

                        body.Children().Append(link);
                    }
                };

            std::optional<bool> heading{};

            for (auto const& use : uses)
            {
                if (!heading.has_value() || *heading != use.IsMemory)
                {
                    heading = use.IsMemory;

                    auto title = Heading(resources::GetString(use.IsMemory ? L"LogicNamesMemoriesHeading" : L"LogicNamesTagsHeading"));
                    title.Margin(xaml::ThicknessHelper::FromLengths(0, 6, 0, 0));
                    content.Children().Append(title);
                }

                controls::StackPanel body{};
                body.Spacing(2);

                // The name, with a memory's value and its Reset button beside it.
                controls::Grid top{};
                top.ColumnSpacing(8);

                controls::ColumnDefinition nameColumn{};
                nameColumn.Width(xaml::GridLengthHelper::FromValueAndType(1, xaml::GridUnitType::Star));
                top.ColumnDefinitions().Append(nameColumn);

                controls::TextBlock name{};
                name.Text(winrt::hstring{ use.Name });
                name.FontWeight(winrt::Microsoft::UI::Text::FontWeights::SemiBold());
                name.TextTrimming(xaml::TextTrimming::CharacterEllipsis);
                name.VerticalAlignment(xaml::VerticalAlignment::Center);
                top.Children().Append(name);

                if (use.IsMemory)
                {
                    controls::ColumnDefinition resetColumn{};
                    resetColumn.Width(xaml::GridLengthHelper::Auto());
                    top.ColumnDefinitions().Append(resetColumn);

                    controls::Button reset{};
                    reset.Content(winrt::box_value(resources::GetString(L"LogicNamesReset")));
                    reset.Padding(xaml::ThicknessHelper::FromLengths(8, 2, 8, 2));
                    reset.FontSize(12);
                    reset.VerticalAlignment(xaml::VerticalAlignment::Center);
                    controls::ToolTipService::SetToolTip(reset, winrt::box_value(resources::GetString(L"LogicNamesResetTip")));
                    xaml::Automation::AutomationProperties::SetName(reset,
                        resources::FormatString(L"LogicNamesResetNameFormat", use.Name));
                    controls::Grid::SetColumn(reset, 1);

                    reset.Click([weak, memory = use.Name](auto&&, auto&&)
                        {
                            if (auto strong = weak.get())
                            {
                                patchbay::RouteEngine::Current().ResetMemory(strong->m_patchKey, memory);
                                strong->UpdateLogicNameValues();
                                strong->UpdateInspectorActivity();
                            }
                        });

                    top.Children().Append(reset);
                }

                body.Children().Append(top);

                if (use.IsMemory)
                {
                    LogicNameValueLine line{};
                    line.Name = use.Name;
                    ValueLookOf(*patch, use, line.Unit, line.Scale);

                    line.Text = controls::TextBlock{};
                    line.Text.FontSize(12);
                    line.Text.Foreground(secondary);

                    body.Children().Append(line.Text);
                    m_logicNameValueLines.push_back(std::move(line));
                }

                // A name nothing sets is almost always a spelling that doesn't match.
                if (use.SetBy.empty() || use.ReadBy.empty())
                {
                    auto const key = use.SetBy.empty()
                        ? (use.IsMemory ? L"LogicNamesNeverSetMemory" : L"LogicNamesNeverSetTag")
                        : (use.IsMemory ? L"LogicNamesNeverReadMemory" : L"LogicNamesNeverReadTag");

                    auto warning = Hint(resources::GetString(key));
                    warning.FontSize(12);
                    warning.Foreground(caution);

                    body.Children().Append(warning);
                }

                addSteps(body, resources::GetString(L"LogicNamesSetBy"), use.SetBy);
                addSteps(body, resources::GetString(L"LogicNamesReadBy"), use.ReadBy);

                auto card = Card(body);

                // Every step that uses the name glows while the pointer or the keyboard is on it.
                std::vector<std::wstring> steps{ use.SetBy };
                steps.insert(steps.end(), use.ReadBy.begin(), use.ReadBy.end());

                auto const highlight = [weak, steps](bool on)
                    {
                        if (auto strong = weak.get())
                        {
                            strong->m_canvas.Highlight(on ? steps : std::vector<std::wstring>{});
                        }
                    };

                card.PointerEntered([highlight](auto&&, auto&&) { highlight(true); });
                card.PointerExited([highlight](auto&&, auto&&) { highlight(false); });
                card.GotFocus([highlight](auto&&, auto&&) { highlight(true); });
                card.LostFocus([highlight](auto&&, auto&&) { highlight(false); });

                content.Children().Append(card);
            }

            UpdateLogicNameValues();
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to list the memories and tags.")
    }

    void MainWindow::UpdateLogicNameValues() noexcept
    {
        try
        {
            if (!m_showingLogicNames || m_logicNameValueLines.empty())
            {
                return;
            }

            auto const& engine = patchbay::RouteEngine::Current();

            for (auto const& line : m_logicNameValueLines)
            {
                if (line.Text == nullptr)
                {
                    continue;
                }

                auto const value = engine.MemoryValue(m_patchKey, line.Name);

                auto const text = resources::FormatString(L"LogicNamesValueFormat",
                    value.has_value() ? patchbay::DescribeLogicValue(*value, line.Unit, line.Scale) : resources::GetString(L"LogicValueEmpty"));

                // Only when it changed, so a screen reader isn't told the same thing again.
                if (line.Text.Text() != text)
                {
                    line.Text.Text(text);
                }
            }
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to show what the memories hold.")
    }

    _Use_decl_annotations_
    void MainWindow::GoToStep(std::wstring const& blockId) noexcept
    {
        try
        {
            auto const* patch = CurrentPatch();

            if (patch == nullptr || patch->FindBlock(blockId) == nullptr)
            {
                return;
            }

            m_canvas.Select(patchbay::CanvasSelectionKind::Block, blockId);
            m_canvas.BringIntoView(blockId);
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to go to the step.")
    }
}
