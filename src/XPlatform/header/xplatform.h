#pragma once

#include <cstddef>
#include <cstdint>
#include <cstdarg>
#include <cstdio>
#include <filesystem>
#include <string>
#include <string_view>

#ifdef _WIN32
#include <windows.h>
#define PATH_SEP '\\'
#else
#define PATH_SEP '/'

using BYTE = std::uint8_t;
using WORD = std::uint16_t;
using DWORD = std::uint32_t;
using UINT = std::uint32_t;

inline constexpr DWORD INFINITE = 0xFFFFFFFFu;
inline constexpr UINT CP_ACP = 0;
inline constexpr UINT CP_THREAD_ACP = 3;
inline constexpr UINT MAXUINT = UINT32_MAX;

template <typename T, std::size_t Size>
constexpr std::size_t rrr3d_array_count(T (&)[Size]) noexcept
{
    return Size;
}

#ifndef _countof
#define _countof(array) rrr3d_array_count(array)
#endif

template <std::size_t Size>
int sprintf_s(char (&buffer)[Size], const char* format, ...)
{
    va_list args;
    va_start(args, format);
    const int result = std::vsnprintf(buffer, Size, format, args);
    va_end(args);
    return result;
}

inline int sprintf_s(char* buffer, std::size_t size, const char* format, ...)
{
    va_list args;
    va_start(args, format);
    const int result = std::vsnprintf(buffer, size, format, args);
    va_end(args);
    return result;
}
#endif

namespace rrr3d::platform
{

using NativeLibraryHandle = void*;

#ifdef _WIN32
using NativeWindowHandle = HWND;
#else
using NativeWindowHandle = void*;
#endif

double steady_seconds() noexcept;
std::uint64_t steady_milliseconds() noexcept;
std::uint64_t steady_nanoseconds() noexcept;
void sleep_for_milliseconds(std::uint64_t milliseconds);

std::filesystem::path normalize_path(std::string_view path);
std::filesystem::path join_path(const std::filesystem::path& base,
                                const std::filesystem::path& child);
std::filesystem::path application_directory();
std::filesystem::path resource_directory();
std::filesystem::path save_directory();
std::filesystem::path log_directory();
bool ensure_application_directories(std::string& error) noexcept;

std::u16string utf8_to_utf16(std::string_view value);
std::string utf16_to_utf8(std::u16string_view value);
std::wstring utf8_to_wide(std::string_view value);
std::string wide_to_utf8(std::wstring_view value);

NativeLibraryHandle load_library(const std::filesystem::path& path);
void* load_symbol(NativeLibraryHandle library, const char* symbol) noexcept;
void free_library(NativeLibraryHandle library) noexcept;

void report_error(std::string_view title, std::string_view message) noexcept;

} // namespace rrr3d::platform
