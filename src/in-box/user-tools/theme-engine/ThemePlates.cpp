// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "ThemePlates.h"
#include "ThemeCanvas.h"
#include "ThemeXaml.h"

#include <cmath>

#include <winrt/Microsoft.UI.Xaml.Documents.h>

namespace themeengine
{
    namespace
    {
        namespace wgc = ::winrt::Microsoft::Graphics::Canvas;
        namespace wgcx = ::winrt::Microsoft::Graphics::Canvas::UI::Xaml;
        namespace mux = ::winrt::Microsoft::UI::Xaml;
        namespace muxc = ::winrt::Microsoft::UI::Xaml::Controls;
        namespace muxcp = ::winrt::Microsoft::UI::Xaml::Controls::Primitives;
        namespace muxm = ::winrt::Microsoft::UI::Xaml::Media;
        namespace muxi = ::winrt::Microsoft::UI::Xaml::Input;
        namespace wf = ::winrt::Windows::Foundation;

        // A button's template with nothing but its content, so the plate behind it shows. It keeps
        // the control's text properties, which the plate sets.
        constexpr wchar_t BareTemplateXaml[] =
            L"<ControlTemplate xmlns=\"http://schemas.microsoft.com/winfx/2006/xaml/presentation\" TargetType=\"ContentControl\">"
            L"<ContentPresenter Background=\"Transparent\""
            L" Content=\"{TemplateBinding Content}\""
            L" ContentTemplate=\"{TemplateBinding ContentTemplate}\""
            L" ContentTransitions=\"{TemplateBinding ContentTransitions}\""
            L" Padding=\"{TemplateBinding Padding}\""
            L" HorizontalContentAlignment=\"{TemplateBinding HorizontalContentAlignment}\""
            L" VerticalContentAlignment=\"{TemplateBinding VerticalContentAlignment}\""
            L" Foreground=\"{TemplateBinding Foreground}\""
            L" FontFamily=\"{TemplateBinding FontFamily}\""
            L" FontSize=\"{TemplateBinding FontSize}\""
            L" FontWeight=\"{TemplateBinding FontWeight}\""
            L" CharacterSpacing=\"{TemplateBinding CharacterSpacing}\""
            L" AutomationProperties.AccessibilityView=\"Raw\" />"
            L"</ControlTemplate>";

        // The buttons whose whole look is their template; a check box or a drop-down button keeps
        // its own, since its template draws more than its background.
        constexpr std::wstring_view BareButtons[]{
            L"Microsoft.UI.Xaml.Controls.Button",
            L"Microsoft.UI.Xaml.Controls.Primitives.ToggleButton",
            L"Microsoft.UI.Xaml.Controls.Primitives.RepeatButton",
        };

        // A combo box's own fill and edge, in every state (WinUI's ComboBox template).
        constexpr std::wstring_view ClearComboKeys[]{
            L"ComboBoxBackground",
            L"ComboBoxBackgroundPointerOver",
            L"ComboBoxBackgroundPressed",
            L"ComboBoxBackgroundDisabled",
            L"ComboBoxBackgroundFocused",
            L"ComboBoxBackgroundBorderBrushFocused",
            L"ComboBoxBorderBrush",
            L"ComboBoxBorderBrushPointerOver",
            L"ComboBoxBorderBrushPressed",
            L"ComboBoxBorderBrushDisabled",
        };

        // The text properties of a control or a text block.
        struct TextProperties
        {
            mux::DependencyProperty Foreground{ nullptr };
            mux::DependencyProperty FontFamily{ nullptr };
            mux::DependencyProperty FontSize{ nullptr };
            mux::DependencyProperty FontWeight{ nullptr };
            mux::DependencyProperty CharacterSpacing{ nullptr };
        };

