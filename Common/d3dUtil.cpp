//***************************************************************************************
// d3dUtil.cpp by Frank Luna (C) 2011 All Rights Reserved.
//***************************************************************************************

#include "d3dUtil.h"
#include "WICTextureLoader11.h"

#ifndef SAFE_RELEASE
#define SAFE_RELEASE(x) \
   if(x != NULL)        \
   {                    \
      x->Release();     \
      x = NULL;         \
   }
#endif

using namespace DirectX;

ID3D11ShaderResourceView* d3dHelper::CreateRandomTexture1DSRV(ID3D11Device* device)
{
	// 
	// Create the random data.
	//
	XMFLOAT4 randomValues[1024];

	for(int i = 0; i < 1024; ++i)
	{
		randomValues[i].x = MathHelper::RandF(-1.0f, 1.0f);
		randomValues[i].y = MathHelper::RandF(-1.0f, 1.0f);
		randomValues[i].z = MathHelper::RandF(-1.0f, 1.0f);
		randomValues[i].w = MathHelper::RandF(-1.0f, 1.0f);
	}

    D3D11_SUBRESOURCE_DATA initData;
    initData.pSysMem = randomValues;
	initData.SysMemPitch = 1024*sizeof(XMFLOAT4);
    initData.SysMemSlicePitch = 0;

	//
	// Create the texture.
	//
    D3D11_TEXTURE1D_DESC texDesc;
    texDesc.Width = 1024;
    texDesc.MipLevels = 1;
    texDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
    texDesc.Usage = D3D11_USAGE_IMMUTABLE;
    texDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    texDesc.CPUAccessFlags = 0;
    texDesc.MiscFlags = 0;
    texDesc.ArraySize = 1;

	ID3D11Texture1D* randomTex = 0;
    HR(device->CreateTexture1D(&texDesc, &initData, &randomTex));

	//
	// Create the resource view.
	//
    D3D11_SHADER_RESOURCE_VIEW_DESC viewDesc;
	viewDesc.Format = texDesc.Format;
    viewDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE1D;
    viewDesc.Texture1D.MipLevels = texDesc.MipLevels;
	viewDesc.Texture1D.MostDetailedMip = 0;
	
	ID3D11ShaderResourceView* randomTexSRV = 0;
    HR(device->CreateShaderResourceView(randomTex, &viewDesc, &randomTexSRV));

	ReleaseCOM(randomTex);

	return randomTexSRV;
}



