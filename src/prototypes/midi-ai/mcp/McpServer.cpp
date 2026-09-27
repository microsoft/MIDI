// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "McpServer.h"
#include "ToolText.h"

namespace midimcp
{
    namespace
    {
        constexpr wchar_t MetaKey[] = L"_meta";
        constexpr wchar_t MetaProtocolVersion[] = L"io.modelcontextprotocol/protocolVersion";
        constexpr wchar_t MetaClientInfo[] = L"io.modelcontextprotocol/clientInfo";
        constexpr wchar_t MetaServerInfo[] = L"io.modelcontextprotocol/serverInfo";

        // Nothing here changes while the process runs, so a client may keep what it was told.
        constexpr double ListTimeToLiveMilliseconds = 60.0 * 60.0 * 1000.0;

        json::JsonObject MakeResponse(json::IJsonValue const& id, json::JsonObject const& result)
        {
            json::JsonObject response{};

            response.SetNamedValue(L"jsonrpc", json::JsonValue::CreateStringValue(L"2.0"));
            response.SetNamedValue(L"id", id != nullptr ? id : json::JsonValue::CreateNullValue());
            response.SetNamedValue(L"result", result);

            return response;
        }

        std::string ToLine(json::JsonObject const& message)
        {
            // Stringify escapes control characters, so the text never holds a raw newline.
            auto line = WideToUtf8(std::wstring{ message.Stringify() });
            line.push_back('\n');

            return line;
        }

        bool IsBlank(std::string_view line) noexcept
        {
            return std::all_of(line.begin(), line.end(), [](char c) { return c == ' ' || c == '\t' || c == '\r'; });
        }

        bool WriteAll(HANDLE output, std::string const& bytes) noexcept
        {
            size_t offset{ 0 };

            while (offset < bytes.size())
            {
                DWORD written{ 0 };
                auto const chunk = static_cast<DWORD>(std::min<size_t>(bytes.size() - offset, 1 << 20));

                if (!::WriteFile(output, bytes.data() + offset, chunk, &written, nullptr) || written == 0)
                {
                    return false;
                }

                offset += written;
            }

            return true;
        }

        json::JsonArray VersionList()
        {
            json::JsonArray versions{};

            versions.Append(json::JsonValue::CreateStringValue(ModernProtocolVersion));

            for (auto const version : LegacyProtocolVersions)
            {
                versions.Append(json::JsonValue::CreateStringValue(version));
            }

            return versions;
        }
    }

    _Use_decl_annotations_
    void ToolResult::AddText(std::wstring const& text)
    {
        json::JsonObject block{};
        block.SetNamedValue(L"type", json::JsonValue::CreateStringValue(L"text"));
        block.SetNamedValue(L"text", json::JsonValue::CreateStringValue(text));

        Content.Append(block);
    }

    _Use_decl_annotations_
    void ToolResult::AddImage(std::wstring const& base64Data, std::wstring const& mimeType)
    {
        json::JsonObject block{};
        block.SetNamedValue(L"type", json::JsonValue::CreateStringValue(L"image"));
        block.SetNamedValue(L"data", json::JsonValue::CreateStringValue(base64Data));
        block.SetNamedValue(L"mimeType", json::JsonValue::CreateStringValue(mimeType));

        Content.Append(block);
    }

    _Use_decl_annotations_
    ToolResult ToolResult::Error(std::wstring const& text)
    {
        ToolResult result{};
        result.AddText(text);
        result.IsError = true;

        return result;
    }

    _Use_decl_annotations_
    json::JsonObject MakeErrorResponse(json::IJsonValue const& id, int32_t code, std::wstring const& message, json::IJsonValue const& data)
    {
        json::JsonObject error{};
        error.SetNamedValue(L"code", json::JsonValue::CreateNumberValue(code));
        error.SetNamedValue(L"message", json::JsonValue::CreateStringValue(message));

        if (data != nullptr)
        {
            error.SetNamedValue(L"data", data);
        }

        json::JsonObject response{};
        response.SetNamedValue(L"jsonrpc", json::JsonValue::CreateStringValue(L"2.0"));
        response.SetNamedValue(L"id", id != nullptr ? id : json::JsonValue::CreateNullValue());
        response.SetNamedValue(L"error", error);

        return response;
    }

    _Use_decl_annotations_
    McpServer::McpServer(ServerIdentity identity) noexcept :
        m_identity{ std::move(identity) }
    {
    }