        TextProperties TextPropertiesOf(mux::FrameworkElement const& element)
        {
            if (element.try_as<muxc::Control>() != nullptr)
            {
                return { muxc::Control::ForegroundProperty(), muxc::Control::FontFamilyProperty(), muxc::Control::FontSizeProperty(),
                    muxc::Control::FontWeightProperty(), muxc::Control::CharacterSpacingProperty() };
            }

            if (element.try_as<muxc::TextBlock>() != nullptr)
            {
                return { muxc::TextBlock::ForegroundProperty(), muxc::TextBlock::FontFamilyProperty(), muxc::TextBlock::FontSizeProperty(),
                    muxc::TextBlock::FontWeightProperty(), muxc::TextBlock::CharacterSpacingProperty() };
            }

            return {};
        }

        bool SameObject(wf::IInspectable const& a, wf::IInspectable const& b)
        {
            if (a == nullptr || b == nullptr)
            {
                return a == nullptr && b == nullptr;
            }

            return winrt::get_abi(a.as<wf::IUnknown>()) == winrt::get_abi(b.as<wf::IUnknown>());
        }
    }

    struct PlateLayer::State : std::enable_shared_from_this<PlateLayer::State>
    {
        struct Original
        {
            mux::DependencyProperty Property{ nullptr };
            wf::IInspectable Value{ nullptr };
        };

        struct Plate
        {
            uint64_t Id{ 0 };
            winrt::weak_ref<mux::FrameworkElement> Element{};
            std::wstring Part{};
            PlateOptions Options{};

            // what the element had before the plate changed it
            std::vector<Original> Originals{};
            std::vector<std::wstring> AddedResources{};
            bool Bare{ false };

            std::vector<std::pair<mux::DependencyProperty, int64_t>> Callbacks{};
            winrt::event_token Entered{};
            winrt::event_token Exited{};
            bool Hover{ false };
        };

        muxc::Panel Host{ nullptr };
        wgcx::CanvasControl Canvas{ nullptr };
        std::shared_ptr<ResolvedTheme const> Look{};
        std::vector<std::unique_ptr<Plate>> Plates{};
        ThemePainter Painter{};
        muxc::ControlTemplate BareTemplate{ nullptr };
        std::vector<float> Layout{};
        uint64_t NextId{ 1 };

        Plate* Find(uint64_t id) noexcept
        {
            for (auto const& plate : Plates)
            {
                if (plate->Id == id)
                {
                    return plate.get();
                }
            }

            return nullptr;
        }

        // ---------------------------------------------------------------- what the element shows

        std::vector<std::wstring> StatesOf(Plate const& plate, mux::FrameworkElement const& element) const
        {
            std::vector<std::wstring> states{};

            if (auto const button = element.try_as<muxcp::ButtonBase>())
            {
                if (button.IsPointerOver())
                {
                    states.emplace_back(L"hover");
                }

                if (auto const toggle = element.try_as<muxcp::ToggleButton>())
                {
                    if (auto const checked = toggle.IsChecked(); checked != nullptr && checked.Value())
                    {
                        states.emplace_back(L"checked");
                    }
                }

                if (button.IsPressed())
                {
                    states.emplace_back(L"pressed");
                }
            }
            else if (plate.Hover)
            {
                states.emplace_back(L"hover");
            }

            if (auto const control = element.try_as<muxc::Control>(); control != nullptr && !control.IsEnabled())
            {
                states.emplace_back(L"disabled");
            }

            if (plate.Options.States)
            {
                plate.Options.States(element, states);
            }

            return states;
        }

        std::shared_ptr<ResolvedStyle const> StyleOf(Plate const& plate, mux::FrameworkElement const& element) const
        {
            auto const states = StatesOf(plate, element);
            std::vector<std::wstring_view> views{ states.begin(), states.end() };
            return Look->Part(plate.Part, std::span<std::wstring_view const>{ views }, plate.Options.Track);
        }

        // ---------------------------------------------------------------- changing the element

        static void Set(Plate& plate, mux::DependencyObject const& element, mux::DependencyProperty const& property, wf::IInspectable const& value)
        {
            auto const found = std::find_if(plate.Originals.begin(), plate.Originals.end(), [&](Original const& o) { return o.Property == property; });

            if (found == plate.Originals.end())
            {
                plate.Originals.push_back({ property, element.ReadLocalValue(property) });
            }

            element.SetValue(property, value);
        }

