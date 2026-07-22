#include "xplatform.h"

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <thread>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#elif defined(__APPLE__)
#include <dlfcn.h>
#include <mach-o/dyld.h>
#else
#include <dlfcn.h>
#include <unistd.h>
#endif

namespace rrr3d::platform
{
namespace
{

using SteadyClock = std::chrono::steady_clock;
const SteadyClock::time_point process_epoch = SteadyClock::now();

void append_utf8(std::string& output, std::uint32_t code_point)
{
    if (code_point <= 0x7Fu)
    {
        output.push_back(static_cast<char>(code_point));
    }
    else if (code_point <= 0x7FFu)
    {
        output.push_back(static_cast<char>(0xC0u | (code_point >> 6u)));
        output.push_back(static_cast<char>(0x80u | (code_point & 0x3Fu)));
    }
    else if (code_point <= 0xFFFFu)
    {
        output.push_back(static_cast<char>(0xE0u | (code_point >> 12u)));
        output.push_back(static_cast<char>(0x80u | ((code_point >> 6u) & 0x3Fu)));
        output.push_back(static_cast<char>(0x80u | (code_point & 0x3Fu)));
    }
    else
    {
        output.push_back(static_cast<char>(0xF0u | (code_point >> 18u)));
        output.push_back(static_cast<char>(0x80u | ((code_point >> 12u) & 0x3Fu)));
        output.push_back(static_cast<char>(0x80u | ((code_point >> 6u) & 0x3Fu)));
        output.push_back(static_cast<char>(0x80u | (code_point & 0x3Fu)));
    }
}

std::vector<std::uint32_t> decode_utf8(std::string_view value)
{
    std::vector<std::uint32_t> output;
    output.reserve(value.size());

    for (std::size_t index = 0; index < value.size();)
    {
        const auto first = static_cast<unsigned char>(value[index]);
        std::uint32_t code_point = 0;
        std::size_t continuation_count = 0;
        std::uint32_t minimum = 0;

        if (first <= 0x7Fu)
        {
            code_point = first;
        }
        else if ((first & 0xE0u) == 0xC0u)
        {
            code_point = first & 0x1Fu;
            continuation_count = 1;
            minimum = 0x80u;
        }
        else if ((first & 0xF0u) == 0xE0u)
        {
            code_point = first & 0x0Fu;
            continuation_count = 2;
            minimum = 0x800u;
        }
        else if ((first & 0xF8u) == 0xF0u)
        {
            code_point = first & 0x07u;
            continuation_count = 3;
            minimum = 0x10000u;
        }
        else
        {
            throw std::invalid_argument("Invalid UTF-8 leading byte");
        }

        if (index + continuation_count >= value.size())
            throw std::invalid_argument("Truncated UTF-8 sequence");

        for (std::size_t offset = 1; offset <= continuation_count; ++offset)
        {
            const auto next = static_cast<unsigned char>(value[index + offset]);
            if ((next & 0xC0u) != 0x80u)
                throw std::invalid_argument("Invalid UTF-8 continuation byte");
            code_point = (code_point << 6u) | (next & 0x3Fu);
        }

        if ((continuation_count != 0 && code_point < minimum) ||
            code_point > 0x10FFFFu ||
            (code_point >= 0xD800u && code_point <= 0xDFFFu))
        {
            throw std::invalid_argument("Invalid UTF-8 code point");
        }

        output.push_back(code_point);
        index += continuation_count + 1;
    }

    return output;
}

std::filesystem::path home_directory()
{
    if (const char* home = std::getenv("HOME"))
        return std::filesystem::path(home);
    return {};
}

} // namespace

double steady_seconds() noexcept
{
    return std::chrono::duration<double>(SteadyClock::now() - process_epoch).count();
}

std::uint64_t steady_milliseconds() noexcept
{
    return static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            SteadyClock::now() - process_epoch)
            .count());
}

std::uint64_t steady_nanoseconds() noexcept
{
    return static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(
            SteadyClock::now() - process_epoch)
            .count());
}

void sleep_for_milliseconds(std::uint64_t milliseconds)
{
    std::this_thread::sleep_for(std::chrono::milliseconds(milliseconds));
}

std::filesystem::path normalize_path(std::string_view path)
{
    std::string portable(path);
#ifndef _WIN32
    std::replace(portable.begin(), portable.end(), '\\', '/');
#endif
    return std::filesystem::path(portable).lexically_normal();
}

std::filesystem::path join_path(const std::filesystem::path& base,
                                const std::filesystem::path& child)
{
    return (base / child).lexically_normal();
}

std::filesystem::path application_directory()
{
#ifdef _WIN32
    std::wstring buffer(32768, L'\0');
    const DWORD length = GetModuleFileNameW(nullptr, buffer.data(),
                                            static_cast<DWORD>(buffer.size()));
    if (length == 0 || length == buffer.size())
        return std::filesystem::current_path();
    buffer.resize(length);
    return std::filesystem::path(buffer).parent_path();
#elif defined(__APPLE__)
    std::uint32_t size = 0;
    _NSGetExecutablePath(nullptr, &size);
    std::vector<char> buffer(size);
    if (_NSGetExecutablePath(buffer.data(), &size) != 0)
        return std::filesystem::current_path();
    std::error_code error;
    const auto executable = std::filesystem::weakly_canonical(buffer.data(), error);
    return (error ? std::filesystem::path(buffer.data()) : executable).parent_path();
#else
    std::vector<char> buffer(4096);
    const auto length = readlink("/proc/self/exe", buffer.data(), buffer.size() - 1);
    if (length <= 0)
        return std::filesystem::current_path();
    buffer[static_cast<std::size_t>(length)] = '\0';
    return std::filesystem::path(buffer.data()).parent_path();
#endif
}