    _Use_decl_annotations_
    void McpServer::AddTool(ToolDefinition tool)
    {
        json::JsonObject schema{ nullptr };

        if (!json::JsonObject::TryParse(tool.InputSchema, schema) || schema == nullptr)
        {
            throw std::invalid_argument("A tool input schema is not a JSON object.");
        }

        json::JsonObject annotations{};
        annotations.SetNamedValue(L"title", json::JsonValue::CreateStringValue(tool.Title));
        annotations.SetNamedValue(L"readOnlyHint", json::JsonValue::CreateBooleanValue(tool.Annotations.ReadOnly));
        annotations.SetNamedValue(L"destructiveHint", json::JsonValue::CreateBooleanValue(tool.Annotations.Destructive));
        annotations.SetNamedValue(L"idempotentHint", json::JsonValue::CreateBooleanValue(tool.Annotations.Idempotent));
        annotations.SetNamedValue(L"openWorldHint", json::JsonValue::CreateBooleanValue(tool.Annotations.OpenWorld));

        json::JsonObject description{};
        description.SetNamedValue(L"name", json::JsonValue::CreateStringValue(tool.Name));
        description.SetNamedValue(L"title", json::JsonValue::CreateStringValue(tool.Title));
        description.SetNamedValue(L"description", json::JsonValue::CreateStringValue(tool.Description));
        description.SetNamedValue(L"inputSchema", schema);
        description.SetNamedValue(L"annotations", annotations);

        m_toolDescriptions.push_back(description);
        m_tools.push_back(std::move(tool));
    }

    _Use_decl_annotations_
    int McpServer::Run(HANDLE input, HANDLE output) noexcept
    {
        std::string pending{};
        std::vector<char> buffer(64 * 1024);
        bool discarding{ false };
        bool firstBytes{ true };

        for (;;)
        {
            DWORD read{ 0 };

            // End of input is how a client ends the session; a broken pipe means it went away.
            if (!::ReadFile(input, buffer.data(), static_cast<DWORD>(buffer.size()), &read, nullptr) || read == 0)
            {
                return 0;
            }

            pending.append(buffer.data(), read);

            if (firstBytes && pending.size() >= 3)
            {
                if (static_cast<unsigned char>(pending[0]) == 0xEF &&
                    static_cast<unsigned char>(pending[1]) == 0xBB &&
                    static_cast<unsigned char>(pending[2]) == 0xBF)
                {
                    pending.erase(0, 3);
                }

                firstBytes = false;
            }

            size_t start{ 0 };

            for (;;)
            {
                auto const newline = pending.find('\n', start);

                if (newline == std::string::npos)
                {
                    break;
                }

                std::string_view line{ pending.data() + start, newline - start };
                start = newline + 1;

                if (discarding)
                {
                    discarding = false;
                    continue;
                }

                if (IsBlank(line))
                {
                    continue;
                }

                if (auto const reply = HandleLine(line); reply.has_value())
                {
                    if (!WriteAll(output, *reply))
                    {
                        return 1;
                    }
                }
            }

            pending.erase(0, start);

            if (pending.size() > MaximumMessageBytes)
            {
                pending.clear();

                if (!discarding)
                {
                    discarding = true;

                    if (!WriteAll(output, ToLine(MakeErrorResponse(nullptr, InvalidRequestCode, L"Message too large."))))
                    {
                        return 1;
                    }
                }
            }
        }
    }