HRESULT d3dHelper::CreateTexture2DArrayFromFile(ID3D11Device* d3dDevice, ID3D11DeviceContext* d3dDeviceContext, const std::vector<std::wstring>& fileNames, ID3D11Texture2D** textureArray, ID3D11ShaderResourceView** textureArrayView, bool generateMips /*= false*/)
{
	// 检查设备、文件名数组是否非空
	if (!d3dDevice || fileNames.empty())
		return E_INVALIDARG;

	HRESULT hr;
	UINT arraySize = (UINT)fileNames.size();

	// ******************
	// 读取第一个纹理
	//
	ID3D11Texture2D* pTexture;
	D3D11_TEXTURE2D_DESC texDesc;

	hr = CreateDDSTextureFromFileEx(d3dDevice,
		fileNames[0].c_str(), 0, D3D11_USAGE_STAGING, 0,
		D3D11_CPU_ACCESS_WRITE | D3D11_CPU_ACCESS_READ,
		0, false, reinterpret_cast<ID3D11Resource**>(&pTexture), nullptr);
	if (FAILED(hr))
	{
		hr = CreateWICTextureFromFileEx(d3dDevice,
			fileNames[0].c_str(), 0, D3D11_USAGE_STAGING, 0,
			D3D11_CPU_ACCESS_WRITE | D3D11_CPU_ACCESS_READ,
			0, false, reinterpret_cast<ID3D11Resource**>(&pTexture), nullptr);
	}

	if (FAILED(hr))
		return hr;




	// 读取创建好的纹理信息
	pTexture->GetDesc(&texDesc);

	// ******************
	// 创建纹理数组
	//
	D3D11_TEXTURE2D_DESC texArrayDesc;
	texArrayDesc.Width = texDesc.Width;
	texArrayDesc.Height = texDesc.Height;
	texArrayDesc.MipLevels = generateMips ? 0 : texDesc.MipLevels;
	texArrayDesc.ArraySize = arraySize;
	texArrayDesc.Format = texDesc.Format;
	texArrayDesc.SampleDesc.Count = 1;		// 不能使用多重采样
	texArrayDesc.SampleDesc.Quality = 0;
	texArrayDesc.Usage = D3D11_USAGE_DEFAULT;
	texArrayDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE | (generateMips ? D3D11_BIND_RENDER_TARGET : 0);
	texArrayDesc.CPUAccessFlags = 0;
	texArrayDesc.MiscFlags = (generateMips ? D3D11_RESOURCE_MISC_GENERATE_MIPS : 0);

	ID3D11Texture2D* pTexArray = nullptr;
	HR(d3dDevice->CreateTexture2D(&texArrayDesc, nullptr, &pTexArray));


	// 获取实际创建的纹理数组信息
	pTexArray->GetDesc(&texArrayDesc);
	UINT updateMipLevels = generateMips ? 1 : texArrayDesc.MipLevels;

	// 写入到纹理数组第一个元素
	D3D11_MAPPED_SUBRESOURCE mappedTex2D;
	for (UINT i = 0; i < updateMipLevels; ++i)
	{
		d3dDeviceContext->Map(pTexture, i, D3D11_MAP_READ, 0, &mappedTex2D);
		d3dDeviceContext->UpdateSubresource(pTexArray, i, nullptr,
			mappedTex2D.pData, mappedTex2D.RowPitch, mappedTex2D.DepthPitch);
		d3dDeviceContext->Unmap(pTexture, i);
	}
	SAFE_RELEASE(pTexture);

	// ******************
	// 读取剩余的纹理并加载入纹理数组
	//
	D3D11_TEXTURE2D_DESC currTexDesc;
	for (UINT i = 1; i < texArrayDesc.ArraySize; ++i)
	{
		hr = CreateDDSTextureFromFileEx(d3dDevice,
			fileNames[0].c_str(), 0, D3D11_USAGE_STAGING, 0,
			D3D11_CPU_ACCESS_WRITE | D3D11_CPU_ACCESS_READ,
			0, false, reinterpret_cast<ID3D11Resource**>(&pTexture), nullptr);
		if (FAILED(hr))
		{
			hr = CreateWICTextureFromFileEx(d3dDevice,
				fileNames[0].c_str(), 0, D3D11_USAGE_STAGING, 0,
				D3D11_CPU_ACCESS_WRITE | D3D11_CPU_ACCESS_READ,
				0, WIC_LOADER_DEFAULT, reinterpret_cast<ID3D11Resource**>(&pTexture), nullptr);
		}

		if (FAILED(hr))
		{
			SAFE_RELEASE(pTexArray);
			return hr;
		}

		pTexture->GetDesc(&currTexDesc);
		// 需要检验所有纹理的mipLevels，宽度和高度，数据格式是否一致，
		// 若存在数据格式不一致的情况，请使用dxtex.exe(DirectX Texture Tool)
		// 将所有的图片转成一致的数据格式
		if (currTexDesc.MipLevels != texDesc.MipLevels || currTexDesc.Width != texDesc.Width ||
			currTexDesc.Height != texDesc.Height || currTexDesc.Format != texDesc.Format)
		{
			SAFE_RELEASE(pTexArray);
			SAFE_RELEASE(pTexture);
			return E_FAIL;
		}
		// 写入到纹理数组的对应元素
		for (UINT j = 0; j < updateMipLevels; ++j)
		{
			// 允许映射索引i纹理中，索引j的mipmap等级的2D纹理
			d3dDeviceContext->Map(pTexture, j, D3D11_MAP_READ, 0, &mappedTex2D);
			d3dDeviceContext->UpdateSubresource(pTexArray,
				D3D11CalcSubresource(j, i, texArrayDesc.MipLevels),	// i * mipLevel + j
				nullptr, mappedTex2D.pData, mappedTex2D.RowPitch, mappedTex2D.DepthPitch);
			// 停止映射
			d3dDeviceContext->Unmap(pTexture, j);
		}
		SAFE_RELEASE(pTexture);
	}

	// ******************
	// 必要时创建纹理数组的SRV
	//
	if (generateMips || textureArrayView)
	{
		D3D11_SHADER_RESOURCE_VIEW_DESC viewDesc;
		viewDesc.Format = texArrayDesc.Format;
		viewDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DARRAY;
		viewDesc.Texture2DArray.MostDetailedMip = 0;
		viewDesc.Texture2DArray.MipLevels = -1;
		viewDesc.Texture2DArray.FirstArraySlice = 0;
		viewDesc.Texture2DArray.ArraySize = arraySize;

		ID3D11ShaderResourceView* pTexArraySRV;
		hr = d3dDevice->CreateShaderResourceView(pTexArray, &viewDesc, &pTexArraySRV);
		if (FAILED(hr))
		{
			SAFE_RELEASE(pTexArray);
			return hr;
		}

		// 生成mipmaps
		if (generateMips)
		{
			d3dDeviceContext->GenerateMips(pTexArraySRV);
		}

		if (textureArrayView)
			*textureArrayView = pTexArraySRV;
		else
			SAFE_RELEASE(pTexArraySRV);
	}

	if (textureArray)
		*textureArray = pTexArray;
	else
		SAFE_RELEASE(pTexArray);

	return S_OK;
}

void ExtractFrustumPlanes(XMFLOAT4 planes[6], CXMMATRIX T)
{
	XMFLOAT4X4 M;
	XMStoreFloat4x4(&M, T);

	//
	// Left
	//

	
	planes[0].x = M(0,3) + M(0,0);
	planes[0].y = M(1,3) + M(1,0);
	planes[0].z = M(2,3) + M(2,0);
	planes[0].w = M(3,3) + M(3,0);

	//
	// Right
	//
	planes[1].x = M(0,3) - M(0,0);
	planes[1].y = M(1,3) - M(1,0);
	planes[1].z = M(2,3) - M(2,0);
	planes[1].w = M(3,3) - M(3,0);

	//
	// Bottom
	//
	planes[2].x = M(0,3) + M(0,1);
	planes[2].y = M(1,3) + M(1,1);
	planes[2].z = M(2,3) + M(2,1);
	planes[2].w = M(3,3) + M(3,1);

	//
	// Top
	//
	planes[3].x = M(0,3) - M(0,1);
	planes[3].y = M(1,3) - M(1,1);
	planes[3].z = M(2,3) - M(2,1);
	planes[3].w = M(3,3) - M(3,1);

	//
	// Near
	//
	planes[4].x = M(0,2);
	planes[4].y = M(1,2);
	planes[4].z = M(2,2);
	planes[4].w = M(3,2);

	//
	// Far
	//
	planes[5].x = M(0,3) - M(0,2);
	planes[5].y = M(1,3) - M(1,2);
	planes[5].z = M(2,3) - M(2,2);
	planes[5].w = M(3,3) - M(3,2);

	// Normalize the plane equations.
	for(int i = 0; i < 6; ++i)
	{
		XMVECTOR v = XMPlaneNormalize(XMLoadFloat4(&planes[i]));
		XMStoreFloat4(&planes[i], v);
	}
}