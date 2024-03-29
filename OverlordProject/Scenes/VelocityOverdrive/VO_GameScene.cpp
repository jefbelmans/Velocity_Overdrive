#include "stdafx.h"
#include <numeric>

#include "VO_GameScene.h"
#include "Materials/Shadow/DiffuseMaterial_Shadow.h"
#include "Materials/Shadow/DiffuseMaterial_Shadow_Skinned.h"
#include "Materials/Post/PostMotionBlur.h"
#include "Materials/BasicMaterial_Deferred.h"
#include "Materials/BasicMaterial_Deferred_Skinned.h"

float gSteerVsForwardSpeedData[2 * 8] =
{
	//SteerAmount	to	Forward Speed
	0.0f,		1.f,
	5.f,		0.8f,
	10.f,		0.6f,
	15.f,		0.5f,
	22.5f,		0.32f,
	30.f,		0.24f,
	PX_MAX_F32,		PX_MAX_F32,
	PX_MAX_F32,		PX_MAX_F32
};

void VO_GameScene::Initialize()
{
	// SCENE SETTINGS
	m_SceneContext.settings.drawGrid = false;
	m_SceneContext.settings.drawPhysXDebug = false;
	m_SceneContext.settings.showInfoOverlay = false;
	m_SceneContext.settings.enableOnGUI = true;
	m_SceneContext.useDeferredRendering = true;

	PhysXManager::Get()->SetVehicleScene(this);

	// SET BAKED DIR LIGHT
	m_SceneContext.pLights->SetBakedDirectionalLight({ -100.f ,150.f, -80.f }, { 0.6f, -0.76f, 0.5f });
	
	// POST PROCESSING STACK
	m_pPostMotionBlur = MaterialManager::Get()->CreateMaterial<PostMotionBlur>();

	AddPostProcessingEffect(m_pPostMotionBlur);

	// UI
	InitializeUI();

	// AUDIO
	InitializeSound();

	// TIMER
	auto pTimerGO = AddChild(new GameObject);
	m_pTimer = pTimerGO->AddComponent(new TimerComponent());
	m_pTimer->EnableDrawing(!m_DoShowControls);

	// LIGHTING
	InitializeLighting();

	// Constructs the entire scene with all the GO's
	ConstructScene();

	// CAMERA
	m_pCamera = AddChild(new FollowCamera(m_pChassis, m_pVehicle, m_CameraPitch));
	m_pCamera->GetComponent<CameraComponent>()->SetActive(true);
	m_pCamera->SetSmoothing(m_CameraSmoothing);
	m_pCamera->SetLookAhead(m_CameraLookAhead);
	m_pCamera->SetOffsetDistance(m_CameraDistance);

	m_pFreeCamera = AddChild(new FreeCamera());

	// PARTICLE SYSTEMS
	m_EmitterSettings.velocity = { 0.f, 2.f, 0.f };
	m_EmitterSettings.minSize = { 0.3f, 1.2f };
	m_EmitterSettings.maxSize = { 0.4f, 1.3f };
	m_EmitterSettings.minEnergy = 0.6f;
	m_EmitterSettings.maxEnergy = 1.f;
	m_EmitterSettings.minScale = 1.4f;
	m_EmitterSettings.maxScale = 1.6f;
	m_EmitterSettings.minEmitterRadius = .1f;
	m_EmitterSettings.maxEmitterRadius = .15f;
	m_EmitterSettings.color = { 100.f, 100.f, 100.f, 1.f };

	auto pEmitterGO = new GameObject();
	m_pEmitterRR = pEmitterGO->AddComponent(new ParticleEmitterComponent(L"Textures/Smoke.png", m_EmitterSettings, 150));
	pEmitterGO->GetTransform()->Translate(XMFLOAT3{ -0.75f, 0.1f, -2.f });
	m_pChassis->AddChild(pEmitterGO);

	pEmitterGO = new GameObject();
	m_pEmitterRL = pEmitterGO->AddComponent(new ParticleEmitterComponent(L"Textures/Smoke.png", m_EmitterSettings, 150));
	pEmitterGO->GetTransform()->Translate(XMFLOAT3{ 0.75f, 0.1f, -2.f });
	m_pChassis->AddChild(pEmitterGO);

	//Input
	auto inputAction = InputAction(SteerLeft, InputState::down, VK_LEFT);
	m_SceneContext.pInput->AddInputAction(inputAction);

	inputAction = InputAction(SteerRight, InputState::down, VK_RIGHT);
	m_SceneContext.pInput->AddInputAction(inputAction);

	inputAction = InputAction(Accelerate, InputState::down, VK_UP);
	m_SceneContext.pInput->AddInputAction(inputAction);

	inputAction = InputAction(Deaccelerate, InputState::down, VK_DOWN);
	m_SceneContext.pInput->AddInputAction(inputAction);

	inputAction = InputAction(HandBrake, InputState::down, VK_SPACE, -1, XINPUT_GAMEPAD_X);
	m_SceneContext.pInput->AddInputAction(inputAction);

	inputAction = InputAction(TogglePause, InputState::pressed, VK_ESCAPE, -1, XINPUT_GAMEPAD_START);
	m_SceneContext.pInput->AddInputAction(inputAction);

	inputAction = InputAction(Confirm, InputState::pressed, VK_RETURN, -1, XINPUT_GAMEPAD_A);
	m_SceneContext.pInput->AddInputAction(inputAction);
}

VO_GameScene::~VO_GameScene()
{
	m_pVehicleTelemetryData->free();
	SafeDelete(m_pVehicleInputData);
	PxCloseVehicleSDK();
}