    _Use_decl_annotations_
    std::optional<std::string> McpServer::HandleLine(std::string_view line) noexcept
    {
        try
        {
            if (line.size() > MaximumMessageBytes)
            {
                return ToLine(MakeErrorResponse(nullptr, InvalidRequestCode, L"Message too large."));
            }

            json::JsonValue parsed{ nullptr };

            if (!json::JsonValue::TryParse(Utf8ToWide(line), parsed) || parsed == nullptr)
            {
                return ToLine(MakeErrorResponse(nullptr, ParseErrorCode, L"Parse error."));
            }

            if (parsed.ValueType() != json::JsonValueType::Object)
            {
                // JSON-RPC batches were removed from MCP in 2025-06-18.
                return ToLine(MakeErrorResponse(nullptr, InvalidRequestCode, L"Expected one JSON-RPC message per line."));
            }

            auto const message = parsed.GetObject();

            // A response to a request we never send, or something that is not a request at all.
            if (!message.HasKey(L"method"))
            {
                return std::nullopt;
            }

            auto const method = StringOrEmpty(message, L"method");
            auto const params = ObjectOrNull(message, L"params");

            if (!message.HasKey(L"id"))
            {
                HandleNotification(method, params);
                return std::nullopt;
            }

            auto const id = message.GetNamedValue(L"id");

            if (id.ValueType() != json::JsonValueType::String && id.ValueType() != json::JsonValueType::Number)
            {
                return ToLine(MakeErrorResponse(nullptr, InvalidRequestCode, L"A request id must be a string or a number."));
            }

            if (StringOrEmpty(message, L"jsonrpc") != L"2.0" || method.empty())
            {
                return ToLine(MakeErrorResponse(id, InvalidRequestCode, L"Not a JSON-RPC 2.0 request."));
            }

            return ToLine(Dispatch(method, params, id));
        }
        catch (...)
        {
        }

        try
        {
            return ToLine(MakeErrorResponse(nullptr, InternalErrorCode, L"Internal error."));
        }
        catch (...)
        {
        }

        return std::nullopt;
    }

    _Use_decl_annotations_
    void McpServer::HandleNotification(std::wstring const&, json::JsonObject const&) noexcept
    {
        // Nothing to do for any of them. Requests are handled one at a time and to completion, so
        // by the time a client cancels a request, it has already been answered.
    }

    _Use_decl_annotations_
    json::JsonObject McpServer::Dispatch(std::wstring const& method, json::JsonObject const& params, json::IJsonValue const& id) noexcept
    {
        try
        {
            // The handshake always selects the old semantics, whatever else the client sends.
            if (method == L"initialize")
            {
                return MakeResponse(id, Initialize(params));
            }

            CallContext context{};

            auto const meta = ObjectOrNull(params, MetaKey);
            auto const requested = StringOrEmpty(meta, MetaProtocolVersion);

            if (!requested.empty())
            {
                if (requested != ModernProtocolVersion)
                {
                    json::JsonObject data{};
                    data.SetNamedValue(L"supported", VersionList());
                    data.SetNamedValue(L"requested", json::JsonValue::CreateStringValue(requested));

                    return MakeErrorResponse(id, UnsupportedProtocolVersionCode, L"Unsupported protocol version", data);
                }

                context.IsModern = true;
                context.ProtocolVersion = requested;
                context.ClientName = StringOrEmpty(ObjectOrNull(meta, MetaClientInfo), L"name");
            }
            else
            {
                // A request with no version on it comes from a client of the handshake era. Serve it
                // even if it skipped the handshake; refusing would help nobody.
                context.ProtocolVersion = m_legacyVersion.empty() ? std::wstring{ LegacyProtocolVersions[0] } : m_legacyVersion;
                context.ClientName = m_legacyClientName;
            }

            if (method == L"server/discover")
            {
                return MakeResponse(id, Discover(context));
            }

            if (method == L"tools/list")
            {
                return MakeResponse(id, ListTools(context));
            }

            if (method == L"tools/call")
            {
                return CallTool(params, context, id);
            }

            // Removed in 2026-07-28 but still sent by clients of the handshake era.
            if (method == L"ping")
            {
                json::JsonObject empty{};

                if (context.IsModern)
                {
                    StampModernResult(empty, false);
                }

                return MakeResponse(id, empty);
            }

            return MakeErrorResponse(id, MethodNotFoundCode, L"Method not found: " + method);
        }
        catch (winrt::hresult_error const& error)
        {
            return MakeErrorResponse(id, InternalErrorCode, std::wstring{ L"Internal error: " } + std::wstring{ error.message() });
        }
        catch (std::exception const&)
        {
        }
        catch (...)
        {
        }

        return MakeErrorResponse(id, InternalErrorCode, L"Internal error.");
    }

    _Use_decl_annotations_
    json::JsonObject McpServer::Initialize(json::JsonObject const& params)
    {
        auto const requested = StringOrEmpty(params, L"protocolVersion");

        // Echo the client's version when this server speaks it; otherwise offer the newest one and
        // let the client decide whether it can carry on.
        m_legacyVersion = std::wstring{ LegacyProtocolVersions[0] };

        for (auto const version : LegacyProtocolVersions)
        {
            if (requested == version)
            {
                m_legacyVersion = requested;
                break;
            }
        }

        m_legacyClientName = StringOrEmpty(ObjectOrNull(params, L"clientInfo"), L"name");

        json::JsonObject result{};
        result.SetNamedValue(L"protocolVersion", json::JsonValue::CreateStringValue(m_legacyVersion));
        result.SetNamedValue(L"capabilities", Capabilities());
        result.SetNamedValue(L"serverInfo", ServerInfo());

        if (!m_identity.Instructions.empty())
        {
            result.SetNamedValue(L"instructions", json::JsonValue::CreateStringValue(m_identity.Instructions));
        }

        return result;
    }

