cmake_minimum_required(VERSION 3.13)
project(UDP_RELIABLE C)

set(CMAKE_C_STANDARD 99)

add_executable(UDP_RELIABLE
        Client/clientUDP.c
        Server/serverUDP.c
        common_functions.h
        server_functions.h
        client_functions.h
        common_functions.c
        config.h
        client_functions.c
        server_functions.c)