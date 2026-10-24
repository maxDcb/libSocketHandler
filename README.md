# libSocketHandler

A small cross-platform C++14 library providing a TCP client and multi-connection server for Linux and Windows.

## Features
- `SocketTunnelClient` to connect to a remote host, send data and receive replies.
- `SocketServer` accepts up to 256 client connections and exposes per-connection tunnels.
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

SocketServer server(8080, "127.0.0.1"); // Omit the address to bind all IPv4 interfaces.
server.launch();

SocketTunnelClient client;
client.init("127.0.0.1", 8080);
std::string reply;
bool sent = false;
client.process("hello", reply, &sent);
```

The bind and destination addresses are numeric IPv4 addresses. `sent` confirms
that the complete request was accepted by the local socket; it is not a peer
acknowledgement. The caller can retain an unsent request and retry it after a
failed connection.

## License
MIT
