// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "ThemeSyntax.h"

#include <algorithm>
#include <cmath>
#include <format>
#include <functional>
#include <string>

namespace themeengine
{
    namespace
    {
        constexpr size_t MaximumTextLength = 4096;
        constexpr int32_t MaximumDepth = 24;
        constexpr double Pi = 3.14159265358979323846;

        wchar_t AsciiLower(wchar_t c) noexcept
        {
            return (c >= L'A' && c <= L'Z') ? static_cast<wchar_t>(c - L'A' + L'a') : c;
        }

        std::wstring Lower(std::wstring_view text)
        {
            std::wstring result{ text };
            std::transform(result.begin(), result.end(), result.begin(), AsciiLower);
            return result;
        }

        bool IsDigit(wchar_t c) noexcept { return c >= L'0' && c <= L'9'; }
        bool IsLetter(wchar_t c) noexcept { return (c >= L'a' && c <= L'z') || (c >= L'A' && c <= L'Z'); }
        bool IsSpace(wchar_t c) noexcept { return c == L' ' || c == L'\t' || c == L'\r' || c == L'\n'; }
        bool IsNameStart(wchar_t c) noexcept { return IsLetter(c) || c == L'_'; }
        bool IsNameChar(wchar_t c) noexcept { return IsNameStart(c) || IsDigit(c) || c == L'-' || c == L'.'; }

        int32_t HexValue(wchar_t c) noexcept
        {
            if (IsDigit(c)) { return c - L'0'; }
            if (c >= L'a' && c <= L'f') { return c - L'a' + 10; }
            if (c >= L'A' && c <= L'F') { return c - L'A' + 10; }
            return -1;
        }

        enum class TokenKind : int32_t
        {
            End,
            Name,
            Number,
            Hash,
            LeftParen,
            RightParen,
            Comma,
            Slash,
            Plus,
            Minus,
            Star,
        };

        struct Token
        {
            TokenKind Kind{ TokenKind::End };
            std::wstring Text{};
            double Value{ 0 };
            std::wstring Unit{};
            size_t Position{ 0 };

            // a name written straight before "(", which is how CSS writes a function
            bool Function{ false };
        };

        bool Tokenize(std::wstring_view text, std::vector<Token>& tokens, std::wstring& error)
        {
            tokens.clear();

            if (text.size() > MaximumTextLength)
            {
                error = L"The value is too long.";
                return false;
            }

            size_t i = 0;
            auto const n = text.size();

            auto const startsNumber = [&](size_t at) noexcept
            {
                if (at >= n) { return false; }
                if (IsDigit(text[at])) { return true; }
                return text[at] == L'.' && at + 1 < n && IsDigit(text[at + 1]);
            };

            while (i < n)
            {
                auto const c = text[i];

                if (IsSpace(c))
                {
                    ++i;
                    continue;
                }

                Token token{};
                token.Position = i;

                if (c == L'#')
                {
                    auto j = i + 1;

                    while (j < n && HexValue(text[j]) >= 0)
                    {
                        ++j;
                    }

                    token.Kind = TokenKind::Hash;
                    token.Text = std::wstring{ text.substr(i + 1, j - i - 1) };
                    i = j;
                }
                else if (startsNumber(i) || ((c == L'-' || c == L'+') && startsNumber(i + 1)))
                {
                    auto j = i;

                    if (text[j] == L'-' || text[j] == L'+')
                    {
                        ++j;
                    }

                    while (j < n && IsDigit(text[j])) { ++j; }

                    if (j < n && text[j] == L'.')
                    {
                        ++j;
                        while (j < n && IsDigit(text[j])) { ++j; }
                    }

                    // an exponent, but not the start of a unit
                    if (j + 1 < n && (text[j] == L'e' || text[j] == L'E') &&
                        (IsDigit(text[j + 1]) || ((text[j + 1] == L'-' || text[j + 1] == L'+') && j + 2 < n && IsDigit(text[j + 2]))))
                    {
                        j += 2;
                        while (j < n && IsDigit(text[j])) { ++j; }
                    }

                    std::wstring const number{ text.substr(i, j - i) };

                    try
                    {
                        token.Value = std::stod(number);
                    }
                    catch (...)
                    {
                        error = std::format(L"\"{}\" at {} isn't a number.", number, i);
                        return false;
                    }

                    if (!std::isfinite(token.Value))
                    {
                        error = std::format(L"\"{}\" at {} is too large.", number, i);
                        return false;
                    }

                    if (j < n && text[j] == L'%')
                    {
                        token.Unit = L"%";
                        ++j;
                    }
                    else
                    {
                        auto k = j;
                        while (k < n && IsLetter(text[k])) { ++k; }
                        token.Unit = Lower(text.substr(j, k - j));
                        j = k;
                    }

                    if (!token.Unit.empty() && token.Unit != L"%" && token.Unit != L"px" && token.Unit != L"deg" && token.Unit != L"turn" && token.Unit != L"rad")
                    {
                        error = std::format(L"\"{}\" at {} isn't a unit this knows. Use px, %, deg, turn or rad.", token.Unit, j - token.Unit.size());
                        return false;
                    }

                    token.Kind = TokenKind::Number;
                    i = j;
                }
                else if (IsNameStart(c))
                {
                    auto j = i + 1;
                    while (j < n && IsNameChar(text[j])) { ++j; }

                    token.Kind = TokenKind::Name;
                    token.Text = std::wstring{ text.substr(i, j - i) };
                    token.Function = j < n && text[j] == L'(';
                    i = j;
                }
                else
                {
                    switch (c)
                    {
                    case L'(': token.Kind = TokenKind::LeftParen; break;
                    case L')': token.Kind = TokenKind::RightParen; break;
                    case L',': token.Kind = TokenKind::Comma; break;
                    case L'/': token.Kind = TokenKind::Slash; break;
                    case L'+': token.Kind = TokenKind::Plus; break;
                    case L'-': token.Kind = TokenKind::Minus; break;
                    case L'*': token.Kind = TokenKind::Star; break;
                    default:
                        error = std::format(L"'{}' at {} isn't something a theme value can have.", c, i);
                        return false;
                    }

                    ++i;
                }

                tokens.push_back(std::move(token));
            }

            Token end{};
            end.Position = n;
            tokens.push_back(end);
            return true;
        }

