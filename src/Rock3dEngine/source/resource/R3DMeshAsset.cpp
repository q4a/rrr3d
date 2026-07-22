#include "resource/R3DMeshAsset.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <string>

namespace r3d::resource
{
namespace
{

class Reader
{
public:
    Reader(const std::vector<std::uint8_t>& bytes, std::string_view name)
        : bytes_(bytes), name_(name)
    {
    }

    std::uint8_t byte(std::string_view field)
    {
        require(1, field);
        return bytes_[offset_++];
    }

    std::uint32_t uint32(std::string_view field)
    {
        require(4, field);
        const std::uint32_t value =
            static_cast<std::uint32_t>(bytes_[offset_]) |
            (static_cast<std::uint32_t>(bytes_[offset_ + 1]) << 8U) |
            (static_cast<std::uint32_t>(bytes_[offset_ + 2]) << 16U) |
            (static_cast<std::uint32_t>(bytes_[offset_ + 3]) << 24U);
        offset_ += 4;
        return value;
    }

    std::int32_t int32(std::string_view field)
    {
        return static_cast<std::int32_t>(uint32(field));
    }

    float float32(std::string_view field)
    {
        static_assert(sizeof(float) == sizeof(std::uint32_t),
                      ".r3d requires IEEE-754 32-bit floats");
        const std::uint32_t bits = uint32(field);
        float value = 0.0F;
        std::memcpy(&value, &bits, sizeof(value));
        return value;
    }

    void finish() const
    {
        if (offset_ != bytes_.size())
        {
            fail("trailing data after material groups");
        }
    }

    [[noreturn]] void fail(std::string_view detail) const
    {
        throw ResourceError(std::string(name_) + ": invalid .r3d: " +
                            std::string(detail));
    }

private:
    void require(std::size_t count, std::string_view field) const
    {
        if (count > bytes_.size() - std::min(offset_, bytes_.size()))
        {
            fail(std::string("truncated ") + std::string(field));
        }
    }

    const std::vector<std::uint8_t>& bytes_;
    std::string name_;
    std::size_t offset_ = 0;
};

std::uint32_t positiveCount(Reader& reader, std::string_view field,
                            std::uint32_t maximum)
{
    const std::int32_t value = reader.int32(field);
    if (value <= 0 || static_cast<std::uint32_t>(value) > maximum)
    {
        reader.fail(std::string(field) + " is outside the supported range");
    }
    return static_cast<std::uint32_t>(value);
}

} // namespace

R3DMeshAsset decodeR3DMeshAsset(const std::vector<std::uint8_t>& bytes,
                                std::string_view resourceName)
{
    if (bytes.empty())
        throw ResourceError(std::string(resourceName) + ": empty .r3d file");

    Reader reader(bytes, resourceName);
    R3DMeshAsset mesh;
    mesh.version = reader.int32("version");
    if (mesh.version != 0)
        reader.fail("unsupported version " + std::to_string(mesh.version));

    const std::uint8_t left_handed = reader.byte("coordinate-system flag");
    const std::uint8_t has_texcoords = reader.byte("texture-coordinate flag");
    if (left_handed > 1 || has_texcoords > 1)
        reader.fail("boolean header flag is not zero or one");
    mesh.leftCoordinateSystem = left_handed != 0;
    mesh.hasTexcoords = has_texcoords != 0;

    const std::uint32_t vertex_count = positiveCount(
        reader, "vertex count", 16U * 1024U * 1024U);
    mesh.vertices.resize(vertex_count);
    for (R3DVertex& vertex : mesh.vertices)
    {
        for (float& value : vertex.position)
        {
            value = reader.float32("vertex position");
            if (!std::isfinite(value))
                reader.fail("vertex position is not finite");
        }
        for (float& value : vertex.normal)
        {
            value = reader.float32("vertex normal");
            if (!std::isfinite(value))
                reader.fail("vertex normal is not finite");
        }
        if (mesh.hasTexcoords)
        {
            for (float& value : vertex.texcoord)
            {
                value = reader.float32("vertex texture coordinate");
                if (!std::isfinite(value))
                    reader.fail("vertex texture coordinate is not finite");
            }
        }
    }

    mesh.minimum = mesh.maximum = mesh.vertices.front().position;
    for (const R3DVertex& vertex : mesh.vertices)
    {
        for (std::size_t axis = 0; axis < 3; ++axis)
        {
            mesh.minimum[axis] =
                std::min(mesh.minimum[axis], vertex.position[axis]);
            mesh.maximum[axis] =
                std::max(mesh.maximum[axis], vertex.position[axis]);
        }
    }

    const std::uint32_t face_count = positiveCount(
        reader, "face count", std::numeric_limits<std::uint32_t>::max() / 3U);
    mesh.indices.resize(static_cast<std::size_t>(face_count) * 3U);
    for (std::uint32_t& index : mesh.indices)
    {
        index = reader.uint32("triangle index");
        if (index >= vertex_count)
            reader.fail("triangle index references a missing vertex");
    }

    const std::uint32_t material_count = positiveCount(
        reader, "material group count", face_count);
    mesh.materialGroups.reserve(material_count);
    std::vector<std::uint8_t> face_coverage(face_count, 0);
    for (std::uint32_t group_index = 0; group_index < material_count;
         ++group_index)
    {
        R3DMaterialGroup group;
        group.materialId = reader.int32("material id");
        const std::int32_t first_face = reader.int32("material first face");
        const std::int32_t group_faces = reader.int32("material face count");
        if (first_face < 0 || group_faces <= 0 ||
            static_cast<std::uint32_t>(first_face) > face_count ||
            static_cast<std::uint32_t>(group_faces) >
                face_count - static_cast<std::uint32_t>(first_face))
        {
            reader.fail("material group has an invalid face range");
        }
        group.firstIndex = static_cast<std::uint32_t>(first_face) * 3U;
        group.indexCount = static_cast<std::uint32_t>(group_faces) * 3U;
        for (std::uint32_t face = static_cast<std::uint32_t>(first_face);
             face < static_cast<std::uint32_t>(first_face + group_faces);
             ++face)
        {
            if (face_coverage[face] != 0)
                reader.fail("material face ranges overlap");
            face_coverage[face] = 1;
        }
        mesh.materialGroups.push_back(group);
    }
    if (std::find(face_coverage.begin(), face_coverage.end(), 0) !=
        face_coverage.end())
    {
        reader.fail("material groups do not cover every face");
    }
    reader.finish();

    std::sort(mesh.materialGroups.begin(), mesh.materialGroups.end(),
              [](const R3DMaterialGroup& left,
                 const R3DMaterialGroup& right) {
                  return left.materialId < right.materialId;
              });
    return mesh;
}

R3DMeshAsset loadR3DMeshAsset(const ResourceFileSystem& resources,
                              std::string_view virtualPath)
{
    return decodeR3DMeshAsset(resources.readBinary(virtualPath), virtualPath);
}

} // namespace r3d::resource
