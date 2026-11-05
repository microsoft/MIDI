// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <winsock2.h>
#include <ws2tcpip.h>

#include "DeviceLinkChannel.h"

#define SECURITY_WIN32
#define SCHANNEL_USE_BLACKLISTS
#include <subauth.h>
#include <sspi.h>
#include <schannel.h>

#include <algorithm>
#include <cstring>
#include <mutex>
#include <string>

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "secur32.lib")

namespace midiapp
{
    namespace
    {
        // A TLS record is at most about 16 KB, so more than this waiting undecrypted means
        // something other than TLS is arriving.
        constexpr size_t MaximumPendingBytes = 64 * 1024;

        constexpr size_t ReceiveChunkBytes = 16 * 1024;

        constexpr int32_t MaximumHandshakeRounds = 32;

        constexpr HRESULT ConnectionClosed = HRESULT_FROM_WIN32(WSAECONNRESET);

        constexpr DWORD ClientRequest =
            ISC_REQ_SEQUENCE_DETECT | ISC_REQ_REPLAY_DETECT | ISC_REQ_CONFIDENTIALITY |
            ISC_REQ_ALLOCATE_MEMORY | ISC_REQ_STREAM | ISC_REQ_USE_SUPPLIED_CREDS |
            ISC_REQ_MANUAL_CRED_VALIDATION | ISC_REQ_EXTENDED_ERROR;

        constexpr DWORD ServerRequest =
            ASC_REQ_SEQUENCE_DETECT | ASC_REQ_REPLAY_DETECT | ASC_REQ_CONFIDENTIALITY |
            ASC_REQ_ALLOCATE_MEMORY | ASC_REQ_STREAM | ASC_REQ_MUTUAL_AUTH | ASC_REQ_EXTENDED_ERROR;

        bool EnsureWinsock() noexcept
        {
            static bool const started = []() noexcept
                {
                    WSADATA data{};
                    return ::WSAStartup(MAKEWORD(2, 2), &data) == 0;
                }();

            return started;
        }

        HRESULT LastSocketError() noexcept
        {
            auto const error = ::WSAGetLastError();
            return error == 0 ? E_FAIL : HRESULT_FROM_WIN32(error);
        }

        void ApplyTimeout(_In_ SOCKET socket, _In_ uint32_t milliseconds) noexcept
        {
            DWORD const value = milliseconds;
            ::setsockopt(socket, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<char const*>(&value), sizeof(value));
            ::setsockopt(socket, SOL_SOCKET, SO_SNDTIMEO, reinterpret_cast<char const*>(&value), sizeof(value));
        }

        void NoDelay(_In_ SOCKET socket) noexcept
        {
            BOOL const on{ TRUE };
            ::setsockopt(socket, IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<char const*>(&on), sizeof(on));
        }

        // TLS 1.3 only. Both ends are this app on a supported Windows, so nothing older is needed.
        constexpr DWORD DisabledProtocols = static_cast<DWORD>(~(SP_PROT_TLS1_3_CLIENT | SP_PROT_TLS1_3_SERVER));

        void FreeOutput(_Inout_ SecBuffer* buffers, _In_ size_t count) noexcept
        {
            for (size_t i = 0; i < count; ++i)
            {
                if (buffers[i].pvBuffer != nullptr)
                {
                    ::FreeContextBuffer(buffers[i].pvBuffer);
                    buffers[i].pvBuffer = nullptr;
                }
            }
        }
    }

    struct DeviceLinkConnection::State
    {
        std::mutex SocketLock{};
        SOCKET Socket{ INVALID_SOCKET };

        bool IsServer{ false };
        PCCERT_CONTEXT Certificate{ nullptr };
        uint32_t TimeoutMilliseconds{ DeviceLinkDefaultTimeoutMilliseconds };

        CredHandle Credentials{};
        bool HasCredentials{ false };

        CtxtHandle Context{};
        bool HasContext{ false };

        SecPkgContext_StreamSizes Sizes{};

        std::vector<uint8_t> Incoming{};
        std::vector<uint8_t> Plain{};
        size_t PlainOffset{ 0 };

        bool PeerClosed{ false };
        bool Closed{ false };
        std::atomic<bool> Aborted{ false };

        SOCKET CurrentSocket()
        {
            std::scoped_lock guard{ SocketLock };
            return Socket;
        }
    };

    DeviceLinkConnection::DeviceLinkConnection() noexcept = default;

    DeviceLinkConnection::~DeviceLinkConnection()
    {
        Close();
    }

