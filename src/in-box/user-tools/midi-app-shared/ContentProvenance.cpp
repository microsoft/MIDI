// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "ContentProvenance.h"

#include <windows.h>

// windows.h defines this as GetObjectW, which turns IJsonValue::GetObject() into a compile error.
#undef GetObject

#include <algorithm>
#include <cmath>
#include <cwchar>
#include <cwctype>
#include <format>
#include <vector>

namespace midiapp
{
    namespace
    {
        namespace mjson = winrt::Windows::Data::Json;

        constexpr wchar_t KeyId[] = L"id";
        constexpr wchar_t KeyVersion[] = L"version";
        constexpr wchar_t KeyAuthor[] = L"author";
        constexpr wchar_t KeyOrganization[] = L"organization";
        constexpr wchar_t KeyUrl[] = L"url";
        constexpr wchar_t KeyLicense[] = L"license";
        constexpr wchar_t KeyCreated[] = L"created";
        constexpr wchar_t KeyTool[] = L"tool";
        constexpr wchar_t KeyDigitalSourceType[] = L"digitalSourceType";
        constexpr wchar_t KeyAiDisclosure[] = L"aiDisclosure";
        constexpr wchar_t KeyHumanOversightLevel[] = L"humanOversightLevel";
        constexpr wchar_t KeyModelName[] = L"modelName";
        constexpr wchar_t KeyBasedOn[] = L"basedOn";
        constexpr wchar_t KeyName[] = L"name";
        constexpr wchar_t KeyBuiltIn[] = L"builtIn";

        constexpr wchar_t IptcPrefix[] = L"http://cv.iptc.org/newscodes/digitalsourcetype/";

        constexpr wchar_t IndentUnit[] = L"  ";

        constexpr wchar_t SharedSettingsKeyPath[] = L"Software\\Microsoft\\Windows MIDI Services\\Tools\\Shared";
        constexpr wchar_t ValueAuthorName[] = L"AuthorName";
        constexpr wchar_t ValueAuthorOrganization[] = L"AuthorOrganization";
        constexpr wchar_t ValueAuthorUrl[] = L"AuthorUrl";
        constexpr wchar_t ValueAuthorLicense[] = L"AuthorLicense";

        constexpr size_t MaximumShortFieldLength = 64;

        bool IsDisguisingCharacter(_In_ char32_t c) noexcept
        {
            if (c < 0x20 || (c >= 0x7F && c <= 0x9F))
            {
                return true;
            }

            switch (c)
            {
            case 0x00AD:
            case 0x061C:
            case 0x180E:
            case 0x200B:
            case 0x200E:
            case 0x200F:
            case 0x2028:
            case 0x2029:
            case 0x2060:
            case 0xFEFF:
                return true;
            default:
                break;
            }

            // Embeddings and overrides, invisible operators and isolates, and tag characters.
            return (c >= 0x202A && c <= 0x202E) ||
                (c >= 0x2061 && c <= 0x2069) ||
                (c >= 0xE0000 && c <= 0xE007F);
        }

        std::wstring QuoteJson(_In_ std::wstring_view value) noexcept
        {
            try
            {
                return std::wstring{ mjson::JsonValue::CreateStringValue(winrt::hstring{ value }).Stringify() };
            }
            catch (...)
            {
                return L"\"\"";
            }
        }

        std::wstring FormatNumber(_In_ double value) noexcept
        {
            if (!std::isfinite(value))
            {
                return L"0";
            }

            if (value == std::floor(value) && std::fabs(value) < 9.0e15)
            {
                return std::format(L"{}", static_cast<int64_t>(value));
            }

            return std::format(L"{}", value);
        }

        std::wstring Indent(_In_ int32_t depth) noexcept
        {
            std::wstring indent{};

            for (int32_t i = 0; i < depth; ++i)
            {
                indent += IndentUnit;
            }

            return indent;
        }

