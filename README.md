# libSocketHandler

A small cross-platform C++14 library providing a TCP client and multi-connection server for Linux and Windows.

## Features
- `SocketTunnelClient` to connect to a remote host, send data and receive replies.
- `SocketServer` accepts multiple client connections concurrently and exposes per-connection tunnels.
- Utility functions for safe send/receive operations.

## Building
```bash
mkdir build && cd build
cmake ..
cmake --build .
```

## Basic usage
```cpp
#include "SocketServer.hpp"
#include "SocketClient.hpp"

SocketServer server(8080);
server.launch();

SocketTunnelClient client;
client.init("127.0.0.1", 8080);
std::string reply;
client.process("hello", reply);
```

## License
MIT