    _Use_decl_annotations_
    json::JsonObject McpServer::Discover(CallContext const&) const
    {
        json::JsonObject result{};
        result.SetNamedValue(L"supportedVersions", VersionList());
        result.SetNamedValue(L"capabilities", Capabilities());

        if (!m_identity.Instructions.empty())
        {
            result.SetNamedValue(L"instructions", json::JsonValue::CreateStringValue(m_identity.Instructions));
        }

        StampModernResult(result, true);

        return result;
    }

    _Use_decl_annotations_
    json::JsonObject McpServer::ListTools(CallContext const& context) const
    {
        json::JsonArray tools{};

        // Registration order, which never changes, so a client's cached copy stays valid.
        for (auto const& description : m_toolDescriptions)
        {
            tools.Append(description);
        }

        json::JsonObject result{};
        result.SetNamedValue(L"tools", tools);

        if (context.IsModern)
        {
            StampModernResult(result, true);
        }

        return result;
    }

    _Use_decl_annotations_
    json::JsonObject McpServer::CallTool(json::JsonObject const& params, CallContext const& context, json::IJsonValue const& id)
    {
        auto const name = StringOrEmpty(params, L"name");

        auto const tool = std::find_if(m_tools.begin(), m_tools.end(),
            [&name](ToolDefinition const& t) { return t.Name == name; });

        if (tool == m_tools.end())
        {
            return MakeErrorResponse(id, InvalidParamsCode, L"Unknown tool: " + name);
        }

        auto arguments = ObjectOrNull(params, L"arguments");

        if (arguments == nullptr)
        {
            arguments = json::JsonObject{};
        }

        ToolResult outcome{};

        try
        {
            outcome = tool->Handler(arguments, context);
        }
        catch (winrt::hresult_error const& error)
        {
            outcome = ToolResult::Error(L"The tool failed: " + std::wstring{ error.message() });
        }
        catch (std::exception const& error)
        {
            outcome = ToolResult::Error(L"The tool failed: " + Utf8ToWide(error.what()));
        }
        catch (...)
        {
            outcome = ToolResult::Error(L"The tool failed.");
        }

        // No structuredContent: a host that gets it, such as VS Code, gives the model that instead of this text.
        json::JsonObject result{};
        result.SetNamedValue(L"content", outcome.Content);
        result.SetNamedValue(L"isError", json::JsonValue::CreateBooleanValue(outcome.IsError));

        if (context.IsModern)
        {
            StampModernResult(result, false);
        }

        return MakeResponse(id, result);
    }

    json::JsonObject McpServer::ServerInfo() const
    {
        json::JsonObject info{};
        info.SetNamedValue(L"name", json::JsonValue::CreateStringValue(m_identity.Name));
        info.SetNamedValue(L"title", json::JsonValue::CreateStringValue(m_identity.Title));
        info.SetNamedValue(L"version", json::JsonValue::CreateStringValue(m_identity.Version));

        return info;
    }

    json::JsonObject McpServer::Capabilities() const
    {
        json::JsonObject tools{};
        tools.SetNamedValue(L"listChanged", json::JsonValue::CreateBooleanValue(false));

        json::JsonObject capabilities{};
        capabilities.SetNamedValue(L"tools", tools);

        return capabilities;
    }

    _Use_decl_annotations_
    void McpServer::StampModernResult(json::JsonObject& result, bool cacheable) const
    {
        result.SetNamedValue(L"resultType", json::JsonValue::CreateStringValue(L"complete"));

        json::JsonObject meta{};
        meta.SetNamedValue(MetaServerInfo, ServerInfo());
        result.SetNamedValue(MetaKey, meta);

        if (cacheable)
        {
            result.SetNamedValue(L"ttlMs", json::JsonValue::CreateNumberValue(ListTimeToLiveMilliseconds));
            result.SetNamedValue(L"cacheScope", json::JsonValue::CreateStringValue(L"private"));
        }
    }
}