        // Matches the sorted-key form MIDI Glass writes unknown fields in.
        std::wstring CanonicalText(_In_ mjson::IJsonValue const& value, _In_ int32_t depth) noexcept
        {
            try
            {
                if (value == nullptr)
                {
                    return L"null";
                }

                auto const indent = Indent(depth);
                auto const inner = indent + IndentUnit;

                switch (value.ValueType())
                {
                case mjson::JsonValueType::Object:
                {
                    auto const object = value.GetObject();
                    std::vector<std::wstring> keys{};

                    for (auto const& pair : object)
                    {
                        keys.push_back(std::wstring{ pair.Key() });
                    }

                    if (keys.empty())
                    {
                        return L"{}";
                    }

                    std::sort(keys.begin(), keys.end());

                    std::wstring text{ L"{" };

                    for (size_t i = 0; i < keys.size(); ++i)
                    {
                        text += (i > 0) ? L",\n" : L"\n";
                        text += inner;
                        text += QuoteJson(keys[i]);
                        text += L": ";
                        text += CanonicalText(object.Lookup(winrt::hstring{ keys[i] }), depth + 1);
                    }

                    text += L"\n";
                    text += indent;
                    text += L"}";

                    return text;
                }

                case mjson::JsonValueType::Array:
                {
                    auto const array = value.GetArray();

                    if (array.Size() == 0)
                    {
                        return L"[]";
                    }

                    std::wstring text{ L"[" };

                    for (uint32_t i = 0; i < array.Size(); ++i)
                    {
                        text += (i > 0) ? L",\n" : L"\n";
                        text += inner;
                        text += CanonicalText(array.GetAt(i), depth + 1);
                    }

                    text += L"\n";
                    text += indent;
                    text += L"]";

                    return text;
                }

                case mjson::JsonValueType::String:
                    return QuoteJson(value.GetString());

                case mjson::JsonValueType::Number:
                    return FormatNumber(value.GetNumber());

                case mjson::JsonValueType::Boolean:
                    return value.GetBoolean() ? L"true" : L"false";

                case mjson::JsonValueType::Null:
                default:
                    return L"null";
                }
            }
            catch (...)
            {
                return L"null";
            }
        }

        // Builds one object's text with its keys in the order they were added.
        class OrderedObjectText
        {
        public:
            explicit OrderedObjectText(_In_ int32_t depth) noexcept :
                m_depth(depth),
                m_inner(Indent(depth + 1))
            {
            }

            void Text(_In_ std::wstring_view key, _In_ std::wstring_view value) noexcept
            {
                if (!value.empty())
                {
                    Raw(key, QuoteJson(value));
                }
            }

            void Raw(_In_ std::wstring_view key, _In_ std::wstring_view json) noexcept
            {
                m_text += m_any ? L",\n" : L"\n";
                m_text += m_inner;
                m_text += QuoteJson(key);
                m_text += L": ";
                m_text += json;
                m_any = true;
            }

            void Unknown(_In_ mjson::JsonObject const& unknown) noexcept
            {
                try
                {
                    if (unknown == nullptr)
                    {
                        return;
                    }

                    std::vector<std::wstring> keys{};

                    for (auto const& pair : unknown)
                    {
                        keys.push_back(std::wstring{ pair.Key() });
                    }

                    std::sort(keys.begin(), keys.end());

                    for (auto const& key : keys)
                    {
                        Raw(key, CanonicalText(unknown.Lookup(winrt::hstring{ key }), m_depth + 1));
                    }
                }
                catch (...)
                {
                }
            }

            int32_t Depth() const noexcept { return m_depth; }

            std::wstring Finish() const noexcept
            {
                if (!m_any)
                {
                    return L"{}";
                }

                return L"{" + m_text + L"\n" + Indent(m_depth) + L"}";
            }

        private:
            int32_t m_depth{ 0 };
            std::wstring m_inner{};
            std::wstring m_text{};
            bool m_any{ false };
        };

        std::wstring ReadText(
            _In_ mjson::JsonObject const& object,
            _In_ std::wstring_view key,
            _In_ size_t maximumLength) noexcept
        {
            try
            {
                auto const value = object.TryLookup(winrt::hstring{ key });

                if (value != nullptr && value.ValueType() == mjson::JsonValueType::String)
                {
                    return SanitizeProvenanceText(value.GetString(), maximumLength);
                }
            }
            catch (...)
            {
            }

            return {};
        }

        std::wstring ReadUrl(_In_ mjson::JsonObject const& object) noexcept
        {
            auto url = ReadText(object, KeyUrl, MaximumProvenanceUrlLength);

            url.erase(std::remove_if(url.begin(), url.end(),
                [](wchar_t c) { return std::iswspace(c) != 0; }), url.end());

            return url;
        }