void VO_GameScene::Update()
{
	UpdateInput();
	UpdateLighting();
	UpdateSound();
	UpdateVehicle();
}

void VO_GameScene::Draw()
{
	if(m_IsPaused)
	{
		if(m_SceneContext.pLights->GetUseBakedShadows())
			TextRenderer::Get()->DrawText(m_pFont24, L"RT SHADOWS", { m_SceneContext.windowWidth - 200.f, m_SceneContext.windowHeight - 405.f }, XMFLOAT4{ Colors::Orange });
		else
			TextRenderer::Get()->DrawText(m_pFont24, L"BAKED SHADOWS", { m_SceneContext.windowWidth - 217.f, m_SceneContext.windowHeight - 405.f }, XMFLOAT4{ Colors::Orange });
		
		TextRenderer::Get()->DrawText(m_pFont32, L"RESTART GAME", { m_SceneContext.windowWidth - 230.f, m_SceneContext.windowHeight - 305.f }, XMFLOAT4{ Colors::Orange });
		TextRenderer::Get()->DrawText(m_pFont32, L"BACK TO MENU", { m_SceneContext.windowWidth - 230.f, m_SceneContext.windowHeight - 205.f }, XMFLOAT4{ Colors::Orange });
		TextRenderer::Get()->DrawText(m_pFont32, L"QUIT GAME", { m_SceneContext.windowWidth - 210.f, m_SceneContext.windowHeight - 105.f }, XMFLOAT4{ Colors::Orange });
	}
}

void VO_GameScene::PostDraw()
{
	if(m_DebugShadowMap)
		ShadowMapRenderer::Get()->Debug_DrawDepthSRV({ m_SceneContext.windowWidth - 10.f, 10.f }, { m_ShadowMapScale, m_ShadowMapScale }, { 1.f,0.f });

	if(m_DebugBakedShadowMap)
		ShadowMapRenderer::Get()->Debug_DrawBakedDepthSRV({ m_SceneContext.windowWidth - 10.f, 10.f }, { m_ShadowMapScale, m_ShadowMapScale }, { 1.f,0.f });
}

void VO_GameScene::OnGUI()
{
	// GAME STATS
	if (ImGui::CollapsingHeader("Game Statistics"))
	{
		ImGui::Text("Next Checkpoint: %i", m_NextCheckpoint);
		ImGui::Text("Rumble Strength: %f", m_RumbleStrength);
	}

	// CAMERA SETTINGS
	if(ImGui::CollapsingHeader("Camera Settings"))
	{
		if (ImGui::Button("Switch camera"))
		{
			isFreeCamActive = !isFreeCamActive;
			m_pCamera->GetComponent<CameraComponent>()->SetActive(!isFreeCamActive);
			m_pFreeCamera->GetComponent<CameraComponent>()->SetActive(isFreeCamActive);
			m_pTimer->EnableDrawing(false);
			m_pBannerLap->SetActive(false);
			m_pBannerBest->SetActive(false);
		}

		ImGui::SliderFloat("Pitch", &m_CameraPitch, 0.f, 90.f);
		// m_pCamera->SetPitch(m_CameraPitch);

		ImGui::SliderFloat("Smoothing", &m_CameraSmoothing, 0.f, 1.f);
		m_pCamera->SetSmoothing(m_CameraSmoothing);

		ImGui::SliderFloat("LookAhead", &m_CameraLookAhead, 0.f, 100.f);
		m_pCamera->SetLookAhead(m_CameraLookAhead);

		ImGui::SliderFloat("Offset Distance", &m_CameraDistance, 0.f, 100.f);
		m_pCamera->SetOffsetDistance(m_CameraDistance);
	}

	// CAR TELEMETRY
	if(ImGui::CollapsingHeader("Car Telemetry"))
	{
		/*float xy[2 * PxVehicleGraph::eMAX_NB_SAMPLES];
		PxVec3 color[PxVehicleGraph::eMAX_NB_SAMPLES];
		char title[PxVehicleGraph::eMAX_NB_TITLE_CHARS];
		m_pVehicleTelemetryData->getEngineGraph().computeGraphChannel(PxVehicleDriveGraphChannel::eENGINE_REVS,
			xy, color, title);*/

		auto carPos{ m_pChassis->GetTransform()->GetPosition()};
		ImGui::Text("Position: [%f, %f, %f]",
			carPos.x, carPos.y, carPos.z);

		ImGui::Text("Speed: %f", m_pVehicle->computeForwardSpeed());
		ImGui::Text("Engine Speed: %f", m_pVehicle->mDriveDynData.mEnginespeed);
		ImGui::Text("Current Gear: %i", m_pVehicle->mDriveDynData.getCurrentGear());

		ImGui::Text("Lateral Slip: %f %f %f %f", 
			m_WheelQueryResults[0].lateralSlip,
			m_WheelQueryResults[1].lateralSlip,
			m_WheelQueryResults[2].lateralSlip,
			m_WheelQueryResults[3].lateralSlip);

		ImGui::Text("Longitudinal Slip: % f % f % f % f",
			m_WheelQueryResults[0].longitudinalSlip,
			m_WheelQueryResults[1].longitudinalSlip,
			m_WheelQueryResults[2].longitudinalSlip,
			m_WheelQueryResults[3].longitudinalSlip);
	}

	// SHADOWMAP
	if(ImGui::CollapsingHeader("Shadows"))
	{
		ImGui::Checkbox("Use Baked Shadows", &m_UseBakedShadows);
		m_SceneContext.pLights->SetUseBakedShadows(m_UseBakedShadows);

		ImGui::Checkbox("Debug Real Time Shadow Map", &m_DebugShadowMap);
		ImGui::Checkbox("Debug Baked Shadow Map", &m_DebugBakedShadowMap);
		ImGui::SliderFloat("Shadow Map Scale", &m_ShadowMapScale, 0.01f, 1.f);
		if (ImGui::Button("Bake Shadows"))
		{
			m_SceneContext.pLights->SetBakeShadows(true);
		}
	}

	// POST PROCESSING
	if (ImGui::CollapsingHeader("Post Processing"))
	{
		bool isEnabled = m_pPostMotionBlur->IsEnabled();
		ImGui::Checkbox("Motion Blur PP", &isEnabled);
		m_pPostMotionBlur->SetIsEnabled(isEnabled);
	}
}

