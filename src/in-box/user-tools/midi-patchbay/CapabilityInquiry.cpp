// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

// Pure: no precompiled header, so the unit tests compile this file exactly as it ships.

#include <windows.h>

#include "CapabilityInquiry.h"
#include "SpinGuard.h"
#include "TextMatch.h"

#include <MidiCiProgramList.h>

#include <algorithm>
#include <cmath>
#include <random>

namespace ci = ::WindowsMidiServicesCapabilityInquiry;

namespace midipatchbay
{
    namespace
    {
        constexpr uint32_t TypeMidi1 = 0x2;
        constexpr uint32_t TypeSysEx7 = 0x3;
        constexpr uint32_t TypeMidi2 = 0x4;

        constexpr uint8_t StatusNoteOff = 0x8;
        constexpr uint8_t StatusNoteOn = 0x9;
        constexpr uint8_t StatusControlChange = 0xB;
        constexpr uint8_t StatusProgramChange = 0xC;
        constexpr uint8_t StatusChannelPressure = 0xD;
        constexpr uint8_t StatusPitchBend = 0xE;

        constexpr uint8_t ControllerBankMsb = 0;
        constexpr uint8_t ControllerBankLsb = 32;
        constexpr uint8_t ControllerAllSoundOff = 120;
        constexpr uint8_t ControllerResetAll = 121;
        constexpr uint8_t ControllerAllNotesOff = 123;

        constexpr uint32_t PitchBendCenter = 0x80000000;

        constexpr uint8_t AddressGroup = 0x7E;
        constexpr uint8_t AddressFunctionBlock = 0x7F;

        constexpr uint8_t NakStatusGeneral = 0x00;
        constexpr uint8_t NakStatusProfileNotSupported = 0x04;

        // What an initiator that hasn't said, in Discovery, how much it can take is sent in.
        constexpr uint32_t DefaultInitiatorSysEx = 512;

        // The same question twice this close together came two ways through the patch.
        constexpr auto RepeatWindow = std::chrono::milliseconds{ 100 };

        // A path that isn't a leaf, for a message with nowhere to answer.
        constexpr uint32_t NoLeaf = 0xFFFFFFFF;

        constexpr uint64_t FnvOffset = 14695981039346656037ull;
        constexpr uint64_t FnvPrime = 1099511628211ull;

        // The MIDI-CI file.
        constexpr wchar_t KeyProfiles[] = L"profiles";
        constexpr wchar_t KeyDeviceInfo[] = L"deviceInfo";
        constexpr wchar_t KeyResources[] = L"resources";
        constexpr wchar_t KeyId[] = L"id";
        constexpr wchar_t KeyName[] = L"name";
        constexpr wchar_t KeyTarget[] = L"target";
        constexpr wchar_t KeyChannel[] = L"channel";
        constexpr wchar_t KeyChannels[] = L"channels";
        constexpr wchar_t KeyEnabled[] = L"enabled";
        constexpr wchar_t KeyDetails[] = L"details";
        constexpr wchar_t KeyData[] = L"data";
        constexpr wchar_t KeyManufacturer[] = L"manufacturer";
        constexpr wchar_t KeyFamily[] = L"family";
        constexpr wchar_t KeyModel[] = L"model";
        constexpr wchar_t KeyVersion[] = L"version";
        constexpr wchar_t KeyResource[] = L"resource";
        constexpr wchar_t KeyResId[] = L"resId";

        constexpr wchar_t TargetChannel[] = L"channel";
        constexpr wchar_t TargetGroup[] = L"group";
        constexpr wchar_t TargetFunctionBlock[] = L"functionBlock";

        constexpr size_t MaximumDetailBytes = 512;
        constexpr size_t MaximumDeviceInfoLength = 128;

        // A property exchange header.
        constexpr wchar_t HeaderResource[] = L"resource";
        constexpr wchar_t HeaderResId[] = L"resId";
        constexpr wchar_t HeaderOffset[] = L"offset";
        constexpr wchar_t HeaderLimit[] = L"limit";
        constexpr wchar_t HeaderCommand[] = L"command";

        constexpr char ResourceList[] = "ResourceList";
        constexpr char DeviceInfo[] = "DeviceInfo";

        // ------------------------------------------------------------------ small helpers

        uint64_t Mix(_In_ uint64_t hash, _In_reads_(count) uint8_t const* bytes, _In_ size_t count) noexcept
        {
            for (size_t i = 0; i < count; i++)
            {
                hash ^= bytes[i];
                hash *= FnvPrime;
            }

            return hash;
        }

        uint64_t MixValue(_In_ uint64_t hash, _In_ uint64_t value) noexcept
        {
            uint8_t bytes[8]{};

            for (size_t i = 0; i < sizeof(bytes); i++)
            {
                bytes[i] = static_cast<uint8_t>(value >> (i * 8));
            }

            return Mix(hash, bytes, sizeof(bytes));
        }

        // With its length after it, so "ab" then "c" never mixes the same as "a" then "bc".
        uint64_t MixText(_In_ uint64_t hash, _In_ std::string const& text) noexcept
        {
            hash = Mix(hash, reinterpret_cast<uint8_t const*>(text.data()), text.size());
            return MixValue(hash, text.size());
        }

        uint8_t WordsFor(_In_ uint32_t word) noexcept
        {
            switch (word >> 28)
            {
            case 0x0: case 0x1: case 0x2: case 0x6: case 0x7:
                return 1;

            case 0x3: case 0x4: case 0x8: case 0x9: case 0xA:
                return 2;

            case 0xB: case 0xC:
                return 3;

            default:
                return 4;
            }
        }

        // Answers made while the lock is held, and sent once it isn't.
        class Collector final : public CiReplyWriter
        {
        public:
            void Write(_In_reads_(wordCount) uint32_t const* words, _In_ uint8_t wordCount) noexcept override
            {
                try
                {
                    m_words.insert(m_words.end(), words, words + wordCount);
                }
                catch (...)
                {
                }
            }

            void SendTo(_Inout_ CiReplyWriter& writer) const noexcept
            {
                size_t i = 0;

                while (i < m_words.size())
                {
                    auto const count = WordsFor(m_words[i]);

                    if (i + count > m_words.size())
                    {
                        break;
                    }

                    writer.Write(m_words.data() + i, count);
                    i += count;
                }
            }

        private:
            std::vector<uint32_t> m_words{};
        };

        // MIDI 2.0's way of widening a value, so the top of the narrow range is the top of the
        // wide one and the middle stays the middle.
        uint32_t Widen(_In_ uint32_t value, _In_ uint8_t fromBits, _In_ uint8_t toBits) noexcept
        {
            auto const shift = static_cast<uint8_t>(toBits - fromBits);
            auto widened = value << shift;
            auto const center = 1u << (fromBits - 1);

            if (value <= center)
            {
                return widened;
            }

            auto const repeatBits = static_cast<uint8_t>(fromBits - 1);
            auto repeat = value & ((1u << repeatBits) - 1);

            if (shift > repeatBits)
            {
                repeat <<= shift - repeatBits;
            }
            else
            {
                repeat >>= repeatBits - shift;
            }

            while (repeat != 0)
            {
                widened |= repeat;
                repeat >>= repeatBits;
            }

            return widened;
        }

        uint32_t NewMuid() noexcept
        {
            try
            {
                std::random_device random{};

                for (int attempt = 0; attempt < 16; attempt++)
                {
                    auto const muid = static_cast<uint32_t>(random()) & ci::MuidMaxValue;

                    if (muid != 0 && ci::MuidIsUsable(muid))
                    {
                        return muid;
                    }
                }
            }
            catch (...)
            {
            }

            // Never zero, which means none, and never one of the reserved ones.
            return static_cast<uint32_t>((::GetTickCount64() * 2654435761ull) % (ci::MuidReservedStart - 1)) + 1;
        }

        // Everything past ASCII as an escape, which is all MIDI-CI can carry. JSON from
        // Windows.Data.Json already escapes quotes and control characters.
        std::string AsciiJson(_In_ std::wstring_view text)
        {
            static constexpr char Digits[] = "0123456789ABCDEF";

            std::string result{};
            result.reserve(text.size());

            for (auto const c : text)
            {
                if (c < 0x80)
                {
                    result += static_cast<char>(c);
                    continue;
                }

                // 0x5C is the backslash.
                result += static_cast<char>(0x5C);
                result += 'u';

                for (int shift = 12; shift >= 0; shift -= 4)
                {
                    result += Digits[(c >> shift) & 0x0F];
                }
            }

            return result;
        }

        std::string Utf8From(_In_ std::wstring_view text)
        {
            std::string result{};
            result.reserve(text.size());

            for (size_t i = 0; i < text.size(); i++)
            {
                uint32_t c = text[i];

                if (c >= 0xD800 && c <= 0xDBFF && i + 1 < text.size() && text[i + 1] >= 0xDC00 && text[i + 1] <= 0xDFFF)
                {
                    c = 0x10000 + ((c - 0xD800) << 10) + (static_cast<uint32_t>(text[i + 1]) - 0xDC00);
                    i++;
                }
                else if (c >= 0xD800 && c <= 0xDFFF)
                {
                    c = 0xFFFD;
                }

                if (c < 0x80)
                {
                    result += static_cast<char>(c);
                }
                else if (c < 0x800)
                {
                    result += static_cast<char>(0xC0 | (c >> 6));
                    result += static_cast<char>(0x80 | (c & 0x3F));
                }
                else if (c < 0x10000)
                {
                    result += static_cast<char>(0xE0 | (c >> 12));
                    result += static_cast<char>(0x80 | ((c >> 6) & 0x3F));
                    result += static_cast<char>(0x80 | (c & 0x3F));
                }
                else
                {
                    result += static_cast<char>(0xF0 | (c >> 18));
                    result += static_cast<char>(0x80 | ((c >> 12) & 0x3F));
                    result += static_cast<char>(0x80 | ((c >> 6) & 0x3F));
                    result += static_cast<char>(0x80 | (c & 0x3F));
                }
            }

            return result;
        }