        mjson::JsonObject CaptureUnknown(
            _In_ mjson::JsonObject const& object,
            _In_ std::initializer_list<std::wstring_view> knownKeys) noexcept
        {
            try
            {
                mjson::JsonObject captured{ nullptr };

                for (auto const& pair : object)
                {
                    auto const keyText = pair.Key();
                    std::wstring_view const key{ keyText };

                    if (std::find(knownKeys.begin(), knownKeys.end(), key) != knownKeys.end())
                    {
                        continue;
                    }

                    if (captured == nullptr)
                    {
                        captured = mjson::JsonObject{};
                    }

                    captured.Insert(pair.Key(), pair.Value());
                }

                return captured;
            }
            catch (...)
            {
                return nullptr;
            }
        }

        void InsertUnknown(_Inout_ mjson::JsonObject& target, _In_ mjson::JsonObject const& unknown) noexcept
        {
            try
            {
                if (unknown == nullptr)
                {
                    return;
                }

                for (auto const& pair : unknown)
                {
                    if (!target.HasKey(pair.Key()))
                    {
                        target.Insert(pair.Key(), pair.Value());
                    }
                }
            }
            catch (...)
            {
            }
        }

        bool AllDigits(_In_ std::wstring_view text) noexcept
        {
            return !text.empty() && std::all_of(text.begin(), text.end(),
                [](wchar_t c) { return c >= L'0' && c <= L'9'; });
        }

        std::vector<std::wstring_view> SplitVersion(_In_ std::wstring_view version) noexcept
        {
            std::vector<std::wstring_view> parts{};

            try
            {
                size_t start{ 0 };

                while (start <= version.size())
                {
                    auto const dot = version.find(L'.', start);
                    auto const end = dot == std::wstring_view::npos ? version.size() : dot;

                    parts.push_back(version.substr(start, end - start));

                    if (dot == std::wstring_view::npos)
                    {
                        break;
                    }

                    start = dot + 1;
                }
            }
            catch (...)
            {
                parts.clear();
            }

            return parts;
        }

        bool ReadDigits(
            _In_ std::wstring_view text,
            _Inout_ size_t& at,
            _In_ size_t count,
            _Out_ int32_t& value) noexcept
        {
            value = 0;

            if (at + count > text.size())
            {
                return false;
            }

            for (size_t i = 0; i < count; ++i)
            {
                auto const c = text[at + i];

                if (c < L'0' || c > L'9')
                {
                    return false;
                }

                value = value * 10 + (c - L'0');
            }

            at += count;
            return true;
        }

        std::wstring ReadRegistryText(_In_ HKEY key, _In_ std::wstring_view valueName) noexcept
        {
            try
            {
                DWORD bytes{ 0 };

                if (::RegGetValueW(key, nullptr, std::wstring{ valueName }.c_str(),
                    RRF_RT_REG_SZ, nullptr, nullptr, &bytes) != ERROR_SUCCESS || bytes == 0)
                {
                    return {};
                }

                std::wstring text(bytes / sizeof(wchar_t), L'\0');

                if (::RegGetValueW(key, nullptr, std::wstring{ valueName }.c_str(),
                    RRF_RT_REG_SZ, nullptr, text.data(), &bytes) != ERROR_SUCCESS)
                {
                    return {};
                }

                text.resize(wcsnlen(text.c_str(), text.size()));

                return text;
            }
            catch (...)
            {
                return {};
            }
        }
    }

    bool ProvenanceSource::IsEmpty() const noexcept
    {
        return Name.empty() && Author.empty() && Id.empty() && Version.empty() && !BuiltIn &&
            (Unknown == nullptr || Unknown.Size() == 0);
    }

    bool ContentProvenance::IsEmpty() const noexcept
    {
        return Id.empty() && Version.empty() && Author.empty() && Organization.empty() &&
            Url.empty() && License.empty() && Created.empty() && Tool.empty() &&
            DigitalSourceType.empty() && HumanOversightLevel.empty() && AiModelName.empty() &&
            (AiDisclosureUnknown == nullptr || AiDisclosureUnknown.Size() == 0) &&
            (!BasedOn.has_value() || BasedOn->IsEmpty()) &&
            (Unknown == nullptr || Unknown.Size() == 0);
    }

