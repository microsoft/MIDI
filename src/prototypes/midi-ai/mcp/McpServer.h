// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

// PROTOTYPE. A Model Context Protocol server over stdio, written against the specification with
// no MCP SDK and no third-party code: JSON-RPC 2.0, one message per line, Windows.Data.Json.
//
// It speaks both eras of the protocol. The 2026-07-28 revision is stateless and every request
// names its own protocol version in _meta; earlier revisions open with an initialize handshake
// and the version agreed there holds for the life of the process. Clients of both kinds exist
// today, so a server that wants to work everywhere has to answer both.

#pragma once

#include "pch.h"

namespace midimcp
{
    // The stateless revision. Requests that carry this in _meta are served without a handshake.
    constexpr wchar_t ModernProtocolVersion[] = L"2026-07-28";

    // Handshake revisions, newest first. The first one is what an unknown request is offered.
    constexpr std::wstring_view LegacyProtocolVersions[] =
    {
        L"2025-11-25",
        L"2025-06-18",
        L"2025-03-26",
        L"2024-11-05",
    };

    // JSON-RPC and MCP error codes this server returns.
    constexpr int32_t ParseErrorCode = -32700;
    constexpr int32_t InvalidRequestCode = -32600;
    constexpr int32_t MethodNotFoundCode = -32601;
    constexpr int32_t InvalidParamsCode = -32602;
    constexpr int32_t InternalErrorCode = -32603;
    constexpr int32_t UnsupportedProtocolVersionCode = -32022;

    // A line longer than this is dropped unread. Messages come from another process on the same
    // PC, but that process is still not something to trust with an unbounded allocation.
    constexpr size_t MaximumMessageBytes = 8 * 1024 * 1024;

    // Who is asking, as far as the request says. Self-reported by the client and never used for a
    // security decision; it goes into a draft so the customer can see which assistant wrote it.
    struct CallContext
    {
        bool IsModern{ false };
        std::wstring ProtocolVersion{};
        std::wstring ClientName{};
    };

    // What a tool hands back to the model.
    struct ToolResult
    {
        json::JsonArray Content{};

        // A tool error, as opposed to a protocol error. The model is shown it and can correct
        // itself, which is what "that endpoint name matches three devices" needs.
        bool IsError{ false };

        void AddText(_In_ std::wstring const& text);
        void AddImage(_In_ std::wstring const& base64Data, _In_ std::wstring const& mimeType);

        static ToolResult Error(_In_ std::wstring const& text);
    };

    // Hints for the host. Untrusted by the host unless it trusts this server, which is fine: they
    // exist so a host can skip the confirmation for a tool that only reads.
    struct ToolAnnotations
    {
        bool ReadOnly{ true };
        bool Destructive{ false };
        bool Idempotent{ true };
        bool OpenWorld{ false };
    };

    using ToolHandler = std::function<ToolResult(json::JsonObject const& arguments, CallContext const& context)>;

    struct ToolDefinition
    {
        std::wstring Name{};
        std::wstring Title{};
        std::wstring Description{};

        // JSON Schema text for the arguments. Parsed once when the tool is added.
        std::wstring InputSchema{};

        ToolAnnotations Annotations{};
        ToolHandler Handler{};
    };

    struct ServerIdentity
    {
        std::wstring Name{};
        std::wstring Title{};
        std::wstring Version{};

        // Guidance for the model on how to use the tools together. Hosts put it in the prompt.
        std::wstring Instructions{};
    };

    class McpServer
    {
    public:
        explicit McpServer(_In_ ServerIdentity identity) noexcept;

        // Throws when the schema is not a JSON object, so a typo fails at startup rather than the
        // first time a client lists the tools.
        void AddTool(_In_ ToolDefinition tool);

        // Serves one message per line until the input ends, which is how a client says it is done.
        int Run(_In_ HANDLE input, _In_ HANDLE output) noexcept;

        // One line in, and the line to write back if there is one. Never throws.
        std::optional<std::string> HandleLine(_In_ std::string_view line) noexcept;

    private:
        json::JsonObject Dispatch(
            _In_ std::wstring const& method,
            _In_ json::JsonObject const& params,
            _In_ json::IJsonValue const& id) noexcept;

        void HandleNotification(_In_ std::wstring const& method, _In_ json::JsonObject const& params) noexcept;

        json::JsonObject Initialize(_In_ json::JsonObject const& params);
        json::JsonObject Discover(_In_ CallContext const& context) const;
        json::JsonObject ListTools(_In_ CallContext const& context) const;
        json::JsonObject CallTool(_In_ json::JsonObject const& params, _In_ CallContext const& context, _In_ json::IJsonValue const& id);

        json::JsonObject ServerInfo() const;
        json::JsonObject Capabilities() const;
        void StampModernResult(_Inout_ json::JsonObject& result, _In_ bool cacheable) const;

        ServerIdentity m_identity{};
        std::vector<ToolDefinition> m_tools{};
        std::vector<json::JsonObject> m_toolDescriptions{};

        // Agreed by initialize for the old handshake revisions. Empty until then.
        std::wstring m_legacyVersion{};
        std::wstring m_legacyClientName{};
    };

    json::JsonObject MakeErrorResponse(
        _In_ json::IJsonValue const& id,
        _In_ int32_t code,
        _In_ std::wstring const& message,
        _In_ json::IJsonValue const& data = nullptr);
}