void VO_GameScene::OnSceneActivated()
{
	m_pEngineChannel->setPaused(false);

	// RESET VEHICLE
	RestartGame();

	ShadowMapRenderer::Get()->SetViewWidthHeight(155.f, 155.f);
}

void VO_GameScene::OnSceneDeactivated()
{
	m_pVehicle->mDriveDynData.forceGearChange(1);
	m_pEngineChannel->setPaused(true);
	if(m_IsPaused)
		TogglePauseMenu();
}

void VO_GameScene::InitializeUI()
{
	// Font
	m_pFont32 = ContentManager::Load<SpriteFont>(L"SpriteFonts/LemonMilk_32.fnt");
	m_pFont24 = ContentManager::Load<SpriteFont>(L"SpriteFonts/LemonMilk_24.fnt");

	// Controller Layout
	m_pControllerLayout = AddChild(new GameObject);
	m_pControllerLayout->AddComponent(new SpriteComponent(L"Textures/UI/ControllerLayout_Colored.png", { .5f, .5f }));
	m_pControllerLayout->GetTransform()->Scale(.8f, .8f, 1.f);
	m_pControllerLayout->GetTransform()->Translate(m_SceneContext.windowWidth * 0.5f, m_SceneContext.windowHeight * 0.5f, 0.1f);

	// Pause Panel
	m_pPausePanel = AddChild(new GameObject);
	m_pPausePanel->SetActive(false);
	m_pPausePanel->AddComponent(new SpriteComponent(L"Textures/UI/Panel.png", { 1.f, 1.f }));
	m_pPausePanel->GetTransform()->Scale(2.6f, 4.5f, 1.f);
	m_pPausePanel->GetTransform()->Translate(m_SceneContext.windowWidth - 20.f, m_SceneContext.windowHeight - 20.f, 0.1f);

#pragma region Buttons
	auto toggleBakedShadows = [&]()
	{
		m_SceneContext.pLights->SetUseBakedShadows(!m_SceneContext.pLights->GetUseBakedShadows());
	};

	// Baked Shadows Button
	m_pBakedShadowsButton = AddChild(new GameObject);
	m_pBakedShadowsButton->SetActive(false);
	auto pButton = m_pBakedShadowsButton->AddComponent(new ButtonComponent(toggleBakedShadows, L"Textures/UI/ButtonBase.png", { m_SceneContext.windowWidth - 240.f, m_SceneContext.windowHeight - 430.f }, { 3.f, 2.7f }));
	pButton->SetSelectedAssetPath(L"Textures/UI/ButtonSelected.png");
	pButton->SetPressedAssetPath(L"Textures/UI/ButtonPressed.png");

	// Restart button
	m_pRestartButton = AddChild(new GameObject);
	m_pRestartButton->SetActive(false);
	pButton = m_pRestartButton->AddComponent(new ButtonComponent(std::bind(&VO_GameScene::RestartGame, this), L"Textures/UI/ButtonBase.png", { m_SceneContext.windowWidth - 240.f, m_SceneContext.windowHeight - 330.f }, { 3.f, 2.7f }));
	pButton->SetSelectedAssetPath(L"Textures/UI/ButtonSelected.png");
	pButton->SetPressedAssetPath(L"Textures/UI/ButtonPressed.png");

	// Back Button
	m_pBackButton = AddChild(new GameObject);
	m_pBackButton->SetActive(false);
	pButton = m_pBackButton->AddComponent(new ButtonComponent(std::bind(&VO_GameScene::LoadMainMenu, this), L"Textures/UI/ButtonBase.png", { m_SceneContext.windowWidth - 240.f, m_SceneContext.windowHeight - 230.f }, { 3.f, 2.7f }));
	pButton->SetSelectedAssetPath(L"Textures/UI/ButtonSelected.png");
	pButton->SetPressedAssetPath(L"Textures/UI/ButtonPressed.png");

	// Quit Button
	m_pQuitButton = AddChild(new GameObject);
	m_pQuitButton->SetActive(false);
	pButton = m_pQuitButton->AddComponent(new ButtonComponent(std::bind(&VO_GameScene::QuitGame, this), L"Textures/UI/ButtonBase.png", { m_SceneContext.windowWidth - 240.f, m_SceneContext.windowHeight - 130.f }, { 3.f, 2.7f }));
	pButton->SetSelectedAssetPath(L"Textures/UI/ButtonSelected.png");
	pButton->SetPressedAssetPath(L"Textures/UI/ButtonPressed.png");
#pragma endregion

	// Banner Lap
	m_pBannerLap = AddChild(new GameObject);
	m_pBannerLap->SetActive(false);
	m_pBannerLap->AddComponent(new SpriteComponent(L"Textures/UI/BannerSpecial.png", { 1.f, 0.f }));
	m_pBannerLap->GetTransform()->Scale(3.f, 3.f, 1.f);
	m_pBannerLap->GetTransform()->Translate(m_SceneContext.windowWidth - 30.f, 20.f, 0.1f);

	// Banner Best
	m_pBannerBest = AddChild(new GameObject);
	m_pBannerBest->SetActive(false);
	m_pBannerBest->AddComponent(new SpriteComponent(L"Textures/UI/BannerSpecial.png", { 1.f, 0.f }));
	m_pBannerBest->GetTransform()->Scale(3.f, 3.f, 1.f);
	m_pBannerBest->GetTransform()->Translate(m_SceneContext.windowWidth - 30.f, 100.f, 0.1f);
}