    _Use_decl_annotations_
    std::wstring SanitizeProvenanceText(std::wstring_view value, size_t maximumLength) noexcept
    {
        try
        {
            std::wstring clean{};
            clean.reserve(std::min(value.size(), maximumLength + 2));

            for (size_t i = 0; i < value.size(); ++i)
            {
                auto const unit = value[i];

                if (unit >= 0xD800 && unit <= 0xDBFF)
                {
                    if (i + 1 < value.size() && value[i + 1] >= 0xDC00 && value[i + 1] <= 0xDFFF)
                    {
                        auto const codePoint = 0x10000 +
                            ((static_cast<char32_t>(unit) - 0xD800) << 10) +
                            (static_cast<char32_t>(value[i + 1]) - 0xDC00);

                        if (!IsDisguisingCharacter(codePoint))
                        {
                            clean += unit;
                            clean += value[i + 1];
                        }

                        ++i;
                    }

                    continue;
                }

                if (unit >= 0xDC00 && unit <= 0xDFFF)
                {
                    continue;
                }

                if (!IsDisguisingCharacter(unit))
                {
                    clean += unit;
                }
            }

            auto const first = clean.find_first_not_of(L' ');

            if (first == std::wstring::npos)
            {
                return {};
            }

            clean.erase(0, first);
            clean.erase(clean.find_last_not_of(L' ') + 1);

            if (clean.size() > maximumLength)
            {
                auto cut = maximumLength;

                if (cut > 0 && clean[cut - 1] >= 0xD800 && clean[cut - 1] <= 0xDBFF)
                {
                    --cut;
                }

                clean.resize(cut);
                clean.erase(clean.find_last_not_of(L' ') + 1);
            }

            return clean;
        }
        catch (...)
        {
            return {};
        }
    }

    _Use_decl_annotations_
    bool IsSafeWebLink(std::wstring_view url) noexcept
    {
        try
        {
            if (url.size() < 9 || url.size() > MaximumProvenanceUrlLength)
            {
                return false;
            }

            if (::CompareStringOrdinal(url.data(), 8, L"https://", 8, TRUE) != CSTR_EQUAL)
            {
                return false;
            }

            for (auto const c : url)
            {
                if (std::iswspace(c) || IsDisguisingCharacter(c))
                {
                    return false;
                }
            }

            winrt::Windows::Foundation::Uri const uri{ winrt::hstring{ url } };

            return ::CompareStringOrdinal(uri.SchemeName().c_str(), -1, L"https", -1, TRUE) == CSTR_EQUAL &&
                !uri.Host().empty() &&
                uri.UserName().empty() &&
                uri.Password().empty();
        }
        catch (...)
        {
            return false;
        }
    }

    _Use_decl_annotations_
    std::wstring NormalizeDigitalSourceType(std::wstring_view value) noexcept
    {
        try
        {
            std::wstring_view const prefix{ IptcPrefix };

            if (value.size() > prefix.size() &&
                ::CompareStringOrdinal(value.data(), static_cast<int>(prefix.size()),
                    prefix.data(), static_cast<int>(prefix.size()), TRUE) == CSTR_EQUAL)
            {
                return std::wstring{ value.substr(prefix.size()) };
            }

            return std::wstring{ value };
        }
        catch (...)
        {
            return {};
        }
    }

    _Use_decl_annotations_
    bool IsKnownDigitalSourceType(std::wstring_view value) noexcept
    {
        return value == DigitalSourceTypes::DigitalCreation ||
            value == DigitalSourceTypes::DigitalCapture ||
            value == DigitalSourceTypes::TrainedAlgorithmicMedia ||
            value == DigitalSourceTypes::CompositeWithTrainedAlgorithmicMedia ||
            value == DigitalSourceTypes::CompositeSynthetic ||
            value == DigitalSourceTypes::Composite ||
            value == DigitalSourceTypes::AlgorithmicMedia;
    }

    _Use_decl_annotations_
    bool IsKnownHumanOversightLevel(std::wstring_view value) noexcept
    {
        return value == HumanOversightLevels::FullyAutonomous ||
            value == HumanOversightLevels::PromptGuided ||
            value == HumanOversightLevels::HumanValidated;
    }

    _Use_decl_annotations_
    bool InvolvesGenerativeAi(std::wstring_view digitalSourceType) noexcept
    {
        return digitalSourceType == DigitalSourceTypes::TrainedAlgorithmicMedia ||
            digitalSourceType == DigitalSourceTypes::CompositeWithTrainedAlgorithmicMedia ||
            digitalSourceType == DigitalSourceTypes::CompositeSynthetic;
    }