        static void Unset(Plate& plate, mux::DependencyObject const& element, mux::DependencyProperty const& property)
        {
            auto const found = std::find_if(plate.Originals.begin(), plate.Originals.end(), [&](Original const& o) { return o.Property == property; });

            if (found == plate.Originals.end())
            {
                return;
            }

            if (found->Value != nullptr && SameObject(found->Value, mux::DependencyProperty::UnsetValue()))
            {
                element.ClearValue(property);
            }
            else
            {
                element.SetValue(property, found->Value);
            }

            plate.Originals.erase(found);
        }

        static bool IsUnset(mux::DependencyObject const& element, mux::DependencyProperty const& property)
        {
            auto const value = element.ReadLocalValue(property);
            return value != nullptr && SameObject(value, mux::DependencyProperty::UnsetValue());
        }

        static bool HasOriginal(Plate const& plate, mux::DependencyProperty const& property)
        {
            return std::any_of(plate.Originals.begin(), plate.Originals.end(), [&](Original const& o) { return o.Property == property; });
        }

        muxc::ControlTemplate Template()
        {
            if (BareTemplate == nullptr)
            {
                BareTemplate = mux::Markup::XamlReader::Load(BareTemplateXaml).as<muxc::ControlTemplate>();
            }

            return BareTemplate;
        }

        // The element's own background goes. True when its resources changed.
        bool MakeBare(Plate& plate, mux::FrameworkElement const& element)
        {
            if (plate.Bare)
            {
                return false;
            }

            plate.Bare = true;
            auto const name = winrt::get_class_name(element);

            if (std::find(std::begin(BareButtons), std::end(BareButtons), std::wstring_view{ name }) != std::end(BareButtons))
            {
                Set(plate, element, muxc::Control::TemplateProperty(), Template());
                return false;
            }

            if (auto const combo = element.try_as<muxc::ComboBox>())
            {
                auto const resources = combo.Resources();
                auto changed = false;

                for (auto const key : ClearComboKeys)
                {
                    auto const boxed = winrt::box_value(winrt::hstring{ key });

                    if (!resources.HasKey(boxed))
                    {
                        resources.Insert(boxed, muxm::SolidColorBrush{ winrt::Microsoft::UI::Colors::Transparent() });
                        plate.AddedResources.emplace_back(key);
                        changed = true;
                    }
                }

                // The style's setters took the old brushes when it was applied.
                Set(plate, element, muxc::Control::BackgroundProperty(), muxm::SolidColorBrush{ winrt::Microsoft::UI::Colors::Transparent() });
                Set(plate, element, muxc::Control::BorderBrushProperty(), muxm::SolidColorBrush{ winrt::Microsoft::UI::Colors::Transparent() });
                return changed;
            }

            return false;
        }

        // Its own look back. True when its resources changed.
        bool Restore(Plate& plate, mux::FrameworkElement const& element)
        {
            while (!plate.Originals.empty())
            {
                Unset(plate, element, plate.Originals.back().Property);
            }

            auto const changed = !plate.AddedResources.empty();

            if (changed)
            {
                auto const resources = element.Resources();

                for (auto const& key : plate.AddedResources)
                {
                    auto const boxed = winrt::box_value(winrt::hstring{ key });

                    if (resources.HasKey(boxed))
                    {
                        resources.Remove(boxed);
                    }
                }

                plate.AddedResources.clear();
            }

            plate.Bare = false;
            return changed;
        }