        // ---- numbers ----

        enum class NumberOp : int32_t
        {
            Literal,
            Keyword,
            Variable,
            Add,
            Subtract,
            Multiply,
            Divide,
            Negate,
            Minimum,
            Maximum,
            Clamp,
        };

        struct NumberNode
        {
            NumberOp Op{ NumberOp::Literal };
            double Value{ 0 };
            std::wstring Unit{};
            std::wstring Name{};
            std::vector<std::shared_ptr<NumberNode const>> Arguments{};
        };

        // The keywords a relative color's channels can use, and what they stand for.
        struct ChannelKeywords
        {
            std::array<std::wstring_view, 4> Names{};
            std::array<double, 4> Values{};
        };

        struct NumberContext
        {
            ExpressionScope const* Scope{ nullptr };
            ChannelKeywords const* Keywords{ nullptr };

            // what 100% is in this channel
            double PercentOf{ 1.0 };
        };

        double EvaluateNumber(NumberNode const& node, NumberContext const& context) noexcept
        {
            auto const argument = [&](size_t index) -> double
            {
                return index < node.Arguments.size() && node.Arguments[index] != nullptr ? EvaluateNumber(*node.Arguments[index], context) : 0.0;
            };

            switch (node.Op)
            {
            case NumberOp::Literal:
                if (node.Unit == L"%") { return node.Value * context.PercentOf / 100.0; }
                if (node.Unit == L"turn") { return node.Value * 360.0; }
                if (node.Unit == L"rad") { return node.Value * 180.0 / Pi; }
                return node.Value;

            case NumberOp::Keyword:
                if (context.Keywords != nullptr)
                {
                    for (size_t i = 0; i < context.Keywords->Names.size(); ++i)
                    {
                        if (context.Keywords->Names[i] == node.Name)
                        {
                            return context.Keywords->Values[i];
                        }
                    }
                }
                return 0.0;

            case NumberOp::Variable:
                if (context.Scope != nullptr)
                {
                    if (auto const value = context.Scope->NamedNumber(node.Name); value.has_value())
                    {
                        return *value;
                    }
                }
                return 0.0;

            case NumberOp::Add: return argument(0) + argument(1);
            case NumberOp::Subtract: return argument(0) - argument(1);
            case NumberOp::Multiply: return argument(0) * argument(1);

            case NumberOp::Divide:
            {
                auto const divisor = argument(1);
                return divisor == 0.0 ? 0.0 : argument(0) / divisor;
            }

            case NumberOp::Negate: return -argument(0);

            case NumberOp::Minimum:
            case NumberOp::Maximum:
            {
                if (node.Arguments.empty())
                {
                    return 0.0;
                }

                auto result = argument(0);

                for (size_t i = 1; i < node.Arguments.size(); ++i)
                {
                    result = node.Op == NumberOp::Minimum ? std::min(result, argument(i)) : std::max(result, argument(i));
                }

                return result;
            }

            case NumberOp::Clamp:
            {
                auto const low = argument(0);
                auto const value = argument(1);
                auto const high = argument(2);
                return std::max(low, std::min(value, high));
            }
            }

            return 0.0;
        }

        void CollectNumberNames(NumberNode const& node, std::vector<std::wstring>& numbers)
        {
            if (node.Op == NumberOp::Variable)
            {
                numbers.push_back(node.Name);
            }

            for (auto const& argument : node.Arguments)
            {
                if (argument != nullptr)
                {
                    CollectNumberNames(*argument, numbers);
                }
            }
        }
    }

    // ---- colors ----

    enum class ColorOp : int32_t
    {
        Literal,
        Name,
        Rgb,
        Oklch,
        Oklab,
        Srgb,
        Mix,
    };

    struct ColorNode
    {
        ColorOp Op{ ColorOp::Literal };
        Color Value{ 0, 0, 0, 0 };
        std::wstring Name{};

        std::shared_ptr<ColorNode const> Origin{};
        std::array<std::shared_ptr<NumberNode const>, 4> Channels{};

        MixSpace Space{ MixSpace::Srgb };
        std::shared_ptr<ColorNode const> First{};
        std::shared_ptr<ColorNode const> Second{};
        std::shared_ptr<NumberNode const> FirstShare{};
        std::shared_ptr<NumberNode const> SecondShare{};
    };

    namespace
    {
        std::optional<Color> BuiltInColor(std::wstring_view name)
        {
            auto const lower = Lower(name);

            if (lower == L"transparent") { return Color{ 0, 0, 0, 0 }; }
            if (lower == L"white") { return Color{ 1, 1, 1, 1 }; }
            if (lower == L"black") { return Color{ 0, 0, 0, 1 }; }

            return std::nullopt;
        }

        constexpr std::array<std::wstring_view, 4> RgbKeywords{ L"r", L"g", L"b", L"alpha" };
        constexpr std::array<std::wstring_view, 4> LchKeywords{ L"l", L"c", L"h", L"alpha" };
        constexpr std::array<std::wstring_view, 4> LabKeywords{ L"l", L"a", L"b", L"alpha" };

        Color EvaluateColor(ColorNode const& node, ExpressionScope const& scope, int32_t depth) noexcept;

