#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace r3d::resource
{

class ResourceError : public std::runtime_error
{
public:
    using std::runtime_error::runtime_error;
};

class ResourceFileSystem
{
public:
    static constexpr std::size_t maximum_text_size = 16U * 1024U * 1024U;
    static constexpr std::size_t maximum_binary_size = 256U * 1024U * 1024U;

    explicit ResourceFileSystem(std::filesystem::path root);

    const std::filesystem::path& root() const noexcept;
    bool exists(std::string_view virtualPath) const;
    std::uintmax_t fileSize(std::string_view virtualPath) const;
    std::filesystem::path resolve(std::string_view virtualPath) const;
    std::string readText(std::string_view virtualPath) const;
    std::vector<std::uint8_t> readBinary(std::string_view virtualPath) const;

private:
    std::filesystem::path root_;

    std::filesystem::path validateVirtualPath(
        std::string_view virtualPath) const;
    std::filesystem::path resolveExactCase(
        const std::filesystem::path& relativePath) const;
};

std::filesystem::path defaultGameDataDirectory();

} // namespace r3d::resource
