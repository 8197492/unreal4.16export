// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "SceneExporterCommands.h"

#define LOCTEXT_NAMESPACE "FSceneExporterModule"

void FSceneExporterCommands::RegisterCommands()
{
	UI_COMMAND(PluginAction, "SceneExporter", "Execute SceneExporter action", EUserInterfaceActionType::Button, FInputGesture());
}

#undef LOCTEXT_NAMESPACE