    _Use_decl_annotations_
    ContentProvenance ProvenanceFromJson(mjson::JsonObject const& block) noexcept
    {
        ContentProvenance provenance{};

        try
        {
            if (block == nullptr)
            {
                return provenance;
            }

            provenance.Id = ReadText(block, KeyId, MaximumProvenanceIdLength);
            provenance.Version = ReadText(block, KeyVersion, MaximumProvenanceVersionLength);
            provenance.Author = ReadText(block, KeyAuthor, MaximumProvenanceNameLength);
            provenance.Organization = ReadText(block, KeyOrganization, MaximumProvenanceNameLength);
            provenance.Url = ReadUrl(block);
            provenance.License = ReadText(block, KeyLicense, MaximumProvenanceNameLength);
            provenance.Created = ReadText(block, KeyCreated, MaximumShortFieldLength);
            provenance.Tool = ReadText(block, KeyTool, MaximumProvenanceNameLength);
            provenance.DigitalSourceType = NormalizeDigitalSourceType(
                ReadText(block, KeyDigitalSourceType, MaximumProvenanceNameLength));

            auto const disclosure = block.TryLookup(KeyAiDisclosure);

            if (disclosure != nullptr && disclosure.ValueType() == mjson::JsonValueType::Object)
            {
                auto const object = disclosure.GetObject();

                provenance.HumanOversightLevel = ReadText(object, KeyHumanOversightLevel, MaximumShortFieldLength);
                provenance.AiModelName = ReadText(object, KeyModelName, MaximumProvenanceNameLength);
                provenance.AiDisclosureUnknown = CaptureUnknown(object, { KeyHumanOversightLevel, KeyModelName });
            }

            auto const basedOn = block.TryLookup(KeyBasedOn);

            if (basedOn != nullptr)
            {
                ProvenanceSource source{};

                if (basedOn.ValueType() == mjson::JsonValueType::Object)
                {
                    auto const object = basedOn.GetObject();

                    source.Name = ReadText(object, KeyName, MaximumProvenanceNameLength);
                    source.Author = ReadText(object, KeyAuthor, MaximumProvenanceNameLength);
                    source.Id = ReadText(object, KeyId, MaximumProvenanceIdLength);
                    source.Version = ReadText(object, KeyVersion, MaximumProvenanceVersionLength);

                    auto const builtIn = object.TryLookup(KeyBuiltIn);
                    source.BuiltIn = builtIn != nullptr &&
                        builtIn.ValueType() == mjson::JsonValueType::Boolean &&
                        builtIn.GetBoolean();

                    source.Unknown = CaptureUnknown(object, { KeyName, KeyAuthor, KeyId, KeyVersion, KeyBuiltIn });
                }
                else if (basedOn.ValueType() == mjson::JsonValueType::String)
                {
                    source.Name = SanitizeProvenanceText(basedOn.GetString(), MaximumProvenanceNameLength);
                }

                if (!source.IsEmpty())
                {
                    provenance.BasedOn = std::move(source);
                }
            }

            provenance.Unknown = CaptureUnknown(block, {
                KeyId, KeyVersion, KeyAuthor, KeyOrganization, KeyUrl, KeyLicense, KeyCreated,
                KeyTool, KeyDigitalSourceType, KeyAiDisclosure, KeyBasedOn });
        }
        catch (...)
        {
            provenance = ContentProvenance{};
        }

        return provenance;
    }

    _Use_decl_annotations_
    std::optional<ContentProvenance> ReadProvenance(mjson::JsonObject const& root) noexcept
    {
        try
        {
            if (root == nullptr)
            {
                return std::nullopt;
            }

            auto const value = root.TryLookup(ProvenanceKey);

            if (value == nullptr || value.ValueType() != mjson::JsonValueType::Object)
            {
                return std::nullopt;
            }

            auto provenance = ProvenanceFromJson(value.GetObject());

            if (provenance.IsEmpty())
            {
                return std::nullopt;
            }

            return provenance;
        }
        catch (...)
        {
            return std::nullopt;
        }
    }

