cmake_minimum_required(VERSION 3.6)
project(unp)

set(CMAKE_CXX_STANDARD 14)

set(SOURCE_FILES utils.h utils.cpp unp.h readn.cpp sock.cpp error.cpp  Signal.cpp web.h web.cpp hostent.cpp daytimetcpclient.cpp daytimeserver.cpp daytimeclient.cpp listtimer.cpp listtimer.h ipc_sem.cpp ipc_chat_server.cpp chat_client.cpp net_server.cpp net_client.cpp mpserver.cpp mpclient.cpp processpool.h cgi_server.cpp cgi_client.cpp)

add_executable(unp ${SOURCE_FILES})