    _Use_decl_annotations_
    std::unique_ptr<DeviceLinkConnection> DeviceLinkConnection::Connect(
        std::wstring const& host,
        uint16_t port,
        DeviceIdentity const& identity,
        uint32_t timeoutMilliseconds,
        HRESULT& error) noexcept
    {
        error = E_FAIL;

        try
        {
            if (!EnsureWinsock())
            {
                error = LastSocketError();
                return nullptr;
            }

            if (host.empty() || port == 0 || identity.Certificate() == nullptr)
            {
                error = E_INVALIDARG;
                return nullptr;
            }

            ADDRINFOW hints{};
            hints.ai_family = AF_UNSPEC;
            hints.ai_socktype = SOCK_STREAM;
            hints.ai_protocol = IPPROTO_TCP;

            PADDRINFOW found{ nullptr };
            auto const service = std::to_wstring(port);

            if (auto const resolved = ::GetAddrInfoW(host.c_str(), service.c_str(), &hints, &found); resolved != 0)
            {
                error = HRESULT_FROM_WIN32(resolved);
                return nullptr;
            }

            SOCKET connected{ INVALID_SOCKET };
            error = HRESULT_FROM_WIN32(WSAETIMEDOUT);

            for (auto address = found; address != nullptr && connected == INVALID_SOCKET; address = address->ai_next)
            {
                auto const candidate = ::socket(address->ai_family, address->ai_socktype, address->ai_protocol);

                if (candidate == INVALID_SOCKET)
                {
                    error = LastSocketError();
                    continue;
                }

                // Non-blocking for the connect alone, so it can give up in time.
                u_long nonBlocking{ 1 };
                ::ioctlsocket(candidate, FIONBIO, &nonBlocking);

                auto ok = ::connect(candidate, address->ai_addr, static_cast<int>(address->ai_addrlen)) == 0;

                if (!ok && ::WSAGetLastError() == WSAEWOULDBLOCK)
                {
                    fd_set writable{};
                    FD_ZERO(&writable);
                    FD_SET(candidate, &writable);

                    fd_set failed{};
                    FD_ZERO(&failed);
                    FD_SET(candidate, &failed);

                    timeval wait{};
                    wait.tv_sec = static_cast<long>(timeoutMilliseconds / 1000);
                    wait.tv_usec = static_cast<long>((timeoutMilliseconds % 1000) * 1000);

                    if (::select(0, nullptr, &writable, &failed, &wait) > 0 && FD_ISSET(candidate, &writable))
                    {
                        int socketError{ 0 };
                        int length{ sizeof(socketError) };

                        ok = ::getsockopt(candidate, SOL_SOCKET, SO_ERROR, reinterpret_cast<char*>(&socketError), &length) == 0 &&
                            socketError == 0;

                        if (!ok)
                        {
                            error = HRESULT_FROM_WIN32(socketError);
                        }
                    }
                    else
                    {
                        error = HRESULT_FROM_WIN32(WSAETIMEDOUT);
                    }
                }
                else if (!ok)
                {
                    error = LastSocketError();
                }

                if (!ok)
                {
                    ::closesocket(candidate);
                    continue;
                }

                u_long blocking{ 0 };
                ::ioctlsocket(candidate, FIONBIO, &blocking);

                connected = candidate;
            }

            ::FreeAddrInfoW(found);

            if (connected == INVALID_SOCKET)
            {
                return nullptr;
            }

            std::unique_ptr<DeviceLinkConnection> connection{ new DeviceLinkConnection{} };
            connection->m_state = std::make_unique<State>();
            connection->m_state->Socket = connected;
            connection->m_state->IsServer = false;
            connection->m_state->Certificate = identity.Certificate();

            NoDelay(connected);
            connection->SetTimeout(timeoutMilliseconds);

            if (error = connection->ClientHandshake(); FAILED(error))
            {
                return nullptr;
            }

            if (error = connection->FinishHandshake(); FAILED(error))
            {
                return nullptr;
            }

            return connection;
        }
        catch (...)
        {
            error = E_OUTOFMEMORY;
            return nullptr;
        }
    }

