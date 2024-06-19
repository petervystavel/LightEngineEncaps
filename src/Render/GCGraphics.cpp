#include "framework.h"

GCGraphics::GCGraphics()
{
    m_pRender = nullptr;
}

void GCGraphics::Initialize(Window* pWindow,int renderWidth,int renderHeight)
{
    //Initializes Graphics for a window
    m_pRender = new GCRender();
    m_pRender->Initialize(this, pWindow, renderWidth, renderHeight);
    //Creates Primitive and parser instances
    m_pPrimitiveFactory = new GCPrimitiveFactory();
    m_pModelParserFactory = new GCModelParserObj();

    m_pPrimitiveFactory->Initialize();

    // Create One camera
    CreateCBCamera<GCVIEWPROJCB>();
}

void GCGraphics::StartFrame()
{
    for (auto& material : m_vMaterials)
    {
        for (auto& cbObject : material->GetObjectCBData())
        {
            cbObject->m_isUsed = false;
        }
    }

    m_pRender->PrepareDraw();

};
void GCGraphics::EndFrame()
{

    GCGraphicsProfiler& profiler = GCGraphicsProfiler::GetInstance();

    m_pRender->PostDraw();


    for (int i = 0; i < m_vMaterials.size(); i++) {
        m_vMaterials[i]->ResetCBCount();
    }

    // Vérification des CB inutilisés dans les matériaux / CHECK FOR REMOVE CB BUFFER IN MATERIALS 
    for (auto& material : m_vMaterials)
    {
        for (auto& cbObject : material->GetObjectCBData())
        {
            if (cbObject->m_isUsed)
                cbObject->m_framesSinceLastUse = 0;
            if (!cbObject->m_isUsed)
            {
                cbObject->m_framesSinceLastUse++;
                if (cbObject->m_framesSinceLastUse > 180)
                {
                    
                    profiler.LogInfo("Constant buffer inutilisé trouvé dans le matériau : ");
                }
            }
        }
    }
};

GCTexture* GCGraphics::CreateTexture(const std::string& filePath) 
{
    //Creates and initializes a texture using a path
    std::wstring wideFilePath(filePath.begin(), filePath.end());
    GCGraphicsProfiler& profiler = GCGraphicsProfiler::GetInstance();

    if (_waccess(wideFilePath.c_str(), 0) == -1)
    {
        OutputDebugString((L"Texture file not found: " + wideFilePath + L"\n").c_str());
        profiler.LogWarning("Texture file not found: " + filePath);
    }
    else
    {
        OutputDebugString((L"Texture file: " + wideFilePath + L" loaded sucessfully\n").c_str());
        profiler.LogInfo("Texture file : " + filePath + " loaded sucessfully");
    }
	GCTexture* texture = new GCTexture();
	texture->Initialize(filePath, this);
    m_vTextures.push_back(texture);
	return texture;
}

GCShader* GCGraphics::CreateShaderColor() 
{
    GCShader* pShader;
    pShader = new GCShaderColor();

    pShader->Initialize(m_pRender, "../../../src/Render/Shaders/color.hlsl", "../../../src/Render/CsoCompiled/color", STEnum::color);

    m_vShaders.push_back(pShader);
    m_shaderId++;

    return pShader;   
}

GCShader* GCGraphics::CreateShaderTexture() 
{
    GCShader* pShader;
    pShader = new GCShaderTexture();
    pShader->Initialize(m_pRender, "../../../src/Render/Shaders/texture.hlsl", "../../../src/Render/CsoCompiled/texture", STEnum::texture);
    m_vShaders.push_back(pShader);
    m_shaderId++;

    return pShader;
}

// Specify the path, with the name of the shader at the file creation , example : CsoCompiled/texture, texture is the name of the file in Cso Compiled Folder
GCShader* GCGraphics::CreateShaderCustom(std::string& filePath, std::string& compiledShaderDestinationPath, int type) {
    GCShader* pShader;
    pShader = new GCShaderCustom();
    pShader->Initialize(m_pRender, filePath, compiledShaderDestinationPath, type);

    m_vShaders.push_back(pShader);
    m_shaderId++;

    return pShader;
}

