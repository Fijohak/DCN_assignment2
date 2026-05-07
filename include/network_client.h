#ifndef NETWORK_CLIENT_H
#define NETWORK_CLIENT_H

#include <string>
#include <winsock2.h>
#include "protocol.h"

/*
 * NetworkClient - TCP client with optional XOR encryption
 *
 * Usage: caller must call WSAStartup before use and WSACleanup after.
 */

class NetworkClient {
private:
    SOCKET m_sock;
    bool m_connected;
    bool m_encrypted;
    std::string m_welcome;

    std::string receiveResponse();

public:
    NetworkClient();
    ~NetworkClient();

    bool connect(const std::string& ip, int port);
    void disconnect();
    bool isConnected() const;
    std::string sendRequest(const std::string& request);
};

#endif // NETWORK_CLIENT_H
