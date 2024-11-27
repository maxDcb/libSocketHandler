#pragma once

#include <string>
#include <stdint.h>


class SocketTunnelClient
{
    public:
        SocketTunnelClient();
        ~SocketTunnelClient(); 

        int init(const std::string& ip_dst, int port);
        int process(const std::string& dataIn, std::string& dataOut);
        int reset();

    private:
        int m_clientfd;

        std::string m_internalBuffer;
};