        Color EvaluateChannels(ColorNode const& node, ExpressionScope const& scope, int32_t depth) noexcept
        {
            std::optional<Color> origin{};

            if (node.Origin != nullptr)
            {
                origin = EvaluateColor(*node.Origin, scope, depth + 1);
            }

            ChannelKeywords keywords{};
            std::array<double, 4> percentOf{ 1.0, 1.0, 1.0, 1.0 };

            switch (node.Op)
            {
            case ColorOp::Rgb:
                keywords.Names = RgbKeywords;
                percentOf = { 255.0, 255.0, 255.0, 1.0 };
                if (origin.has_value())
                {
                    keywords.Values = { origin->R * 255.0, origin->G * 255.0, origin->B * 255.0, origin->A };
                }
                break;

            case ColorOp::Srgb:
                keywords.Names = RgbKeywords;
                if (origin.has_value())
                {
                    keywords.Values = { origin->R, origin->G, origin->B, origin->A };
                }
                break;

            case ColorOp::Oklch:
                keywords.Names = LchKeywords;
                percentOf = { 1.0, 0.4, 1.0, 1.0 };
                if (origin.has_value())
                {
                    auto const lch = ToOklch(*origin);
                    keywords.Values = { lch.L, lch.C, lch.H, origin->A };
                }
                break;

            case ColorOp::Oklab:
            default:
                keywords.Names = LabKeywords;
                percentOf = { 1.0, 0.4, 0.4, 1.0 };
                if (origin.has_value())
                {
                    auto const lab = ToOklab(*origin);
                    keywords.Values = { lab.L, lab.A, lab.B, origin->A };
                }
                break;
            }

            std::array<double, 4> values{};

            for (size_t i = 0; i < 4; ++i)
            {
                if (node.Channels[i] == nullptr)
                {
                    // Only the alpha can be left out: the origin's, or opaque.
                    values[i] = i == 3 ? (origin.has_value() ? origin->A : 1.0) : 0.0;
                    continue;
                }

                NumberContext context{ &scope, origin.has_value() ? &keywords : nullptr, percentOf[i] };
                values[i] = EvaluateNumber(*node.Channels[i], context);
            }

            auto const alpha = std::clamp(values[3], 0.0, 1.0);

            switch (node.Op)
            {
            case ColorOp::Rgb:
                return { values[0] / 255.0, values[1] / 255.0, values[2] / 255.0, alpha };

            case ColorOp::Srgb:
                return { values[0], values[1], values[2], alpha };

            case ColorOp::Oklch:
                return FromOklch({ std::max(0.0, values[0]), std::max(0.0, values[1]), values[2] }, alpha);

            case ColorOp::Oklab:
            default:
                return FromOklab({ std::max(0.0, values[0]), values[1], values[2] }, alpha);
            }
        }

        Color EvaluateColor(ColorNode const& node, ExpressionScope const& scope, int32_t depth) noexcept
        {
            if (depth > MaximumDepth)
            {
                return { 0, 0, 0, 0 };
            }

            switch (node.Op)
            {
            case ColorOp::Literal:
                return node.Value;

            case ColorOp::Name:
                if (auto const named = scope.NamedColor(node.Name); named.has_value())
                {
                    return *named;
                }

                return BuiltInColor(node.Name).value_or(Color{ 0, 0, 0, 0 });

            case ColorOp::Mix:
            {
                auto const first = node.First != nullptr ? EvaluateColor(*node.First, scope, depth + 1) : Color{ 0, 0, 0, 0 };
                auto const second = node.Second != nullptr ? EvaluateColor(*node.Second, scope, depth + 1) : Color{ 0, 0, 0, 0 };

                std::optional<double> firstShare{};
                std::optional<double> secondShare{};
                NumberContext context{ &scope, nullptr, 1.0 };

                if (node.FirstShare != nullptr) { firstShare = std::clamp(EvaluateNumber(*node.FirstShare, context), 0.0, 1.0); }
                if (node.SecondShare != nullptr) { secondShare = std::clamp(EvaluateNumber(*node.SecondShare, context), 0.0, 1.0); }

                return MixColors(node.Space, first, firstShare, second, secondShare);
            }

            default:
                return EvaluateChannels(node, scope, depth);
            }
        }

        void CollectColorNames(ColorNode const& node, std::vector<std::wstring>& colors, std::vector<std::wstring>& numbers)
        {
            if (node.Op == ColorOp::Name && !BuiltInColor(node.Name).has_value())
            {
                colors.push_back(node.Name);
            }

            for (auto const& child : { node.Origin, node.First, node.Second })
            {
                if (child != nullptr)
                {
                    CollectColorNames(*child, colors, numbers);
                }
            }

            for (auto const& channel : node.Channels)
            {
                if (channel != nullptr)
                {
                    CollectNumberNames(*channel, numbers);
                }
            }

            for (auto const& share : { node.FirstShare, node.SecondShare })
            {
                if (share != nullptr)
                {
                    CollectNumberNames(*share, numbers);
                }
            }
        }
    }

    // ---- the parser ----

    class SyntaxParser
    {
    public:
        explicit SyntaxParser(std::vector<Token> tokens) : m_tokens(std::move(tokens)) {}

        std::wstring const& Error() const noexcept { return m_error; }
        bool Failed() const noexcept { return !m_error.empty(); }

        bool AtEnd() const noexcept { return Peek().Kind == TokenKind::End; }
        Token const& Peek(size_t ahead = 0) const noexcept
        {
            auto const at = std::min(m_index + ahead, m_tokens.size() - 1);
            return m_tokens[at];
        }

        Token const& Advance() noexcept
        {
            auto const& token = m_tokens[std::min(m_index, m_tokens.size() - 1)];

            if (m_index < m_tokens.size() - 1)
            {
                ++m_index;
            }

            return token;
        }

        bool Accept(TokenKind kind) noexcept
        {
            if (Peek().Kind == kind)
            {
                Advance();
                return true;
            }

            return false;
        }

        bool AcceptName(std::wstring_view lowerName) noexcept
        {
            if (Peek().Kind == TokenKind::Name && Lower(Peek().Text) == lowerName)
            {
                Advance();
                return true;
            }

            return false;
        }

        bool Expect(TokenKind kind, std::wstring_view what)
        {
            if (Accept(kind))
            {
                return true;
            }

            Fail(std::format(L"Expected {} at {}.", what, Peek().Position));
            return false;
        }

        void Fail(std::wstring message)
        {
            if (m_error.empty())
            {
                m_error = std::move(message);
            }
        }

