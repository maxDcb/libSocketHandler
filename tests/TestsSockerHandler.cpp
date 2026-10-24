#include <iostream>
#include <string>
#include <chrono>
#include <thread>

#ifdef __linux__
#include <dirent.h>
#elif _WIN32
#include <windows.h>
#endif

#include "SocketServer.hpp"
#include "SocketClient.hpp"

static int openHandleCount()
{
#ifdef __linux__
    DIR* directory = opendir("/proc/self/fd");
    if (!directory)
        return -1;
    int count = 0;
    while (readdir(directory))
        ++count;
    closedir(directory);
    return count;
#elif _WIN32
    DWORD count = 0;
    return GetProcessHandleCount(GetCurrentProcess(), &count) ? static_cast<int>(count) : -1;
#endif
}

int main(int argc, char *argv[])
{
    SocketTunnelClient failedClient;
    const int handlesBefore = openHandleCount();
    for (int i = 0; i < 100; ++i)
        failedClient.init("127.0.0.1", 0);
    const int handlesAfter = openHandleCount();
    if (handlesBefore < 0 || handlesAfter > handlesBefore + 2)
    {
        std::cerr << "Failed connections leaked socket handles" << std::endl;
        return 1;
    }

    SocketServer loopbackOnly(0, "127.0.0.2");
    loopbackOnly.launch();
    if (!loopbackOnly.isServerLaunched())
        return 1;
    SocketTunnelClient wrongAddress;
    if (wrongAddress.init("127.0.0.1", loopbackOnly.getServerPort()))
    {
        std::cerr << "Loopback-only server accepted the wrong address" << std::endl;
        return 1;
    }
    SocketTunnelClient rightAddress;
    if (!rightAddress.init("127.0.0.2", loopbackOnly.getServerPort()))
    {
        std::cerr << "Loopback-only server rejected its bind address" << std::endl;
        return 1;
    }
    rightAddress.reset();
    loopbackOnly.stop();

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

    std::string finalMessage = "lastFrame";
    client.process(finalMessage, dummy);
    client.reset();
    std::string finalRead;
    if (tunnel->recv(finalRead) <= 0 || finalRead != finalMessage)
    {
        std::cerr << "Final bytes were lost when the peer closed" << std::endl;
        return 1;
    }
    std::string tmp;
    if (tunnel->recv(tmp) != -1)
    {
        std::cerr << "Closed tunnel was not detected after final bytes" << std::endl;
        return 1;
    }
    server.cleanTunnel();
    if (!server.getTunnels().empty())
    {
        std::cerr << "Tunnel cleanup failed" << std::endl;
        return 1;
    }
    server.stop();

    return 0;
}