        // The element takes the part's text and size for the states it's in. True when its
        // resources changed.
        bool StyleElement(Plate& plate)
        {
            auto const element = plate.Element.get();

            if (element == nullptr)
            {
                return false;
            }

            if (Look == nullptr || !Look->HasPart(plate.Part))
            {
                return Restore(plate, element);
            }

            auto const changed = MakeBare(plate, element);
            auto const style = StyleOf(plate, element);
            auto const text = TextPropertiesOf(element);

            if (text.Foreground != nullptr)
            {
                // A color the app set itself stays: it may be a theme brush, which a value put back
                // later wouldn't follow when the theme changes.
                if (style->Foreground.has_value() && (HasOriginal(plate, text.Foreground) || IsUnset(element, text.Foreground)))
                {
                    auto const color = ToWindowsColor(*style->Foreground);
                    auto const current = element.GetValue(text.Foreground).try_as<muxm::SolidColorBrush>();

                    if (!HasOriginal(plate, text.Foreground) || current == nullptr || current.Color() != color)
                    {
                        Set(plate, element, text.Foreground, muxm::SolidColorBrush{ color });
                    }
                }
                else
                {
                    Unset(plate, element, text.Foreground);
                }

                if (!style->FontFamily.empty())
                {
                    auto const family = Look->Font(style->FontFamily, style->FontFamily);
                    auto const current = element.GetValue(text.FontFamily).try_as<muxm::FontFamily>();

                    if (current == nullptr || std::wstring_view{ current.Source() } != family)
                    {
                        Set(plate, element, text.FontFamily, muxm::FontFamily{ winrt::hstring{ family } });
                    }
                }
                else
                {
                    Unset(plate, element, text.FontFamily);
                }

                if (style->FontSize > 0)
                {
                    Set(plate, element, text.FontSize, winrt::box_value(style->FontSize));
                }
                else
                {
                    Unset(plate, element, text.FontSize);
                }

                if (style->FontWeight > 0)
                {
                    Set(plate, element, text.FontWeight, winrt::box_value(winrt::Windows::UI::Text::FontWeight{ static_cast<uint16_t>(style->FontWeight) }));
                }
                else
                {
                    Unset(plate, element, text.FontWeight);
                }

                // "numerals": "lining" asks a font with old-style figures (Candara) for figures
                // that sit on the line and line up, as CSS's lining-nums tabular-nums do.
                if (style->Word(L"numerals", {}) == L"lining")
                {
                    Set(plate, element, mux::Documents::Typography::NumeralStyleProperty(), winrt::box_value(mux::FontNumeralStyle::Lining));
                    Set(plate, element, mux::Documents::Typography::NumeralAlignmentProperty(), winrt::box_value(mux::FontNumeralAlignment::Tabular));
                }
                else
                {
                    Unset(plate, element, mux::Documents::Typography::NumeralStyleProperty());
                    Unset(plate, element, mux::Documents::Typography::NumeralAlignmentProperty());
                }

                // CSS's letter-spacing is in pixels; XAML's in thousandths of the font's size.
                if (auto const spacing = style->Number(L"letter-spacing", 0.0); spacing != 0.0)
                {
                    auto const size = winrt::unbox_value_or<double>(element.GetValue(text.FontSize), 14.0);
                    Set(plate, element, text.CharacterSpacing, winrt::box_value(static_cast<int32_t>(std::lround(spacing / std::max(1.0, size) * 1000.0))));
                }
                else
                {
                    Unset(plate, element, text.CharacterSpacing);
                }
            }

            if (plate.Options.Sized)
            {
                if (style->Width.has_value())
                {
                    Set(plate, element, mux::FrameworkElement::WidthProperty(), winrt::box_value(*style->Width));
                }
                else
                {
                    Unset(plate, element, mux::FrameworkElement::WidthProperty());
                }

                if (style->Height.has_value())
                {
                    Set(plate, element, mux::FrameworkElement::HeightProperty(), winrt::box_value(*style->Height));
                }
                else
                {
                    Unset(plate, element, mux::FrameworkElement::HeightProperty());
                }
            }

            return changed;
        }

        // ---------------------------------------------------------------- watching the element