void VO_GameScene::TogglePauseMenu()
{
	m_IsPaused = !m_IsPaused;

	m_pPausePanel->SetActive(m_IsPaused);
	m_pBackButton->SetActive(m_IsPaused);
	m_pRestartButton->SetActive(m_IsPaused);
	m_pQuitButton->SetActive(m_IsPaused);
	m_pBakedShadowsButton->SetActive(m_IsPaused);

	if (m_IsPaused)
	{
		// Register buttons
		EventSystem::Get()->RegisterInteractable(m_pRestartButton->GetComponent<ButtonComponent>());
		EventSystem::Get()->RegisterInteractable(m_pBackButton->GetComponent<ButtonComponent>());
		EventSystem::Get()->RegisterInteractable(m_pQuitButton->GetComponent<ButtonComponent>());
		EventSystem::Get()->RegisterInteractable(m_pBakedShadowsButton->GetComponent<ButtonComponent>());

	}
	else
	{
		// Unregister buttons
		EventSystem::Get()->UnregisterInteractable(m_pBackButton->GetComponent<ButtonComponent>());
		EventSystem::Get()->UnregisterInteractable(m_pRestartButton->GetComponent<ButtonComponent>());
		EventSystem::Get()->UnregisterInteractable(m_pQuitButton->GetComponent<ButtonComponent>());
		EventSystem::Get()->UnregisterInteractable(m_pBakedShadowsButton->GetComponent<ButtonComponent>());
	}
}

void VO_GameScene::RestartGame()
{
	// RESET VEHICLE
	m_pChassis->GetTransform()->Translate(XMFLOAT3{ -48.f, 0.2f, -100.f });
	m_pChassis->GetTransform()->Rotate(0.f, -90.f, 0.f);

	m_pTimer->Pause();
	m_pTimer->Reset();
	m_pTimer->EnableDrawing(false);

	m_NextCheckpoint = 0;
	if (m_IsPaused)
		TogglePauseMenu();

	// SHOW CONTROLLER LAYOUT
	m_DoShowControls = true;

	m_pControllerLayout->SetActive(true);
	m_pBannerBest->SetActive(!m_DoShowControls);
	m_pBannerLap->SetActive(!m_DoShowControls);

	// DESELECT INTERACTABLE
	EventSystem::Get()->SetSelectedInteractable(nullptr);
}

void VO_GameScene::LoadMainMenu()
{
	SceneManager::Get()->PreviousScene();
}

void VO_GameScene::QuitGame()
{
	PostQuitMessage(0);
}

void VO_GameScene::InitializeSound()
{
	auto pFmodSystem = SoundManager::Get()->GetSystem();
	auto result = pFmodSystem->createStream("Resources/Sounds/Engine.wav", FMOD_LOOP_NORMAL, nullptr, &m_pEngineSound);
	if (result != FMOD_OK)
	{
		std::wstringstream strstr;
		strstr << L"FMOD error! \n[" << result << L"] " << FMOD_ErrorString(result) << std::endl;
		Logger::LogError(strstr.str());
	}

	result = pFmodSystem->playSound(m_pEngineSound, nullptr, true, &m_pEngineChannel);
	if (result != FMOD_OK)
	{
		std::wstringstream strstr;
		strstr << L"FMOD error! \n[" << result << L"] " << FMOD_ErrorString(result) << std::endl;
		Logger::LogError(strstr.str());
	}
}

void VO_GameScene::UpdateSound()
{
	const float pitch = MathHelper::remap(m_pVehicle->mDriveDynData.mEnginespeed, 0.f, 1500.f, 0.6f, 1.f);
	m_pEngineChannel->setPitch(pitch);
}

void VO_GameScene::InitializeLighting()
{
	//Point Light
	Light light = {};
	light.isEnabled = true;
	light.position = { 0.f,10.f,0.f,1.0f };
	light.direction = { 0.f,0.f,1.f,0.f };
	light.color = { 1.f, 0.1f, 0.1f, 1.f };
	light.intensity = 1.f;
	light.range = 20.0f;
	light.type = LightType::Point;
	// m_SceneContext.pLights->AddLight(light);
}

void VO_GameScene::UpdateLighting()
{

}