        // Printable ASCII, the only thing a resource name or id can be.
        bool IsPlainName(_In_ std::wstring_view text, _In_ bool allowEmpty) noexcept
        {
            if (text.empty())
            {
                return allowEmpty;
            }

            if (text.size() > MaximumCiNameLength)
            {
                return false;
            }

            return std::all_of(text.begin(), text.end(), [](wchar_t c) { return c >= 0x20 && c <= 0x7E; });
        }

        std::string Narrow(_In_ std::wstring_view text)
        {
            std::string result{};
            result.reserve(text.size());

            for (auto const c : text)
            {
                result += c < 0x80 ? static_cast<char>(c) : '?';
            }

            return result;
        }

        bool IsWhole(_In_ json::IJsonValue const& value, _In_ double lowest, _In_ double highest, _Out_ uint32_t& number) noexcept
        {
            number = 0;

            try
            {
                if (value == nullptr || value.ValueType() != json::JsonValueType::Number)
                {
                    return false;
                }

                auto const read = value.GetNumber();

                if (!std::isfinite(read) || read < lowest || read > highest || std::floor(read) != read)
                {
                    return false;
                }

                number = static_cast<uint32_t>(read);
                return true;
            }
            catch (...)
            {
            }

            return false;
        }

        // Five numbers, or the same five as hex like "7E 21 00 01 01".
        bool ReadProfileId(_In_ json::IJsonValue const& value, _Out_ std::array<uint8_t, 5>& id) noexcept
        {
            id = {};

            try
            {
                if (value == nullptr)
                {
                    return false;
                }

                if (value.ValueType() == json::JsonValueType::Array)
                {
                    auto const list = value.GetArray();

                    if (list.Size() != id.size())
                    {
                        return false;
                    }

                    for (uint32_t i = 0; i < id.size(); i++)
                    {
                        uint32_t number{ 0 };

                        if (!IsWhole(list.GetAt(i), 0, 127, number))
                        {
                            return false;
                        }

                        id[i] = static_cast<uint8_t>(number);
                    }

                    return true;
                }

                if (value.ValueType() == json::JsonValueType::String)
                {
                    std::vector<int> digits{};

                    for (auto const c : value.GetString())
                    {
                        if (c == L' ')
                        {
                            continue;
                        }

                        auto const digit = HexDigit(c);

                        if (digit < 0)
                        {
                            return false;
                        }

                        digits.push_back(digit);
                    }

                    if (digits.size() != id.size() * 2)
                    {
                        return false;
                    }

                    for (size_t i = 0; i < id.size(); i++)
                    {
                        auto const byte = digits[i * 2] * 16 + digits[i * 2 + 1];

                        if (byte > 0x7F)
                        {
                            return false;
                        }

                        id[i] = static_cast<uint8_t>(byte);
                    }

                    return true;
                }
            }
            catch (...)
            {
            }

            return false;
        }

        bool ReadDetails(_In_ json::IJsonValue const& value, _Inout_ CiProfile& profile)
        {
            if (value == nullptr || value.ValueType() != json::JsonValueType::Array)
            {
                return false;
            }

            for (auto const& item : value.GetArray())
            {
                if (profile.Details.size() >= MaximumCiProfileDetails ||
                    item == nullptr || item.ValueType() != json::JsonValueType::Object)
                {
                    return false;
                }

                auto const object = item.GetObject();

                for (auto const& entry : object)
                {
                    if (entry.Key() != KeyTarget && entry.Key() != KeyData)
                    {
                        return false;
                    }
                }

                CiProfileDetail detail{};
                uint32_t target{ 0 };

                if (!object.HasKey(KeyTarget) || !IsWhole(object.GetNamedValue(KeyTarget), 0, 127, target))
                {
                    return false;
                }

                detail.Target = static_cast<uint8_t>(target);

                if (!object.HasKey(KeyData) || object.GetNamedValue(KeyData).ValueType() != json::JsonValueType::Array)
                {
                    return false;
                }

                for (auto const& byte : object.GetNamedValue(KeyData).GetArray())
                {
                    uint32_t number{ 0 };

                    if (detail.Data.size() >= MaximumDetailBytes || !IsWhole(byte, 0, 127, number))
                    {
                        return false;
                    }

                    detail.Data.push_back(static_cast<uint8_t>(number));
                }

                for (auto const& other : profile.Details)
                {
                    if (other.Target == detail.Target)
                    {
                        return false;
                    }
                }

                profile.Details.push_back(std::move(detail));
            }

            return true;
        }

        bool ReadProfile(
            _In_ json::IJsonValue const& item,
            _In_ int32_t index,
            _Out_ CiProfile& profile,
            _Inout_ std::vector<CiFileProblem>& problems)
        {
            profile = CiProfile{};

            if (item == nullptr || item.ValueType() != json::JsonValueType::Object)
            {
                return false;
            }

            auto const object = item.GetObject();

            for (auto const& entry : object)
            {
                auto const key = entry.Key();

                if (key != KeyId && key != KeyName && key != KeyTarget && key != KeyChannel &&
                    key != KeyChannels && key != KeyEnabled && key != KeyDetails)
                {
                    problems.push_back(CiFileProblem{ CiFileProblemKind::UnknownKey, index, std::wstring{ key } });
                }
            }

            if (!object.HasKey(KeyId) || !ReadProfileId(object.GetNamedValue(KeyId), profile.Id))
            {
                return false;
            }

            if (object.HasKey(KeyName) && object.GetNamedValue(KeyName).ValueType() != json::JsonValueType::String)
            {
                return false;
            }

            if (object.HasKey(KeyTarget))
            {
                auto const target = object.GetNamedValue(KeyTarget);

                if (target.ValueType() != json::JsonValueType::String)
                {
                    return false;
                }

                auto const name = target.GetString();

                if (name == TargetChannel)
                {
                    profile.Target = CiProfileTarget::Channel;
                }
                else if (name == TargetGroup)
                {
                    profile.Target = CiProfileTarget::Group;
                }
                else if (name == TargetFunctionBlock)
                {
                    profile.Target = CiProfileTarget::FunctionBlock;
                }
                else
                {
                    return false;
                }
            }
            else
            {
                profile.Target = object.HasKey(KeyChannel) ? CiProfileTarget::Channel : CiProfileTarget::FunctionBlock;
            }

            if (profile.Target == CiProfileTarget::Channel)
            {
                uint32_t channel{ 0 };

                if (!object.HasKey(KeyChannel) || !IsWhole(object.GetNamedValue(KeyChannel), 0, 15, channel))
                {
                    return false;
                }

                profile.Channel = static_cast<uint8_t>(channel);

                if (object.HasKey(KeyChannels))
                {
                    uint32_t count{ 0 };

                    if (!IsWhole(object.GetNamedValue(KeyChannels), 1, 16, count))
                    {
                        return false;
                    }

                    profile.ChannelCount = static_cast<uint16_t>(count);
                }
            }
            else if (object.HasKey(KeyChannel) || object.HasKey(KeyChannels))
            {
                // A channel on a group or function block profile is a mistake, not a detail.
                return false;
            }

            if (object.HasKey(KeyEnabled))
            {
                auto const enabled = object.GetNamedValue(KeyEnabled);

                if (enabled.ValueType() != json::JsonValueType::Boolean)
                {
                    return false;
                }

                profile.Enabled = enabled.GetBoolean();
            }

            if (object.HasKey(KeyDetails) && !ReadDetails(object.GetNamedValue(KeyDetails), profile))
            {
                return false;
            }

            return true;
        }

        bool SameAddress(_In_ CiProfile const& a, _In_ CiProfile const& b) noexcept
        {
            return a.Target == b.Target && (a.Target != CiProfileTarget::Channel || a.Channel == b.Channel);
        }

        void ReadProfiles(
            _In_ json::IJsonValue const& value,
            _Inout_ CiDescription& description,
            _Inout_ std::vector<CiFileProblem>& problems)
        {
            if (value == nullptr || value.ValueType() != json::JsonValueType::Array)
            {
                problems.push_back(CiFileProblem{ CiFileProblemKind::BadProfile, -1, {} });
                return;
            }

            int32_t index{ -1 };

            for (auto const& item : value.GetArray())
            {
                index++;

                if (description.Profiles.size() >= MaximumCiProfiles)
                {
                    problems.push_back(CiFileProblem{ CiFileProblemKind::TooMany, index, std::wstring{ KeyProfiles } });
                    break;
                }

                CiProfile profile{};

                if (!ReadProfile(item, index, profile, problems))
                {
                    problems.push_back(CiFileProblem{ CiFileProblemKind::BadProfile, index, {} });
                    continue;
                }

                auto const duplicate = std::any_of(description.Profiles.begin(), description.Profiles.end(),
                    [&profile](CiProfile const& other) { return other.Id == profile.Id && SameAddress(other, profile); });

                if (duplicate)
                {
                    problems.push_back(CiFileProblem{ CiFileProblemKind::DuplicateProfile, index, {} });
                    continue;
                }

                description.Profiles.push_back(std::move(profile));
            }
        }

        void ReadDeviceInfo(
            _In_ json::IJsonValue const& value,
            _Inout_ CiDescription& description,
            _Inout_ std::vector<CiFileProblem>& problems)
        {
            if (value == nullptr || value.ValueType() != json::JsonValueType::Object)
            {
                problems.push_back(CiFileProblem{ CiFileProblemKind::BadDeviceInfo, -1, {} });
                return;
            }

            description.HasDeviceInfo = true;

            for (auto const& entry : value.GetObject())
            {
                auto const key = entry.Key();

                std::string* target{ nullptr };

                if (key == KeyManufacturer) { target = &description.Manufacturer; }
                else if (key == KeyFamily) { target = &description.Family; }
                else if (key == KeyModel) { target = &description.Model; }
                else if (key == KeyVersion) { target = &description.Version; }

                if (target == nullptr)
                {
                    problems.push_back(CiFileProblem{ CiFileProblemKind::UnknownKey, -1, std::wstring{ key } });
                    continue;
                }

                auto const text = entry.Value();

                if (text == nullptr || text.ValueType() != json::JsonValueType::String ||
                    text.GetString().size() > MaximumDeviceInfoLength)
                {
                    problems.push_back(CiFileProblem{ CiFileProblemKind::BadDeviceInfo, -1, std::wstring{ key } });
                    continue;
                }

                *target = Utf8From(text.GetString());
            }
        }