    _Use_decl_annotations_
    mjson::JsonObject ProvenanceToJson(ContentProvenance const& provenance) noexcept
    {
        try
        {
            mjson::JsonObject block{};

            auto const put = [](mjson::JsonObject& target, std::wstring_view key, std::wstring const& value)
                {
                    if (!value.empty())
                    {
                        target.Insert(winrt::hstring{ key }, mjson::JsonValue::CreateStringValue(value));
                    }
                };

            put(block, KeyId, provenance.Id);
            put(block, KeyVersion, provenance.Version);
            put(block, KeyAuthor, provenance.Author);
            put(block, KeyOrganization, provenance.Organization);
            put(block, KeyUrl, provenance.Url);
            put(block, KeyLicense, provenance.License);
            put(block, KeyCreated, provenance.Created);
            put(block, KeyTool, provenance.Tool);
            put(block, KeyDigitalSourceType, provenance.DigitalSourceType);

            mjson::JsonObject disclosure{};
            put(disclosure, KeyHumanOversightLevel, provenance.HumanOversightLevel);
            put(disclosure, KeyModelName, provenance.AiModelName);
            InsertUnknown(disclosure, provenance.AiDisclosureUnknown);

            if (disclosure.Size() > 0)
            {
                block.Insert(KeyAiDisclosure, disclosure);
            }

            if (provenance.BasedOn.has_value() && !provenance.BasedOn->IsEmpty())
            {
                auto const& source = *provenance.BasedOn;

                mjson::JsonObject basedOn{};
                put(basedOn, KeyName, source.Name);
                put(basedOn, KeyAuthor, source.Author);
                put(basedOn, KeyId, source.Id);
                put(basedOn, KeyVersion, source.Version);

                if (source.BuiltIn)
                {
                    basedOn.Insert(KeyBuiltIn, mjson::JsonValue::CreateBooleanValue(true));
                }

                InsertUnknown(basedOn, source.Unknown);
                block.Insert(KeyBasedOn, basedOn);
            }

            InsertUnknown(block, provenance.Unknown);

            return block;
        }
        catch (...)
        {
            return mjson::JsonObject{};
        }
    }

    _Use_decl_annotations_
    std::wstring ProvenanceToJsonText(ContentProvenance const& provenance, int32_t indentDepth) noexcept
    {
        try
        {
            OrderedObjectText block{ indentDepth };

            block.Text(KeyId, provenance.Id);
            block.Text(KeyVersion, provenance.Version);
            block.Text(KeyAuthor, provenance.Author);
            block.Text(KeyOrganization, provenance.Organization);
            block.Text(KeyUrl, provenance.Url);
            block.Text(KeyLicense, provenance.License);
            block.Text(KeyCreated, provenance.Created);
            block.Text(KeyTool, provenance.Tool);
            block.Text(KeyDigitalSourceType, provenance.DigitalSourceType);

            OrderedObjectText disclosure{ indentDepth + 1 };
            disclosure.Text(KeyHumanOversightLevel, provenance.HumanOversightLevel);
            disclosure.Text(KeyModelName, provenance.AiModelName);
            disclosure.Unknown(provenance.AiDisclosureUnknown);

            auto const disclosureText = disclosure.Finish();

            if (disclosureText != L"{}")
            {
                block.Raw(KeyAiDisclosure, disclosureText);
            }

            if (provenance.BasedOn.has_value() && !provenance.BasedOn->IsEmpty())
            {
                auto const& source = *provenance.BasedOn;

                OrderedObjectText basedOn{ indentDepth + 1 };
                basedOn.Text(KeyName, source.Name);
                basedOn.Text(KeyAuthor, source.Author);
                basedOn.Text(KeyId, source.Id);
                basedOn.Text(KeyVersion, source.Version);

                if (source.BuiltIn)
                {
                    basedOn.Raw(KeyBuiltIn, L"true");
                }

                basedOn.Unknown(source.Unknown);
                block.Raw(KeyBasedOn, basedOn.Finish());
            }

            block.Unknown(provenance.Unknown);

            return block.Finish();
        }
        catch (...)
        {
            return L"{}";
        }
    }

    std::wstring NewProvenanceId() noexcept
    {
        try
        {
            auto text = std::wstring{ winrt::to_hstring(winrt::Windows::Foundation::GuidHelper::CreateNewGuid()) };

            text.erase(std::remove_if(text.begin(), text.end(),
                [](wchar_t c) { return c == L'{' || c == L'}'; }), text.end());

            std::transform(text.begin(), text.end(), text.begin(),
                [](wchar_t c) { return static_cast<wchar_t>(std::towlower(c)); });

            return text;
        }
        catch (...)
        {
            return {};
        }
    }

    std::wstring CurrentProvenanceTime() noexcept
    {
        try
        {
            SYSTEMTIME now{};
            ::GetSystemTime(&now);

            return std::format(L"{:04}-{:02}-{:02}T{:02}:{:02}:{:02}Z",
                now.wYear, now.wMonth, now.wDay, now.wHour, now.wMinute, now.wSecond);
        }
        catch (...)
        {
            return {};
        }
    }