void VO_GameScene::ConstructScene()
{
	// PHYSX
	auto pDefaultMaterial = PhysXManager::Get()->GetPhysics()->createMaterial(0.5f, 0.5f, 0.f);
	auto pConeMaterial = PhysXManager::Get()->GetPhysics()->createMaterial(0.7f, 0.7f, 0.4f);

	// MATERIALS
	auto pVehicleMat = MaterialManager::Get()->CreateMaterial<BasicMaterial_Deferred>();
	pVehicleMat->SetDiffuseMap(L"Textures/F1_Car.png");
	pVehicleMat->WriteToMask(false);

	auto pTrackMat = MaterialManager::Get()->CreateMaterial<BasicMaterial_Deferred>();
	pTrackMat->SetDiffuseMap(L"Textures/F1_Track.png");

	auto pBuildingMat = MaterialManager::Get()->CreateMaterial<BasicMaterial_Deferred>();
	pBuildingMat->SetDiffuseMap(L"Textures/F1_Building.png");

	auto pGroundMat = MaterialManager::Get()->CreateMaterial<BasicMaterial_Deferred>();
	pGroundMat->SetDiffuseMap(L"Textures/F1_Ground.png");

	// CROWD
	for (int i = 0; i < 8; i++)
	{
		const auto pSkinnedMaterial = MaterialManager::Get()->CreateMaterial<BasicMaterial_Deferred_Skinned>();
		pSkinnedMaterial->SetDiffuseMap(L"Textures/Character_Diffuse.png");

		const auto pObject = AddChild(new GameObject);
		const auto pModel = pObject->AddComponent(new ModelComponent(L"Meshes/Character.ovm"));
		pModel->SetMaterial(pSkinnedMaterial);

		pObject->GetTransform()->Scale(0.02f);
		pObject->GetTransform()->Translate(-15.5f - i * 2, 1.f, -71.f);

		auto pAnimator = pModel->GetAnimator();
		pAnimator->SetAnimation(rand()%2);
		pAnimator->Play();
	}
	

	// GROUND PLANE
	GameSceneExt::CreatePhysXGroundPlane(*this, pDefaultMaterial);

	// PHYSX DEBUG RENDERING
	GetPhysxProxy()->EnablePhysxDebugRendering(true);
	GetPhysxProxy()->GetPhysxScene()->setVisualizationParameter(PxVisualizationParameter::eBODY_LIN_VELOCITY, 1.f);

	// TRACK
	m_pTrack = new GameObject(true);
	m_pTrack->AddComponent(new ModelComponent(L"Meshes/F1_Track.ovm"))->SetMaterial(pTrackMat);
	m_pTrack->GetTransform()->Translate(0.f, -0.1f, 0.f);
	AddChild(m_pTrack);

	// FENCE01
	auto go = new GameObject(true);
	go->AddComponent(new ModelComponent(L"Meshes/F1_Fence01.ovm"))->SetMaterial(pTrackMat);

	auto pConvexMesh = ContentManager::Load<PxConvexMesh>(L"Meshes/F1_Fence01.ovpc");
	auto pRb = go->AddComponent(new RigidBodyComponent(true));
	pRb->SetCollisionGroup(CollisionGroup::Group0 | CollisionGroup::Group1);
	pRb->AddCollider(PxConvexMeshGeometry{ pConvexMesh }, *pDefaultMaterial);
	AddChild(go);

	// FENCE02
	go = new GameObject(true);
	go->AddComponent(new ModelComponent(L"Meshes/F1_Fence02.ovm"))->SetMaterial(pTrackMat);

	pConvexMesh = ContentManager::Load<PxConvexMesh>(L"Meshes/F1_Fence02.ovpc");
	pRb = go->AddComponent(new RigidBodyComponent(true));
	pRb->SetCollisionGroup(CollisionGroup::Group0 | CollisionGroup::Group1);
	pRb->AddCollider(PxConvexMeshGeometry{ pConvexMesh }, *pDefaultMaterial);
	AddChild(go);

	// FENCE03
	go = new GameObject(true);
	go->AddComponent(new ModelComponent(L"Meshes/F1_Fence03.ovm"))->SetMaterial(pTrackMat);

	pConvexMesh = ContentManager::Load<PxConvexMesh>(L"Meshes/F1_Fence03.ovpc");
	pRb = go->AddComponent(new RigidBodyComponent(true));
	pRb->SetCollisionGroup(CollisionGroup::Group0 | CollisionGroup::Group1);
	pRb->AddCollider(PxConvexMeshGeometry{ pConvexMesh }, *pDefaultMaterial);
	AddChild(go);

	// FENCE04
	go = new GameObject(true);
	go->AddComponent(new ModelComponent(L"Meshes/F1_Fence04.ovm"))->SetMaterial(pTrackMat);

	pConvexMesh = ContentManager::Load<PxConvexMesh>(L"Meshes/F1_Fence04.ovpc");
	pRb = go->AddComponent(new RigidBodyComponent(true));
	pRb->SetCollisionGroup(CollisionGroup::Group0 | CollisionGroup::Group1);
	pRb->AddCollider(PxConvexMeshGeometry{ pConvexMesh }, *pDefaultMaterial);
	AddChild(go);

	// FENCE05
	go = new GameObject(true);
	go->AddComponent(new ModelComponent(L"Meshes/F1_Fence05.ovm"))->SetMaterial(pTrackMat);

	pConvexMesh = ContentManager::Load<PxConvexMesh>(L"Meshes/F1_Fence05.ovpc");
	pRb = go->AddComponent(new RigidBodyComponent(true));
	pRb->SetCollisionGroup(CollisionGroup::Group0 | CollisionGroup::Group1);
	pRb->AddCollider(PxConvexMeshGeometry{ pConvexMesh }, *pDefaultMaterial);
	AddChild(go);

	// FENCE OUTER
	go = new GameObject(true);
	go->AddComponent(new ModelComponent(L"Meshes/F1_FenceOuter.ovm"))->SetMaterial(pTrackMat);

	auto pTriangleMesh = ContentManager::Load<PxTriangleMesh>(L"Meshes/F1_FenceOuter.ovpt");
	pRb = go->AddComponent(new RigidBodyComponent(true));
	pRb->SetCollisionGroup(CollisionGroup::Group0 | CollisionGroup::Group1);
	pRb->AddCollider(PxTriangleMeshGeometry{ pTriangleMesh }, *pDefaultMaterial);
	AddChild(go);

	// BUILDING01
	go = new GameObject(true);
	go->AddComponent(new ModelComponent(L"Meshes/F1_Building01.ovm"))->SetMaterial(pBuildingMat);

	pConvexMesh = ContentManager::Load<PxConvexMesh>(L"Meshes/F1_Building01.ovpc");
	pRb = go->AddComponent(new RigidBodyComponent(true));
	pRb->SetCollisionGroup(CollisionGroup::Group0 | CollisionGroup::Group1);
	pRb->AddCollider(PxConvexMeshGeometry{ pConvexMesh }, *pDefaultMaterial);
	AddChild(go);

	// BUILDING02
	go = new GameObject(true);
	go->AddComponent(new ModelComponent(L"Meshes/F1_Building02.ovm"))->SetMaterial(pBuildingMat);

	pConvexMesh = ContentManager::Load<PxConvexMesh>(L"Meshes/F1_Building02.ovpc");
	pRb = go->AddComponent(new RigidBodyComponent(true));
	pRb->SetCollisionGroup(CollisionGroup::Group0 | CollisionGroup::Group1);
	pRb->AddCollider(PxConvexMeshGeometry{ pConvexMesh }, *pDefaultMaterial);
	AddChild(go);

	// BUILDING03
	go = new GameObject(true);
	go->AddComponent(new ModelComponent(L"Meshes/F1_Building03.ovm"))->SetMaterial(pBuildingMat);
	AddChild(go);

	// GRANDSTAND01
	go = new GameObject(true);
	go->AddComponent(new ModelComponent(L"Meshes/F1_GrandStand01.ovm"))->SetMaterial(pBuildingMat);
	AddChild(go);

	// GRANDSTANDCANOPY01
	/*go = new GameObject();
	go->AddComponent(new ModelComponent(L"Meshes/F1_GrandStandCanpoy01.ovm"))->SetMaterial(pTrackMat);
	AddChild(go);*/

	// SPOTLIGHTS01
	go = new GameObject(true);
	go->AddComponent(new ModelComponent(L"Meshes/F1_Spotlights01.ovm"))->SetMaterial(pTrackMat);
	AddChild(go);

	// SIGNS01
	go = new GameObject(true);
	go->AddComponent(new ModelComponent(L"Meshes/F1_Signs01.ovm"))->SetMaterial(pTrackMat);
	AddChild(go);

	// GROUND01
	go = new GameObject(true);
	go->AddComponent(new ModelComponent(L"Meshes/F1_Ground01.ovm"))->SetMaterial(pGroundMat);
	AddChild(go);

	// CONES
	go = new GameObject();
	go->AddComponent(new ModelComponent(L"Meshes/F1_Cone01.ovm"))->SetMaterial(pTrackMat);
	go->GetTransform()->Translate(XMFLOAT3{ -30.f, 0.f, 10.f });

	pConvexMesh = ContentManager::Load<PxConvexMesh>(L"Meshes/F1_Cone01.ovpc");
	pRb = go->AddComponent(new RigidBodyComponent(false));
	pRb->AddCollider(PxConvexMeshGeometry{ pConvexMesh }, *pConeMaterial);
	pRb->SetCollisionGroup(CollisionGroup::Group0 | CollisionGroup::Group1);
	AddChild(go);

	// CHASIS
	m_pChassis = new GameObject();
	AddChild(m_pChassis);

	m_pChassis->GetTransform()->Translate(XMFLOAT3{ -48.f, 2.f, -100.f });
	m_pChassis->GetTransform()->Rotate(0.f, -90.f, 0.f);
	m_pChassis->AddComponent(new ModelComponent(L"Meshes/F1_Car.ovm"))->SetMaterial(pVehicleMat);

	m_pChassis->SetTag(L"Player");

	pRb = m_pChassis->AddComponent(new RigidBodyComponent());
	auto pActor = pRb->GetPxRigidActor();

	// INIT VEHICLE SDK
	m_pVehicle = PhysXManager::Get()->InitializeVehicleSDK(pActor);
	m_pVehicleInputData = new PxVehicleDrive4WRawInputData();
	m_SteerVsForwardSpeedTable = PxFixedSizeLookupTable<8>(gSteerVsForwardSpeedData, 6);

	// TODO: Fix user data getting overwritten during vehicle init
	// Quick fix is to restore it here with the RigidbodyComponent, else the trigger callbacks will fail to get the overlapping GameObject
	pActor->userData = pRb;

	// WHEELS
	for (int i = 0; i < 4; i++)
	{
		m_pWheels[i] = new GameObject();
		AddChild(m_pWheels[i]);
		m_pWheels[i]->AddComponent(new ModelComponent(L"Meshes/F1_Wheel.ovm"))->SetMaterial(pVehicleMat);
	}
	SetupTelemetryData();

	// CHECKPOINTS
	for (int i = 0; i < m_CheckpointPositions.size(); i++)
	{
		// CREATE GO
		auto checkpoint = new GameObject();
		checkpoint->GetTransform()->Translate(m_CheckpointPositions[i]);
		checkpoint->GetTransform()->Rotate(m_CheckpointRotations[i]);

		// ADD RIGIDBODY
		pRb = checkpoint->AddComponent(new RigidBodyComponent());
		pRb->SetKinematic(true);

		// ADD COLLIDER
		pRb->AddCollider(PxBoxGeometry{ 14.f, 3.f, 0.5f }, *pDefaultMaterial, true);
		checkpoint->SetOnTriggerCallBack([&](GameObject* pTrigger, GameObject* pOther, PxTriggerAction action) {
			OnTriggerCallback(pTrigger, pOther, action);
			});

		// ADD TO SCENE
		AddChild(checkpoint);
		m_pCheckpoints.emplace_back(checkpoint);
	}
}

