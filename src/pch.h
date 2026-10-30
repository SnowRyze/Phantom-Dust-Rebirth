#pragma once
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <roapi.h>
#include <winstring.h>
#include <psapi.h>

#include <winrt/base.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Storage.h>
#include <winrt/Windows.Storage.Streams.h>
#include <winrt/Windows.System.h>
#include <winrt/Windows.Gaming.XboxLive.Storage.h>

#include <atomic>
#include <chrono>
#include <filesystem>
#include <functional>
#include <map>
#include <mutex>
#include <string>
#include <vector>
