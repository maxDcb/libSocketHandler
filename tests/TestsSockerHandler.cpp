#include <iostream>
#include <string>
#include <chrono>
#include <thread>

#include "SocketServer.hpp"
#include "SocketClient.hpp"

int main(int argc, char *argv[])
{
    // Start server
    SocketServer server(0);
    server.launch();

    int port = 0;
    for (int i = 0; i < 100; ++i)
    {
        if (server.isServerLaunched())
        {
            port = server.getServerPort();
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    if (port == 0)
    {
        std::cerr << "Server did not publish a listening port" << std::endl;
        return 1;
    }

    // Connect client
    SocketTunnelClient client;
    if (!client.init("127.0.0.1", port))
    {
        std::cerr << "Client failed to connect" << std::endl;
        return 1;
    }

    // Wait until server registers the tunnel
    for (int i = 0; i < 10 && server.m_socketTunnelServers.empty(); ++i)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    if (server.m_socketTunnelServers.empty())
    {
        std::cerr << "Server did not accept connection" << std::endl;
        return 1;
    }

    auto &tunnel = server.m_socketTunnelServers.front();

    // Test server -> client
    std::string msg = "fromServer";
    tunnel->send(msg);

    std::string reply;
    client.process("", reply);
    if (reply != msg)
    {
        std::cerr << "Client did not receive expected message" << std::endl;
        return 1;
    }

    // Test client -> server
    std::string clientMsg = "fromClient";
    std::string dummy;
    client.process(clientMsg, dummy);

    std::string serverRecv;
    tunnel->recv(serverRecv);
    if (serverRecv != clientMsg)
    {
        std::cerr << "Server did not receive expected message" << std::endl;
        return 1;
    }

    client.reset();
    std::string tmp;
    tunnel->recv(tmp); // trigger detection of closed socket
    server.cleanTunnel();
    if (!server.m_socketTunnelServers.empty())
    {
        std::cerr << "Tunnel cleanup failed" << std::endl;
        return 1;
    }
    server.stop();

    return 0;
}