void VO_GameScene::SetupTelemetryData()
{
	m_pVehicleTelemetryData = PxVehicleTelemetryData::allocate(4);

	const PxF32 graphSizeX = 0.25f;
	const PxF32 graphSizeY = 0.25f;
	const PxF32 engineGraphPosX = 0.5f;
	const PxF32 engineGraphPosY = 0.5f;
	const PxF32 wheelGraphPosX[4] = { 0.75f,0.25f,0.75f,0.25f };
	const PxF32 wheelGraphPosY[4] = { 0.75f,0.75f,0.25f,0.25f };
	const PxVec3 backgroundColor(255, 255, 255);
	const PxVec3 lineColorHigh(255, 0, 0);
	const PxVec3 lineColorLow(0, 0, 0);
	m_pVehicleTelemetryData->setup
	(graphSizeX, graphSizeY,
		engineGraphPosX, engineGraphPosY,
		wheelGraphPosX, wheelGraphPosY,
		backgroundColor, lineColorHigh, lineColorLow);
}

void VO_GameScene::UpdateInput()
{
	ReleaseAllControls();

	auto input{ m_SceneContext.pInput };

	// PAUSE MENU
	if (input->IsActionTriggered(TogglePause))
		TogglePauseMenu();

	// Close controller layout
	if (input->IsActionTriggered(Confirm))
	{
		m_DoShowControls = false;
		m_pControllerLayout->SetActive(false);

		m_pBannerBest->SetActive(true);
		m_pBannerLap->SetActive(true);

		m_pTimer->EnableDrawing(true);
	}

	if (m_DoShowControls || m_IsPaused) return;

	// Vibrations
	// Calculate the average  lateral slip of all wheels
	const float avgLateralSlip = std::accumulate(m_WheelQueryResults, m_WheelQueryResults + 4, 0.0f,
		[](float sum, const physx::PxWheelQueryResult& wheelQueryResult) {
			return sum + std::abs(wheelQueryResult.lateralSlip);
		}) / 4.f;
	
	m_RumbleStrength = MathHelper::remap(avgLateralSlip, 0.f, .6f, 0.f, 0.3f);

	input->SetVibration(m_RumbleStrength * .05f, m_RumbleStrength * .15f);

	float steerInput = input->GetThumbstickPosition(true).x;
	float throtleInput = input->GetTriggerPressure(false);
	float reverseThrotleInput = input->GetTriggerPressure(true);

	if (steerInput != 0.f)
		Steer(steerInput);

	if(throtleInput != 0.f)
		AccelerateForward(throtleInput);

	if (reverseThrotleInput != 0.f)
		AccelerateReverse(reverseThrotleInput);

	if (input->IsActionTriggered(HandBrake))
		Handbrake();

	if (input->IsActionTriggered(SteerLeft))
		Steer(-1.f);
	else if (input->IsActionTriggered(SteerRight))
		Steer(1.f);

	if (input->IsActionTriggered(Accelerate))
		AccelerateForward(1.f);
	else if (input->IsActionTriggered(Deaccelerate))
		AccelerateReverse(1.f);
}