GCMesh* GCGraphics::CreateMesh(GCGeometry* pGeometry) 
{
    GCGraphicsProfiler& profiler = GCGraphicsProfiler::GetInstance();
    //Creates mesh using a specific geometry
    if (CheckNull(pGeometry))
    {
        OutputDebugString(L"Mesh Geometry is empty \n");
        profiler.LogWarning("Mesh Geometry is empty");
    }

    GCMesh* pMesh = new GCMesh();
    pMesh->Initialize(m_pRender);
    if (pGeometry->texC.size() == 0)
    {
        pMesh->UploadGeometryDataColor(pGeometry);
    }
    else
    {
        pMesh->UploadGeometryDataTexture(pGeometry);
    }

    m_vMeshes.push_back(pMesh);
    return pMesh;
}



//Creates a material (WIP)
GCMaterial* GCGraphics::CreateMaterial(GCShader* pShader, GCTexture* pTexture) {
    GCMaterial* material = new GCMaterial();
    material->Initialize(pShader, pTexture, m_pRender);
    m_vMaterials.push_back(material);
    m_materialId++;
    return material;
}

std::vector<GCShader*> GCGraphics::GetShaders() 
{
    return m_vShaders;
}

std::vector<GCMaterial*> GCGraphics::GetMaterials() 
{
    return m_vMaterials;
}

std::vector<GCMesh*> GCGraphics::GetMeshes() 
{
    return m_vMeshes;
}

std::vector<GCTexture*> GCGraphics::GetTextures() 
{
    return m_vTextures;
}

void GCGraphics::RemoveShader(GCShader* pShader) 
{
    //Removes Shader both from vector and the shader itself
    auto it = std::find(m_vShaders.begin(), m_vShaders.end(), pShader);

    if (it == m_vShaders.end())
    {
        OutputDebugString(L"Shader not found");
    }

    m_vShaders.erase(it);

    delete pShader;
}

void GCGraphics::RemoveMaterial(GCMaterial* pMaterial) 
{
    //Removes material from the vector then the material itself
    auto it = std::find(m_vMaterials.begin(), m_vMaterials.end(), pMaterial);

    if (it == m_vMaterials.end())
    {
        OutputDebugString(L"Material not found");
    }

    m_vMaterials.erase(it);

    delete pMaterial;
}

void GCGraphics::RemoveMesh(GCMesh* pMesh) 
{
    //Removes Mesh
    auto it = std::find(m_vMeshes.begin(), m_vMeshes.end(), pMesh);

    if (it == m_vMeshes.end())
    {
        OutputDebugString(L"Mesh not found");
    }

    m_vMeshes.erase(it);

    delete pMesh;
}

void GCGraphics::RemoveTexture(GCTexture* pTexture)
{
    //Removes Texture
    auto it = std::find(m_vTextures.begin(), m_vTextures.end(), pTexture);

    if (it == m_vTextures.end())
    {
        OutputDebugString(L"Texture not found");
    }

    m_vTextures.erase(it);

    delete pTexture;
}



















void GCGraphics::UpdateViewProjConstantBuffer(DirectX::XMFLOAT4X4 projectionMatrix, DirectX::XMFLOAT4X4 viewMatrix) {
    GCVIEWPROJCB cameraData;
    cameraData.view = viewMatrix;
    cameraData.proj = projectionMatrix;

    UpdateConstantBuffer(cameraData, m_vpCameraCB[0]);

    m_pRender->m_pCurrentViewProj = m_vpCameraCB[0];
}

//Updates a cb data of a given material using the three matrices world/view/proj
//using a count for now that'll need to be reset after each draw,might be subject to changes in the near future
void GCGraphics::UpdateWorldConstantBuffer(GCMaterial* pMaterial, DirectX::XMFLOAT4X4 worldMatrix) {

    if (pMaterial->GetCount() >= pMaterial->GetObjectCBData().size())
        pMaterial->CreateCBObject<GCWORLDCB>();

    GCWORLDCB worldData;
    worldData.world = worldMatrix;
    // Update 
    pMaterial->UpdateConstantBuffer(worldData, pMaterial->GetObjectCBData()[pMaterial->GetCount()]);

}





//Update Camera and Object Constant Buffer / But They can also send their own structure
void GCGraphics::UpdateConstantBuffer(const GCSHADERCB& objectData, GCShaderUploadBufferBase* uploadBufferInstance)
{
    uploadBufferInstance->CopyData(0, objectData);
}