        bool StartsColor() const noexcept
        {
            auto const& token = Peek();
            return token.Kind == TokenKind::Hash || token.Kind == TokenKind::Name;
        }

        std::shared_ptr<ColorNode const> ParseColor()
        {
            DepthGuard guard{ *this };

            if (Failed())
            {
                return nullptr;
            }

            auto const& token = Peek();

            if (token.Kind == TokenKind::Hash)
            {
                return ParseHash();
            }

            if (token.Kind != TokenKind::Name)
            {
                Fail(std::format(L"Expected a color at {}.", token.Position));
                return nullptr;
            }

            auto const name = Lower(token.Text);

            if (token.Function)
            {
                if (name == L"rgb" || name == L"rgba") { return ParseChannels(ColorOp::Rgb); }
                if (name == L"oklch") { return ParseChannels(ColorOp::Oklch); }
                if (name == L"oklab") { return ParseChannels(ColorOp::Oklab); }
                if (name == L"color") { return ParseChannels(ColorOp::Srgb); }
                if (name == L"color-mix") { return ParseMix(); }

                Fail(std::format(L"\"{}()\" at {} isn't a color function this knows.", token.Text, token.Position));
                return nullptr;
            }

            Advance();

            auto node = std::make_shared<ColorNode>();

            if (auto const builtIn = BuiltInColor(token.Text); builtIn.has_value())
            {
                node->Op = ColorOp::Literal;
                node->Value = *builtIn;
            }
            else
            {
                node->Op = ColorOp::Name;
                node->Name = token.Text;
            }

            return node;
        }

        // One CSS component value: a number, a name, a math function, or a parenthesized sum.
        std::shared_ptr<NumberNode const> ParseComponent(std::array<std::wstring_view, 4> const* keywords)
        {
            DepthGuard guard{ *this };

            if (Failed())
            {
                return nullptr;
            }

            auto const& token = Peek();

            if (token.Kind == TokenKind::Number)
            {
                Advance();
                auto node = std::make_shared<NumberNode>();
                node->Op = NumberOp::Literal;
                node->Value = token.Value;
                node->Unit = token.Unit;
                return node;
            }

            if (token.Kind == TokenKind::Minus)
            {
                Advance();
                auto node = std::make_shared<NumberNode>();
                node->Op = NumberOp::Negate;
                node->Arguments.push_back(ParseComponent(keywords));
                return node;
            }

            if (token.Kind == TokenKind::LeftParen)
            {
                Advance();
                auto inner = ParseSum(keywords);
                Expect(TokenKind::RightParen, L"\")\"");
                return inner;
            }

            if (token.Kind == TokenKind::Name)
            {
                auto const name = Lower(token.Text);

                if (token.Function)
                {
                    Advance();
                    Advance();

                    auto node = std::make_shared<NumberNode>();

                    if (name == L"calc")
                    {
                        auto inner = ParseSum(keywords);
                        Expect(TokenKind::RightParen, L"\")\"");
                        return inner;
                    }

                    if (name == L"min") { node->Op = NumberOp::Minimum; }
                    else if (name == L"max") { node->Op = NumberOp::Maximum; }
                    else if (name == L"clamp") { node->Op = NumberOp::Clamp; }
                    else
                    {
                        Fail(std::format(L"\"{}()\" at {} isn't a math function this knows.", token.Text, token.Position));
                        return nullptr;
                    }

                    do
                    {
                        node->Arguments.push_back(ParseSum(keywords));
                    } while (!Failed() && Accept(TokenKind::Comma));

                    Expect(TokenKind::RightParen, L"\")\"");

                    if (node->Op == NumberOp::Clamp && node->Arguments.size() != 3)
                    {
                        Fail(std::format(L"clamp() at {} takes three values.", token.Position));
                    }

                    return node;
                }

                Advance();
                auto node = std::make_shared<NumberNode>();

                if (name == L"none")
                {
                    node->Op = NumberOp::Literal;
                    return node;
                }

                if (keywords != nullptr && std::find(keywords->begin(), keywords->end(), name) != keywords->end())
                {
                    node->Op = NumberOp::Keyword;
                    node->Name = name;
                    return node;
                }

                node->Op = NumberOp::Variable;
                node->Name = token.Text;
                return node;
            }

            Fail(std::format(L"Expected a number at {}.", token.Position));
            return nullptr;
        }

        std::shared_ptr<NumberNode const> ParseSum(std::array<std::wstring_view, 4> const* keywords)
        {
            auto left = ParseProduct(keywords);

            while (!Failed() && (Peek().Kind == TokenKind::Plus || Peek().Kind == TokenKind::Minus))
            {
                auto const op = Advance().Kind == TokenKind::Plus ? NumberOp::Add : NumberOp::Subtract;
                auto node = std::make_shared<NumberNode>();
                node->Op = op;
                node->Arguments.push_back(left);
                node->Arguments.push_back(ParseProduct(keywords));
                left = node;
            }

            return left;
        }

        std::shared_ptr<NumberNode const> ParseProduct(std::array<std::wstring_view, 4> const* keywords)
        {
            auto left = ParseComponent(keywords);

            while (!Failed() && (Peek().Kind == TokenKind::Star || Peek().Kind == TokenKind::Slash))
            {
                auto const op = Advance().Kind == TokenKind::Star ? NumberOp::Multiply : NumberOp::Divide;
                auto node = std::make_shared<NumberNode>();
                node->Op = op;
                node->Arguments.push_back(left);
                node->Arguments.push_back(ParseComponent(keywords));
                left = node;
            }

            return left;
        }

        // A share in color-mix() or a gradient stop position: a percentage.
        std::optional<double> TryParsePercent()
        {
            if (Peek().Kind == TokenKind::Number && Peek().Unit == L"%")
            {
                return Advance().Value / 100.0;
            }

            return std::nullopt;
        }

        // A length: a number with px or no unit.
        bool TryParseLength(double& value)
        {
            if (Peek().Kind == TokenKind::Number && (Peek().Unit.empty() || Peek().Unit == L"px"))
            {
                value = Advance().Value;
                return true;
            }

            return false;
        }

