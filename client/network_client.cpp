// NetworkClient - TCP client with encryption support.
// Handles connection, send/receive, and response parsing.

#include "../include/network_client.h"
#include <iostream>
#include <ws2tcpip.h>

NetworkClient::NetworkClient()
    : m_sock(INVALID_SOCKET), m_connected(false), m_encrypted(false) {}

NetworkClient::~NetworkClient() {
    disconnect();
}

bool NetworkClient::connect(const std::string& ip, int port) {
    if (m_connected) {
        disconnect();
    }

    sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = inet_addr(ip.c_str());

    m_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (m_sock == INVALID_SOCKET) {
        std::cerr << "Socket creation failed: " << WSAGetLastError() << std::endl;
        return false;
    }

    if (::connect(m_sock, (sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) {
        std::cerr << "Connection failed: " << WSAGetLastError() << std::endl;
        closesocket(m_sock);
        m_sock = INVALID_SOCKET;
        return false;
    }

    // Read the server's welcome message
    char welcomeBuf[256] = {0};
    int len = recv(m_sock, welcomeBuf, sizeof(welcomeBuf) - 1, 0);
    if (len <= 0) {
        std::cerr << "No welcome message from server" << std::endl;
        closesocket(m_sock);
        m_sock = INVALID_SOCKET;
        return false;
    }
    m_welcome = std::string(welcomeBuf, len);

    m_connected = true;
    return true;
}

void NetworkClient::disconnect() {
    if (m_sock != INVALID_SOCKET) {
        closesocket(m_sock);
        m_sock = INVALID_SOCKET;
    }
    m_connected = false;
    m_encrypted = false;
    m_welcome.clear();
}

bool NetworkClient::isConnected() const {
    return m_connected && m_sock != INVALID_SOCKET;
}

std::string NetworkClient::sendRequest(const std::string& request) {
    // Empty request returns the stored welcome message
    if (request.empty()) {
        return m_welcome;
    }

    if (!m_connected || m_sock == INVALID_SOCKET) {
        return std::string(ERROR_PREFIX) + "Not connected" + MSG_TERMINATOR;
    }

    // Build wire format: append newline terminator
    std::string data;
    if (m_encrypted) {
        std::string clean = request;
        if (!clean.empty() && clean.back() == '\n') {
            clean.pop_back();
        }
        data = encryptMessage(clean) + MSG_TERMINATOR;
    } else {
        data = request + MSG_TERMINATOR;
    }

    if (send(m_sock, data.c_str(), static_cast<int>(data.length()), 0) == SOCKET_ERROR) {
        m_connected = false;
        return std::string(ERROR_PREFIX) + "Send failed" + MSG_TERMINATOR;
    }

    return receiveResponse();
}

std::string NetworkClient::receiveResponse() {
    std::string buffer;
    char chunk[4096];

    while (true) {
        int bytes = recv(m_sock, chunk, sizeof(chunk) - 1, 0);
        if (bytes <= 0) {
            m_connected = false;
            if (bytes == 0) {
                return buffer.empty()
                    ? std::string(ERROR_PREFIX) + "Connection closed" + MSG_TERMINATOR
                    : buffer;
            }
            return buffer.empty()
                ? std::string(ERROR_PREFIX) + "Receive failed" + MSG_TERMINATOR
                : buffer;
        }

        chunk[bytes] = '\0';
        buffer.append(chunk, bytes);

        // Safety limit: reject excessively large responses
        if (buffer.size() > 65536) {
            m_connected = false;
            return std::string(ERROR_PREFIX) + "Response too large" + MSG_TERMINATOR;
        }

        if (m_encrypted) {
            // Encrypted response: single hex-encoded line terminated by '\n'
            size_t nl = buffer.find('\n');
            if (nl != std::string::npos) {
                std::string decrypted = decryptMessage(buffer.substr(0, nl));
                if (decrypted.empty()) {
                    return std::string(ERROR_PREFIX) + "Decryption failed" + MSG_TERMINATOR;
                }
                return decrypted;
            }
        } else {
            // Multi-line query response: starts with "RESULT", ends with "\nEND"
            if (buffer.find(RESULT_START) == 0) {
                size_t end = buffer.find(RESULT_END);
                if (end != std::string::npos) {
                    return buffer;
                }
            } else {
                // Single-line response: terminated by '\n'
                size_t nl = buffer.find('\n');
                if (nl != std::string::npos) {
                    return buffer.substr(0, nl + 1);
                }
            }
        }
    }
}