        void ReadResources(
            _In_ json::IJsonValue const& value,
            _Inout_ CiDescription& description,
            _Inout_ std::vector<CiFileProblem>& problems)
        {
            if (value == nullptr || value.ValueType() != json::JsonValueType::Array)
            {
                problems.push_back(CiFileProblem{ CiFileProblemKind::BadResource, -1, {} });
                return;
            }

            int32_t index{ -1 };

            for (auto const& item : value.GetArray())
            {
                index++;

                if (description.Resources.size() >= MaximumCiResources)
                {
                    problems.push_back(CiFileProblem{ CiFileProblemKind::TooMany, index, std::wstring{ KeyResources } });
                    break;
                }

                if (item == nullptr || item.ValueType() != json::JsonValueType::Object)
                {
                    problems.push_back(CiFileProblem{ CiFileProblemKind::BadResource, index, {} });
                    continue;
                }

                auto const object = item.GetObject();

                for (auto const& entry : object)
                {
                    auto const key = entry.Key();

                    if (key != KeyResource && key != KeyResId && key != KeyData)
                    {
                        problems.push_back(CiFileProblem{ CiFileProblemKind::UnknownKey, index, std::wstring{ key } });
                    }
                }

                auto const name = object.HasKey(KeyResource) ? object.GetNamedValue(KeyResource) : nullptr;
                auto const resId = object.HasKey(KeyResId) ? object.GetNamedValue(KeyResId) : nullptr;

                auto const nameOk = name != nullptr && name.ValueType() == json::JsonValueType::String &&
                    IsPlainName(name.GetString(), false);

                auto const resIdOk = resId == nullptr ||
                    (resId.ValueType() == json::JsonValueType::String && IsPlainName(resId.GetString(), true));

                if (!nameOk || !resIdOk || !object.HasKey(KeyData))
                {
                    problems.push_back(CiFileProblem{ CiFileProblemKind::BadResource, index, {} });
                    continue;
                }

                CiResource resource{};
                resource.Name = Narrow(name.GetString());
                resource.ResourceId = resId == nullptr ? std::string{} : Narrow(resId.GetString());

                // These two are made from the step and the rest of the file.
                if (resource.Name == ResourceList || resource.Name == DeviceInfo)
                {
                    problems.push_back(CiFileProblem{ CiFileProblemKind::BadResource, index, {} });
                    continue;
                }

                auto const data = object.GetNamedValue(KeyData);

                resource.Json = AsciiJson(data.Stringify());

                if (resource.Json.size() > MaximumCiResourceBytes)
                {
                    problems.push_back(CiFileProblem{ CiFileProblemKind::TooLarge, index, {} });
                    continue;
                }

                if (data.ValueType() == json::JsonValueType::Array)
                {
                    resource.IsArray = true;

                    for (auto const& element : data.GetArray())
                    {
                        resource.Items.push_back(element == nullptr ? std::string{ "null" } : AsciiJson(element.Stringify()));
                    }
                }

                auto const duplicate = std::any_of(description.Resources.begin(), description.Resources.end(),
                    [&resource](CiResource const& other) { return other.Name == resource.Name && other.ResourceId == resource.ResourceId; });

                if (duplicate)
                {
                    problems.push_back(CiFileProblem{ CiFileProblemKind::DuplicateResource, index, {} });
                    continue;
                }

                description.Resources.push_back(std::move(resource));
            }
        }

        void MarkSection(_Inout_ std::vector<CiFileProblem>& problems, _In_ size_t first, _In_ CiFileSection section) noexcept
        {
            for (auto i = first; i < problems.size(); i++)
            {
                problems[i].Section = section;
            }
        }

        uint64_t FingerprintOf(_In_ CiDescription const& description) noexcept
        {
            auto hash = FnvOffset;

            for (auto const& profile : description.Profiles)
            {
                hash = Mix(hash, profile.Id.data(), profile.Id.size());
                hash = MixValue(hash, static_cast<uint64_t>(profile.Target));
                hash = MixValue(hash, profile.Channel);
                hash = MixValue(hash, profile.ChannelCount);
                hash = MixValue(hash, profile.Enabled ? 1 : 0);

                for (auto const& detail : profile.Details)
                {
                    hash = MixValue(hash, detail.Target);
                    hash = Mix(hash, detail.Data.data(), detail.Data.size());
                    hash = MixValue(hash, detail.Data.size());
                }

                hash = MixValue(hash, profile.Details.size());
            }

            hash = MixValue(hash, description.Profiles.size());
            hash = MixValue(hash, description.HasDeviceInfo ? 1 : 0);
            hash = MixText(hash, description.Manufacturer);
            hash = MixText(hash, description.Family);
            hash = MixText(hash, description.Model);
            hash = MixText(hash, description.Version);

            for (auto const& resource : description.Resources)
            {
                hash = MixText(hash, resource.Name);
                hash = MixText(hash, resource.ResourceId);
                hash = MixText(hash, resource.Json);
            }

            hash = MixValue(hash, description.Resources.size());

            // Zero means no file at all.
            return hash == 0 ? 1 : hash;
        }

        // ------------------------------------------------------------------ what was sent

        CiChannelState* TrackedChannel(_Inout_ CiResponderState& state, _In_ uint8_t group, _In_ uint8_t channel) noexcept
        {
            auto slot = state.TrackedGroupSlot[group & 0x0F];

            if (slot < 0)
            {
                if (state.TrackedGroupCount >= MaximumCiTrackedGroups)
                {
                    return nullptr;
                }

                slot = static_cast<int8_t>(state.TrackedGroupCount++);
                state.TrackedGroupSlot[group & 0x0F] = slot;
            }

            return &state.Tracked[static_cast<size_t>(slot)][channel & 0x0F];
        }

        // What Reset All Controllers puts back, from RP-015. Only for what has been sent.
        void ResetControllers(_Inout_ CiChannelState& channel) noexcept
        {
            constexpr uint8_t ResetToZero[]{ 1, 64, 65, 66, 67, 68, 69 };

            channel.PitchBend = PitchBendCenter;
            channel.Pressure = 0;

            for (auto const controller : ResetToZero)
            {
                channel.Controllers[controller] = 0;
            }

            channel.Controllers[11] = 0xFFFFFFFF;
        }

        void Track(_Inout_ CiResponderState& state, _In_reads_(wordCount) uint32_t const* words, _In_ uint8_t wordCount) noexcept
        {
            auto const midi2 = (words[0] >> 28) == TypeMidi2;

            if (midi2 && wordCount < 2)
            {
                return;
            }

            auto const group = static_cast<uint8_t>((words[0] >> 24) & 0x0F);
            auto const status = static_cast<uint8_t>((words[0] >> 20) & 0x0F);
            auto const channelIndex = static_cast<uint8_t>((words[0] >> 16) & 0x0F);
            auto const index = static_cast<uint8_t>((words[0] >> 8) & 0x7F);
            auto const data2 = static_cast<uint8_t>(words[0] & 0x7F);

            switch (status)
            {
            case StatusNoteOff:
            case StatusNoteOn:
            case StatusControlChange:
            case StatusProgramChange:
            case StatusChannelPressure:
            case StatusPitchBend:
                break;

            default:
                return;
            }

            SpinGuard guard{ state.TrackingLock };

            auto* channel = TrackedChannel(state, group, channelIndex);

            if (channel == nullptr)
            {
                return;
            }

            channel->Midi2 = midi2;

            switch (status)
            {
            case StatusNoteOff:
                channel->NotesOn.reset(index);
                break;

            case StatusNoteOn:
                // A MIDI 1.0 note on at velocity zero is a note off.
                if (!midi2 && data2 == 0)
                {
                    channel->NotesOn.reset(index);
                }
                else
                {
                    channel->NotesOn.set(index);
                    channel->Velocities[index] = midi2
                        ? static_cast<uint16_t>(words[1] >> 16)
                        : static_cast<uint16_t>(Widen(data2, 7, 16));
                }
                break;

            case StatusControlChange:
            {
                auto const value = midi2 ? words[1] : Widen(data2, 7, 32);

                channel->Controllers[index] = value;
                channel->ControllersSent.set(index);

                if (index == ControllerBankMsb)
                {
                    channel->BankMsb = static_cast<uint8_t>(value >> 25);
                    channel->BankSent = true;
                }
                else if (index == ControllerBankLsb)
                {
                    channel->BankLsb = static_cast<uint8_t>(value >> 25);
                    channel->BankSent = true;
                }
                else if (index == ControllerAllSoundOff || index == ControllerAllNotesOff)
                {
                    channel->NotesOn.reset();
                }
                else if (index == ControllerResetAll)
                {
                    ResetControllers(*channel);
                }
                break;
            }

            case StatusProgramChange:
                if (midi2)
                {
                    channel->Program = static_cast<uint8_t>((words[1] >> 24) & 0x7F);

                    // Option flag 0x01 says the bank is there too.
                    if ((words[0] & 0x01) != 0)
                    {
                        channel->BankMsb = static_cast<uint8_t>((words[1] >> 8) & 0x7F);
                        channel->BankLsb = static_cast<uint8_t>(words[1] & 0x7F);
                        channel->BankSent = true;
                    }
                }
                else
                {
                    channel->Program = index;
                }

                channel->ProgramSent = true;
                break;

            case StatusChannelPressure:
                channel->Pressure = midi2 ? words[1] : Widen(index, 7, 32);
                channel->PressureSent = true;
                break;

            case StatusPitchBend:
                channel->PitchBend = midi2 ? words[1] : Widen((static_cast<uint32_t>(data2) << 7) | index, 14, 32);
                channel->PitchBendSent = true;
                break;

            default:
                break;
            }
        }

        // What a report that asks only for changes leaves out.
        uint8_t DefaultController(_In_ uint8_t controller) noexcept
        {
            switch (controller)
            {
            case 7:
                return 100;

            case 8:
            case 10:
                return 64;

            case 11:
                return 127;

            default:
                return 0;
            }
        }