    private:
        struct DepthGuard
        {
            explicit DepthGuard(SyntaxParser& parser) : Parser(parser)
            {
                if (++Parser.m_depth > MaximumDepth)
                {
                    Parser.Fail(L"The value is nested too deeply.");
                }
            }

            ~DepthGuard() { --Parser.m_depth; }

            DepthGuard(DepthGuard const&) = delete;
            DepthGuard& operator=(DepthGuard const&) = delete;

            SyntaxParser& Parser;
        };

        std::shared_ptr<ColorNode const> ParseHash()
        {
            auto const& token = Advance();
            auto const& digits = token.Text;

            std::array<int32_t, 4> channels{ 0, 0, 0, 255 };

            if (digits.size() == 3 || digits.size() == 4)
            {
                for (size_t i = 0; i < digits.size(); ++i)
                {
                    channels[i] = HexValue(digits[i]) * 17;
                }
            }
            else if (digits.size() == 6 || digits.size() == 8)
            {
                for (size_t i = 0; i < digits.size() / 2; ++i)
                {
                    channels[i] = HexValue(digits[i * 2]) * 16 + HexValue(digits[i * 2 + 1]);
                }
            }
            else
            {
                Fail(std::format(L"\"#{}\" at {} needs 3, 4, 6 or 8 hex digits (CSS order: RRGGBB or RRGGBBAA).", digits, token.Position));
                return nullptr;
            }

            auto node = std::make_shared<ColorNode>();
            node->Op = ColorOp::Literal;
            node->Value = { channels[0] / 255.0, channels[1] / 255.0, channels[2] / 255.0, channels[3] / 255.0 };
            return node;
        }

        std::shared_ptr<ColorNode const> ParseChannels(ColorOp op)
        {
            auto const& function = Advance();
            Advance();

            auto node = std::make_shared<ColorNode>();
            node->Op = op;

            if (AcceptName(L"from"))
            {
                node->Origin = ParseColor();
            }

            if (op == ColorOp::Srgb && !AcceptName(L"srgb"))
            {
                Fail(std::format(L"color() at {} only takes srgb.", function.Position));
                return nullptr;
            }

            auto const* keywords =
                op == ColorOp::Oklch ? &LchKeywords :
                op == ColorOp::Oklab ? &LabKeywords : &RgbKeywords;

            auto const relative = node->Origin != nullptr ? keywords : nullptr;

            node->Channels[0] = ParseComponent(relative);

            // rgb(r, g, b, a), the older way to write one
            if (op == ColorOp::Rgb && Accept(TokenKind::Comma))
            {
                node->Channels[1] = ParseComponent(relative);
                Expect(TokenKind::Comma, L"\",\"");
                node->Channels[2] = ParseComponent(relative);

                if (Accept(TokenKind::Comma))
                {
                    node->Channels[3] = ParseComponent(relative);
                }
            }
            else
            {
                node->Channels[1] = ParseComponent(relative);
                node->Channels[2] = ParseComponent(relative);

                if (Accept(TokenKind::Slash))
                {
                    node->Channels[3] = ParseComponent(relative);
                }
            }

            Expect(TokenKind::RightParen, std::format(L"\")\" to close {}()", function.Text));
            return Failed() ? nullptr : node;
        }

        std::shared_ptr<ColorNode const> ParseMix()
        {
            auto const& function = Advance();
            Advance();

            auto node = std::make_shared<ColorNode>();
            node->Op = ColorOp::Mix;

            if (!AcceptName(L"in"))
            {
                Fail(std::format(L"color-mix() at {} starts with \"in\" and a color space.", function.Position));
                return nullptr;
            }

            auto const space = Peek().Kind == TokenKind::Name ? Lower(Advance().Text) : std::wstring{};

            if (space == L"srgb") { node->Space = MixSpace::Srgb; }
            else if (space == L"srgb-linear") { node->Space = MixSpace::SrgbLinear; }
            else if (space == L"oklab") { node->Space = MixSpace::Oklab; }
            else if (space == L"oklch") { node->Space = MixSpace::Oklch; }
            else
            {
                Fail(std::format(L"color-mix() at {} mixes in srgb, srgb-linear, oklab or oklch.", function.Position));
                return nullptr;
            }

            // "shorter hue" is the only way round this does, and CSS's default
            if (AcceptName(L"shorter"))
            {
                AcceptName(L"hue");
            }

            auto const parseItem = [this](std::shared_ptr<ColorNode const>& color, std::shared_ptr<NumberNode const>& share)
            {
                if (Peek().Kind == TokenKind::Number)
                {
                    share = ParseComponent(nullptr);
                    color = ParseColor();
                }
                else
                {
                    color = ParseColor();

                    if (Peek().Kind == TokenKind::Number || (Peek().Kind == TokenKind::Name && Peek().Function))
                    {
                        share = ParseComponent(nullptr);
                    }
                }
            };

            Expect(TokenKind::Comma, L"\",\"");
            parseItem(node->First, node->FirstShare);
            Expect(TokenKind::Comma, L"\",\"");
            parseItem(node->Second, node->SecondShare);
            Expect(TokenKind::RightParen, L"\")\" to close color-mix()");

            return Failed() ? nullptr : node;
        }

        std::vector<Token> m_tokens{};
        size_t m_index{ 0 };
        int32_t m_depth{ 0 };
        std::wstring m_error{};
    };

    // ---- ColorExpression ----

    _Use_decl_annotations_
    ColorExpression ColorExpression::FromColor(Color const& color)
    {
        auto node = std::make_shared<ColorNode>();
        node->Op = ColorOp::Literal;
        node->Value = color;

        ColorExpression expression{};
        expression.m_root = node;
        return expression;
    }

