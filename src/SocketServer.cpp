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
#define MAX_ACTIVE_TUNNELS 256


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
    close();
}

void SocketTunnelServer::close()
{
#ifdef __linux__
    if (m_serverfd != -1)
    {
        shutdown(m_serverfd, SHUT_RDWR);
        ::close(m_serverfd);
        m_serverfd = -1;
    }
#elif _WIN32
    if (m_serverfd != INVALID_SOCKET)
    {
        closesocket(m_serverfd);
        m_serverfd = INVALID_SOCKET;
    }
#endif
}


int SocketTunnelServer::recv(std::string& dataOut)
{
    // The listener scans all tunnels; an idle tunnel must not cost 10 ms.
    int res = readAllDataFromSocket(m_serverfd, dataOut, 0, 64 * 1024);
    if (res == -1)
        close();
    return res;
}

int SocketTunnelServer::send(std::string& dataIn)
{
    if(dataIn.size()>0)
    {
        if (send_sock(m_serverfd, dataIn.data(), dataIn.size()) != static_cast<int>(dataIn.size()))
        {
            close();
            return -1;
        }
    }

    return 1;
}


//
// SocketServer
// 
SocketServer::SocketServer(int serverPort, std::string bindAddress)
    : m_serverPort(serverPort)
    , m_bindAddress(std::move(bindAddress))
    , m_listen_sock(-1)
    , m_isLaunched(false)
    , m_isStoped(true)
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
    if(m_socketServer && m_socketServer->joinable())
        this->m_socketServer->join();

    #ifdef __linux__  
    #elif _WIN32
        WSACleanup();
    #endif
}


void SocketServer::launch()
{
    stop();
    if (m_socketServer && m_socketServer->joinable())
        m_socketServer->join();
    m_socketServer.reset();
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        for (const auto& tunnel : m_socketTunnelServers)
            tunnel->close();
        m_socketTunnelServers.clear();
    }

    struct sockaddr_in clientAddress;
    const int listenSocket = createServerSocket(clientAddress);
    if (listenSocket == -1)
        return;

    m_listen_sock = listenSocket;
    m_isStoped = false;
    m_isLaunched = true;
    try
    {
        m_socketServer = std::make_unique<std::thread>(&SocketServer::handleConnection, this, listenSocket);
    }
    catch (...)
    {
        stop();
        throw;
    }
}


void SocketServer::stop()
{
    m_isStoped = true;
    m_isLaunched = false;
    const int listenSocket = m_listen_sock.exchange(-1);
    if (listenSocket == -1)
        return;

#ifdef __linux__
    shutdown(listenSocket, SHUT_RDWR);
    close(listenSocket);
#elif _WIN32
    closesocket(listenSocket);
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
#ifdef _WIN32
    // SO_REUSEADDR lets another Windows socket take over an occupied port.
    const int addressOption = SO_EXCLUSIVEADDRUSE;
#else
    const int addressOption = SO_REUSEADDR;
#endif
    if (setsockopt(serversock, SOL_SOCKET, addressOption, (const char*)&opt, sizeof(opt)) < 0)
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
    if (inet_pton(AF_INET, m_bindAddress.c_str(), &echoserver.sin_addr) != 1)
    {
#ifdef __linux__
        close(serversock);
#elif _WIN32
        closesocket(serversock);
#endif
        return -1;
    }
    echoserver.sin_port = htons(m_serverPort);       // server port 

    // Bind the server socket 
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

    struct sockaddr_in boundAddress;
    memset(&boundAddress, 0, sizeof(boundAddress));
    #ifdef __linux__
        socklen_t boundAddressSize = sizeof(boundAddress);
    #elif _WIN32
        int boundAddressSize = sizeof(boundAddress);
    #endif
    if (getsockname(serversock, (struct sockaddr *)&boundAddress, &boundAddressSize) == 0)
    {
        m_serverPort = ntohs(boundAddress.sin_port);
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


void SocketServer::handleConnection(int listenSocket)
{
    struct sockaddr_in echoclient;

    #ifdef __linux__
        signal(SIGPIPE, sig_handler);  
    #elif _WIN32  
    #endif

    while(!m_isStoped) 
    {
        int clientlen = sizeof(echoclient);
        int clientsock;
        #ifdef __linux__
        if ((clientsock = accept(listenSocket, (struct sockaddr *) &echoclient, (uint*)&clientlen)) >= 0)
        #elif _WIN32
        if ((clientsock = accept(listenSocket, (struct sockaddr *) &echoclient, &clientlen)) >= 0)
        #endif
        {
            // 
            // mode indirect
            //
            std::shared_ptr<SocketTunnelServer> socksTunnelServer = std::make_shared<SocketTunnelServer>(clientsock, m_serverPort);
            std::lock_guard<std::mutex> lock(m_mutex);
            m_socketTunnelServers.erase(
                std::remove_if(m_socketTunnelServers.begin(), m_socketTunnelServers.end(),
                    [](const auto& tunnel) { return !tunnel || tunnel->isClosed(); }),
                m_socketTunnelServers.end());
            if (m_socketTunnelServers.size() >= MAX_ACTIVE_TUNNELS)
                socksTunnelServer->close();
            else
                m_socketTunnelServers.push_back(std::move(socksTunnelServer));
        }
    }

    // std::cout << "handleConnection stoped\n";

    stop();
}

std::vector<std::shared_ptr<SocketTunnelServer>> SocketServer::getTunnels()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_socketTunnelServers;
}


void SocketServer::cleanTunnel()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_socketTunnelServers.erase(
        std::remove_if(
            m_socketTunnelServers.begin(),
            m_socketTunnelServers.end(),
            [](const std::shared_ptr<SocketTunnelServer>& ptr)
            {
                return ptr == nullptr || ptr->isClosed();
            }),
        m_socketTunnelServers.end());
}
