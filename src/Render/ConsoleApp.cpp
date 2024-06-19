#include "framework.h"

struct GCTest : GCSHADERCB {
	DirectX::XMFLOAT4X4 world; // Matrice du monde
	DirectX::XMFLOAT4 color;
};

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE prevInstance, PSTR cmdLine, int showCmd)
{

	GCGraphicsProfiler& profiler = GCGraphicsProfiler::GetInstance();

	profiler.InitializeConsole();

	Window* window = new Window(hInstance);
	window->Initialize();

	GCGraphics* graphics = new GCGraphics();
	graphics->Initialize(window, 1920, 1064);

	graphics->GetPrimitiveFactory()->Initialize();
	//graphics->GetModelParserFactory()->Initialize();

	// Geometry (Resource)
	GCGeometry* geo = graphics->GetPrimitiveFactory()->BuildGeometryColor(L"cube", DirectX::XMFLOAT4(DirectX::Colors::Red));
	DirectX::XMFLOAT4 color = DirectX::XMFLOAT4(1.0f, 0.0f, 0.0f, 0.2f); // Rouge (1.0f, 0.0f, 0.0f) avec alpha 0.5 (50% d'opacité)
	GCGeometry* geo1 = graphics->GetPrimitiveFactory()->BuildGeometryColor(L"cube", color);
	GCGeometry* geo2 = graphics->GetModelParserFactory()->BuildModelTexture("../../../src/Render/monkeyUv.obj", obj);

	GCShader* shader1 = graphics->CreateShaderColor();

	std::string shaderFilePath = "../../../src/Render/Shaders/customTest.hlsl";
	std::string csoDestinationPath = "../../../src/Render/CsoCompiled/custom";
	//GCShader* shader2 = graphics->CreateShaderCustom(shaderFilePath, csoDestinationPath, STEnum::texture);
	GCShader* shader2 = graphics->CreateShaderTexture();

	///// Create Render Resources
	graphics->GetRender()->ResetCommandList(); // Reset Command List Before Resources Creation


	shader1->Load();
	shader2->Load();


	// Mesh
	GCMesh* mesh = graphics->CreateMesh(geo);
	GCMesh* mesh1 = graphics->CreateMesh(geo1);
	GCMesh* mesh2 = graphics->CreateMesh(geo2);
	



	std::string texturePath = "../../../src/Render/Textures/texture.dds";
	GCTexture* tex1 = graphics->CreateTexture(texturePath);
	GCMaterial* material = graphics->CreateMaterial(shader1, nullptr);
	GCMaterial* material2 = graphics->CreateMaterial(shader2, tex1);

	graphics->GetRender()->CloseCommandList(); // Close and Execute after creation
	graphics->GetRender()->ExecuteCommandList();// Close and Execute after creation

	///// 

	// SET CAMERA 

	DirectX::XMVECTOR cameraPosition = DirectX::XMVectorSet(0.0f, -10.0f, 5.0f, 1.0f);
	DirectX::XMVECTOR targetPosition = DirectX::XMVectorZero();
	DirectX::XMVECTOR upVector = DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);


	DirectX::XMMATRIX projectionMatrix = DirectX::XMMatrixPerspectiveFovLH(0.25f * MathHelper::Pi, window->AspectRatio(), 1.0f, 1000.0f);
	DirectX::XMMATRIX viewMatrix = DirectX::XMMatrixLookAtLH(cameraPosition, targetPosition, upVector);

	// Transposez les matrices
	DirectX::XMMATRIX transposedProjectionMatrix = DirectX::XMMatrixTranspose(projectionMatrix);
	DirectX::XMMATRIX transposedViewMatrix = DirectX::XMMatrixTranspose(viewMatrix);

	// Stockez les matrices transpos�es dans des XMFLOAT4X4
	DirectX::XMFLOAT4X4 storedProjectionMatrix;
	DirectX::XMFLOAT4X4 storedViewMatrix;

	DirectX::XMStoreFloat4x4(&storedProjectionMatrix, transposedProjectionMatrix);
	DirectX::XMStoreFloat4x4(&storedViewMatrix, transposedViewMatrix);

	// SET CAMERA


	DirectX::XMFLOAT4X4 I(
		0.5f, 0.0f, 0.0f, 0.0f,
		0.0f, 0.5f, 0.0f, 0.0f,
		0.0f, 0.0f, 1.0f, 0.0f,
		1.5f, 0.0f, 0.0f, 1.0f);

	DirectX::XMFLOAT4X4 transposedWorld;
	DirectX::XMStoreFloat4x4(&transposedWorld, DirectX::XMMatrixTranspose(DirectX::XMLoadFloat4x4(&I)));

	// Réinitialisation des constant buffers des matériaux
	for (auto& material : graphics->GetMaterials())
	{
		for (auto& cbObject : material->GetObjectCBData())
		{
			cbObject->m_isUsed = false;
		}
	}

	// Préparation du rendu
	graphics->GetRender()->PrepareDraw();

	// Première objet
	GCTest worldData;
	worldData.world = I;
	worldData.color = DirectX::XMFLOAT4(1.0f, 0.0f, 0.0f, 1.0f);




	graphics->UpdateViewProjConstantBuffer(storedProjectionMatrix, storedViewMatrix);
	//graphics->UpdateCustomCBObject<GCTest>(material2, worldData);
	graphics->GetRender()->DrawObjectPixel(mesh2, material2, 800, 800, transposedProjectionMatrix, transposedViewMatrix, graphics);
	//graphics->GetRender()->DrawObject(mesh2, material2);

	//// Deuxième objet
	//graphics->UpdateCustomCBObject<GCTest>(material2, worldData);
	//graphics->UpdateViewProjConstantBuffer(storedProjectionMatrix, storedViewMatrix);
	//graphics->GetRender()->DrawObject(mesh2, material2);

	//// Troisième objet
	//graphics->UpdateCustomCBObject<GCTest>(material2, worldData);
	//graphics->UpdateViewProjConstantBuffer(storedProjectionMatrix, storedViewMatrix);
	//graphics->GetRender()->DrawObject(mesh2, material2);

	// Post traitement du rendu
	graphics->GetRender()->PostDraw();

	// Réinitialisation des compteurs CB pour tous les matériaux
	for (int i = 0; i < graphics->GetMaterials().size(); i++) {
		graphics->GetMaterials()[i]->ResetCBCount();
	}

	// Vérification des CB inutilisés dans les matériaux
	// CHECK FOR REMOVE CB BUFFER IN MATERIALS 
	for (auto& material : graphics->GetMaterials())
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

	// Log de la taille des CB dans material2
	profiler.LogInfo(std::to_string(material2->GetObjectCBData().size()));


	//// DEUXIEME FRAME  

	//// Réinitialisation des constant buffers des matériaux
	//for (auto& material : graphics->GetMaterials())
	//{
	//	for (auto& cbObject : material->GetObjectCBData())
	//	{
	//		cbObject->m_isUsed = false;
	//	}
	//}

	//// Préparation du rendu
	//graphics->GetRender()->PrepareDraw();

	//// Premier objet
	//graphics->UpdateCustomCBObject<GCTest>(material2, worldData);
	//graphics->UpdateViewProjConstantBuffer(storedProjectionMatrix, storedViewMatrix);
	//graphics->GetRender()->DrawObject(mesh2, material2);

	//// Deuxième objet
	//graphics->UpdateCustomCBObject<GCTest>(material2, worldData);
	//graphics->UpdateViewProjConstantBuffer(storedProjectionMatrix, storedViewMatrix);
	//graphics->GetRender()->DrawObject(mesh2, material2);

	//// Post traitement du rendu
	//graphics->GetRender()->PostDraw();

	//for (auto& material : graphics->GetMaterials())
	//{
	//	for (auto& cbObject : material->GetObjectCBData())
	//	{
	//		if (cbObject->m_isUsed)
	//			cbObject->m_framesSinceLastUse = 0;
	//		if (!cbObject->m_isUsed)
	//		{
	//			cbObject->m_framesSinceLastUse++;
	//			if (cbObject->m_framesSinceLastUse > 180)
	//			{
	//				profiler.LogInfo("Constant buffer inutilisé trouvé dans le matériau : ");
	//			}
	//		}
	//	}
	//}

	//// Log de la taille des CB dans material2
	//profiler.LogInfo(std::to_string(material2->GetObjectCBData().size()));


	window->Run(graphics->GetRender());


}