    _Use_decl_annotations_
    std::optional<ColorExpression> ColorExpression::Parse(std::wstring_view text, std::wstring& error)
    {
        error.clear();

        std::vector<Token> tokens{};

        if (!Tokenize(text, tokens, error))
        {
            return std::nullopt;
        }

        SyntaxParser parser{ std::move(tokens) };
        auto root = parser.ParseColor();

        if (!parser.Failed() && !parser.AtEnd())
        {
            parser.Fail(std::format(L"Unexpected text at {} after the color.", parser.Peek().Position));
        }

        if (parser.Failed() || root == nullptr)
        {
            error = parser.Failed() ? parser.Error() : std::wstring{ L"Expected a color." };
            return std::nullopt;
        }

        ColorExpression expression{};
        expression.m_root = std::move(root);
        expression.m_text = std::wstring{ text };
        return expression;
    }

    _Use_decl_annotations_
    Color ColorExpression::Evaluate(ExpressionScope const& scope) const noexcept
    {
        return m_root != nullptr ? EvaluateColor(*m_root, scope, 0) : Color{ 0, 0, 0, 0 };
    }

    _Use_decl_annotations_
    void ColorExpression::CollectNames(std::vector<std::wstring>& colors, std::vector<std::wstring>& numbers) const
    {
        if (m_root != nullptr)
        {
            CollectColorNames(*m_root, colors, numbers);
        }
    }

    _Use_decl_annotations_
    bool ColorExpression::RefersTo(std::wstring_view colorName) const
    {
        std::vector<std::wstring> colors{};
        std::vector<std::wstring> numbers{};
        CollectNames(colors, numbers);

        return std::find(colors.begin(), colors.end(), colorName) != colors.end();
    }

    // ---- style values ----

    // Wraps a color the parser has just read into an expression of its own.
    ColorExpression MakeExpressionFromNode(std::shared_ptr<ColorNode const> root)
    {
        ColorExpression expression{};
        expression.m_root = std::move(root);
        return expression;
    }

    namespace
    {
        ColorExpression MakeExpression(std::shared_ptr<ColorNode const> root, std::wstring_view)
        {
            return MakeExpressionFromNode(std::move(root));
        }

        bool ParseStops(SyntaxParser& parser, std::vector<GradientStop>& stops)
        {
            do
            {
                auto color = parser.ParseColor();

                if (color == nullptr)
                {
                    return false;
                }

                GradientStop stop{};
                stop.Color = MakeExpression(color, {});
                stop.Position = parser.TryParsePercent();
                stops.push_back(std::move(stop));
            } while (parser.Accept(TokenKind::Comma));

            if (stops.size() < 2)
            {
                parser.Fail(L"A gradient needs at least two colors.");
                return false;
            }

            return true;
        }

        bool ParsePosition(SyntaxParser& parser, double& x, double& y)
        {
            auto const one = [&parser](double& value, bool horizontal) -> bool
            {
                if (auto const percent = parser.TryParsePercent(); percent.has_value())
                {
                    value = *percent;
                    return true;
                }

                if (parser.Peek().Kind != TokenKind::Name)
                {
                    return false;
                }

                auto const word = Lower(parser.Peek().Text);

                if (word == L"center") { value = 0.5; }
                else if (horizontal && word == L"left") { value = 0.0; }
                else if (horizontal && word == L"right") { value = 1.0; }
                else if (!horizontal && word == L"top") { value = 0.0; }
                else if (!horizontal && word == L"bottom") { value = 1.0; }
                else { return false; }

                parser.Advance();
                return true;
            };

            if (!one(x, true))
            {
                parser.Fail(std::format(L"Expected a position at {}.", parser.Peek().Position));
                return false;
            }

            if (!one(y, false))
            {
                y = 0.5;
            }

            return true;
        }

        bool ParseLayer(SyntaxParser& parser, BackgroundLayer& layer)
        {
            auto const& token = parser.Peek();
            auto const name = token.Kind == TokenKind::Name ? Lower(token.Text) : std::wstring{};

            if (token.Kind == TokenKind::Name && token.Function && name == L"linear-gradient")
            {
                parser.Advance();
                parser.Advance();

                layer.Kind = BackgroundKind::Linear;

                if (parser.Peek().Kind == TokenKind::Number && (parser.Peek().Unit == L"deg" || parser.Peek().Unit == L"turn" || parser.Peek().Unit == L"rad"))
                {
                    auto const& angle = parser.Advance();
                    layer.Angle = angle.Unit == L"turn" ? angle.Value * 360.0 : angle.Unit == L"rad" ? angle.Value * 180.0 / Pi : angle.Value;
                    parser.Expect(TokenKind::Comma, L"\",\"");
                }
                else if (parser.AcceptName(L"to"))
                {
                    double x = 0.0;
                    double y = 0.0;

                    for (int i = 0; i < 2 && parser.Peek().Kind == TokenKind::Name; ++i)
                    {
                        auto const side = Lower(parser.Peek().Text);

                        if (side == L"top") { y = -1.0; }
                        else if (side == L"bottom") { y = 1.0; }
                        else if (side == L"left") { x = -1.0; }
                        else if (side == L"right") { x = 1.0; }
                        else { break; }

                        parser.Advance();
                    }

                    layer.Angle = std::fmod(std::atan2(x, -y) * 180.0 / Pi + 360.0, 360.0);
                    parser.Expect(TokenKind::Comma, L"\",\"");
                }

                if (!ParseStops(parser, layer.Stops))
                {
                    return false;
                }

                return parser.Expect(TokenKind::RightParen, L"\")\" to close linear-gradient()");
            }

            if (token.Kind == TokenKind::Name && token.Function && name == L"radial-gradient")
            {
                parser.Advance();
                parser.Advance();

                layer.Kind = BackgroundKind::Radial;
                auto shaped = false;

                while (!parser.Failed())
                {
                    if (parser.AcceptName(L"circle"))
                    {
                        layer.Circle = true;
                        shaped = true;
                    }
                    else if (parser.AcceptName(L"ellipse") || parser.AcceptName(L"farthest-corner"))
                    {
                        shaped = true;
                    }
                    else if (auto const percent = parser.TryParsePercent(); percent.has_value())
                    {
                        if (!layer.SizeX.has_value()) { layer.SizeX = *percent; }
                        else { layer.SizeY = *percent; }
                        shaped = true;
                    }
                    else if (parser.AcceptName(L"at"))
                    {
                        if (!ParsePosition(parser, layer.CenterX, layer.CenterY))
                        {
                            return false;
                        }
                        shaped = true;
                    }
                    else
                    {
                        break;
                    }
                }

                if (layer.SizeX.has_value() && !layer.SizeY.has_value())
                {
                    layer.SizeY = layer.SizeX;
                }

                if (shaped)
                {
                    parser.Expect(TokenKind::Comma, L"\",\"");
                }

                if (!ParseStops(parser, layer.Stops))
                {
                    return false;
                }

                return parser.Expect(TokenKind::RightParen, L"\")\" to close radial-gradient()");
            }

            auto color = parser.ParseColor();

            if (color == nullptr)
            {
                return false;
            }

            layer.Kind = BackgroundKind::Solid;
            layer.Color = MakeExpression(color, {});
            return true;
        }