std::filesystem::path resource_directory()
{
    const auto executable_directory = application_directory();
#ifdef __APPLE__
    if (executable_directory.filename() == "MacOS" &&
        executable_directory.parent_path().filename() == "Contents")
    {
        return executable_directory.parent_path() / "Resources";
    }
#endif
    return executable_directory;
}

std::filesystem::path save_directory()
{
#ifdef __APPLE__
    return home_directory() / "Library" / "Application Support" / "RRR3d";
#elif defined(_WIN32)
    if (const wchar_t* app_data = _wgetenv(L"APPDATA"))
        return std::filesystem::path(app_data) / "RRR3d";
    return application_directory();
#else
    if (const char* data_home = std::getenv("XDG_DATA_HOME"))
        return std::filesystem::path(data_home) / "rrr3d";
    return home_directory() / ".local" / "share" / "rrr3d";
#endif
}

std::filesystem::path log_directory()
{
#ifdef __APPLE__
    return home_directory() / "Library" / "Logs" / "RRR3d";
#else
    return save_directory() / "logs";
#endif
}

bool ensure_application_directories(std::string& error) noexcept
{
    try
    {
        std::error_code save_error;
        std::filesystem::create_directories(save_directory(), save_error);
        if (save_error)
        {
            error = "Unable to create application data directory: " +
                    save_error.message();
            return false;
        }

        std::error_code log_error;
        std::filesystem::create_directories(log_directory(), log_error);
        if (log_error)
        {
            error = "Unable to create log directory: " + log_error.message();
            return false;
        }
        return true;
    }
    catch (const std::exception& exception)
    {
        error = exception.what();
        return false;
    }
}

std::u16string utf8_to_utf16(std::string_view value)
{
    std::u16string output;
    for (const std::uint32_t code_point : decode_utf8(value))
    {
        if (code_point <= 0xFFFFu)
        {
            output.push_back(static_cast<char16_t>(code_point));
        }
        else
        {
            const std::uint32_t adjusted = code_point - 0x10000u;
            output.push_back(static_cast<char16_t>(0xD800u + (adjusted >> 10u)));
            output.push_back(static_cast<char16_t>(0xDC00u + (adjusted & 0x3FFu)));
        }
    }
    return output;
}

std::string utf16_to_utf8(std::u16string_view value)
{
    std::string output;
    for (std::size_t index = 0; index < value.size(); ++index)
    {
        std::uint32_t code_point = value[index];
        if (code_point >= 0xD800u && code_point <= 0xDBFFu)
        {
            if (++index >= value.size())
                throw std::invalid_argument("Truncated UTF-16 surrogate pair");
            const std::uint32_t low = value[index];
            if (low < 0xDC00u || low > 0xDFFFu)
                throw std::invalid_argument("Invalid UTF-16 surrogate pair");
            code_point = 0x10000u + ((code_point - 0xD800u) << 10u) +
                         (low - 0xDC00u);
        }
        else if (code_point >= 0xDC00u && code_point <= 0xDFFFu)
        {
            throw std::invalid_argument("Unpaired UTF-16 low surrogate");
        }
        append_utf8(output, code_point);
    }
    return output;
}

std::wstring utf8_to_wide(std::string_view value)
{
    const auto code_points = decode_utf8(value);
    std::wstring output;
    if constexpr (sizeof(wchar_t) == sizeof(char16_t))
    {
        const auto utf16 = utf8_to_utf16(value);
        output.assign(utf16.begin(), utf16.end());
    }
    else
    {
        output.reserve(code_points.size());
        for (const auto code_point : code_points)
            output.push_back(static_cast<wchar_t>(code_point));
    }
    return output;
}

std::string wide_to_utf8(std::wstring_view value)
{
    if constexpr (sizeof(wchar_t) == sizeof(char16_t))
    {
        return utf16_to_utf8(
            std::u16string_view(reinterpret_cast<const char16_t*>(value.data()),
                                value.size()));
    }
    else
    {
        std::string output;
        for (const wchar_t wide : value)
        {
            const auto code_point = static_cast<std::uint32_t>(wide);
            if (code_point > 0x10FFFFu ||
                (code_point >= 0xD800u && code_point <= 0xDFFFu))
            {
                throw std::invalid_argument("Invalid wide code point");
            }
            append_utf8(output, code_point);
        }
        return output;
    }
}

NativeLibraryHandle load_library(const std::filesystem::path& path)
{
#ifdef _WIN32
    return reinterpret_cast<NativeLibraryHandle>(LoadLibraryW(path.c_str()));
#else
    return dlopen(path.c_str(), RTLD_NOW | RTLD_LOCAL);
#endif
}

void* load_symbol(NativeLibraryHandle library, const char* symbol) noexcept
{
    if (!library || !symbol)
        return nullptr;
#ifdef _WIN32
    return reinterpret_cast<void*>(
        GetProcAddress(reinterpret_cast<HMODULE>(library), symbol));
#else
    return dlsym(library, symbol);
#endif
}

void free_library(NativeLibraryHandle library) noexcept
{
    if (!library)
        return;
#ifdef _WIN32
    FreeLibrary(reinterpret_cast<HMODULE>(library));
#else
    dlclose(library);
#endif
}

void report_error(std::string_view title, std::string_view message) noexcept
{
    std::cerr << title << ": " << message << '\n';
}

} // namespace rrr3d::platform