        // Choosing or stepping a parameter, and the channel mode messages, aren't state. The bank
        // goes with the program.
        bool IsReportedController(_In_ uint8_t controller) noexcept
        {
            switch (controller)
            {
            case ControllerBankMsb:
            case ControllerBankLsb:
            case 6:
            case 38:
            case 96:
            case 97:
            case 98:
            case 99:
            case 100:
            case 101:
                return false;

            default:
                return controller < 120;
            }
        }

        void SendMidi1(
            _Inout_ CiReplyWriter& out,
            _In_ uint8_t group,
            _In_ uint8_t status,
            _In_ uint8_t channel,
            _In_ uint8_t data1,
            _In_ uint8_t data2) noexcept
        {
            uint32_t const word = (TypeMidi1 << 28) | (static_cast<uint32_t>(group & 0x0F) << 24) |
                (static_cast<uint32_t>(status) << 20) | (static_cast<uint32_t>(channel & 0x0F) << 16) |
                (static_cast<uint32_t>(data1 & 0x7F) << 8) | (data2 & 0x7Fu);

            out.Write(&word, 1);
        }

        void SendMidi2(
            _Inout_ CiReplyWriter& out,
            _In_ uint8_t group,
            _In_ uint8_t status,
            _In_ uint8_t channel,
            _In_ uint8_t index1,
            _In_ uint8_t index2,
            _In_ uint32_t data) noexcept
        {
            uint32_t const words[2]
            {
                (TypeMidi2 << 28) | (static_cast<uint32_t>(group & 0x0F) << 24) |
                    (static_cast<uint32_t>(status) << 20) | (static_cast<uint32_t>(channel & 0x0F) << 16) |
                    (static_cast<uint32_t>(index1) << 8) | index2,
                data,
            };

            out.Write(words, 2);
        }

        // The families go in the order of their bits in the inquiry, then the notes that are on.
        void ReportChannel(
            _In_ CiChannelState const& state,
            _In_ uint8_t group,
            _In_ uint8_t channel,
            _In_ ci::MidiMessageReportFields const& reported,
            _In_ bool onlyChanged,
            _Inout_ CiReplyWriter& out) noexcept
        {
            auto const midi2 = state.Midi2;

            if ((reported.ChannelControllerMessages & ci::ChannelControllerPitchBend) != 0 && state.PitchBendSent &&
                (!onlyChanged || state.PitchBend != PitchBendCenter))
            {
                if (midi2)
                {
                    SendMidi2(out, group, StatusPitchBend, channel, 0, 0, state.PitchBend);
                }
                else
                {
                    auto const value = state.PitchBend >> 18;
                    SendMidi1(out, group, StatusPitchBend, channel, static_cast<uint8_t>(value & 0x7F), static_cast<uint8_t>(value >> 7));
                }
            }

            if ((reported.ChannelControllerMessages & ci::ChannelControllerControlChange) != 0)
            {
                for (uint8_t controller = 0; controller < 128; controller++)
                {
                    if (!state.ControllersSent.test(controller) || !IsReportedController(controller))
                    {
                        continue;
                    }

                    auto const value = state.Controllers[controller];

                    if (onlyChanged && (value >> 25) == DefaultController(controller))
                    {
                        continue;
                    }

                    if (midi2)
                    {
                        SendMidi2(out, group, StatusControlChange, channel, controller, 0, value);
                    }
                    else
                    {
                        SendMidi1(out, group, StatusControlChange, channel, controller, static_cast<uint8_t>(value >> 25));
                    }
                }
            }

            auto const bankChanged = state.BankSent && (state.BankMsb != 0 || state.BankLsb != 0);

            if ((reported.ChannelControllerMessages & ci::ChannelControllerProgramChange) != 0 && state.ProgramSent &&
                (!onlyChanged || state.Program != 0 || bankChanged))
            {
                if (midi2)
                {
                    SendMidi2(out, group, StatusProgramChange, channel, 0, state.BankSent ? 0x01 : 0x00,
                        (static_cast<uint32_t>(state.Program & 0x7F) << 24) |
                        (static_cast<uint32_t>(state.BankMsb & 0x7F) << 8) | (state.BankLsb & 0x7Fu));
                }
                else
                {
                    if (state.BankSent)
                    {
                        SendMidi1(out, group, StatusControlChange, channel, ControllerBankMsb, state.BankMsb);
                        SendMidi1(out, group, StatusControlChange, channel, ControllerBankLsb, state.BankLsb);
                    }

                    SendMidi1(out, group, StatusProgramChange, channel, state.Program, 0);
                }
            }

            if ((reported.ChannelControllerMessages & ci::ChannelControllerChannelPressure) != 0 && state.PressureSent &&
                (!onlyChanged || state.Pressure != 0))
            {
                if (midi2)
                {
                    SendMidi2(out, group, StatusChannelPressure, channel, 0, 0, state.Pressure);
                }
                else
                {
                    SendMidi1(out, group, StatusChannelPressure, channel, static_cast<uint8_t>(state.Pressure >> 25), 0);
                }
            }

            if ((reported.NoteDataMessages & ci::NoteDataNotes) != 0)
            {
                for (uint8_t note = 0; note < 128; note++)
                {
                    if (!state.NotesOn.test(note))
                    {
                        continue;
                    }

                    if (midi2)
                    {
                        SendMidi2(out, group, StatusNoteOn, channel, note, 0, static_cast<uint32_t>(state.Velocities[note]) << 16);
                    }
                    else
                    {
                        auto const velocity = static_cast<uint8_t>(state.Velocities[note] >> 9);
                        SendMidi1(out, group, StatusNoteOn, channel, note, velocity == 0 ? 1 : velocity);
                    }
                }
            }
        }

        // ------------------------------------------------------------------ answering

        void Log(
            _Inout_ CiResponderState& state,
            _In_ ci::MessageType type,
            _In_ uint32_t initiator,
            _In_ CiOutcome outcome) noexcept
        {
            auto& entry = state.Activity[state.NextActivity];

            entry.Time = std::chrono::system_clock::now();
            entry.MessageType = static_cast<uint8_t>(type);
            entry.InitiatorMuid = initiator;
            entry.Outcome = outcome;

            state.NextActivity = (state.NextActivity + 1) % MaximumCiActivity;
            state.ActivityCount = (std::min)(state.ActivityCount + 1, MaximumCiActivity);
        }

        void RememberInitiator(_Inout_ CiResponderState& state, _In_ uint32_t muid, _In_ uint32_t maximumSysEx) noexcept
        {
            auto* chosen = &state.Initiators[0];

            for (auto& initiator : state.Initiators)
            {
                if (initiator.Muid == muid)
                {
                    chosen = &initiator;
                    break;
                }

                if (initiator.LastUsed < chosen->LastUsed)
                {
                    chosen = &initiator;
                }
            }

            chosen->Muid = muid;
            chosen->MaximumSysEx = maximumSysEx;
            chosen->LastUsed = ++state.Clock;
        }

        void ForgetInitiator(_Inout_ CiResponderState& state, _In_ uint32_t muid) noexcept
        {
            for (auto& initiator : state.Initiators)
            {
                if (initiator.Muid == muid)
                {
                    initiator = CiResponderState::Initiator{};
                }
            }
        }

        size_t InitiatorMaximum(_In_ CiResponderState const& state, _In_ uint32_t muid) noexcept
        {
            uint32_t maximum{ DefaultInitiatorSysEx };

            for (auto const& initiator : state.Initiators)
            {
                if (initiator.Muid == muid && initiator.MaximumSysEx != 0)
                {
                    maximum = initiator.MaximumSysEx;
                    break;
                }
            }

            return (std::min)(static_cast<size_t>(maximum), MaximumCiMessageBytes);
        }

        uint64_t ResponderFingerprint(_In_ CiResponderSettings const& settings) noexcept
        {
            auto hash = Mix(FnvOffset, settings.Manufacturer.data(), settings.Manufacturer.size());

            hash = MixValue(hash, settings.Family);
            hash = MixValue(hash, settings.Model);
            hash = Mix(hash, settings.Version.data(), settings.Version.size());

            for (auto const c : settings.ProductInstanceId)
            {
                hash = MixValue(hash, c);
            }

            hash = MixValue(hash, settings.ProductInstanceId.size());
            hash = MixValue(hash, settings.ProcessInquiry ? 1 : 0);
            hash = MixValue(hash, settings.Description == nullptr ? 0 : settings.Description->Fingerprint);

            return hash;
        }

        // Keeps the MUID, so a change to the step is not a new device to whoever asked before.
        void Configure(_In_ CiResponderSettings const& settings, _Inout_ CiResponderState& state) noexcept
        {
            auto const fingerprint = ResponderFingerprint(settings);

            if (state.Configured && fingerprint == state.ConfiguredFingerprint)
            {
                return;
            }

            auto const* description = settings.Description.get();

            ci::ResponderConfig config{};

            config.Muid = state.Responder.Muid() != 0 ? state.Responder.Muid() : NewMuid();

            for (size_t i = 0; i < 3; i++)
            {
                config.ManufacturerSysExId[i] = static_cast<uint8_t>(settings.Manufacturer[i] & 0x7F);
            }

            config.DeviceFamily = static_cast<uint16_t>(settings.Family & 0x3FFF);
            config.DeviceFamilyModelNumber = static_cast<uint16_t>(settings.Model & 0x3FFF);

            for (size_t i = 0; i < 4; i++)
            {
                config.SoftwareRevisionLevel[i] = static_cast<uint8_t>(settings.Version[i] & 0x7F);
            }

            config.CapabilityCategories = description != nullptr && description->HasProfiles()
                ? ci::CategoryProfileConfiguration
                : static_cast<uint8_t>(0);

            config.ReceivableMaximumSysExSize = static_cast<uint32_t>(MaximumCiMessageBytes);

            // A MIDI 1.0 device has no function blocks.
            config.FunctionBlockNumber = ci::DeviceIdFunctionBlock;

            config.SupportsPropertyExchange = description != nullptr && description->HasProperties();
            config.SimultaneousPropertyRequests = 1;
            config.SupportsProcessInquiry = settings.ProcessInquiry;

            size_t count{ 0 };

            for (auto const c : settings.ProductInstanceId)
            {
                if (count >= ci::ProductInstanceIdMaximumByteCount)
                {
                    break;
                }

                if (c >= 0x20 && c <= 0x7E)
                {
                    config.ProductInstanceId[count++] = static_cast<uint8_t>(c);
                }
            }

            config.ProductInstanceIdByteCount = static_cast<uint8_t>(count);

            state.Responder.Initialize(config);
            state.ConfiguredFingerprint = fingerprint;
            state.Configured = true;
        }

