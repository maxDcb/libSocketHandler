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
#include <algorithm>

#include "SocketServer.hpp"
#include "SocketDef.hpp"


#define MAXPENDING 200


using namespace std;


//
// SocketTunnelServer
// 
SocketTunnelServer::SocketTunnelServer(int serverfd, int serverPort)
: m_serverfd(serverfd)
, m_serverPort(serverPort)
{
}


SocketTunnelServer::~SocketTunnelServer()
{
    #ifdef __linux__
        shutdown(m_serverfd, SHUT_RDWR);
        m_serverfd=-1;
        close(m_serverfd);    
    #elif _WIN32
        closesocket(m_serverfd);    
        m_serverfd=-1;
    #endif
}


int SocketTunnelServer::recv(std::string& dataOut)
{
    int res = readAllDataFromSocket(m_serverfd, dataOut);
    return res;
}

int SocketTunnelServer::send(std::string& dataIn)
{
    if(dataIn.size()>0)
        send_sock(m_serverfd, dataIn.data(), dataIn.size());

    return 1;
}


//
// SocketServer
// 
SocketServer::SocketServer(int serverPort)
: m_serverPort(serverPort)
, m_isStoped(true)
, m_isLaunched(false)
{
    #ifdef __linux__
    #elif _WIN32
        WSADATA wsaData;
        int iResult = WSAStartup(MAKEWORD(2,2), &wsaData);
    #endif
}


SocketServer::~SocketServer() 
{ 
    stop();
    if(m_isLaunched == true)
        this->m_socketServer->join();

    #ifdef __linux__  
    #elif _WIN32
        WSACleanup();
    #endif
}


void SocketServer::launch()
{
    this->m_socketServer = std::make_unique<std::thread>(&SocketServer::handleConnection, this);
}


void SocketServer::stop()
{
    m_isStoped=true;
    if(m_listen_sock==-1)
        return;

    #ifdef __linux__
        shutdown(m_listen_sock, SHUT_RDWR);
        m_listen_sock=-1;
        close(m_listen_sock);    
    #elif _WIN32
        closesocket(m_listen_sock);    
        m_listen_sock=-1;
    #endif
}


int SocketServer::createServerSocket(struct sockaddr_in &echoclient) 
{
    int serversock;
    struct sockaddr_in echoserver;

    if ((serversock = socket(AF_INET, SOCK_STREAM, 0)) < 0) 
    {
        // std::cout << "[-] Could not create socket.\n";
        return -1;
    }

    int opt = 1;
    if (setsockopt(serversock, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt)) < 0) 
    {
        // std::cout << "[-] Error setting socket options: " << strerror(errno) << std::endl;
        #ifdef __linux__
            close(serversock);
        #elif _WIN32
            closesocket(serversock);
        #endif

        return -1;
    }

    // Construct the server sockaddr_in structure 
    memset(&echoserver, 0, sizeof(echoserver));       // Clear struct 
    echoserver.sin_family = AF_INET;                  // Internet/IP 
    echoserver.sin_addr.s_addr = htonl(INADDR_ANY);   // Incoming addr 
    echoserver.sin_port = htons(m_serverPort);       // server port 

    // Bind the server socket 
    // TODO should fail if the port is already in use, but doesn't
    if (bind(serversock, (struct sockaddr *)&echoserver, sizeof(echoserver)) < 0) 
    {
        #ifdef __linux__
            close(serversock);
        #elif _WIN32
            closesocket(serversock);
        #endif

        // std::cout << "[-] Bind error.\n";
        return -1;
    }

    // Listen on the server socket 
    if (listen(serversock, MAXPENDING) < 0) 
    {
        #ifdef __linux__
            close(serversock);
        #elif _WIN32
            closesocket(serversock);
        #endif

        // std::cout << "[-] Listen error.\n";
        return -1;
    }
    return serversock;
}


int SocketServer::handleConnection()
{
    struct sockaddr_in echoclient;
    m_listen_sock = createServerSocket(echoclient);
    
    if(m_listen_sock == -1) 
    {
        // std::cout << "[-] Failed to create server\n";
        return -1;
    }

    #ifdef __linux__
        signal(SIGPIPE, sig_handler);  
    #elif _WIN32  
    #endif

    m_isStoped = false;
    m_isLaunched = true;
    while(!m_isStoped) 
    {
        int clientlen = sizeof(echoclient);
        int clientsock;
        if ((clientsock = accept(m_listen_sock, (struct sockaddr *) &echoclient, &clientlen)) > 0) 
        {
            // 
            // mode indirect
            //
            std::unique_ptr<SocketTunnelServer> socksTunnelServer = std::make_unique<SocketTunnelServer>(clientsock, m_serverPort);
            std::lock_guard<std::mutex> lock(m_mutex);
            m_socketTunnelServers.push_back(std::move(socksTunnelServer));
        }
    }

    // std::cout << "handleConnection stoped\n";

    #ifdef __linux__
        shutdown(m_listen_sock, SHUT_RDWR);
        close(m_listen_sock);    
    #elif _WIN32
        closesocket(m_listen_sock);    
    #endif

    return 1;
}


void SocketServer::cleanTunnel()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_socketTunnelServers.erase(std::remove_if(m_socketTunnelServers.begin(), m_socketTunnelServers.end(),
                             [](const std::unique_ptr<SocketTunnelServer>& ptr) { return ptr == nullptr; }),
              m_socketTunnelServers.end());
}