    _Use_decl_annotations_
    std::unique_ptr<DeviceLinkConnection> DeviceLinkConnection::Accept(
        uintptr_t acceptedSocket,
        DeviceIdentity const& identity,
        uint32_t timeoutMilliseconds,
        HRESULT& error) noexcept
    {
        error = E_FAIL;

        auto socket = static_cast<SOCKET>(acceptedSocket);

        try
        {
            if (socket == 0 || socket == INVALID_SOCKET || identity.Certificate() == nullptr)
            {
                if (socket != 0 && socket != INVALID_SOCKET)
                {
                    ::closesocket(socket);
                }

                error = E_INVALIDARG;
                return nullptr;
            }

            std::unique_ptr<DeviceLinkConnection> connection{ new DeviceLinkConnection{} };
            connection->m_state = std::make_unique<State>();
            connection->m_state->Socket = socket;
            socket = INVALID_SOCKET;

            connection->m_state->IsServer = true;
            connection->m_state->Certificate = identity.Certificate();

            NoDelay(connection->m_state->Socket);
            connection->SetTimeout(timeoutMilliseconds);

            if (error = connection->ServerHandshake(); FAILED(error))
            {
                return nullptr;
            }

            if (error = connection->FinishHandshake(); FAILED(error))
            {
                return nullptr;
            }

            return connection;
        }
        catch (...)
        {
            if (socket != 0 && socket != INVALID_SOCKET)
            {
                ::closesocket(socket);
            }

            error = E_OUTOFMEMORY;
            return nullptr;
        }
    }

    _Use_decl_annotations_
    void DeviceLinkConnection::SetTimeout(uint32_t milliseconds) noexcept
    {
        if (m_state == nullptr)
        {
            return;
        }

        m_state->TimeoutMilliseconds = milliseconds;
        ApplyTimeout(m_state->CurrentSocket(), milliseconds);
    }

    HRESULT DeviceLinkConnection::ClientHandshake() noexcept
    {
        auto& state = *m_state;

        PCCERT_CONTEXT certificates[1]{ state.Certificate };

        TLS_PARAMETERS tls{};
        tls.grbitDisabledProtocols = DisabledProtocols;

        SCH_CREDENTIALS credentials{};
        credentials.dwVersion = SCH_CREDENTIALS_VERSION;
        credentials.cCreds = 1;
        credentials.paCred = certificates;
        credentials.cTlsParameters = 1;
        credentials.pTlsParameters = &tls;

        // The other side's certificate is self-signed on purpose: the caller compares its key
        // with the one remembered from pairing, which is a stronger check than any chain.
        credentials.dwFlags = SCH_CRED_MANUAL_CRED_VALIDATION | SCH_CRED_NO_DEFAULT_CREDS | SCH_USE_STRONG_CRYPTO;

        auto status = ::AcquireCredentialsHandleW(nullptr, const_cast<LPWSTR>(UNISP_NAME_W), SECPKG_CRED_OUTBOUND,
            nullptr, &credentials, nullptr, nullptr, &state.Credentials, nullptr);

        if (status != SEC_E_OK)
        {
            return status;
        }

        state.HasCredentials = true;

        auto firstCall = true;
        auto needData = false;

        for (int32_t round = 0; round < MaximumHandshakeRounds; ++round)
        {
            if (state.Aborted)
            {
                return E_ABORT;
            }

            if (needData)
            {
                if (auto const received = ReceiveMore(); FAILED(received))
                {
                    return received;
                }
            }

            SecBuffer input[2]{};
            input[0].BufferType = SECBUFFER_TOKEN;
            input[0].cbBuffer = static_cast<unsigned long>(state.Incoming.size());
            input[0].pvBuffer = state.Incoming.empty() ? nullptr : state.Incoming.data();
            input[1].BufferType = SECBUFFER_EMPTY;

            SecBufferDesc inputDescription{ SECBUFFER_VERSION, 2, input };

            SecBuffer output[2]{};
            output[0].BufferType = SECBUFFER_TOKEN;
            output[1].BufferType = SECBUFFER_ALERT;

            SecBufferDesc outputDescription{ SECBUFFER_VERSION, 2, output };

            ULONG attributes{ 0 };

            status = ::InitializeSecurityContextW(
                &state.Credentials,
                firstCall ? nullptr : &state.Context,
                nullptr,
                ClientRequest,
                0,
                0,
                firstCall ? nullptr : &inputDescription,
                0,
                &state.Context,
                &outputDescription,
                &attributes,
                nullptr);

            if (status == SEC_E_INCOMPLETE_MESSAGE)
            {
                FreeOutput(output, 2);
                needData = true;
                continue;
            }

            auto sent = true;

            if (output[0].cbBuffer > 0 && output[0].pvBuffer != nullptr)
            {
                sent = SendAll(static_cast<uint8_t const*>(output[0].pvBuffer), output[0].cbBuffer);
            }

            FreeOutput(output, 2);

            if (FAILED(status))
            {
                return status;
            }

            if (firstCall)
            {
                state.HasContext = true;
                firstCall = false;
            }
            else if (input[1].BufferType == SECBUFFER_EXTRA && input[1].cbBuffer > 0 && input[1].cbBuffer <= state.Incoming.size())
            {
                state.Incoming.erase(state.Incoming.begin(), state.Incoming.end() - input[1].cbBuffer);
            }
            else
            {
                state.Incoming.clear();
            }

            if (!sent)
            {
                return ConnectionClosed;
            }

            if (status == SEC_E_OK)
            {
                return S_OK;
            }

            if (status != SEC_I_CONTINUE_NEEDED)
            {
                // SEC_I_INCOMPLETE_CREDENTIALS among others: the certificate is always supplied.
                return SEC_E_NO_CREDENTIALS;
            }

            needData = state.Incoming.empty();
        }

        return SEC_E_INTERNAL_ERROR;
    }