        void Nak(
            _Inout_ CiResponderState& state,
            _In_ ci::ParsedMessage const& message,
            _In_ uint8_t address,
            _In_ uint8_t status,
            _In_reads_opt_(5) uint8_t const* details,
            _In_ CiReturn const& from,
            _Inout_ CiReplyWriter& replies) noexcept
        {
            ci::AcknowledgmentFields fields{};

            fields.OriginalMessageType = static_cast<uint8_t>(message.Type);
            fields.StatusCode = status;

            if (details != nullptr)
            {
                std::copy_n(details, 5, fields.Details);
            }

            auto const written = ci::BuildAcknowledgment(
                ci::MessageType::Nak, address, state.Responder.Muid(), message.SourceMuid,
                fields, nullptr, 0, state.Reply.data(), state.Reply.size());

            if (written != 0)
            {
                WriteSysEx(from.Group, state.Reply.data(), written, replies);
            }

            Log(state, message.Type, message.SourceMuid, CiOutcome::Refused);
        }

        bool ProfileIsAt(_In_ CiProfile const& profile, _In_ uint8_t address) noexcept
        {
            switch (profile.Target)
            {
            case CiProfileTarget::Channel:
                return address == profile.Channel;

            case CiProfileTarget::Group:
                return address == AddressGroup;

            default:
                return address == AddressFunctionBlock;
            }
        }

        bool HasProfileAt(_In_ CiDescription const& description, _In_ uint8_t address) noexcept
        {
            return std::any_of(description.Profiles.begin(), description.Profiles.end(),
                [address](CiProfile const& profile) { return ProfileIsAt(profile, address); });
        }

        CiProfile const* FindProfile(
            _In_ CiDescription const& description,
            _In_ uint8_t address,
            _In_reads_(5) uint8_t const* id) noexcept
        {
            for (auto const& profile : description.Profiles)
            {
                if (ProfileIsAt(profile, address) && std::equal(profile.Id.begin(), profile.Id.end(), id))
                {
                    return &profile;
                }
            }

            return nullptr;
        }

        void SendProfileList(
            _In_ CiDescription const& description,
            _Inout_ CiResponderState& state,
            _In_ ci::ParsedMessage const& message,
            _In_ uint8_t address,
            _In_ CiReturn const& from,
            _Inout_ CiReplyWriter& replies)
        {
            std::vector<uint8_t> enabled{};
            std::vector<uint8_t> disabled{};

            for (auto const& profile : description.Profiles)
            {
                if (ProfileIsAt(profile, address))
                {
                    auto& list = profile.Enabled ? enabled : disabled;
                    list.insert(list.end(), profile.Id.begin(), profile.Id.end());
                }
            }

            auto const written = ci::BuildProfileInquiryReply(
                address, state.Responder.Muid(), message.SourceMuid,
                enabled.empty() ? nullptr : enabled.data(), static_cast<uint16_t>(enabled.size() / 5),
                disabled.empty() ? nullptr : disabled.data(), static_cast<uint16_t>(disabled.size() / 5),
                state.Reply.data(), state.Reply.size());

            if (written != 0)
            {
                WriteSysEx(from.Group, state.Reply.data(), written, replies);
            }
        }

        void AnswerProfile(
            _In_ CiDescription const& description,
            _Inout_ CiResponderState& state,
            _In_ ci::ParsedMessage const& message,
            _In_ CiReturn const& from,
            _Inout_ CiReplyWriter& replies)
        {
            auto const muid = state.Responder.Muid();

            // Answering a conversation between two other devices would spoil it.
            if (muid == 0 || (message.DestinationMuid != muid && message.DestinationMuid != ci::MuidBroadcast))
            {
                return;
            }

            auto const address = message.DeviceId;
            auto const addressable = address <= 0x0F || address == AddressGroup || address == AddressFunctionBlock;

            switch (message.Type)
            {
            case ci::MessageType::ProfileInquiry:
                if (!addressable)
                {
                    Nak(state, message, AddressFunctionBlock, ci::NakStatusNotInUse, nullptr, from, replies);
                    return;
                }

                // Asked of the function block: channels first, then the group, and the function
                // block last, which tells the initiator nothing more is coming.
                if (address == AddressFunctionBlock)
                {
                    for (uint8_t channel = 0; channel < 16; channel++)
                    {
                        if (HasProfileAt(description, channel))
                        {
                            SendProfileList(description, state, message, channel, from, replies);
                        }
                    }

                    if (HasProfileAt(description, AddressGroup))
                    {
                        SendProfileList(description, state, message, AddressGroup, from, replies);
                    }
                }

                SendProfileList(description, state, message, address, from, replies);
                Log(state, message.Type, message.SourceMuid, CiOutcome::Answered);
                return;

            case ci::MessageType::SetProfileOn:
            case ci::MessageType::SetProfileOff:
            {
                if (!message.HasProfileFields || !message.Profile.HasProfileId)
                {
                    Nak(state, message, AddressFunctionBlock, ci::NakStatusMessageMalformed, nullptr, from, replies);
                    return;
                }

                auto const* profile = addressable ? FindProfile(description, address, message.Profile.ProfileId) : nullptr;

                if (profile == nullptr)
                {
                    Nak(state, message, addressable ? address : AddressFunctionBlock,
                        NakStatusProfileNotSupported, message.Profile.ProfileId, from, replies);
                    return;
                }

                // The device does what it does, so the report says how it is, whichever was asked.
                ci::ProfileMessageFields fields{};

                fields.Type = profile->Enabled ? ci::MessageType::ProfileEnabledReport : ci::MessageType::ProfileDisabledReport;
                fields.DeviceId = address;
                fields.SourceMuid = muid;
                fields.DestinationMuid = ci::MuidBroadcast;
                std::copy(profile->Id.begin(), profile->Id.end(), fields.ProfileId);
                fields.ChannelCount = profile->Target == CiProfileTarget::Channel ? profile->ChannelCount : static_cast<uint16_t>(0);
                fields.MessageVersion = ci::ReplyVersionFor(message.VersionFormat);

                auto const written = ci::BuildProfileMessage(fields, state.Reply.data(), state.Reply.size());

                if (written != 0)
                {
                    WriteSysEx(from.Group, state.Reply.data(), written, replies);
                }

                Log(state, message.Type, message.SourceMuid, CiOutcome::Answered);
                return;
            }

            case ci::MessageType::ProfileDetailsInquiry:
            {
                auto const* profile = addressable && message.HasProfileFields && message.Profile.HasProfileId
                    ? FindProfile(description, address, message.Profile.ProfileId)
                    : nullptr;

                if (profile == nullptr)
                {
                    Nak(state, message, addressable ? address : AddressFunctionBlock, NakStatusProfileNotSupported,
                        message.Profile.HasProfileId ? message.Profile.ProfileId : nullptr, from, replies);
                    return;
                }

                for (auto const& detail : profile->Details)
                {
                    if (detail.Target != message.Profile.InquiryTarget)
                    {
                        continue;
                    }

                    ci::ProfileMessageFields fields{};

                    fields.Type = ci::MessageType::ProfileDetailsInquiryReply;
                    fields.DeviceId = address;
                    fields.SourceMuid = muid;
                    fields.DestinationMuid = message.SourceMuid;
                    std::copy(profile->Id.begin(), profile->Id.end(), fields.ProfileId);
                    fields.InquiryTarget = detail.Target;
                    fields.Data = detail.Data.empty() ? nullptr : detail.Data.data();
                    fields.DataByteCount = static_cast<uint32_t>(detail.Data.size());
                    fields.MessageVersion = ci::ReplyVersionFor(message.VersionFormat);

                    auto const written = ci::BuildProfileMessage(fields, state.Reply.data(), state.Reply.size());

                    if (written != 0)
                    {
                        WriteSysEx(from.Group, state.Reply.data(), written, replies);
                    }

                    Log(state, message.Type, message.SourceMuid, CiOutcome::Answered);
                    return;
                }

                Nak(state, message, address, NakStatusGeneral, message.Profile.ProfileId, from, replies);
                return;
            }

            default:
                // Replies and reports are for an initiator, and profile specific data is the
                // device's business.
                return;
            }
        }

        json::JsonObject ParseHeader(_In_reads_(byteCount) uint8_t const* bytes, _In_ size_t byteCount) noexcept
        {
            try
            {
                std::wstring text{};
                text.reserve(byteCount);

                for (size_t i = 0; i < byteCount; i++)
                {
                    text += static_cast<wchar_t>(bytes[i]);
                }

                json::JsonObject object{ nullptr };

                if (json::JsonObject::TryParse(text, object))
                {
                    return object;
                }
            }
            catch (...)
            {
            }

            return nullptr;
        }

        std::string HeaderText(_In_ json::JsonObject const& header, _In_ wchar_t const* key)
        {
            if (header == nullptr || !header.HasKey(key))
            {
                return {};
            }

            auto const value = header.GetNamedValue(key);

            return value.ValueType() == json::JsonValueType::String ? Narrow(value.GetString()) : std::string{};
        }

        bool HeaderNumber(_In_ json::JsonObject const& header, _In_ wchar_t const* key, _Out_ int64_t& number) noexcept
        {
            number = 0;

            try
            {
                if (header == nullptr || !header.HasKey(key))
                {
                    return false;
                }

                auto const value = header.GetNamedValue(key);

                if (value.ValueType() != json::JsonValueType::Number)
                {
                    return false;
                }

                auto const read = value.GetNumber();

                if (!std::isfinite(read))
                {
                    return false;
                }

                number = static_cast<int64_t>(std::floor(std::clamp(read, -1.0e12, 1.0e12)));
                return true;
            }
            catch (...)
            {
            }

            return false;
        }