    _Use_decl_annotations_
    int64_t ParseProvenanceTime(std::wstring_view value) noexcept
    {
        try
        {
            size_t at{ 0 };
            int32_t year{}, month{}, day{};

            if (!ReadDigits(value, at, 4, year) || at >= value.size() || value[at++] != L'-' ||
                !ReadDigits(value, at, 2, month) || at >= value.size() || value[at++] != L'-' ||
                !ReadDigits(value, at, 2, day))
            {
                return 0;
            }

            int32_t hour{}, minute{}, second{};
            int32_t offsetMinutes{ 0 };

            if (at < value.size())
            {
                if (value[at] != L'T' && value[at] != L't' && value[at] != L' ')
                {
                    return 0;
                }

                ++at;

                if (!ReadDigits(value, at, 2, hour) || at >= value.size() || value[at++] != L':' ||
                    !ReadDigits(value, at, 2, minute))
                {
                    return 0;
                }

                if (at < value.size() && value[at] == L':')
                {
                    ++at;

                    if (!ReadDigits(value, at, 2, second))
                    {
                        return 0;
                    }

                    if (at < value.size() && value[at] == L'.')
                    {
                        ++at;

                        auto const digitsStart = at;

                        while (at < value.size() && value[at] >= L'0' && value[at] <= L'9')
                        {
                            ++at;
                        }

                        if (at == digitsStart)
                        {
                            return 0;
                        }
                    }
                }

                if (at < value.size())
                {
                    auto const zone = value[at++];

                    if (zone == L'Z' || zone == L'z')
                    {
                        offsetMinutes = 0;
                    }
                    else if (zone == L'+' || zone == L'-')
                    {
                        int32_t offsetHours{}, offsetMinutePart{};

                        if (!ReadDigits(value, at, 2, offsetHours) || at >= value.size() || value[at++] != L':' ||
                            !ReadDigits(value, at, 2, offsetMinutePart) || offsetHours > 23 || offsetMinutePart > 59)
                        {
                            return 0;
                        }

                        offsetMinutes = (offsetHours * 60 + offsetMinutePart) * (zone == L'-' ? -1 : 1);
                    }
                    else
                    {
                        return 0;
                    }
                }
            }

            if (at != value.size() || year < 1601 || year > 9999 || month < 1 || month > 12 ||
                day < 1 || day > 31 || hour > 23 || minute > 59 || second > 60)
            {
                return 0;
            }

            SYSTEMTIME time{};
            time.wYear = static_cast<WORD>(year);
            time.wMonth = static_cast<WORD>(month);
            time.wDay = static_cast<WORD>(day);
            time.wHour = static_cast<WORD>(hour);
            time.wMinute = static_cast<WORD>(minute);
            time.wSecond = static_cast<WORD>(std::min(second, 59));

            FILETIME fileTime{};

            if (!::SystemTimeToFileTime(&time, &fileTime))
            {
                return 0;
            }

            auto ticks = static_cast<int64_t>(
                (static_cast<uint64_t>(fileTime.dwHighDateTime) << 32) | fileTime.dwLowDateTime);

            ticks -= static_cast<int64_t>(offsetMinutes) * 60 * 10'000'000;

            return ticks > 0 ? ticks : 0;
        }
        catch (...)
        {
            return 0;
        }
    }

    _Use_decl_annotations_
    int32_t CompareContentVersions(std::wstring_view left, std::wstring_view right) noexcept
    {
        try
        {
            auto const leftParts = SplitVersion(left);
            auto const rightParts = SplitVersion(right);
            auto const count = std::max(leftParts.size(), rightParts.size());

            for (size_t i = 0; i < count; ++i)
            {
                std::wstring_view const a = i < leftParts.size() ? leftParts[i] : std::wstring_view{ L"0" };
                std::wstring_view const b = i < rightParts.size() ? rightParts[i] : std::wstring_view{ L"0" };

                if (AllDigits(a) && AllDigits(b))
                {
                    auto const trim = [](std::wstring_view digits)
                        {
                            auto const first = digits.find_first_not_of(L'0');
                            return first == std::wstring_view::npos ? std::wstring_view{} : digits.substr(first);
                        };

                    auto const ta = trim(a);
                    auto const tb = trim(b);

                    if (ta.size() != tb.size())
                    {
                        return ta.size() < tb.size() ? -1 : 1;
                    }

                    auto const compared = ta.compare(tb);

                    if (compared != 0)
                    {
                        return compared < 0 ? -1 : 1;
                    }

                    continue;
                }

                auto const compared = ::CompareStringOrdinal(
                    a.empty() ? L"" : a.data(), static_cast<int>(a.size()),
                    b.empty() ? L"" : b.data(), static_cast<int>(b.size()), TRUE);

                if (compared == CSTR_LESS_THAN)
                {
                    return -1;
                }

                if (compared == CSTR_GREATER_THAN)
                {
                    return 1;
                }
            }

            return 0;
        }
        catch (...)
        {
            return 0;
        }
    }