    HRESULT DeviceLinkConnection::ServerHandshake() noexcept
    {
        auto& state = *m_state;

        PCCERT_CONTEXT certificates[1]{ state.Certificate };

        TLS_PARAMETERS tls{};
        tls.grbitDisabledProtocols = DisabledProtocols;

        SCH_CREDENTIALS credentials{};
        credentials.dwVersion = SCH_CREDENTIALS_VERSION;
        credentials.cCreds = 1;
        credentials.paCred = certificates;
        credentials.cTlsParameters = 1;
        credentials.pTlsParameters = &tls;

        // No mapping of the client's certificate to a Windows account, and no resumed sessions:
        // every connection shows its certificate again, so its key is checked every time.
        credentials.dwFlags = SCH_CRED_NO_SYSTEM_MAPPER | SCH_CRED_DISABLE_RECONNECTS | SCH_USE_STRONG_CRYPTO;

        auto status = ::AcquireCredentialsHandleW(nullptr, const_cast<LPWSTR>(UNISP_NAME_W), SECPKG_CRED_INBOUND,
            nullptr, &credentials, nullptr, nullptr, &state.Credentials, nullptr);

        if (status != SEC_E_OK)
        {
            return status;
        }

        state.HasCredentials = true;

        auto firstCall = true;
        auto needData = true;

        for (int32_t round = 0; round < MaximumHandshakeRounds; ++round)
        {
            if (state.Aborted)
            {
                return E_ABORT;
            }

            if (needData)
            {
                if (auto const received = ReceiveMore(); FAILED(received))
                {
                    return received;
                }
            }

            SecBuffer input[2]{};
            input[0].BufferType = SECBUFFER_TOKEN;
            input[0].cbBuffer = static_cast<unsigned long>(state.Incoming.size());
            input[0].pvBuffer = state.Incoming.empty() ? nullptr : state.Incoming.data();
            input[1].BufferType = SECBUFFER_EMPTY;

            SecBufferDesc inputDescription{ SECBUFFER_VERSION, 2, input };

            SecBuffer output[2]{};
            output[0].BufferType = SECBUFFER_TOKEN;
            output[1].BufferType = SECBUFFER_ALERT;

            SecBufferDesc outputDescription{ SECBUFFER_VERSION, 2, output };

            ULONG attributes{ 0 };

            status = ::AcceptSecurityContext(
                &state.Credentials,
                firstCall ? nullptr : &state.Context,
                &inputDescription,
                ServerRequest,
                0,
                &state.Context,
                &outputDescription,
                &attributes,
                nullptr);

            if (status == SEC_E_INCOMPLETE_MESSAGE)
            {
                FreeOutput(output, 2);
                needData = true;
                continue;
            }

            auto sent = true;

            if (output[0].cbBuffer > 0 && output[0].pvBuffer != nullptr)
            {
                sent = SendAll(static_cast<uint8_t const*>(output[0].pvBuffer), output[0].cbBuffer);
            }

            FreeOutput(output, 2);

            if (FAILED(status))
            {
                return status;
            }

            if (firstCall)
            {
                state.HasContext = true;
                firstCall = false;
            }

            if (input[1].BufferType == SECBUFFER_EXTRA && input[1].cbBuffer > 0 && input[1].cbBuffer <= state.Incoming.size())
            {
                state.Incoming.erase(state.Incoming.begin(), state.Incoming.end() - input[1].cbBuffer);
            }
            else
            {
                state.Incoming.clear();
            }

            if (!sent)
            {
                return ConnectionClosed;
            }

            if (status == SEC_E_OK)
            {
                return S_OK;
            }

            if (status != SEC_I_CONTINUE_NEEDED)
            {
                return SEC_E_INTERNAL_ERROR;
            }

            needData = state.Incoming.empty();
        }

        return SEC_E_INTERNAL_ERROR;
    }

