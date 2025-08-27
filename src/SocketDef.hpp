#pragma once

#ifdef __linux__
#elif _WIN32
#include <winsock2.h>
#include <windows.h>
#endif


#define BUF_SIZE 2048


// windows compatibility
#ifndef _SSIZE_T_DEFINED
#ifdef  _WIN64
typedef unsigned __int64    ssize_t;
#endif
#define _SSIZE_T_DEFINED
#endif

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


inline int readAllDataFromSocket(int sockfd, std::string& output)
{
    // return codes:
    //  1  - data read successfully
    //  0  - error while waiting or receiving
    // -1  - connection closed by peer

    char buffer[BUF_SIZE];
    ssize_t bytes_received;

    output.clear(); // Ensure the output string is empty

    while (true)
    {
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(sockfd, &readfds);

        struct timeval timeout;
        timeout.tv_sec = 0;
        timeout.tv_usec = 10000; // 10ms

        // Check for socket readiness
        int activity = select(sockfd + 1, &readfds, NULL, NULL, &timeout);

        if (activity < 0)
        {
            // select error
            return 0;
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
            {
                return 0;
            }
            else if (bytes_received == 0)
            {
                // Connection closed by the peer
                return -1;
            }

            // Append received data to the output string
            output.append(buffer, bytes_received);
        }
    }

    return 1;
}