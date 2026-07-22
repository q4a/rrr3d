#include "resource/ResourceFileSystem.h"

#include "xplatform.h"

#include <algorithm>
#include <fstream>
#include <limits>

namespace r3d::resource
{
namespace
{

bool pathStartsWith(const std::filesystem::path& path,
                    const std::filesystem::path& root)
{
    auto path_iterator = path.begin();
    for (auto root_iterator = root.begin(); root_iterator != root.end();
         ++root_iterator, ++path_iterator)
    {
        if (path_iterator == path.end() || *path_iterator != *root_iterator)
            return false;
    }
    return true;
}

std::string displayPath(std::string_view path)
{
    return std::string(path.begin(), path.end());
}

} // namespace

ResourceFileSystem::ResourceFileSystem(std::filesystem::path root)
{
    if (root.empty())
        throw ResourceError("Game-data root is empty");

    const std::string requested_root = root.string();
    std::error_code error;
    root_ = std::filesystem::canonical(std::move(root), error);
    if (error || !std::filesystem::is_directory(root_))
    {
        throw ResourceError("Game-data directory is unavailable: " +
                            requested_root);
    }
}

const std::filesystem::path& ResourceFileSystem::root() const noexcept
{
    return root_;
}

std::filesystem::path ResourceFileSystem::validateVirtualPath(
    std::string_view virtualPath) const
{
    if (virtualPath.empty())
        throw ResourceError("Resource path is empty");

    if (virtualPath.size() >= 2 &&
        ((virtualPath[0] >= 'A' && virtualPath[0] <= 'Z') ||
         (virtualPath[0] >= 'a' && virtualPath[0] <= 'z')) &&
        virtualPath[1] == ':')
    {
        throw ResourceError("Windows drive paths are not valid resources: " +
                            displayPath(virtualPath));
    }

    const std::filesystem::path relative =
        rrr3d::platform::normalize_path(virtualPath);
    if (relative.empty() || relative.is_absolute() || relative.has_root_name() ||
        relative.has_root_directory())
    {
        throw ResourceError("Resource path must be relative: " +
                            displayPath(virtualPath));
    }

    for (const auto& component : relative)
    {
        if (component == "..")
        {
            throw ResourceError("Resource path escapes game-data: " +
                                displayPath(virtualPath));
        }
    }
    return relative;
}

std::filesystem::path ResourceFileSystem::resolveExactCase(
    const std::filesystem::path& relativePath) const
{
    std::filesystem::path current = root_;
    for (const auto& component : relativePath)
    {
        if (component == ".")
            continue;

        std::error_code iteration_error;
        bool found = false;
        for (const auto& entry :
             std::filesystem::directory_iterator(current, iteration_error))
        {
            if (entry.path().filename() == component)
            {
                current = entry.path();
                found = true;
                break;
            }
        }
        if (iteration_error || !found)
        {
            throw ResourceError(
                "Resource is missing or has different letter case: " +
                relativePath.generic_string());
        }
    }

    std::error_code canonical_error;
    const auto canonical = std::filesystem::canonical(current, canonical_error);
    if (canonical_error || !pathStartsWith(canonical, root_))
    {
        throw ResourceError("Resolved resource escapes game-data: " +
                            relativePath.generic_string());
    }
    return canonical;
}

bool ResourceFileSystem::exists(std::string_view virtualPath) const
{
    try
    {
        const auto path = resolve(virtualPath);
        return std::filesystem::is_regular_file(path);
    }
    catch (const ResourceError&)
    {
        return false;
    }
}

std::uintmax_t ResourceFileSystem::fileSize(
    std::string_view virtualPath) const
{
    const auto path = resolve(virtualPath);
    std::error_code error;
    const auto size = std::filesystem::file_size(path, error);
    if (error)
    {
        throw ResourceError("Unable to query resource size: " +
                            path.string());
    }
    return size;
}

std::filesystem::path ResourceFileSystem::resolve(
    std::string_view virtualPath) const
{
    const auto relative = validateVirtualPath(virtualPath);
    const auto resolved = resolveExactCase(relative);
    if (!std::filesystem::is_regular_file(resolved))
    {
        throw ResourceError("Resource is not a regular file: " +
                            relative.generic_string());
    }
    return resolved;
}

std::vector<std::uint8_t> ResourceFileSystem::readBinary(
    std::string_view virtualPath) const
{
    const auto path = resolve(virtualPath);
    std::error_code size_error;
    const auto size = std::filesystem::file_size(path, size_error);
    if (size_error || size > maximum_binary_size ||
        size > static_cast<std::uintmax_t>(
                   std::numeric_limits<std::size_t>::max()))
    {
        throw ResourceError("Resource has invalid size: " + path.string());
    }

    std::ifstream stream(path, std::ios::binary);
    if (!stream)
        throw ResourceError("Unable to open resource: " + path.string());

    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(size));
    if (!bytes.empty())
    {
        stream.read(reinterpret_cast<char*>(bytes.data()),
                    static_cast<std::streamsize>(bytes.size()));
    }
    if (!stream && !bytes.empty())
        throw ResourceError("Unable to read resource: " + path.string());
    return bytes;
}

std::string ResourceFileSystem::readText(std::string_view virtualPath) const
{
    const auto bytes = readBinary(virtualPath);
    if (bytes.size() > maximum_text_size)
    {
        throw ResourceError("Text resource is too large: " +
                            displayPath(virtualPath));
    }
    if (std::find(bytes.begin(), bytes.end(), std::uint8_t{0}) != bytes.end())
    {
        throw ResourceError("Text resource contains a NUL byte: " +
                            displayPath(virtualPath));
    }
    return std::string(bytes.begin(), bytes.end());
}

std::filesystem::path defaultGameDataDirectory()
{
    return rrr3d::platform::resource_directory() / "game-data";
}

} // namespace r3d::resource
