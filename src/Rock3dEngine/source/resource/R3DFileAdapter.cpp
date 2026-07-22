#include "stdafx.h"

#include "res/R3DFile.h"
#include "resource/R3DMeshAsset.h"

#include <cstring>
#include <iterator>

namespace r3d::res
{

void R3DMeshFile::RegistredFile()
{
    MeshData::GetResFormats().Add<R3DMeshFile>(".r3d");
}

void R3DMeshFile::LoadFromStream(Resource& outData, std::istream& stream)
{
    const std::vector<std::uint8_t> bytes(
        std::istreambuf_iterator<char>(stream),
        std::istreambuf_iterator<char>());
    const resource::R3DMeshAsset decoded =
        resource::decodeR3DMeshAsset(bytes, "legacy R3DMeshFile stream");

    VertexData::Format vertex_format;
    vertex_format.set(VertexData::vtPos3);
    vertex_format.set(VertexData::vtNormal);
    vertex_format.set(VertexData::vtTex0, decoded.hasTexcoords);

    outData.vb.SetFormat(vertex_format);
    outData.vb.SetVertexCount(
        static_cast<unsigned>(decoded.vertices.size()));
    outData.vb.Init();
    for (unsigned index = 0; index < decoded.vertices.size(); ++index)
    {
        const resource::R3DVertex& vertex = decoded.vertices[index];
        const glm::vec3 position(vertex.position[0], vertex.position[1],
                                 vertex.position[2]);
        const glm::vec3 normal(vertex.normal[0], vertex.normal[1],
                               vertex.normal[2]);
        outData.vb.SetVertex(index, VertexData::vtPos3,
            reinterpret_cast<const char*>(&position));
        outData.vb.SetVertex(index, VertexData::vtNormal,
            reinterpret_cast<const char*>(&normal));
        if (decoded.hasTexcoords)
        {
            const glm::vec2 texcoord(vertex.texcoord[0], vertex.texcoord[1]);
            outData.vb.SetVertex(index, VertexData::vtTex0,
                reinterpret_cast<const char*>(&texcoord));
        }
    }
    outData.vb.Update();

    outData.fb.SetIndexFormat(D3DFMT_INDEX32);
    outData.fb.SetFaceCount(
        static_cast<unsigned>(decoded.indices.size() / 3U));
    outData.fb.Init();
    std::memcpy(outData.fb.GetData(), decoded.indices.data(),
                decoded.indices.size() * sizeof(std::uint32_t));
    outData.fb.Update();

    outData.faceGroups.clear();
    for (const resource::R3DMaterialGroup& decoded_group :
         decoded.materialGroups)
    {
        FaceGroup face_group;
        face_group.mathId = decoded_group.materialId;
        face_group.sFace =
            static_cast<int>(decoded_group.firstIndex / 3U);
        face_group.faceCnt =
            static_cast<int>(decoded_group.indexCount / 3U);
        face_group.sVertex = 0;
        face_group.vertexCnt =
            static_cast<int>(decoded.vertices.size());
        outData.faceGroups.push_back(face_group);
    }

    outData.Init();
    outData.Update();
}

void R3DMeshFile::SaveToStream(const Resource&, std::ostream&)
{
    LSL_ASSERT(false);
}

} // namespace r3d::res
