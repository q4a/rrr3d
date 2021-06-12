#include "stdafx.h"

#include "res\\D3DXImageFile.h"
#include "graph\\Engine.h"

namespace r3d
{

namespace res
{

graph::RenderDriver* D3DXImageFile::driver;
graph::RenderDriver* D3DXCubeImageFile::driver;




void D3DXImageFile::RegistredFile()
{
	ImageResource::GetResFormats().Add<D3DXImageFile>(".jpg");
	ImageResource::GetResFormats().Add<D3DXImageFile>(".bmp");
	ImageResource::GetResFormats().Add<D3DXImageFile>(".dds");
	ImageResource::GetResFormats().Add<D3DXImageFile>(".tga");
	ImageResource::GetResFormats().Add<D3DXImageFile>(".png");
}

void D3DXImageFile::LoadFromStream(Resource& outData, std::istream& stream)
{
	lsl::BinaryResource tmpBuff;
	tmpBuff.LoadFromStream(stream, "");

	D3DXIMAGE_INFO info;
	IDirect3DTexture9* tmpTex;	
	HRESULT hr = D3DXCreateTextureFromFileInMemoryEx(driver->GetDevice(), tmpBuff.GetData(), tmpBuff.GetSize(), D3DX_FROM_FILE, D3DX_FROM_FILE, 1, 0, D3DFMT_UNKNOWN, D3DPOOL_SCRATCH, D3DX_DEFAULT, D3DX_DEFAULT, 0, &info, 0, &tmpTex);
	
	LSL_ASSERT(hr != D3DERR_NOTAVAILABLE);
	LSL_ASSERT(hr != D3DERR_OUTOFVIDEOMEMORY);
	LSL_ASSERT(hr != D3DERR_INVALIDCALL);
	LSL_ASSERT(hr != D3DXERR_INVALIDDATA);
	LSL_ASSERT(hr != E_OUTOFMEMORY);

	outData.SetWidth(info.Width);
	outData.SetHeight(info.Height);
	outData.SetFormat(info.Format);

	D3DLOCKED_RECT data;
	tmpTex->LockRect(0, &data, 0, D3DLOCK_READONLY);

	outData.Init();

	switch (info.Format)
	{
	case D3DFMT_DXT1:
	case D3DFMT_DXT2:
	case D3DFMT_DXT3:
	case D3DFMT_DXT4:
	case D3DFMT_DXT5:
	{
		unsigned rowSize = (info.Width + 3) / 4 * outData.GetPixelSize();
		CopyPitchData(outData.GetData(), rowSize, static_cast<char*>(data.pBits), data.Pitch, rowSize, (info.Height + 3) / 4);
		break;
	}

	default:
	{
		unsigned rowSize = info.Width * outData.GetPixelSize();
		CopyPitchData(outData.GetData(), rowSize, static_cast<char*>(data.pBits), data.Pitch, rowSize, info.Height);
		break;
	}
	}

	tmpTex->UnlockRect(0);
	tmpTex->Release();

	outData.Update();
}

void D3DXImageFile::SaveToStream(const Resource& inData, std::ostream& stream)
{
}




void D3DXCubeImageFile::RegistredFile()
{
	CubeImageResource::GetResFormats().Add<D3DXCubeImageFile>(".dds");
}

void D3DXCubeImageFile::LoadFromStream(Resource& outData, std::istream& stream)
{
	lsl::BinaryResource tmpBuff;
	tmpBuff.LoadFromStream(stream, "");

	D3DXIMAGE_INFO info;
	IDirect3DCubeTexture9* cubeTex;	
	HRESULT hr = D3DXCreateCubeTextureFromFileInMemoryEx(driver->GetDevice(), tmpBuff.GetData(), tmpBuff.GetSize(), D3DX_FROM_FILE, 1, 0, D3DFMT_UNKNOWN, D3DPOOL_SCRATCH, D3DX_DEFAULT, D3DX_DEFAULT, 0, &info, 0, &cubeTex);

	LSL_ASSERT(hr != D3DERR_NOTAVAILABLE);
	LSL_ASSERT(hr != D3DERR_OUTOFVIDEOMEMORY);
	LSL_ASSERT(hr != D3DERR_INVALIDCALL);
	LSL_ASSERT(hr != D3DXERR_INVALIDDATA);
	LSL_ASSERT(hr != E_OUTOFMEMORY);

	outData.SetWidth(info.Width * 6);
	outData.SetHeight(info.Height);
	outData.SetFormat(info.Format);
	unsigned pixSize = outData.GetPixelSize();
	outData.Init();

	int numFace = 0;
	for (D3DCUBEMAP_FACES i = D3DCUBEMAP_FACE_POSITIVE_X; i != D3DCUBEMAP_FACE_FORCE_DWORD; ++i, ++numFace)
	{
		D3DLOCKED_RECT data;
		cubeTex->LockRect(i, 0, &data, 0, D3DLOCK_READONLY);

		switch (info.Format)
		{
		case D3DFMT_DXT1:
		case D3DFMT_DXT2:
		case D3DFMT_DXT3:
		case D3DFMT_DXT4:
		case D3DFMT_DXT5:
			{
				int width = (outData.GetWidth()/6 + 3) / 4;
				int height = (outData.GetHeight() + 3) / 4;
				int rowSize = width * outData.GetPixelSize();
				CopyPitchData(outData.GetData() + numFace * height * width * outData.GetPixelSize(), rowSize, static_cast<char*>(data.pBits), data.Pitch, rowSize, height);
				break;
			}

		default:
			{
				int rowSize = outData.GetWidth()/6 * outData.GetPixelSize();
				CopyPitchData(outData.GetData() + numFace * outData.GetHeight() * outData.GetWidth()/6 * outData.GetPixelSize(), rowSize, static_cast<char*>(data.pBits), data.Pitch, rowSize, outData.GetHeight());
				break;
			}
		}

		cubeTex->UnlockRect(i, 0);
	}

	cubeTex->Release();
	outData.Update();
}

void D3DXCubeImageFile::SaveToStream(const Resource& inData, std::ostream& stream)
{
}

}

}