void VO_GameScene::UpdateVehicle()
{
	if (m_pVehicle == nullptr) return;

	vehicle::VehicleSceneQueryData* vehicleSceneQueryData = PhysXManager::Get()->GetVehicleSceneQueryData();
	PxBatchQuery* batchQuery = PhysXManager::Get()->GetBatchQuery();
	auto tireFrictionPairs = PhysXManager::Get()->GetFrictionPairs();

	//Update the control inputs for the vehicle
	if (m_IsDigitalControl)
		PxVehicleDrive4WSmoothDigitalRawInputsAndSetAnalogInputs(m_keySmoothingData, m_SteerVsForwardSpeedTable,
			*m_pVehicleInputData, m_SceneContext.pGameTime->GetElapsed(), m_IsVehicleInAir, *m_pVehicle);
	else
		PxVehicleDrive4WSmoothAnalogRawInputsAndSetAnalogInputs(m_padSmoothingData, m_SteerVsForwardSpeedTable,
			*m_pVehicleInputData, m_SceneContext.pGameTime->GetElapsed(), m_IsVehicleInAir, *m_pVehicle);

	//Raycasts
	PxVehicleWheels* vehicleWheels[6] = { m_pVehicle };
	PxRaycastQueryResult* raycastResults = vehicleSceneQueryData->getRaycastQueryResultBuffer(0);
	const PxU32 raycastResultsSize = vehicleSceneQueryData->getQueryResultBufferSize();
	PxVehicleSuspensionRaycasts(batchQuery, 1, vehicleWheels, raycastResultsSize, raycastResults);

	//Vehicle update
	const PxVec3 grav = GetPhysxProxy()->GetPhysxScene()->getGravity();
	PxVehicleWheelQueryResult vehicleQueryResults[1] = { {m_WheelQueryResults, m_pVehicle->mWheelsSimData.getNbWheels()} };

	PxVehicleUpdateSingleVehicleAndStoreTelemetryData(m_SceneContext.pGameTime->GetElapsed(), grav,
		*tireFrictionPairs, m_pVehicle, vehicleQueryResults, *m_pVehicleTelemetryData);

	//Work out if the vehicle is in the air.
	m_IsVehicleInAir = m_pVehicle->getRigidDynamicActor()->isSleeping() ? false : PxVehicleIsInAir(vehicleQueryResults[0]);

	PxShape* shapes[5];
	m_pVehicle->getRigidDynamicActor()->getShapes(shapes, 5);

	// Translate and rotate m_pWheelFL gameobject to match with shape of the wheel attached to rigid body of m_pVehicle
	for (int i = 0; i < 4; i++)
	{
		PxTransform localTm = shapes[i]->getLocalPose();
		PxMat44 shapePose = PxMat44(localTm);
		PxMat44 vehiclePose = PxMat44(m_pVehicle->getRigidDynamicActor()->getGlobalPose());
		PxMat44 wheelPose = vehiclePose * shapePose;

		auto wheelPos = wheelPose.getPosition();
		m_pWheels[i]->GetTransform()->Translate(wheelPos.x, wheelPos.y, wheelPos.z);

		auto wheelRot = PxQuat(PxMat33(wheelPose.getBasis(0), wheelPose.getBasis(1), wheelPose.getBasis(2)));

		// Left wheels are facing inwards, rotate them 180 degrees
		if (i == 0 || i == 2)
		{
			PxQuat rotate180(PxPi, PxVec3(0, 1, 0));
			wheelRot = wheelRot * rotate180;
		}

		m_pWheels[i]->GetTransform()->Rotate(XMVectorSet(wheelRot.x, wheelRot.y, wheelRot.z, wheelRot.w));
	}

	// Drift VFX
	m_pEmitterRL->Pause();
	m_pEmitterRR->Pause();

	if (std::abs(m_WheelQueryResults[0].lateralSlip) >= 0.3f)
		m_pEmitterRL->Play();

	if (std::abs(m_WheelQueryResults[1].lateralSlip) >= 0.3f)
		m_pEmitterRR->Play();
}