        // A request without a resource id gets the one without an id, as M2-103 asks.
        CiResource const* FindResource(
            _In_ CiDescription const& description,
            _In_ std::string const& name,
            _In_ std::string const& resId) noexcept
        {
            for (auto const& resource : description.Resources)
            {
                if (resource.Name == name && resource.ResourceId == resId)
                {
                    return &resource;
                }
            }

            return nullptr;
        }

        bool IsKnownResource(_In_ CiDescription const* description, _In_ std::string const& name) noexcept
        {
            if (name == ResourceList || name == DeviceInfo)
            {
                return true;
            }

            return description != nullptr &&
                std::any_of(description->Resources.begin(), description->Resources.end(),
                    [&name](CiResource const& resource) { return resource.Name == name; });
        }

        std::string ResourceListJson(_In_ CiDescription const& description)
        {
            std::vector<std::string> names{};

            for (auto const& resource : description.Resources)
            {
                if (std::find(names.begin(), names.end(), resource.Name) == names.end())
                {
                    names.push_back(resource.Name);
                }
            }

            std::vector<ci::ResourceListEntry> entries{};

            ci::ResourceListEntry deviceInfo{};
            deviceInfo.Resource = DeviceInfo;
            entries.push_back(deviceInfo);

            for (auto const& name : names)
            {
                ci::ResourceListEntry entry{};
                entry.Resource = name.c_str();

                // An entry without an id answers a request without one.
                entry.RequireResourceId = std::none_of(description.Resources.begin(), description.Resources.end(),
                    [&name](CiResource const& resource) { return resource.Name == name && resource.ResourceId.empty(); });

                // Every reply for a list carries its total, so a list can be asked for in pages.
                entry.CanPaginate = std::all_of(description.Resources.begin(), description.Resources.end(),
                    [&name](CiResource const& resource) { return resource.Name != name || resource.IsArray; });

                entries.push_back(entry);
            }

            auto const length = ci::BuildResourceListJson(entries.data(), entries.size(), nullptr, 0);

            std::string text(length, ' ');

            if (length == 0 || ci::BuildResourceListJson(entries.data(), entries.size(), text.data(), text.size()) != length)
            {
                return "[]";
            }

            return text;
        }

        std::string DeviceInfoJson(_In_ CiResponderSettings const& settings, _In_ CiDescription const& description)
        {
            ci::DeviceInfoFields fields{};

            for (size_t i = 0; i < 3; i++)
            {
                fields.ManufacturerId[i] = static_cast<uint8_t>(settings.Manufacturer[i] & 0x7F);
            }

            // Least significant first, the way Discovery carries them.
            fields.FamilyId[0] = static_cast<uint8_t>(settings.Family & 0x7F);
            fields.FamilyId[1] = static_cast<uint8_t>((settings.Family >> 7) & 0x7F);
            fields.ModelId[0] = static_cast<uint8_t>(settings.Model & 0x7F);
            fields.ModelId[1] = static_cast<uint8_t>((settings.Model >> 7) & 0x7F);

            for (size_t i = 0; i < 4; i++)
            {
                fields.VersionId[i] = static_cast<uint8_t>(settings.Version[i] & 0x7F);
            }

            fields.Manufacturer = description.Manufacturer.c_str();
            fields.Family = description.Family.c_str();
            fields.Model = description.Model.c_str();
            fields.Version = description.Version.c_str();

            auto const length = ci::BuildDeviceInfoJson(fields, nullptr, 0);

            std::string text(length, ' ');

            if (length == 0 || ci::BuildDeviceInfoJson(fields, text.data(), text.size()) != length)
            {
                return "{}";
            }

            return text;
        }

        void SendStatusOnly(
            _Inout_ CiResponderState& state,
            _In_ ci::ParsedMessage const& message,
            _In_ ci::MessageType type,
            _In_ std::string const& header,
            _In_ CiReturn const& from,
            _Inout_ CiReplyWriter& replies) noexcept
        {
            ci::PropertyExchangeMessageFields fields{};

            fields.Type = type;
            fields.SourceMuid = state.Responder.Muid();
            fields.DestinationMuid = message.SourceMuid;
            fields.RequestId = message.PropertyExchange.RequestId;
            fields.Header = reinterpret_cast<uint8_t const*>(header.data());
            fields.HeaderByteCount = static_cast<uint16_t>(header.size());
            fields.ChunkCount = 1;
            fields.ChunkNumber = 1;

            auto const written = ci::BuildPropertyExchangeMessage(fields, state.Reply.data(), state.Reply.size());

            if (written != 0)
            {
                WriteSysEx(from.Group, state.Reply.data(), written, replies);
            }
        }

        std::string StatusHeader(_In_ int32_t status)
        {
            return std::string{ R"({"status":)" } + std::to_string(status) + "}";
        }

        void AnswerGet(
            _In_ CiResponderSettings const& settings,
            _Inout_ CiResponderState& state,
            _In_ ci::ParsedMessage const& message,
            _In_reads_(byteCount) uint8_t const* bytes,
            _In_ size_t byteCount,
            _In_ CiReturn const& from,
            _Inout_ CiReplyWriter& replies)
        {
            auto const& exchange = message.PropertyExchange;
            auto const* description = settings.Description.get();

            if (static_cast<size_t>(exchange.HeaderOffset) + exchange.HeaderByteCount > byteCount)
            {
                return;
            }

            auto const request = ParseHeader(bytes + exchange.HeaderOffset, exchange.HeaderByteCount);

            int32_t status{ 200 };
            std::string body{};
            bool paged{ false };
            size_t total{ 0 };

            if (request == nullptr || description == nullptr)
            {
                status = 400;
            }
            else
            {
                auto const resource = HeaderText(request, HeaderResource);
                auto const resId = HeaderText(request, HeaderResId);

                if (resource == ResourceList)
                {
                    body = ResourceListJson(*description);
                }
                else if (resource == DeviceInfo)
                {
                    body = DeviceInfoJson(settings, *description);
                }
                else if (auto const* found = FindResource(*description, resource, resId))
                {
                    if (found->IsArray)
                    {
                        paged = true;
                        total = found->Items.size();

                        int64_t offset{ 0 };
                        int64_t limit{ 0 };

                        HeaderNumber(request, HeaderOffset, offset);

                        auto const hasLimit = HeaderNumber(request, HeaderLimit, limit);

                        auto const first = static_cast<size_t>(std::clamp<int64_t>(offset, 0, static_cast<int64_t>(total)));
                        auto const last = !hasLimit || limit < 1
                            ? total
                            : static_cast<size_t>((std::min)(static_cast<int64_t>(total), static_cast<int64_t>(first) + limit));

                        body = "[";

                        for (size_t i = first; i < last; i++)
                        {
                            if (i > first)
                            {
                                body += ',';
                            }

                            body += found->Items[i];
                        }

                        body += ']';
                    }
                    else
                    {
                        body = found->Json;
                    }
                }
                else
                {
                    status = 404;
                }
            }

            std::string header{ R"({"status":)" };
            header += std::to_string(status);

            if (paged)
            {
                header += R"(,"totalCount":)";
                header += std::to_string(total);
            }

            header += '}';

            ci::PropertyReplyChunker chunker{};

            chunker.Type = ci::MessageType::PropertyGetDataReply;
            chunker.Resource = body.empty() ? nullptr : reinterpret_cast<uint8_t const*>(body.data());
            chunker.ResourceByteCount = body.size();
            chunker.Header = reinterpret_cast<uint8_t const*>(header.data());
            chunker.HeaderByteCount = static_cast<uint16_t>(header.size());

            auto const maximum = InitiatorMaximum(state, message.SourceMuid);

            if (!chunker.Plan(maximum))
            {
                // More than this initiator can take in chunks it can read.
                status = 413;
                header = StatusHeader(status);
                body.clear();

                chunker.Resource = nullptr;
                chunker.ResourceByteCount = 0;
                chunker.Header = reinterpret_cast<uint8_t const*>(header.data());
                chunker.HeaderByteCount = static_cast<uint16_t>(header.size());

                if (!chunker.Plan(maximum))
                {
                    return;
                }
            }

            for (uint16_t chunk = 1; chunk <= chunker.ChunkCount; chunk++)
            {
                auto const written = chunker.BuildChunk(
                    chunk, state.Responder.Muid(), message.SourceMuid, exchange.RequestId,
                    state.Reply.data(), state.Reply.size());

                if (written == 0)
                {
                    break;
                }

                WriteSysEx(from.Group, state.Reply.data(), written, replies);
            }

            Log(state, message.Type, message.SourceMuid, status == 200 ? CiOutcome::Answered : CiOutcome::Refused);
        }

        // Nothing here changes, so there is never anything to subscribe to.
        void AnswerSubscription(
            _In_ CiResponderSettings const& settings,
            _Inout_ CiResponderState& state,
            _In_ ci::ParsedMessage const& message,
            _In_reads_(byteCount) uint8_t const* bytes,
            _In_ size_t byteCount,
            _In_ CiReturn const& from,
            _Inout_ CiReplyWriter& replies)
        {
            auto const& exchange = message.PropertyExchange;

            if (static_cast<size_t>(exchange.HeaderOffset) + exchange.HeaderByteCount > byteCount)
            {
                return;
            }

            auto const request = ParseHeader(bytes + exchange.HeaderOffset, exchange.HeaderByteCount);
            auto const command = HeaderText(request, HeaderCommand);

            int32_t status{ 400 };

            if (request != nullptr && command == "end")
            {
                status = 200;
            }
            else if (request != nullptr && command == "start")
            {
                status = IsKnownResource(settings.Description.get(), HeaderText(request, HeaderResource)) ? 405 : 404;
            }

            SendStatusOnly(state, message, ci::MessageType::PropertySubscriptionReply, StatusHeader(status), from, replies);
            Log(state, message.Type, message.SourceMuid, status == 200 ? CiOutcome::Answered : CiOutcome::Refused);
        }

