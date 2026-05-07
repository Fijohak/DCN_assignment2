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
    inet_pton(AF_INET, ip.c_str(), &addr.sin_addr);

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

    // Read server welcome message
    char welcomeBuf[256] = {0};
    int len = recv(m_sock, welcomeBuf, sizeof(welcomeBuf) - 1, 0);
    if (len <= 0) {
        std::cerr << "No welcome message from server" << std::endl;
        closesocket(m_sock);
        m_sock = INVALID_SOCKET;
        return false;
    }
    m_welcome = std::string(welcomeBuf, len);

    // Negotiate encryption
    std::string encReq = std::string("ENCRYPT 1") + MSG_TERMINATOR;
    if (send(m_sock, encReq.c_str(), static_cast<int>(encReq.length()), 0) == SOCKET_ERROR) {
        std::cerr << "Encryption request failed: " << WSAGetLastError() << std::endl;
        closesocket(m_sock);
        m_sock = INVALID_SOCKET;
        return false;
    }

    char encBuf[64] = {0};
    int encLen = recv(m_sock, encBuf, sizeof(encBuf) - 1, 0);
    if (encLen > 0) {
        std::string response(encBuf, encLen);
        if (response.find("ENCRYPT_OK") == 0) {
            m_encrypted = true;
        }
    }

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

    if (!isConnected()) {
        m_connected = false;
        return std::string(ERROR_PREFIX) + "Connection lost" + MSG_TERMINATOR;
    }

    // Build wire format
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

        if (buffer.size() > 65536) {
            m_connected = false;
            return std::string(ERROR_PREFIX) + "Response too large" + MSG_TERMINATOR;
        }

        if (m_encrypted) {
            // Encrypted responses: single hex line terminated by '\n'
            size_t nl = buffer.find('\n');
            if (nl != std::string::npos) {
                std::string decrypted = decryptMessage(buffer.substr(0, nl));
                if (decrypted.empty()) {
                    return std::string(ERROR_PREFIX) + "Decryption failed" + MSG_TERMINATOR;
                }
                return decrypted;
            }
        } else {
            // Plaintext query response: RESULT\n...\nEND\n
            if (buffer.find(RESULT_START) == 0) {
                size_t end = buffer.find(RESULT_END);
                if (end != std::string::npos) {
                    return buffer;
                }
            } else {
                // Plaintext non-query: single line terminated by '\n'
                size_t nl = buffer.find('\n');
                if (nl != std::string::npos) {
                    return buffer.substr(0, nl + 1);
                }
            }
        }
    }
}