void VO_GameScene::OnTriggerCallback(GameObject* trigger, GameObject* other, PxTriggerAction action)
{
	if (action == PxTriggerAction::ENTER)
	{
		// Ignore everything that is not the player
		if (other->GetTag() != L"Player") return;

		if (trigger == m_pCheckpoints[m_NextCheckpoint])
		{
			m_NextCheckpoint = (m_NextCheckpoint + 1) % int(m_pCheckpoints.size());
			if (m_NextCheckpoint == 1)
			{
				m_pTimer->Start();
				m_pTimer->Lap();
			}
		}
	}
}

#pragma region Vehicle Controls
void VO_GameScene::AccelerateForward(float analogAcc)
{
	//If I am going reverse, change to first gear
	if (m_pVehicle->mDriveDynData.getCurrentGear() == PxVehicleGearsData::eREVERSE)
	{
		m_pVehicle->mDriveDynData.forceGearChange(PxVehicleGearsData::eFIRST);
	}

	if (m_IsDigitalControl)
	{
		m_pVehicleInputData->setDigitalAccel(true);
	}
	else
	{
		m_pVehicleInputData->setAnalogAccel(analogAcc);
	}
}

void VO_GameScene::AccelerateReverse(float analogAcc /*= 0.f*/)
{
	//Force gear change to reverse
	m_pVehicle->mDriveDynData.forceGearChange(PxVehicleGearsData::eREVERSE);

	if (m_IsDigitalControl)
	{
		m_pVehicleInputData->setDigitalAccel(true);
	}
	else
	{
		m_pVehicleInputData->setAnalogAccel(analogAcc);
	}
}

void VO_GameScene::Brake()
{
	if (m_IsDigitalControl)
	{
		m_pVehicleInputData->setDigitalBrake(true);
	}
	else
	{
		m_pVehicleInputData->setAnalogBrake(1.f);
	}
}

void VO_GameScene::Steer(float analogSteer /*= 0.f*/)
{
	if (m_IsDigitalControl)
	{
		if (analogSteer <= 0.f)
			m_pVehicleInputData->setDigitalSteerLeft(true);
		else
			m_pVehicleInputData->setDigitalSteerRight(true);
	}
	else
	{
		m_pVehicleInputData->setAnalogSteer(analogSteer);
	}
}

void VO_GameScene::Handbrake()
{
	if (m_IsDigitalControl)
	{
		m_pVehicleInputData->setDigitalHandbrake(true);
	}
	else
	{
		m_pVehicleInputData->setAnalogHandbrake(1.f);
	}
}

void VO_GameScene::ReleaseAllControls()
{
	if (m_IsDigitalControl)
	{
		m_pVehicleInputData->setDigitalAccel(false);
		m_pVehicleInputData->setDigitalSteerLeft(false);
		m_pVehicleInputData->setDigitalSteerRight(false);
		m_pVehicleInputData->setDigitalBrake(false);
		m_pVehicleInputData->setDigitalHandbrake(false);
	}
	else
	{
		m_pVehicleInputData->setAnalogAccel(0.0f);
		m_pVehicleInputData->setAnalogSteer(0.0f);
		m_pVehicleInputData->setAnalogBrake(0.0f);
		m_pVehicleInputData->setAnalogHandbrake(0.0f);
	}
}
#pragma endregion