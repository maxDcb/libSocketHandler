#pragma once

#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>

#ifdef __linux__
#include <sys/select.h>
#include <sys/socket.h>
#elif _WIN32
#include <winsock2.h>
#include <windows.h>
#endif


#define BUF_SIZE 2048


#ifndef SHUT_RDWR
#define SHUT_RDWR 2 
#endif


// handle sig_pipe that can crash the app otherwise
inline void sig_handler(int signum) 
{
}


inline int send_sock(int sock, const char *buffer, uint32_t size) 
{
    int index = 0, ret;
    while(size) 
    {
        if((ret = send(sock, &buffer[index], size, 0)) <= 0)
            return (!ret) ? index : -1;
        index += ret;
        size -= ret;
    }
    return index;
}


inline int readAllDataFromSocket(int sockfd, std::string& output,
                                 int waitUsec = 10000,
                                 std::size_t maxBytes = std::numeric_limits<std::size_t>::max())
{
    // return codes:
    //  1  - data read successfully
    //  0  - error while waiting or receiving
    // -1  - connection closed by peer

    char buffer[BUF_SIZE];
    // recv returns int on both supported platforms. Keep SOCKET_ERROR signed.
    int bytes_received;

    output.clear(); // Ensure the output string is empty

    while (true)
    {
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(sockfd, &readfds);

        struct timeval timeout;
        timeout.tv_sec = 0;
        timeout.tv_usec = waitUsec;

        // Check for socket readiness
        int activity = select(sockfd + 1, &readfds, NULL, NULL, &timeout);

        if (activity < 0)
        {
            // select error
            return output.empty() ? 0 : 1;
        }
        else if (activity == 0)
        {
            // Timeout, no more data
            break;
        }
        else if (FD_ISSET(sockfd, &readfds))
        {
            // Data is available to read
            bytes_received = recv(sockfd, buffer, BUF_SIZE, 0);
            if (bytes_received < 0)
                return output.empty() ? 0 : 1;
            else if (bytes_received == 0)
            {
                // Connection closed by the peer
                return output.empty() ? -1 : 1;
            }

            // Append received data to the output string
            output.append(buffer, bytes_received);
            if (output.size() >= maxBytes)
                return 1;
        }
    }

    return 1;
}
