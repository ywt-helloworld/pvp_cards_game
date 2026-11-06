#pragma once
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <sys/socket.h>
#endif
namespace cardgame::network {
#ifdef _WIN32
using NativeSocket = SOCKET;
inline constexpr NativeSocket INVALID_NATIVE_SOCKET = INVALID_SOCKET;
#else
using NativeSocket = int;
inline constexpr NativeSocket INVALID_NATIVE_SOCKET = -1;
#endif
} // namespace cardgame::network
