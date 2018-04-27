// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "SceneExporter.h"
#include "SceneExporterStyle.h"
#include "SceneExporterCommands.h"
#include "Misc/MessageDialog.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"

#include "LevelEditor.h"

#include "Developer/DesktopPlatform/Public/DesktopPlatformModule.h"
#include "EditorDirectories.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Public/Framework/Docking/TabCommands.h"
#include "Public/Framework/Notifications/NotificationManager.h"

#include "Utility.h"

static const FName SceneExporterTabName("SceneExporter");
#define LOCTEXT_NAMESPACE "FSceneExporterModule"

void FSceneExporterModule::StartupModule()
{
	// This code will execute after your module is loaded into memory; the exact timing is specified in the .uplugin file per-module
	
	FSceneExporterStyle::Initialize();
	FSceneExporterStyle::ReloadTextures();

	FSceneExporterCommands::Register();
	
	PluginCommands = MakeShareable(new FUICommandList);

	PluginCommands->MapAction(
		FSceneExporterCommands::Get().PluginAction,
		FExecuteAction::CreateRaw(this, &FSceneExporterModule::PluginButtonClicked),
		FCanExecuteAction());
		
	FLevelEditorModule& LevelEditorModule = FModuleManager::LoadModuleChecked<FLevelEditorModule>("LevelEditor");
	
	{
		TSharedPtr<FExtender> MenuExtender = MakeShareable(new FExtender());
		MenuExtender->AddMenuExtension("WindowLayout", EExtensionHook::After, PluginCommands, FMenuExtensionDelegate::CreateRaw(this, &FSceneExporterModule::AddMenuExtension));

		LevelEditorModule.GetMenuExtensibilityManager()->AddExtender(MenuExtender);
	}
	
	{
		TSharedPtr<FExtender> ToolbarExtender = MakeShareable(new FExtender);
		ToolbarExtender->AddToolBarExtension("Settings", EExtensionHook::After, PluginCommands, FToolBarExtensionDelegate::CreateRaw(this, &FSceneExporterModule::AddToolbarExtension));
		
		LevelEditorModule.GetToolBarExtensibilityManager()->AddExtender(ToolbarExtender);
	}
}

void FSceneExporterModule::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.
	FSceneExporterStyle::Shutdown();

	FSceneExporterCommands::Unregister();
}

void ShowOutputLog()
{
	FGlobalTabmanager::Get()->InvokeTab(FName("OutputLog"));
}

void FSceneExporterModule::PluginButtonClicked()
{
	auto pkPlatform = FDesktopPlatformModule::Get();
	if (!pkPlatform) return;
	FString kPath;
	bool bRes = pkPlatform->OpenDirectoryDialog(
		FSlateApplication::Get().FindBestParentWindowHandleForDialogs(nullptr),
		LOCTEXT("Choose a Unity Assets folder to export", "Export").ToString(),
		FEditorDirectories::Get().GetLastDirectory(ELastDirectory::GENERIC_EXPORT),
		kPath);
	if (!bRes) return;
	
	ExportTo(kPath);
}

void FSceneExporterModule::AddMenuExtension(FMenuBuilder& Builder)
{
	Builder.AddMenuEntry(FSceneExporterCommands::Get().PluginAction);
}

void FSceneExporterModule::AddToolbarExtension(FToolBarBuilder& Builder)
{
	Builder.AddToolBarButton(FSceneExporterCommands::Get().PluginAction);
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FSceneExporterModule, SceneExporter)