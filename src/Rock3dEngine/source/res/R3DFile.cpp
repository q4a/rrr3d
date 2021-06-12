#include "stdafx.h"

#include "res\\R3DFile.h"

namespace r3d
{

namespace res
{

void R3DMeshFile::RegistredFile()
{
	MeshData::GetResFormats().Add<R3DMeshFile>(".r3d");
}




void R3DMeshFile::LoadFromStream(Resource& outData, std::istream& stream)	
{
	int version;
	bool leftCoordSys;
	bool storedTexCoord;	
	int numVerts;
	int numFaces;
	int numMaterials;	

	//header
	stream.read((char*)&version, sizeof(int));
	stream.read((char*)&leftCoordSys, sizeof(bool));
	stream.read((char*)&storedTexCoord, sizeof(bool));
	VertexData::Format vertFormat;
	vertFormat.set(VertexData::vtPos3);
	vertFormat.set(VertexData::vtNormal);
	vertFormat.set(VertexData::vtTex0, storedTexCoord);

	//Vertex buffer
	stream.read((char*)&numVerts, sizeof(int));	
	outData.vb.SetFormat(vertFormat);
	outData.vb.SetVertexCount(numVerts);
	outData.vb.Init();
	stream.read(outData.vb.GetData(), outData.vb.GetSize());
	outData.vb.Update();

	//Index buffer
	stream.read((char*)&numFaces, sizeof(int));
	outData.fb.SetIndexFormat(D3DFMT_INDEX32);
	outData.fb.SetFaceCount(numFaces); 	
	outData.fb.Init();
	stream.read(outData.fb.GetData(), outData.fb.GetSize());
	outData.fb.Update();

	//Materials
	outData.faceGroups.clear();
	stream.read((char*)&numMaterials, sizeof(int));
	for (int i = 0; i < numMaterials; ++i)
	{
		FaceGroup faceGroup;
		stream.read((char*)&faceGroup.mathId, sizeof(int));
		stream.read((char*)&faceGroup.sFace, sizeof(int));
		stream.read((char*)&faceGroup.faceCnt, sizeof(int));
		faceGroup.sVertex = 0;
		faceGroup.vertexCnt = numVerts;
		outData.faceGroups.push_back(faceGroup);
	}
	//sort face groups by mathId
	struct Pred
	{
		bool operator()(const FaceGroup& gr1, const FaceGroup& gr2)
		{
			return gr1.mathId < gr2.mathId;
		}
	};
	std::sort(outData.faceGroups.begin(), outData.faceGroups.end(), Pred());

	//
	outData.Init();
	outData.Update();

	if (stream.fail())
		throw lsl::Error("void R3DMeshFile::LoadFromStream(Resource& outData, std::istream& stream) failed");

	//Чтобы бит переключился в eof нужна неудачная попытка извлечь байт
	stream.get();

	if (!stream.eof())
		throw lsl::Error("void R3DMeshFile::LoadFromStream(Resource& outData, std::istream& stream) eof failed");
}

void R3DMeshFile::SaveToStream(const Resource& inData, std::ostream& stream)
{
	LSL_ASSERT(false);
}

}

}