    HRESULT DeviceLinkConnection::FinishHandshake() noexcept
    {
        auto& state = *m_state;

        if (auto const sizes = ::QueryContextAttributesW(&state.Context, SECPKG_ATTR_STREAM_SIZES, &state.Sizes); sizes != SEC_E_OK)
        {
            return sizes;
        }

        if (state.Sizes.cbMaximumMessage == 0)
        {
            return SEC_E_INTERNAL_ERROR;
        }

        // Without a certificate from the other side there is nothing to know it by.
        PCCERT_CONTEXT remote{ nullptr };

        if (auto const queried = ::QueryContextAttributesW(&state.Context, SECPKG_ATTR_REMOTE_CERT_CONTEXT, &remote);
            queried != SEC_E_OK || remote == nullptr)
        {
            return SEC_E_NO_CREDENTIALS;
        }

        auto const key = KeyHashOfCertificate(remote);
        ::CertFreeCertificateContext(remote);

        if (!key)
        {
            return SEC_E_CERT_UNKNOWN;
        }

        m_peerKey = *key;

        return S_OK;
    }

    _Use_decl_annotations_
    bool DeviceLinkConnection::SendAll(uint8_t const* data, size_t size) noexcept
    {
        auto const socket = m_state->CurrentSocket();

        while (size > 0)
        {
            if (m_state->Aborted || socket == INVALID_SOCKET)
            {
                return false;
            }

            auto const chunk = static_cast<int>(std::min<size_t>(size, 64 * 1024));
            auto const sent = ::send(socket, reinterpret_cast<char const*>(data), chunk, 0);

            if (sent <= 0)
            {
                return false;
            }

            data += sent;
            size -= static_cast<size_t>(sent);
        }

        return true;
    }

    HRESULT DeviceLinkConnection::ReceiveMore() noexcept
    {
        auto& state = *m_state;

        if (state.Aborted)
        {
            return E_ABORT;
        }

        if (state.Incoming.size() >= MaximumPendingBytes)
        {
            return SEC_E_ILLEGAL_MESSAGE;
        }

        try
        {
            auto const socket = state.CurrentSocket();

            if (socket == INVALID_SOCKET)
            {
                return E_ABORT;
            }

            auto const before = state.Incoming.size();
            auto const room = std::min(ReceiveChunkBytes, MaximumPendingBytes - before);

            state.Incoming.resize(before + room);

            auto const received = ::recv(socket, reinterpret_cast<char*>(state.Incoming.data() + before), static_cast<int>(room), 0);

            if (received <= 0)
            {
                state.Incoming.resize(before);

                if (received == 0)
                {
                    state.PeerClosed = true;
                    return ConnectionClosed;
                }

                return LastSocketError();
            }

            state.Incoming.resize(before + static_cast<size_t>(received));

            return S_OK;
        }
        catch (...)
        {
            return E_OUTOFMEMORY;
        }
    }

