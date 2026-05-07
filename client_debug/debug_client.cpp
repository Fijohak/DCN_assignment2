#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/socket.h>
#include <unistd.h>
typedef int SOCKET;
#define INVALID_SOCKET (-1)
#define SOCKET_ERROR (-1)
#endif

#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>
#include <sstream>
#include <iomanip>

namespace {

bool connectToServer(SOCKET& sock, const std::string& host, const std::string& port) {
    addrinfo hints;
    std::memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    addrinfo* result = NULL;
    const int lookupResult = getaddrinfo(host.c_str(), port.c_str(), &hints, &result);
    if (lookupResult != 0) {
        std::cerr << "getaddrinfo failed" << std::endl;
        return false;
    }

    for (addrinfo* ptr = result; ptr != NULL; ptr = ptr->ai_next) {
        sock = socket(ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol);
        if (sock == INVALID_SOCKET) {
            continue;
        }

        if (connect(sock, ptr->ai_addr, static_cast<int>(ptr->ai_addrlen)) != SOCKET_ERROR) {
            freeaddrinfo(result);
            return true;
        }

#ifdef _WIN32
        closesocket(sock);
#else
        close(sock);
#endif
        sock = INVALID_SOCKET;
    }

    freeaddrinfo(result);
    return false;
}

bool sendAll(SOCKET sock, const std::string& text) {
    const char* data = text.c_str();
    int remaining = static_cast<int>(text.size());

    while (remaining > 0) {
        const int sent = send(sock, data, remaining, 0);
        if (sent <= 0) {
            return false;
        }

        data += sent;
        remaining -= sent;
    }

    return true;
}

bool receiveLine(SOCKET sock, std::string& line) {
    line.clear();
    char ch = '\0';

    while (true) {
        const int received = recv(sock, &ch, 1, 0);
        if (received <= 0) {
            return false;
        }

        if (ch == '\n') {
            return true;
        }

        if (ch != '\r') {
            line.push_back(ch);
        }
    }
}

bool readResponse(SOCKET sock) {
    std::string line;
    if (!receiveLine(sock, line)) {
        return false;
    }

    std::cout << line << std::endl;

    if (line.rfind("RESULT count=", 0) == 0) {
        while (receiveLine(sock, line)) {
            std::cout << line << std::endl;
            if (line == "END") {
                break;
            }
        }
    }

    return true;
}

void closeSocket(SOCKET sock) {
#ifdef _WIN32
    closesocket(sock);
#else
    close(sock);
#endif
}

std::string bytesToHex(const std::string& data) {
    std::ostringstream oss;
    for (std::size_t i = 0; i < data.size(); ++i) {
        oss << std::hex << std::setw(2) << std::setfill('0')
            << static_cast<int>(static_cast<unsigned char>(data[i]));
    }
    return oss.str();
}

std::string xorEncrypt(const std::string& data, const std::string& key) {
    if (key.empty()) return data;
    std::string result;
    result.reserve(data.size());
    for (std::size_t i = 0; i < data.size(); ++i) {
        result.push_back(static_cast<char>(data[i] ^ key[i % key.size()]));
    }
    return result;
}

}  // namespace

int main(int argc, char* argv[]) {
    std::string host = "127.0.0.1";
    std::string port = "54000";

    if (argc >= 2) {
        host = argv[1];
    }
    if (argc >= 3) {
        port = argv[2];
    }

#ifdef _WIN32
    WSADATA wsaData;
    const int wsaResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (wsaResult != 0) {
        std::cerr << "WSAStartup failed: " << wsaResult << std::endl;
        return 1;
    }
#endif

    SOCKET sock = INVALID_SOCKET;
    if (!connectToServer(sock, host, port)) {
        std::cerr << "Could not connect to " << host << ":" << port << std::endl;
#ifdef _WIN32
        WSACleanup();
#endif
        return 1;
    }

    std::cout << "Connected to " << host << ":" << port << std::endl;
    std::cout << "Type commands such as PING, LIST_ALL, QUERY_CODE COMP3003, or QUIT." << std::endl;
    std::cout << "For encryption demo: ENCRYPT <text>|<key> (e.g., ENCRYPT HelloWorld|key123)" << std::endl;

    if (!readResponse(sock)) {
        std::cerr << "Server closed the connection." << std::endl;
        closeSocket(sock);
#ifdef _WIN32
        WSACleanup();
#endif
        return 1;
    }

    bool localEncryptDemo = false;
    std::string command;
    while (true) {
        std::cout << "> ";
        if (!std::getline(std::cin, command)) {
            break;
        }

        if (command.empty()) {
            continue;
        }

        // Handle local encryption demo (client-side only, no server needed)
        if (command == "ENCRYPT" || command == "encrypt") {
            std::string text, key;
            std::cout << "Enter text to encrypt: ";
            if (!std::getline(std::cin, text)) break;
            std::cout << "Enter encryption key: ";
            if (!std::getline(std::cin, key)) break;

            std::string encrypted = xorEncrypt(text, key);
            std::string decrypted = xorEncrypt(encrypted, key);

            std::cout << "=== XOR Encryption Demo (Client-Side) ===" << std::endl;
            std::cout << "Original:  [" << text << "]" << std::endl;
            std::cout << "Key:       [" << key << "]" << std::endl;
            std::cout << "Encrypted: [" << bytesToHex(encrypted) << "] (hex)" << std::endl;
            std::cout << "Decrypted: [" << decrypted << "]" << std::endl;
            std::cout << "=========================================" << std::endl;
            continue;
        }

        if (!sendAll(sock, command + "\n")) {
            std::cerr << "Failed to send command." << std::endl;
            break;
        }

        if (!readResponse(sock)) {
            std::cerr << "Server closed the connection." << std::endl;
            break;
        }

        if (command == "QUIT" || command == "quit") {
            break;
        }
    }

    closeSocket(sock);

#ifdef _WIN32
    WSACleanup();
#endif

    return 0;
}