        void Watch(Plate& plate, mux::FrameworkElement const& element)
        {
            auto const weak = weak_from_this();
            auto const id = plate.Id;

            auto const watch = [&](mux::DependencyProperty const& property)
            {
                auto const token = element.RegisterPropertyChangedCallback(property, [weak, id](mux::DependencyObject const&, mux::DependencyProperty const&)
                {
                    if (auto const state = weak.lock())
                    {
                        state->Changed(id);
                    }
                });

                plate.Callbacks.emplace_back(property, token);
            };

            if (element.try_as<muxcp::ButtonBase>() != nullptr)
            {
                watch(muxcp::ButtonBase::IsPointerOverProperty());
                watch(muxcp::ButtonBase::IsPressedProperty());
            }
            else
            {
                plate.Entered = element.PointerEntered([weak, id](wf::IInspectable const&, muxi::PointerRoutedEventArgs const&)
                {
                    if (auto const state = weak.lock())
                    {
                        if (auto const found = state->Find(id))
                        {
                            found->Hover = true;
                            state->Changed(id);
                        }
                    }
                });

                plate.Exited = element.PointerExited([weak, id](wf::IInspectable const&, muxi::PointerRoutedEventArgs const&)
                {
                    if (auto const state = weak.lock())
                    {
                        if (auto const found = state->Find(id))
                        {
                            found->Hover = false;
                            state->Changed(id);
                        }
                    }
                });
            }

            if (element.try_as<muxcp::ToggleButton>() != nullptr)
            {
                watch(muxcp::ToggleButton::IsCheckedProperty());
            }

            if (element.try_as<muxc::Control>() != nullptr)
            {
                watch(muxc::Control::IsEnabledProperty());
            }

            watch(mux::UIElement::VisibilityProperty());
        }

        void Unwatch(Plate& plate) noexcept
        {
            try
            {
                if (auto const element = plate.Element.get())
                {
                    for (auto const& [property, token] : plate.Callbacks)
                    {
                        element.UnregisterPropertyChangedCallback(property, token);
                    }

                    if (plate.Entered.value != 0)
                    {
                        element.PointerEntered(plate.Entered);
                        element.PointerExited(plate.Exited);
                    }
                }
            }
            catch (...)
            {
            }

            plate.Callbacks.clear();
            plate.Entered = {};
            plate.Exited = {};
        }

        void Changed(uint64_t id) noexcept
        {
            try
            {
                if (auto const plate = Find(id))
                {
                    StyleElement(*plate);
                }

                if (Canvas != nullptr)
                {
                    Canvas.Invalidate();
                }
            }
            catch (...)
            {
            }
        }

        // ---------------------------------------------------------------- where it is

        // The element's box on the canvas, when it's shown: loaded, and neither it nor anything
        // it's in up to the host collapsed or see-through. The opacity is what it and those have
        // together, to draw the plate as see-through as the element.
        bool Bounds(mux::FrameworkElement const& element, PaintRect& rect, float* opacity = nullptr) const
        {
            if (!element.IsLoaded())
            {
                return false;
            }

            auto const host = winrt::get_abi(Host.as<wf::IUnknown>());
            auto inHost = false;
            auto alpha = 1.0;

            for (mux::DependencyObject at = element; at != nullptr; at = muxm::VisualTreeHelper::GetParent(at))
            {
                auto const isHost = winrt::get_abi(at.as<wf::IUnknown>()) == host;

                if (auto const ui = at.try_as<mux::UIElement>())
                {
                    if (ui.Visibility() == mux::Visibility::Collapsed || ui.Opacity() <= 0.0)
                    {
                        return false;
                    }

                    // The host's own opacity is the canvas's too.
                    if (!isHost)
                    {
                        alpha *= ui.Opacity();
                    }
                }

                if (isHost)
                {
                    inHost = true;
                    break;
                }
            }

            auto const width = element.ActualWidth();
            auto const height = element.ActualHeight();

            if (!inHost || width <= 0.0 || height <= 0.0)
            {
                return false;
            }

            auto const box = element.TransformToVisual(Canvas).TransformBounds(wf::Rect{ 0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height) });
            rect = { box.X, box.Y, box.Width, box.Height };

            if (opacity != nullptr)
            {
                *opacity = static_cast<float>(alpha);
            }

            return true;
        }

