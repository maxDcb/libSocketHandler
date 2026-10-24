#pragma once

#include <thread>
#include <vector>
#include <string>
#include <mutex>
#include <memory>
#include <atomic>


class SocketTunnelServer
{
    public:
        SocketTunnelServer(int serverfd, int serverPort);
        ~SocketTunnelServer();

        int recv(std::string& dataOut);
        int send(std::string& dataIn);
        void close();
        bool isClosed() const { return m_serverfd == -1; }

    private:
        int m_serverfd;
        int m_serverPort;
};


class SocketServer
{

public:
    SocketServer(int serverPort, std::string bindAddress = "0.0.0.0");
    ~SocketServer(); 

    void launch();
    void stop();
    void cleanTunnel();
    std::vector<std::shared_ptr<SocketTunnelServer>> getTunnels();
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

private:
    int createServerSocket(struct sockaddr_in &echoclient);
    void handleConnection(int listenSocket);

    int m_serverPort;
    std::string m_bindAddress;
    std::atomic<int> m_listen_sock;

    std::atomic<bool> m_isLaunched;
    std::atomic<bool> m_isStoped;
    std::unique_ptr<std::thread> m_socketServer;

    std::mutex m_mutex;
    std::vector<std::shared_ptr<SocketTunnelServer>> m_socketTunnelServers;
};