        void AnswerReport(
            _Inout_ CiResponderState& state,
            _In_ ci::ParsedMessage const& message,
            _In_ uint8_t group,
            _In_ CiReturn const& from,
            _Inout_ CiReplyWriter& replies)
        {
            constexpr uint8_t ReportableControllers =
                ci::ChannelControllerPitchBend |
                ci::ChannelControllerControlChange |
                ci::ChannelControllerProgramChange |
                ci::ChannelControllerChannelPressure;

            auto const& requested = message.MidiMessageReport;

            ci::MidiMessageReportFields reported{};
            reported.ChannelControllerMessages = requested.ChannelControllerMessages & ReportableControllers;
            reported.NoteDataMessages = requested.NoteDataMessages & ci::NoteDataNotes;

            auto const muid = state.Responder.Muid();
            auto const version = ci::ReplyVersionFor(message.VersionFormat);

            auto written = ci::BuildMidiMessageReportReply(
                message.DeviceId, muid, message.SourceMuid, reported, state.Reply.data(), state.Reply.size(), version);

            if (written == 0)
            {
                return;
            }

            WriteSysEx(from.Group, state.Reply.data(), written, replies);

            // Data control 0x00 asks only what could be reported, which the reply has said.
            if (requested.MessageDataControl != ci::MessageDataControlNone)
            {
                // A copy, so notes keep moving while the report is written.
                auto channels = std::make_unique<std::array<CiChannelState, 16>>();
                bool tracked{ false };

                {
                    SpinGuard guard{ state.TrackingLock };

                    auto const slot = state.TrackedGroupSlot[group & 0x0F];

                    if (slot >= 0)
                    {
                        *channels = state.Tracked[static_cast<size_t>(slot)];
                        tracked = true;
                    }
                }

                if (tracked)
                {
                    auto const onlyChanged = requested.MessageDataControl == ci::MessageDataControlNonDefault;
                    auto const oneChannel = message.DeviceId <= 0x0F;
                    auto const first = oneChannel ? message.DeviceId : static_cast<uint8_t>(0);
                    auto const last = oneChannel ? message.DeviceId : static_cast<uint8_t>(15);

                    for (uint8_t channel = first; channel <= last; channel++)
                    {
                        ReportChannel((*channels)[channel], from.Group, channel, reported, onlyChanged, replies);
                    }
                }
            }

            written = ci::BuildMidiMessageReportEnd(
                message.DeviceId, muid, message.SourceMuid, state.Reply.data(), state.Reply.size(), version);

            if (written != 0)
            {
                WriteSysEx(from.Group, state.Reply.data(), written, replies);
            }

            Log(state, message.Type, message.SourceMuid, CiOutcome::Answered);
        }

        // One whole MIDI-CI message, as it reached the step.
        void Answer(
            _In_ CiResponderSettings const& settings,
            _Inout_ CiResponderState& state,
            _In_ CiReturn const& from,
            _In_ uint8_t group,
            _In_reads_(byteCount) uint8_t const* bytes,
            _In_ size_t byteCount,
            _Inout_ CiReplyWriter& replies)
        {
            // Nowhere to answer: a message from a throttle or a generator.
            if (!from.CanReply)
            {
                return;
            }

            ci::ParsedMessage message{};

            if (ci::Parse(bytes, byteCount, message) != ci::ParseStatus::Ok)
            {
                return;
            }

            auto hash = MixValue(MixValue(FnvOffset, from.Leaf), from.Group);
            hash = Mix(hash, bytes, byteCount);

            auto const now = std::chrono::steady_clock::now();

            for (auto const& recent : state.RecentRequests)
            {
                if (recent.Hash == hash && now - recent.Time < RepeatWindow)
                {
                    return;
                }
            }

            state.RecentRequests[state.NextRecentRequest] = CiResponderState::RecentRequest{ hash, now };
            state.NextRecentRequest = (state.NextRecentRequest + 1) % CiRecentRequestSlots;

            Configure(settings, state);

            // How much an initiator can take, which is the size of every chunk it is sent.
            if (message.Type == ci::MessageType::Discovery && byteCount >= 29)
            {
                RememberInitiator(state, message.SourceMuid, ci::ReadTwentyEightBitValue(bytes + 25));
            }

            auto const* description = settings.Description.get();

            if (ci::MessageTypeIsProfileConfiguration(message.Type) && description != nullptr && description->HasProfiles())
            {
                AnswerProfile(*description, state, message, from, replies);
                return;
            }

            size_t written{ 0 };

            auto const action = state.Responder.ProcessMessage(message, state.Reply.data(), state.Reply.size(), &written);

            switch (action)
            {
            case ci::ResponderAction::Replied:
            {
                WriteSysEx(from.Group, state.Reply.data(), written, replies);

                auto const refused = written > ci::OffsetSubId2 &&
                    state.Reply[ci::OffsetSubId2] == static_cast<uint8_t>(ci::MessageType::Nak);

                Log(state, message.Type, message.SourceMuid, refused ? CiOutcome::Refused : CiOutcome::Answered);
                break;
            }

            case ci::ResponderAction::MuidCollision:
                // The reply withdraws the MUID both devices had.
                WriteSysEx(from.Group, state.Reply.data(), written, replies);
                state.Responder.SetMuid(NewMuid());
                Log(state, message.Type, message.SourceMuid, CiOutcome::NewMuid);
                break;

            case ci::ResponderAction::MuidInvalidated:
                state.Responder.SetMuid(NewMuid());
                Log(state, message.Type, message.SourceMuid, CiOutcome::NewMuid);
                break;

            case ci::ResponderAction::InitiatorMuidInvalidated:
                ForgetInitiator(state, message.TargetMuid);
                break;

            case ci::ResponderAction::PropertyDataRequested:
                AnswerGet(settings, state, message, bytes, byteCount, from, replies);
                break;

            case ci::ResponderAction::PropertySubscriptionRequested:
                AnswerSubscription(settings, state, message, bytes, byteCount, from, replies);
                break;

            case ci::ResponderAction::MidiMessageReportRequested:
                AnswerReport(state, message, group, from, replies);
                break;

            default:
                break;
            }
        }

        CiAssembly* FindAssembly(
            _Inout_ CiResponderState& state,
            _In_ uint32_t path,
            _In_ uint32_t leaf,
            _In_ uint8_t group) noexcept
        {
            for (auto& slot : state.Assemblies)
            {
                if (slot.Active && slot.Path == path && slot.Leaf == leaf && slot.Group == group)
                {
                    return &slot;
                }
            }

            return nullptr;
        }

        // A free slot, or the one left longest.
        CiAssembly& ClaimAssembly(_Inout_ CiResponderState& state) noexcept
        {
            auto* chosen = &state.Assemblies[0];

            for (auto& slot : state.Assemblies)
            {
                if (!slot.Active)
                {
                    return slot;
                }

                if (slot.LastUsed < chosen->LastUsed)
                {
                    chosen = &slot;
                }
            }

            return *chosen;
        }

        void Append(_Inout_ CiAssembly& slot, _In_ SysExPacket const& packet) noexcept
        {
            if (slot.Overflowed)
            {
                return;
            }

            if (slot.ByteCount + packet.ByteCount > slot.Bytes.size())
            {
                slot.Overflowed = true;
                return;
            }

            std::copy_n(packet.Bytes.begin(), packet.ByteCount, slot.Bytes.begin() + slot.ByteCount);
            slot.ByteCount += packet.ByteCount;
        }

        // True keeps the packet out.
        bool TakePacket(
            _In_ CiResponderSettings const& settings,
            _Inout_ CiResponderState& state,
            _In_ uint32_t path,
            _In_ CiReturn const& from,
            _In_ SysExPacket const& packet,
            _Inout_ CiReplyWriter& replies)
        {
            auto const leaf = from.CanReply ? from.Leaf : NoLeaf;
            auto const isCi = CiCategoryOfStart(packet) != 0;
            auto const tick = ++state.Clock;

            switch (packet.Status)
            {
            case SysExComplete:
                // Interrupts whatever was arriving, and is too short to be a question.
                if (auto* stale = FindAssembly(state, path, leaf, packet.Group))
                {
                    stale->Active = false;
                }

                return isCi && !settings.PassMidiCi;

            case SysExStart:
            {
                if (auto* stale = FindAssembly(state, path, leaf, packet.Group))
                {
                    stale->Active = false;
                }

                // Other system exclusive is none of the step's business.
                if (!isCi)
                {
                    return false;
                }

                auto& slot = ClaimAssembly(state);

                slot.Active = true;
                slot.KeepOut = !settings.PassMidiCi;
                slot.Overflowed = false;
                slot.Path = path;
                slot.Leaf = leaf;
                slot.Group = packet.Group;
                slot.LastUsed = tick;
                slot.ByteCount = 0;

                Append(slot, packet);

                return slot.KeepOut;
            }

            default:
            {
                auto* slot = FindAssembly(state, path, leaf, packet.Group);

                if (slot == nullptr)
                {
                    return false;
                }

                slot->LastUsed = tick;
                Append(*slot, packet);

                auto const keepOut = slot->KeepOut;

                if (packet.Status == SysExEnd)
                {
                    slot->Active = false;

                    // A message too long to keep can't be answered, and is still kept out.
                    if (!slot->Overflowed)
                    {
                        Answer(settings, state, from, packet.Group, slot->Bytes.data(), slot->ByteCount, replies);
                    }
                }

                return keepOut;
            }
            }
        }

        // ------------------------------------------------------------------ the filter

        constexpr uint64_t FilterInUse = 0x1;
        constexpr uint64_t FilterPasses = 0x2;

        uint64_t FilterKey(_In_ uint32_t path, _In_ uint8_t group) noexcept
        {
            return (static_cast<uint64_t>(path) << 32) | (static_cast<uint64_t>(group & 0x0F) << 8) | FilterInUse;
        }