        // Every plate's box, to tell when one moved.
        std::vector<float> Measure() const
        {
            std::vector<float> layout{};
            layout.reserve(Plates.size() * 4);

            for (auto const& plate : Plates)
            {
                PaintRect rect{ -1.0f, -1.0f, 0.0f, 0.0f };

                if (auto const element = plate->Element.get())
                {
                    try
                    {
                        if (!Bounds(element, rect))
                        {
                            rect = { -1.0f, -1.0f, 0.0f, 0.0f };
                        }
                    }
                    catch (...)
                    {
                    }
                }

                layout.insert(layout.end(), { rect.X, rect.Y, rect.Width, rect.Height });
            }

            return layout;
        }

        void OnLayout() noexcept
        {
            try
            {
                if (Look == nullptr || Plates.empty() || Canvas == nullptr)
                {
                    return;
                }

                auto layout = Measure();

                if (layout != Layout)
                {
                    Layout = std::move(layout);
                    Canvas.Invalidate();
                }
            }
            catch (...)
            {
            }
        }

        // ---------------------------------------------------------------- drawing

        void Draw(wgcx::CanvasControl const& sender, wgcx::CanvasDrawEventArgs const& args)
        {
            if (Look == nullptr)
            {
                return;
            }

            auto const ds = args.DrawingSession();
            auto const creator = sender.as<wgc::ICanvasResourceCreatorWithDpi>();

            for (auto const& plate : Plates)
            {
                try
                {
                    auto const element = plate->Element.get();
                    PaintRect space{};
                    auto opacity = 1.0f;

                    if (element == nullptr || !Look->HasPart(plate->Part) || !Bounds(element, space, &opacity))
                    {
                        continue;
                    }

                    auto const style = StyleOf(*plate, element);

                    if (!style->HasPlate() && !plate->Options.Decorate)
                    {
                        continue;
                    }

                    auto const rect = PartRect(*style, space);
                    wgc::CanvasActiveLayer clip{ nullptr };

                    if (plate->Options.ClipTo != nullptr)
                    {
                        PaintRect area{};

                        if (!Bounds(plate->Options.ClipTo, area))
                        {
                            continue;
                        }

                        clip = ds.CreateLayer(1.0f, wf::Rect{ area.X, area.Y, area.Width, area.Height });
                    }

                    wgc::CanvasActiveLayer fade{ nullptr };

                    if (opacity < 0.999f)
                    {
                        fade = ds.CreateLayer(opacity);
                    }

                    Painter.Paint(ds, creator, *style, rect);

                    if (plate->Options.Decorate)
                    {
                        plate->Options.Decorate(ds, creator, Painter, *Look, *style, rect);
                    }

                    if (fade != nullptr)
                    {
                        fade.Close();
                    }

                    if (clip != nullptr)
                    {
                        clip.Close();
                    }
                }
                catch (...)
                {
                }
            }
        }
    };

    PlateLayer::PlateLayer() : m_state{ std::make_shared<State>() }
    {
    }

    PlateLayer::~PlateLayer()
    {
        Clear();
    }

    _Use_decl_annotations_
    void PlateLayer::Attach(winrt::Microsoft::UI::Xaml::Controls::Panel const& host, uint32_t index)
    {
        auto& state = *m_state;

        if (state.Canvas != nullptr)
        {
            return;
        }

        state.Host = host;
        state.Canvas = wgcx::CanvasControl{};
        state.Canvas.ClearColor(winrt::Microsoft::UI::Colors::Transparent());
        state.Canvas.IsHitTestVisible(false);
        KeepWholePixels(state.Canvas);
        winrt::Microsoft::UI::Xaml::Automation::AutomationProperties::SetAccessibilityView(state.Canvas, winrt::Microsoft::UI::Xaml::Automation::Peers::AccessibilityView::Raw);

        auto const children = host.Children();
        children.InsertAt(std::min(index, children.Size()), state.Canvas);

        auto const weak = std::weak_ptr<State>{ m_state };

        state.Canvas.Draw([weak](wgcx::CanvasControl const& sender, wgcx::CanvasDrawEventArgs const& args)
        {
            if (auto const strong = weak.lock())
            {
                try
                {
                    strong->Draw(sender, args);
                }
                catch (...)
                {
                }
            }
        });

        state.Canvas.CreateResources([weak](wgcx::CanvasControl const&, winrt::Microsoft::Graphics::Canvas::UI::CanvasCreateResourcesEventArgs const&)
        {
            if (auto const strong = weak.lock())
            {
                strong->Painter.Reset();
            }
        });

        state.Canvas.LayoutUpdated([weak](wf::IInspectable const&, wf::IInspectable const&)
        {
            if (auto const strong = weak.lock())
            {
                strong->OnLayout();
            }
        });
    }

    winrt::Microsoft::UI::Xaml::FrameworkElement PlateLayer::Surface() const noexcept
    {
        return m_state->Canvas;
    }

    _Use_decl_annotations_
    bool PlateLayer::Theme(std::shared_ptr<ResolvedTheme const> theme)
    {
        auto& state = *m_state;
        state.Look = std::move(theme);
        state.Layout.clear();

        auto changed = false;

        for (auto const& plate : state.Plates)
        {
            try
            {
                changed = state.StyleElement(*plate) || changed;
            }
            catch (...)
            {
            }
        }

        if (state.Canvas != nullptr)
        {
            state.Canvas.Invalidate();
        }

        return changed;
    }

    std::shared_ptr<ResolvedTheme const> PlateLayer::Theme() const noexcept
    {
        return m_state->Look;
    }

    _Use_decl_annotations_
    bool PlateLayer::Add(winrt::Microsoft::UI::Xaml::FrameworkElement const& element, std::wstring part, PlateOptions options)
    {
        if (element == nullptr)
        {
            return false;
        }

        auto& state = *m_state;
        auto plate = std::make_unique<State::Plate>();
        plate->Id = state.NextId++;
        plate->Element = winrt::make_weak(element);
        plate->Part = std::move(part);
        plate->Options = std::move(options);

        auto& added = *plate;
        state.Plates.push_back(std::move(plate));
        state.Watch(added, element);

        auto const changed = state.StyleElement(added);

        if (state.Canvas != nullptr && state.Look != nullptr)
        {
            state.Canvas.Invalidate();
        }

        return changed;
    }

    void PlateLayer::Clear() noexcept
    {
        auto& state = *m_state;

        for (auto const& plate : state.Plates)
        {
            state.Unwatch(*plate);

            try
            {
                if (auto const element = plate->Element.get())
                {
                    state.Restore(*plate, element);
                }
            }
            catch (...)
            {
            }
        }

        state.Plates.clear();
        state.Layout.clear();

        try
        {
            if (state.Canvas != nullptr)
            {
                state.Canvas.Invalidate();
            }
        }
        catch (...)
        {
        }
    }

    _Use_decl_annotations_
    void PlateLayer::Remove(std::wstring_view group) noexcept
    {
        auto& state = *m_state;
        auto const end = std::stable_partition(state.Plates.begin(), state.Plates.end(), [group](auto const& plate) { return plate->Options.Group != group; });

        for (auto at = end; at != state.Plates.end(); ++at)
        {
            state.Unwatch(**at);

            try
            {
                if (auto const element = (*at)->Element.get())
                {
                    state.Restore(**at, element);
                }
            }
            catch (...)
            {
            }
        }

        state.Plates.erase(end, state.Plates.end());
        state.Layout.clear();
        Invalidate();
    }

    void PlateLayer::Refresh() noexcept
    {
        auto& state = *m_state;

        for (auto const& plate : state.Plates)
        {
            try
            {
                state.StyleElement(*plate);
            }
            catch (...)
            {
            }
        }

        Invalidate();
    }

    void PlateLayer::Invalidate() noexcept
    {
        try
        {
            if (m_state->Canvas != nullptr && m_state->Look != nullptr)
            {
                m_state->Canvas.Invalidate();
            }
        }
        catch (...)
        {
        }
    }
}