        bool ParseWithTokens(std::wstring_view text, std::wstring& error, std::function<bool(SyntaxParser&)> const& body)
        {
            error.clear();

            std::vector<Token> tokens{};

            if (!Tokenize(text, tokens, error))
            {
                return false;
            }

            SyntaxParser parser{ std::move(tokens) };
            auto const ok = body(parser);

            if (ok && !parser.Failed() && !parser.AtEnd())
            {
                parser.Fail(std::format(L"Unexpected text at {}.", parser.Peek().Position));
            }

            if (parser.Failed() || !ok)
            {
                error = parser.Failed() ? parser.Error() : std::wstring{ L"The value couldn't be read." };
                return false;
            }

            return true;
        }

        bool IsNone(std::wstring_view text) noexcept
        {
            auto start = text.find_first_not_of(L" \t\r\n");
            auto end = text.find_last_not_of(L" \t\r\n");

            if (start == std::wstring_view::npos)
            {
                return false;
            }

            return Lower(text.substr(start, end - start + 1)) == L"none";
        }
    }

    _Use_decl_annotations_
    bool ParseBackground(std::wstring_view text, std::vector<BackgroundLayer>& layers, std::wstring& error)
    {
        layers.clear();

        if (IsNone(text))
        {
            error.clear();
            return true;
        }

        return ParseWithTokens(text, error, [&layers](SyntaxParser& parser)
        {
            do
            {
                BackgroundLayer layer{};

                if (!ParseLayer(parser, layer))
                {
                    return false;
                }

                layers.push_back(std::move(layer));
            } while (parser.Accept(TokenKind::Comma));

            return true;
        });
    }

    _Use_decl_annotations_
    bool ParseShadows(std::wstring_view text, std::vector<ShadowLayer>& shadows, std::wstring& error)
    {
        shadows.clear();

        if (IsNone(text))
        {
            error.clear();
            return true;
        }

        return ParseWithTokens(text, error, [&shadows](SyntaxParser& parser)
        {
            do
            {
                ShadowLayer shadow{};
                std::vector<double> lengths{};
                auto hasColor = false;
                auto const start = parser.Peek().Position;

                // A bare name with nothing else is a shadow of the theme's own.
                if (parser.Peek().Kind == TokenKind::Name && !parser.Peek().Function && Lower(parser.Peek().Text) != L"inset" &&
                    (parser.Peek(1).Kind == TokenKind::Comma || parser.Peek(1).Kind == TokenKind::End))
                {
                    shadow.Reference = parser.Advance().Text;
                    shadows.push_back(std::move(shadow));
                    continue;
                }

                while (!parser.Failed() && parser.Peek().Kind != TokenKind::Comma && parser.Peek().Kind != TokenKind::End)
                {
                    double length{};

                    if (parser.AcceptName(L"inset"))
                    {
                        shadow.Inset = true;
                    }
                    else if (parser.TryParseLength(length))
                    {
                        lengths.push_back(length);
                    }
                    else if (!hasColor && parser.StartsColor())
                    {
                        auto color = parser.ParseColor();

                        if (color == nullptr)
                        {
                            return false;
                        }

                        shadow.Color = MakeExpression(color, {});
                        hasColor = true;
                    }
                    else
                    {
                        parser.Fail(std::format(L"Unexpected text at {} in a shadow.", parser.Peek().Position));
                        return false;
                    }
                }

                if (lengths.size() < 2 || lengths.size() > 4)
                {
                    parser.Fail(std::format(L"The shadow at {} needs two to four lengths: x, y, blur and spread.", start));
                    return false;
                }

                if (!hasColor)
                {
                    parser.Fail(std::format(L"The shadow at {} needs a color.", start));
                    return false;
                }

                shadow.X = lengths[0];
                shadow.Y = lengths[1];
                shadow.Blur = lengths.size() > 2 ? std::max(0.0, lengths[2]) : 0.0;
                shadow.Spread = lengths.size() > 3 ? lengths[3] : 0.0;
                shadows.push_back(std::move(shadow));
            } while (parser.Accept(TokenKind::Comma));

            return true;
        });
    }

    _Use_decl_annotations_
    bool ParseLine(std::wstring_view text, Line& line, std::wstring& error)
    {
        line = Line{};

        if (IsNone(text))
        {
            error.clear();
            return true;
        }

        return ParseWithTokens(text, error, [&line](SyntaxParser& parser)
        {
            auto hasWidth = false;
            auto hasColor = false;
            line.Style = LineStyle::Solid;
            line.Color = ColorExpression::FromColor({ 0, 0, 0, 1 });

            while (!parser.Failed() && !parser.AtEnd())
            {
                double width{};

                if (!hasWidth && parser.TryParseLength(width))
                {
                    line.Width = std::max(0.0, width);
                    hasWidth = true;
                }
                else if (parser.AcceptName(L"solid")) { line.Style = LineStyle::Solid; }
                else if (parser.AcceptName(L"dashed")) { line.Style = LineStyle::Dashed; }
                else if (parser.AcceptName(L"dotted")) { line.Style = LineStyle::Dotted; }
                else if (parser.AcceptName(L"none")) { line.Style = LineStyle::None; }
                else if (!hasColor && parser.StartsColor())
                {
                    auto color = parser.ParseColor();

                    if (color == nullptr)
                    {
                        return false;
                    }

                    line.Color = MakeExpression(color, {});
                    hasColor = true;
                }
                else
                {
                    parser.Fail(std::format(L"Unexpected text at {} in a line.", parser.Peek().Position));
                    return false;
                }
            }

            if (!hasWidth)
            {
                parser.Fail(L"A line needs a width.");
                return false;
            }

            return true;
        });
    }

