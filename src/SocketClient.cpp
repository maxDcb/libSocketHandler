#include <cstring>


#include <signal.h>

#ifdef __linux__
#include <unistd.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#elif _WIN32
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#endif

#include <iostream>
#include <string>
#include <sstream>
#include <thread>

#include "SocketClient.hpp"
#include "SocketDef.hpp"


using namespace std;


int connect_to_host(const std::string& ip_dst, int port) 
{
    
    struct sockaddr_in serv_addr;
    struct hostent *server;
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0)
    {
        return -1;
    }

    memset((char *) &serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET; 

#ifdef __linux__   
    
    serv_addr.sin_addr.s_addr = inet_addr(ip_dst.c_str());
    serv_addr.sin_port = htons(port);

#elif _WIN32

    inet_pton(AF_INET, ip_dst.c_str(), &serv_addr.sin_addr.s_addr);
    serv_addr.sin_port = htons(port);

#endif

    return !connect(sockfd, (const sockaddr*)&serv_addr, sizeof(serv_addr)) ? sockfd : -1;
}


//
// SocketTunnelClient
// 
SocketTunnelClient::SocketTunnelClient()
{
    m_internalBuffer.resize(BUF_SIZE);
    m_clientfd = -1;

#ifdef __linux__
#elif _WIN32
    WSADATA wsaData;
    int iResult = WSAStartup(MAKEWORD(2,2), &wsaData);
#endif
}


SocketTunnelClient::~SocketTunnelClient()
{
#ifdef __linux__
    shutdown(m_clientfd, SHUT_RDWR);
    m_clientfd=-1;
    close(m_clientfd);    
#elif _WIN32
    WSACleanup();
    closesocket(m_clientfd);    
    m_clientfd=-1;
#endif
}


int SocketTunnelClient::reset()
{            
    #ifdef __linux__
        shutdown(m_clientfd, SHUT_RDWR);
        m_clientfd=-1;
    #elif _WIN32
        closesocket(m_clientfd);    
        m_clientfd=-1;
    #endif
    return 1;
}


int SocketTunnelClient::init(const std::string& ip_dst, int port)
{
    if(m_clientfd==-1)
    {
    #ifdef __linux__
        signal(SIGPIPE, sig_handler); 
    #elif _WIN32
        
    #endif

        m_clientfd = connect_to_host(ip_dst, port);
        if(m_clientfd == -1)
        {
            return 0;
        }
    }

    return 1;
}


int SocketTunnelClient::process(const std::string& dataIn, std::string& dataOut)
{
    if(dataIn.size()>0)
        send_sock(m_clientfd, dataIn.data(), dataIn.size());

    int res = readAllDataFromSocket(m_clientfd, dataOut);
    return res;
}