        void RememberDecision(_Inout_ CiFilterMemory& memory, _In_ uint64_t key, _In_ bool passes) noexcept
        {
            auto const value = key | (passes ? FilterPasses : 0);

            for (auto& slot : memory.Slots)
            {
                if ((slot.load(std::memory_order_relaxed) & ~FilterPasses) == key)
                {
                    slot.store(value, std::memory_order_relaxed);
                    return;
                }
            }

            for (auto& slot : memory.Slots)
            {
                uint64_t empty{ 0 };

                if (slot.compare_exchange_strong(empty, value, std::memory_order_relaxed))
                {
                    return;
                }
            }

            // Sixteen messages part way through at once: one of them may go the wrong way.
            memory.Slots[(key >> 8) % memory.Slots.size()].store(value, std::memory_order_relaxed);
        }

        int32_t RecallDecision(_Inout_ CiFilterMemory& memory, _In_ uint64_t key, _In_ bool forget) noexcept
        {
            for (auto& slot : memory.Slots)
            {
                auto current = slot.load(std::memory_order_relaxed);

                if ((current & ~FilterPasses) == key)
                {
                    if (forget)
                    {
                        slot.compare_exchange_strong(current, 0, std::memory_order_relaxed);
                    }

                    return (current & FilterPasses) != 0 ? 1 : 0;
                }
            }

            return -1;
        }

        bool FilterDecides(_In_ CiFilterSettings const& settings, _In_ uint8_t category) noexcept
        {
            auto const keepingOut = settings.Action == FilterAction::KeepOut;

            // Not MIDI-CI: through when keeping some out, out when only letting some through.
            if (category == 0)
            {
                return keepingOut;
            }

            auto const chosen = (settings.Categories & category) != 0;

            return keepingOut ? !chosen : chosen;
        }
    }

    // ---------------------------------------------------------------------- system exclusive

    _Use_decl_annotations_
    bool ReadSysExPacket(uint32_t const* words, uint8_t wordCount, SysExPacket& packet) noexcept
    {
        packet = SysExPacket{};

        if (words == nullptr || wordCount < 2 || (words[0] >> 28) != TypeSysEx7)
        {
            return false;
        }

        packet.Group = static_cast<uint8_t>((words[0] >> 24) & 0x0F);
        packet.Status = static_cast<uint8_t>((words[0] >> 20) & 0x0F);
        packet.ByteCount = static_cast<uint8_t>((words[0] >> 16) & 0x0F);

        if (packet.Status > SysExEnd || packet.ByteCount > packet.Bytes.size())
        {
            return false;
        }

        packet.Bytes[0] = static_cast<uint8_t>((words[0] >> 8) & 0x7F);
        packet.Bytes[1] = static_cast<uint8_t>(words[0] & 0x7F);
        packet.Bytes[2] = static_cast<uint8_t>((words[1] >> 24) & 0x7F);
        packet.Bytes[3] = static_cast<uint8_t>((words[1] >> 16) & 0x7F);
        packet.Bytes[4] = static_cast<uint8_t>((words[1] >> 8) & 0x7F);
        packet.Bytes[5] = static_cast<uint8_t>(words[1] & 0x7F);

        return true;
    }

    _Use_decl_annotations_
    void WriteSysEx(uint8_t group, uint8_t const* bytes, size_t byteCount, CiReplyWriter& writer) noexcept
    {
        if (bytes == nullptr || byteCount == 0)
        {
            return;
        }

        constexpr size_t BytesPerPacket = 6;

        for (size_t offset = 0; offset < byteCount; offset += BytesPerPacket)
        {
            auto const count = static_cast<uint8_t>((std::min)(BytesPerPacket, byteCount - offset));
            auto const first = offset == 0;
            auto const last = offset + count >= byteCount;

            auto const status = first && last ? SysExComplete : first ? SysExStart : last ? SysExEnd : SysExContinue;

            uint8_t packet[BytesPerPacket]{};

            for (uint8_t i = 0; i < count; i++)
            {
                packet[i] = static_cast<uint8_t>(bytes[offset + i] & 0x7F);
            }

            uint32_t const words[2]
            {
                (TypeSysEx7 << 28) | (static_cast<uint32_t>(group & 0x0F) << 24) |
                    (static_cast<uint32_t>(status) << 20) | (static_cast<uint32_t>(count) << 16) |
                    (static_cast<uint32_t>(packet[0]) << 8) | packet[1],

                (static_cast<uint32_t>(packet[2]) << 24) | (static_cast<uint32_t>(packet[3]) << 16) |
                    (static_cast<uint32_t>(packet[4]) << 8) | packet[5],
            };

            writer.Write(words, 2);
        }
    }

    _Use_decl_annotations_
    uint8_t CiCategoryOfStart(SysExPacket const& packet) noexcept
    {
        if ((packet.Status != SysExComplete && packet.Status != SysExStart) || packet.ByteCount < 3)
        {
            return 0;
        }

        if (packet.Bytes[0] != ci::UniversalSystemExclusiveId || packet.Bytes[2] != ci::SubId1CapabilityInquiry)
        {
            return 0;
        }

        if (packet.ByteCount < 4)
        {
            return CiCategoryManagement;
        }

        auto const type = packet.Bytes[3];

        if (type >= 0x20 && type <= 0x2F)
        {
            return CiCategoryProfiles;
        }

        if (type >= 0x30 && type <= 0x3F)
        {
            return CiCategoryPropertyExchange;
        }

        if (type >= 0x40 && type <= 0x4F)
        {
            return CiCategoryProcessInquiry;
        }

        return CiCategoryManagement;
    }

    // ---------------------------------------------------------------------- the MIDI-CI file

    _Use_decl_annotations_
    std::shared_ptr<CiDescription> ParseCiDescription(std::wstring_view text, std::vector<CiFileProblem>& problems) noexcept
    {
        try
        {
            problems.clear();

            if (text.size() > MaximumCiFileBytes)
            {
                problems.push_back(CiFileProblem{ CiFileProblemKind::TooLarge, -1, {} });
                return nullptr;
            }

            json::JsonObject root{ nullptr };

            if (!json::JsonObject::TryParse(winrt::hstring{ text }, root) || root == nullptr)
            {
                problems.push_back(CiFileProblem{ CiFileProblemKind::NotJson, -1, {} });
                return nullptr;
            }

            auto description = std::make_shared<CiDescription>();

            for (auto const& entry : root)
            {
                auto const key = entry.Key();

                if (key == KeyProfiles)
                {
                    auto const first = problems.size();

                    ReadProfiles(entry.Value(), *description, problems);
                    MarkSection(problems, first, CiFileSection::Profiles);
                }
                else if (key == KeyDeviceInfo)
                {
                    auto const first = problems.size();

                    ReadDeviceInfo(entry.Value(), *description, problems);
                    MarkSection(problems, first, CiFileSection::DeviceInfo);
                }
                else if (key == KeyResources)
                {
                    auto const first = problems.size();

                    ReadResources(entry.Value(), *description, problems);
                    MarkSection(problems, first, CiFileSection::Resources);
                }
                else
                {
                    problems.push_back(CiFileProblem{ CiFileProblemKind::UnknownKey, -1, std::wstring{ key } });
                }
            }

            description->Fingerprint = FingerprintOf(*description);

            return description;
        }
        catch (...)
        {
        }

        try
        {
            problems.clear();
            problems.push_back(CiFileProblem{ CiFileProblemKind::NotJson, -1, {} });
        }
        catch (...)
        {
        }

        return nullptr;
    }

    // ---------------------------------------------------------------------- the responder

    CiResponderState::CiResponderState()
    {
        ci::ResponderConfig config{};
        config.Muid = NewMuid();

        Responder.Initialize(config);
    }

    CiResponderSnapshot CiResponderState::Snapshot() const
    {
        CiResponderSnapshot snapshot{};

        std::scoped_lock guard{ Lock };

        snapshot.Muid = Responder.Muid();
        snapshot.Recent.reserve(ActivityCount);

        for (size_t i = 0; i < ActivityCount; i++)
        {
            snapshot.Recent.push_back(Activity[(NextActivity + MaximumCiActivity - 1 - i) % MaximumCiActivity]);
        }

        return snapshot;
    }

    _Use_decl_annotations_
    bool RunCiResponder(
        CiResponderSettings const& settings,
        CiResponderState& state,
        uint32_t path,
        CiReturn const& from,
        uint32_t const* words,
        uint8_t wordCount,
        CiReplyWriter& writer) noexcept
    {
        if (words == nullptr || wordCount == 0)
        {
            return false;
        }

        auto const type = words[0] >> 28;

        if (type == TypeMidi1 || type == TypeMidi2)
        {
            if (settings.ProcessInquiry)
            {
                Track(state, words, wordCount);
            }

            return true;
        }

        SysExPacket packet{};

        if (!ReadSysExPacket(words, wordCount, packet))
        {
            return true;
        }

        try
        {
            Collector replies{};
            bool keepOut{ false };

            {
                std::scoped_lock guard{ state.Lock };
                keepOut = TakePacket(settings, state, path, from, packet, replies);
            }

            // Sent once the lock is free, so nothing else waits while the service takes them.
            replies.SendTo(writer);

            return !keepOut;
        }
        catch (...)
        {
        }

        return settings.PassMidiCi || CiCategoryOfStart(packet) == 0;
    }

    // ---------------------------------------------------------------------- the filter

    _Use_decl_annotations_
    bool RunCiFilter(
        CiFilterSettings const& settings,
        CiFilterMemory& memory,
        uint32_t path,
        uint32_t const* words,
        uint8_t wordCount) noexcept
    {
        if (words == nullptr || wordCount == 0)
        {
            return false;
        }

        SysExPacket packet{};

        if (!ReadSysExPacket(words, wordCount, packet))
        {
            return FilterDecides(settings, 0);
        }

        auto const key = FilterKey(path, packet.Group);

        switch (packet.Status)
        {
        case SysExComplete:
            RecallDecision(memory, key, true);
            return FilterDecides(settings, CiCategoryOfStart(packet));

        case SysExStart:
        {
            auto const passes = FilterDecides(settings, CiCategoryOfStart(packet));
            RememberDecision(memory, key, passes);
            return passes;
        }

        default:
        {
            auto const decided = RecallDecision(memory, key, packet.Status == SysExEnd);

            // The start went by before the step was there: decided as if it weren't MIDI-CI.
            return decided < 0 ? FilterDecides(settings, 0) : decided == 1;
        }
        }
    }
}
