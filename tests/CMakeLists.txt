add_executable(TestsSockerHandler "TestsSockerHandler.cpp" )
set_property(TARGET TestsSockerHandler PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded")
target_link_libraries(TestsSockerHandler SocksServer )