    HRESULT DeviceLinkConnection::DecryptAvailable() noexcept
    {
        auto& state = *m_state;

        try
        {
            if (state.Incoming.empty())
            {
                return SEC_E_INCOMPLETE_MESSAGE;
            }

            SecBuffer buffers[4]{};
            buffers[0].BufferType = SECBUFFER_DATA;
            buffers[0].cbBuffer = static_cast<unsigned long>(state.Incoming.size());
            buffers[0].pvBuffer = state.Incoming.data();
            buffers[1].BufferType = SECBUFFER_EMPTY;
            buffers[2].BufferType = SECBUFFER_EMPTY;
            buffers[3].BufferType = SECBUFFER_EMPTY;

            SecBufferDesc description{ SECBUFFER_VERSION, 4, buffers };

            auto const status = ::DecryptMessage(&state.Context, &description, 0, nullptr);

            if (status == SEC_E_INCOMPLETE_MESSAGE)
            {
                return status;
            }

            if (status == SEC_I_CONTEXT_EXPIRED)
            {
                state.PeerClosed = true;
                return status;
            }

            if (status != SEC_E_OK && status != SEC_I_RENEGOTIATE)
            {
                return status;
            }

            // Decrypted in place, inside Incoming, so both are copied out before it changes.
            std::vector<uint8_t> extra{};

            for (auto const& buffer : buffers)
            {
                if (buffer.BufferType == SECBUFFER_DATA && buffer.cbBuffer > 0 && buffer.pvBuffer != nullptr)
                {
                    auto const begin = static_cast<uint8_t const*>(buffer.pvBuffer);
                    state.Plain.insert(state.Plain.end(), begin, begin + buffer.cbBuffer);
                }
                else if (buffer.BufferType == SECBUFFER_EXTRA && buffer.cbBuffer > 0 && buffer.pvBuffer != nullptr)
                {
                    auto const begin = static_cast<uint8_t const*>(buffer.pvBuffer);
                    extra.assign(begin, begin + buffer.cbBuffer);
                }
            }

            state.Incoming = std::move(extra);

            if (status == SEC_I_RENEGOTIATE)
            {
                // A TLS 1.3 message after the handshake, such as a key update. SChannel takes it
                // back through the handshake call, with whatever came after it.
                if (state.IsServer)
                {
                    return SEC_E_ILLEGAL_MESSAGE;
                }

                for (int32_t round = 0; round < MaximumHandshakeRounds; ++round)
                {
                    SecBuffer input[2]{};
                    input[0].BufferType = SECBUFFER_TOKEN;
                    input[0].cbBuffer = static_cast<unsigned long>(state.Incoming.size());
                    input[0].pvBuffer = state.Incoming.empty() ? nullptr : state.Incoming.data();
                    input[1].BufferType = SECBUFFER_EMPTY;

                    SecBufferDesc inputDescription{ SECBUFFER_VERSION, 2, input };

                    SecBuffer output[2]{};
                    output[0].BufferType = SECBUFFER_TOKEN;
                    output[1].BufferType = SECBUFFER_ALERT;

                    SecBufferDesc outputDescription{ SECBUFFER_VERSION, 2, output };

                    ULONG attributes{ 0 };

                    auto const step = ::InitializeSecurityContextW(&state.Credentials, &state.Context, nullptr,
                        ClientRequest, 0, 0, &inputDescription, 0, &state.Context, &outputDescription, &attributes, nullptr);

                    if (step == SEC_E_INCOMPLETE_MESSAGE)
                    {
                        FreeOutput(output, 2);

                        if (auto const received = ReceiveMore(); FAILED(received))
                        {
                            return received;
                        }

                        continue;
                    }

                    auto sent = true;

                    if (output[0].cbBuffer > 0 && output[0].pvBuffer != nullptr)
                    {
                        sent = SendAll(static_cast<uint8_t const*>(output[0].pvBuffer), output[0].cbBuffer);
                    }

                    FreeOutput(output, 2);

                    if (FAILED(step))
                    {
                        return step;
                    }

                    if (input[1].BufferType == SECBUFFER_EXTRA && input[1].cbBuffer > 0 && input[1].cbBuffer <= state.Incoming.size())
                    {
                        state.Incoming.erase(state.Incoming.begin(), state.Incoming.end() - input[1].cbBuffer);
                    }
                    else
                    {
                        state.Incoming.clear();
                    }

                    if (!sent)
                    {
                        return ConnectionClosed;
                    }

                    if (step == SEC_E_OK)
                    {
                        return SEC_E_OK;
                    }

                    if (step != SEC_I_CONTINUE_NEEDED)
                    {
                        return SEC_E_INTERNAL_ERROR;
                    }

                    if (state.Incoming.empty())
                    {
                        if (auto const received = ReceiveMore(); FAILED(received))
                        {
                            return received;
                        }
                    }
                }

                return SEC_E_INTERNAL_ERROR;
            }

            return SEC_E_OK;
        }
        catch (...)
        {
            return E_OUTOFMEMORY;
        }
    }

    _Use_decl_annotations_
    bool DeviceLinkConnection::SendPlain(uint8_t const* data, size_t size) noexcept
    {
        try
        {
            auto& state = *m_state;

            if (state.Closed || state.Aborted || !state.HasContext)
            {
                return false;
            }

            std::vector<uint8_t> record{};

            while (size > 0)
            {
                auto const chunk = std::min<size_t>(size, state.Sizes.cbMaximumMessage);

                record.resize(state.Sizes.cbHeader + chunk + state.Sizes.cbTrailer);
                std::memcpy(record.data() + state.Sizes.cbHeader, data, chunk);

                SecBuffer buffers[4]{};
                buffers[0].BufferType = SECBUFFER_STREAM_HEADER;
                buffers[0].cbBuffer = state.Sizes.cbHeader;
                buffers[0].pvBuffer = record.data();
                buffers[1].BufferType = SECBUFFER_DATA;
                buffers[1].cbBuffer = static_cast<unsigned long>(chunk);
                buffers[1].pvBuffer = record.data() + state.Sizes.cbHeader;
                buffers[2].BufferType = SECBUFFER_STREAM_TRAILER;
                buffers[2].cbBuffer = state.Sizes.cbTrailer;
                buffers[2].pvBuffer = record.data() + state.Sizes.cbHeader + chunk;
                buffers[3].BufferType = SECBUFFER_EMPTY;

                SecBufferDesc description{ SECBUFFER_VERSION, 4, buffers };

                if (::EncryptMessage(&state.Context, 0, &description, 0) != SEC_E_OK)
                {
                    return false;
                }

                auto const total = static_cast<size_t>(buffers[0].cbBuffer) + buffers[1].cbBuffer + buffers[2].cbBuffer;

                if (!SendAll(record.data(), total))
                {
                    return false;
                }

                data += chunk;
                size -= chunk;
            }

            return true;
        }
        catch (...)
        {
            return false;
        }
    }

