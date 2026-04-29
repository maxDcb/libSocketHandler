#pragma once

#include <thread>
#include <vector>
#include <string>
#include <mutex>
#include <memory>


class SocketTunnelServer
{
    public:
        SocketTunnelServer(int serverfd, int serverPort);
        ~SocketTunnelServer();

        int recv(std::string& dataOut);
        int send(std::string& dataIn);
        bool isClosed() const { return m_serverfd == -1; }

    private:
        int m_serverfd;
        int m_serverPort;
};


class SocketServer
{

public:
    SocketServer(int serverPort);
    ~SocketServer(); 

    void launch();
    void stop();
    void cleanTunnel();
    bool isServerStoped()
    {
        return m_isStoped;
    }
    bool isServerLaunched()
    {
        return m_isLaunched;
    }
    int getServerPort()
    {
        return m_serverPort;
    }

    std::vector<std::unique_ptr<SocketTunnelServer>> m_socketTunnelServers;

private:
    int createListenSocket(struct sockaddr_in &echoclient) ;
    int createServerSocket(struct sockaddr_in &echoclient);
    int handleConnection();

    int m_serverPort;
    int m_listen_sock;

    bool m_isLaunched;
    bool m_isStoped;
    std::unique_ptr<std::thread> m_socketServer;

    std::mutex m_mutex;    
};