    _Use_decl_annotations_
    bool ParseCorners(std::wstring_view text, Corners& corners, std::wstring& error)
    {
        corners = {};

        return ParseWithTokens(text, error, [&corners](SyntaxParser& parser)
        {
            std::vector<Corner> values{};

            while (!parser.Failed() && !parser.AtEnd() && values.size() < 4)
            {
                if (auto const percent = parser.TryParsePercent(); percent.has_value())
                {
                    values.push_back({ *percent, true });
                    continue;
                }

                double length{};

                if (!parser.TryParseLength(length))
                {
                    parser.Fail(std::format(L"Expected a radius at {}.", parser.Peek().Position));
                    return false;
                }

                values.push_back({ std::max(0.0, length), false });
            }

            if (values.empty())
            {
                parser.Fail(L"Expected a radius.");
                return false;
            }

            // CSS's shorthand: one for all, then top left and bottom right with the other two.
            switch (values.size())
            {
            case 1: corners = { values[0], values[0], values[0], values[0] }; break;
            case 2: corners = { values[0], values[1], values[0], values[1] }; break;
            case 3: corners = { values[0], values[1], values[2], values[1] }; break;
            default: corners = { values[0], values[1], values[2], values[3] }; break;
            }

            return true;
        });
    }

    _Use_decl_annotations_
    bool ParseEdges(std::wstring_view text, Edges& edges, std::wstring& error)
    {
        edges = {};

        return ParseWithTokens(text, error, [&edges](SyntaxParser& parser)
        {
            std::vector<double> values{};

            while (!parser.Failed() && !parser.AtEnd() && values.size() < 4)
            {
                double length{};

                if (!parser.TryParseLength(length))
                {
                    parser.Fail(std::format(L"Expected a length at {}.", parser.Peek().Position));
                    return false;
                }

                values.push_back(length);
            }

            switch (values.size())
            {
            case 0: parser.Fail(L"Expected a length."); return false;
            case 1: edges = { values[0], values[0], values[0], values[0] }; break;
            case 2: edges = { values[0], values[1], values[0], values[1] }; break;
            case 3: edges = { values[0], values[1], values[2], values[1] }; break;
            default: edges = { values[0], values[1], values[2], values[3] }; break;
            }

            return true;
        });
    }

    _Use_decl_annotations_
    bool ParseLength(std::wstring_view text, double& value, std::wstring& error)
    {
        value = 0;

        return ParseWithTokens(text, error, [&value](SyntaxParser& parser)
        {
            if (!parser.TryParseLength(value))
            {
                parser.Fail(std::format(L"Expected a length at {}.", parser.Peek().Position));
                return false;
            }

            return true;
        });
    }

    _Use_decl_annotations_
    bool ParseFilter(std::wstring_view text, FilterValue& filter, std::wstring& error)
    {
        filter = {};

        if (IsNone(text))
        {
            error.clear();
            return true;
        }

        return ParseWithTokens(text, error, [&filter](SyntaxParser& parser)
        {
            while (!parser.Failed() && !parser.AtEnd())
            {
                auto const& token = parser.Peek();
                auto const name = token.Kind == TokenKind::Name && token.Function ? Lower(token.Text) : std::wstring{};

                if (name != L"saturate" && name != L"brightness")
                {
                    parser.Fail(std::format(L"Expected saturate() or brightness() at {}.", token.Position));
                    return false;
                }

                parser.Advance();
                parser.Advance();

                auto const& amount = parser.Peek();

                if (amount.Kind != TokenKind::Number || (!amount.Unit.empty() && amount.Unit != L"%"))
                {
                    parser.Fail(std::format(L"Expected an amount at {}.", amount.Position));
                    return false;
                }

                auto const value = amount.Unit == L"%" ? amount.Value / 100.0 : amount.Value;
                parser.Advance();
                parser.Expect(TokenKind::RightParen, L"\")\"");

                (name == L"saturate" ? filter.Saturate : filter.Brightness) = std::max(0.0, value);
            }

            return true;
        });
    }

    _Use_decl_annotations_
    bool ParseFontWeight(std::wstring_view text, int32_t& weight, std::wstring& error)
    {
        error.clear();
        weight = 400;

        auto const lower = Lower(text);

        static constexpr std::pair<std::wstring_view, int32_t> names[]{
            { L"thin", 100 }, { L"extralight", 200 }, { L"light", 300 }, { L"semilight", 350 }, { L"normal", 400 },
            { L"regular", 400 }, { L"medium", 500 }, { L"semibold", 600 }, { L"bold", 700 }, { L"extrabold", 800 }, { L"black", 900 } };

        for (auto const& [name, value] : names)
        {
            if (lower == name)
            {
                weight = value;
                return true;
            }
        }

        try
        {
            size_t used{};
            auto const value = std::stoi(lower, &used);

            if (used == lower.size() && value >= 1 && value <= 1000)
            {
                weight = value;
                return true;
            }
        }
        catch (...)
        {
        }

        error = std::format(L"\"{}\" isn't a font weight.", text);
        return false;
    }

    _Use_decl_annotations_
    bool IsName(std::wstring_view text) noexcept
    {
        if (text.empty() || !IsNameStart(text.front()))
        {
            return false;
        }

        return std::all_of(text.begin(), text.end(), IsNameChar);
    }
}
