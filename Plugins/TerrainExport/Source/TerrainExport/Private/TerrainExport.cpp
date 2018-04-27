// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "TerrainExport.h"
#include "TerrainExportStyle.h"
#include "TerrainExportCommands.h"
#include "Misc/MessageDialog.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"

#include "LevelEditor.h"
#include "Notifications/NotificationManager.h"
#include "Exporters/Exporter.h"
#include "Public/ObjectTools.h"
#include "Public/HAL/Runnable.h"
#include "Public/HAL/RunnableThread.h"
#include "Public/HAL/PlatformFilemanager.h"
#include "Public/Misc/SingleThreadRunnable.h"
#include "Public/SceneTypes.h"
#include "Public/LightMap.h"
#include "Public/ShadowMap.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/DirectionalLight.h"
#include "Engine/ExponentialHeightFog.h"
#include "Engine/SphereReflectionCapture.h"
#include "Engine/MapBuildDataRegistry.h"
#include "Engine/ShadowMapTexture2D.h"
#include "Components/ReflectionCaptureComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include <functional>
#include <fstream>
#include "CubemapUnwrapUtils.h"
#include "Developer/DesktopPlatform/Public/DesktopPlatformModule.h"
#include "EditorDirectories.h"
#include "Landscape.h"
#include "Landscapeinfo.h"
#include "LandscapeLayerInfoObject.h"
#include "LandscapeGizmoActiveActor.h"
#include "LandscapeComponent.h"

static const FName TerrainExportTabName("TerrainExport");

#define LOCTEXT_NAMESPACE "FTerrainExportModule"

void FTerrainExportModule::StartupModule()
{
	// This code will execute after your module is loaded into memory; the exact timing is specified in the .uplugin file per-module

	FTerrainExportStyle::Initialize();
	FTerrainExportStyle::ReloadTextures();

	FTerrainExportCommands::Register();

	PluginCommands = MakeShareable(new FUICommandList);

	PluginCommands->MapAction(
		FTerrainExportCommands::Get().PluginAction,
		FExecuteAction::CreateRaw(this, &FTerrainExportModule::PluginButtonClicked),
		FCanExecuteAction());

	FLevelEditorModule& LevelEditorModule = FModuleManager::LoadModuleChecked<FLevelEditorModule>("LevelEditor");

	{
		TSharedPtr<FExtender> MenuExtender = MakeShareable(new FExtender());
		MenuExtender->AddMenuExtension("WindowLayout", EExtensionHook::After, PluginCommands, FMenuExtensionDelegate::CreateRaw(this, &FTerrainExportModule::AddMenuExtension));

		LevelEditorModule.GetMenuExtensibilityManager()->AddExtender(MenuExtender);
	}

	{
		TSharedPtr<FExtender> ToolbarExtender = MakeShareable(new FExtender);
		ToolbarExtender->AddToolBarExtension("Settings", EExtensionHook::After, PluginCommands, FToolBarExtensionDelegate::CreateRaw(this, &FTerrainExportModule::AddToolbarExtension));

		LevelEditorModule.GetToolBarExtensibilityManager()->AddExtender(ToolbarExtender);
	}
}

void FTerrainExportModule::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.
	FTerrainExportStyle::Shutdown();

	FTerrainExportCommands::Unregister();
}

void FTerrainExportModule::PluginButtonClicked()
{
	UWorld* pkWorld = GEditor->GetEditorWorldContext().World();
	if (!pkWorld)
	{
		return;
	}

	auto pkPlatform = FDesktopPlatformModule::Get();
	if (!pkPlatform)
		return;
	FString kPath;
	bool bRes = pkPlatform->OpenDirectoryDialog(
		FSlateApplication::Get().FindBestParentWindowHandleForDialogs(nullptr),
		LOCTEXT("Choose a folder to export", "Export").ToString(),
		FEditorDirectories::Get().GetLastDirectory(ELastDirectory::GENERIC_EXPORT),
		kPath);
	if (!bRes)
	{
		return;
	}

	for (auto pkLevel : pkWorld->GetLevels())
	{
		for (auto pkActor : pkLevel->Actors)
		{
			if (pkActor->IsA(ALandscape::StaticClass()))
			{
				ALandscape* pLandscape = (ALandscape*)pkActor;

				for (int i = 0; i < pLandscape->LandscapeComponents.Num(); ++i)
				{
					ULandscapeComponent* kkk = pLandscape->LandscapeComponents[i];
					if (kkk->GrassData->HasData())
					{
						for (auto Iter(kkk->GrassData->WeightData.CreateIterator()); Iter; ++Iter)
						{
							auto aa = (*Iter).Key;
							auto bb = (*Iter).Value;

							int ff = 0;
							ff = ff;
						}
					}
				}



				UMaterialInterface* dddd = pLandscape->LandscapeMaterial;

				ULandscapeInfo* pLInfo = pLandscape->GetLandscapeInfo();

				pLInfo->ExportHeightmap(FString(kPath + "/" + pkActor->GetName() + "_Heightmap.png"));

				for (int i = 0; i < pLInfo->Layers.Num(); ++i)
				{
					if (pLInfo->Layers[i].LayerInfoObj != nullptr)
					{
						pLInfo->ExportLayer(pLInfo->Layers[i].LayerInfoObj, kPath + "/" + pLInfo->Layers[i].GetLayerName().ToString() + ".png");
					}
				}
			}
			else if (pkActor->IsA(ALandscapeGizmoActiveActor::StaticClass()))
			{
				ALandscapeGizmoActiveActor* pLGAA = (ALandscapeGizmoActiveActor*)pkActor;

				int hh = 0;
				hh = 1;
			}
		}
	}
}

void FTerrainExportModule::AddMenuExtension(FMenuBuilder& Builder)
{
	Builder.AddMenuEntry(FTerrainExportCommands::Get().PluginAction);
}

void FTerrainExportModule::AddToolbarExtension(FToolBarBuilder& Builder)
{
	Builder.AddToolBarButton(FTerrainExportCommands::Get().PluginAction);
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FTerrainExportModule, TerrainExport)