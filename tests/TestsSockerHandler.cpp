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

    SocketServer occupied(port);
    occupied.launch();
    if (occupied.isServerLaunched())
    {
        std::cerr << "Second server unexpectedly bound occupied port" << std::endl;
        return 1;
    }
    occupied.launch(); // A failed launch must not leave a joinable thread behind.
    if (occupied.isServerLaunched())
    {
        std::cerr << "Retry unexpectedly bound occupied port" << std::endl;
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
    for (int i = 0; i < 10 && server.getTunnels().empty(); ++i)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    auto tunnels = server.getTunnels();
    if (tunnels.empty())
    {
        std::cerr << "Server did not accept connection" << std::endl;
        return 1;
    }

    auto tunnel = tunnels.front();

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
    if (!server.getTunnels().empty())
    {
        std::cerr << "Tunnel cleanup failed" << std::endl;
        return 1;
    }
    server.stop();

    return 0;
}