    _Use_decl_annotations_
    bool DeviceLinkConnection::ReadPlain(uint8_t* destination, size_t size, bool& closedCleanly) noexcept
    {
        closedCleanly = false;

        auto& state = *m_state;
        size_t copied{ 0 };

        while (copied < size)
        {
            if (state.Aborted || state.Closed)
            {
                return false;
            }

            if (state.PlainOffset < state.Plain.size())
            {
                auto const take = std::min(size - copied, state.Plain.size() - state.PlainOffset);
                std::memcpy(destination + copied, state.Plain.data() + state.PlainOffset, take);

                copied += take;
                state.PlainOffset += take;

                continue;
            }

            state.Plain.clear();
            state.PlainOffset = 0;

            auto const status = DecryptAvailable();

            if (status == SEC_E_OK)
            {
                continue;
            }

            if (status == SEC_I_CONTEXT_EXPIRED)
            {
                closedCleanly = copied == 0;
                return false;
            }

            if (status != SEC_E_INCOMPLETE_MESSAGE)
            {
                return false;
            }

            if (auto const received = ReceiveMore(); FAILED(received))
            {
                closedCleanly = copied == 0 && state.PeerClosed && state.Incoming.empty();
                return false;
            }
        }

        return true;
    }

    _Use_decl_annotations_
    bool DeviceLinkConnection::WriteFrame(uint16_t type, std::span<uint8_t const> payload) noexcept
    {
        if (m_state == nullptr)
        {
            return false;
        }

        auto const frame = EncodeDeviceLinkFrame(type, payload);

        if (frame.empty())
        {
            return false;
        }

        return SendPlain(frame.data(), frame.size());
    }

    _Use_decl_annotations_
    DeviceLinkFrameStatus DeviceLinkConnection::ReadFrame(DeviceLinkFrameCap const& capFor, DeviceLinkFrame& frame) noexcept
    {
        frame = DeviceLinkFrame{};

        if (m_state == nullptr)
        {
            return DeviceLinkFrameStatus::Truncated;
        }

        return ReadDeviceLinkFrame(
            [this](uint8_t* destination, size_t size, bool& closedCleanly)
            {
                return ReadPlain(destination, size, closedCleanly);
            },
            capFor,
            frame);
    }

    void DeviceLinkConnection::Close() noexcept
    {
        if (m_state == nullptr)
        {
            return;
        }

        auto& state = *m_state;

        if (state.Closed)
        {
            return;
        }

        state.Closed = true;

        // Best effort: a close_notify, so the other side can tell an ending from a cut.
        if (state.HasContext && !state.Aborted && !state.PeerClosed)
        {
            DWORD shutdownToken{ SCHANNEL_SHUTDOWN };

            SecBuffer control{};
            control.BufferType = SECBUFFER_TOKEN;
            control.cbBuffer = sizeof(shutdownToken);
            control.pvBuffer = &shutdownToken;

            SecBufferDesc controlDescription{ SECBUFFER_VERSION, 1, &control };

            if (::ApplyControlToken(&state.Context, &controlDescription) == SEC_E_OK)
            {
                SecBuffer output[1]{};
                output[0].BufferType = SECBUFFER_TOKEN;

                SecBufferDesc outputDescription{ SECBUFFER_VERSION, 1, output };
                ULONG attributes{ 0 };

                auto const status = state.IsServer
                    ? ::AcceptSecurityContext(&state.Credentials, &state.Context, nullptr, ServerRequest, 0,
                        nullptr, &outputDescription, &attributes, nullptr)
                    : ::InitializeSecurityContextW(&state.Credentials, &state.Context, nullptr, ClientRequest, 0, 0,
                        nullptr, 0, nullptr, &outputDescription, &attributes, nullptr);

                if (SUCCEEDED(status) && output[0].cbBuffer > 0 && output[0].pvBuffer != nullptr)
                {
                    SendAll(static_cast<uint8_t const*>(output[0].pvBuffer), output[0].cbBuffer);
                }

                FreeOutput(output, 1);
            }
        }

        {
            std::scoped_lock guard{ state.SocketLock };

            if (state.Socket != INVALID_SOCKET)
            {
                ::shutdown(state.Socket, SD_BOTH);
                ::closesocket(state.Socket);
                state.Socket = INVALID_SOCKET;
            }
        }

        if (state.HasContext)
        {
            ::DeleteSecurityContext(&state.Context);
            state.HasContext = false;
        }

        if (state.HasCredentials)
        {
            ::FreeCredentialsHandle(&state.Credentials);
            state.HasCredentials = false;
        }

        // Nothing decrypted stays in memory longer than it has to.
        ::SecureZeroMemory(state.Plain.data(), state.Plain.size());
        state.Plain.clear();
        state.Incoming.clear();
    }