    _Use_decl_annotations_
    ContentProvenance StartProvenance(AuthorProfile const& author, std::wstring_view toolName) noexcept
    {
        ContentProvenance provenance{};

        try
        {
            provenance.Id = NewProvenanceId();
            provenance.Version = L"1.0";
            provenance.Author = SanitizeProvenanceText(author.Name, MaximumProvenanceNameLength);
            provenance.Organization = SanitizeProvenanceText(author.Organization, MaximumProvenanceNameLength);
            provenance.Url = SanitizeProvenanceText(author.Url, MaximumProvenanceUrlLength);
            provenance.License = SanitizeProvenanceText(author.License, MaximumProvenanceNameLength);
            provenance.Created = CurrentProvenanceTime();
            provenance.Tool = SanitizeProvenanceText(toolName, MaximumProvenanceNameLength);
            provenance.DigitalSourceType = DigitalSourceTypes::DigitalCreation;
        }
        catch (...)
        {
        }

        return provenance;
    }

    _Use_decl_annotations_
    ProvenanceSource SourceOf(std::wstring_view name, std::optional<ContentProvenance> const& provenance) noexcept
    {
        ProvenanceSource source{};

        try
        {
            source.Name = SanitizeProvenanceText(name, MaximumProvenanceNameLength);

            if (provenance.has_value())
            {
                source.Author = provenance->Author;
                source.Id = provenance->Id;
                source.Version = provenance->Version;
            }
        }
        catch (...)
        {
        }

        return source;
    }

    _Use_decl_annotations_
    ContentProvenance DeriveProvenance(
        AuthorProfile const& author,
        std::wstring_view toolName,
        std::wstring_view sourceName,
        std::optional<ContentProvenance> const& source,
        bool sourceIsBuiltIn) noexcept
    {
        auto provenance = StartProvenance(author, toolName);

        try
        {
            provenance.BasedOn = SourceOf(sourceName, source);
            provenance.BasedOn->BuiltIn = sourceIsBuiltIn;

            if (source.has_value() && InvolvesGenerativeAi(source->DigitalSourceType))
            {
                provenance.DigitalSourceType = DigitalSourceTypes::CompositeWithTrainedAlgorithmicMedia;
                provenance.AiModelName = source->AiModelName;
            }
        }
        catch (...)
        {
        }

        return provenance;
    }

    AuthorProfile LoadAuthorProfile() noexcept
    {
        AuthorProfile profile{};

        HKEY key{ nullptr };

        if (::RegOpenKeyExW(HKEY_CURRENT_USER, SharedSettingsKeyPath, 0, KEY_READ, &key) != ERROR_SUCCESS)
        {
            return profile;
        }

        profile.Name = SanitizeProvenanceText(ReadRegistryText(key, ValueAuthorName), MaximumProvenanceNameLength);
        profile.Organization = SanitizeProvenanceText(ReadRegistryText(key, ValueAuthorOrganization), MaximumProvenanceNameLength);
        profile.Url = SanitizeProvenanceText(ReadRegistryText(key, ValueAuthorUrl), MaximumProvenanceUrlLength);
        profile.License = SanitizeProvenanceText(ReadRegistryText(key, ValueAuthorLicense), MaximumProvenanceNameLength);

        ::RegCloseKey(key);

        return profile;
    }

    _Use_decl_annotations_
    void SaveAuthorProfile(AuthorProfile const& profile) noexcept
    {
        auto const write = [](wchar_t const* valueName, std::wstring const& value)
            {
                ::RegSetKeyValueW(HKEY_CURRENT_USER, SharedSettingsKeyPath, valueName, REG_SZ,
                    value.c_str(), static_cast<DWORD>((value.size() + 1) * sizeof(wchar_t)));
            };

        try
        {
            write(ValueAuthorName, SanitizeProvenanceText(profile.Name, MaximumProvenanceNameLength));
            write(ValueAuthorOrganization, SanitizeProvenanceText(profile.Organization, MaximumProvenanceNameLength));
            write(ValueAuthorUrl, SanitizeProvenanceText(profile.Url, MaximumProvenanceUrlLength));
            write(ValueAuthorLicense, SanitizeProvenanceText(profile.License, MaximumProvenanceNameLength));
        }
        catch (...)
        {
        }
    }
}