    void DeviceLinkConnection::Abort() noexcept
    {
        if (m_state == nullptr)
        {
            return;
        }

        m_state->Aborted = true;

        try
        {
            std::scoped_lock guard{ m_state->SocketLock };

            if (m_state->Socket != INVALID_SOCKET)
            {
                // Ends a waiting send or recv. The socket itself is closed by the thread using it.
                ::shutdown(m_state->Socket, SD_BOTH);
            }
        }
        catch (...)
        {
        }
    }

    // ------------------------------------------------------------------------ listener

    DeviceLinkListener::~DeviceLinkListener()
    {
        Stop();
    }

    _Use_decl_annotations_
    HRESULT DeviceLinkListener::Start(uint16_t port, bool loopbackOnly) noexcept
    {
        Stop();

        if (!EnsureWinsock())
        {
            return LastSocketError();
        }

        auto const socket = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

        if (socket == INVALID_SOCKET)
        {
            return LastSocketError();
        }

        // Nobody else can take the same port while this has it.
        BOOL exclusive{ TRUE };
        ::setsockopt(socket, SOL_SOCKET, SO_EXCLUSIVEADDRUSE, reinterpret_cast<char const*>(&exclusive), sizeof(exclusive));

        sockaddr_in address{};
        address.sin_family = AF_INET;
        address.sin_port = ::htons(port);
        address.sin_addr.s_addr = ::htonl(loopbackOnly ? INADDR_LOOPBACK : INADDR_ANY);

        if (::bind(socket, reinterpret_cast<sockaddr const*>(&address), sizeof(address)) != 0 ||
            ::listen(socket, 4) != 0)
        {
            auto const error = LastSocketError();
            ::closesocket(socket);
            return error;
        }

        sockaddr_in bound{};
        int length{ sizeof(bound) };

        if (::getsockname(socket, reinterpret_cast<sockaddr*>(&bound), &length) != 0)
        {
            auto const error = LastSocketError();
            ::closesocket(socket);
            return error;
        }

        m_port = ::ntohs(bound.sin_port);
        m_socket = static_cast<uintptr_t>(socket);

        return S_OK;
    }

    _Use_decl_annotations_
    uintptr_t DeviceLinkListener::Accept(uint32_t timeoutMilliseconds) noexcept
    {
        auto const socket = static_cast<SOCKET>(m_socket.load());

        if (socket == 0 || socket == INVALID_SOCKET)
        {
            return 0;
        }

        fd_set readable{};
        FD_ZERO(&readable);
        FD_SET(socket, &readable);

        timeval wait{};
        wait.tv_sec = static_cast<long>(timeoutMilliseconds / 1000);
        wait.tv_usec = static_cast<long>((timeoutMilliseconds % 1000) * 1000);

        if (::select(0, &readable, nullptr, nullptr, &wait) <= 0 || !FD_ISSET(socket, &readable))
        {
            return 0;
        }

        auto const accepted = ::accept(socket, nullptr, nullptr);

        if (accepted == INVALID_SOCKET)
        {
            return 0;
        }

        return static_cast<uintptr_t>(accepted);
    }

    void DeviceLinkListener::Stop() noexcept
    {
        auto const socket = static_cast<SOCKET>(m_socket.exchange(0));

        if (socket != 0 && socket != INVALID_SOCKET)
        {
            ::closesocket(socket);
        }

        m_port = 0;
    }
}
