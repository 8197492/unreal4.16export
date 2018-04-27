// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "ExportCubemapCommands.h"

#define LOCTEXT_NAMESPACE "FExportCubemapModule"

void FExportCubemapCommands::RegisterCommands()
{
	UI_COMMAND(PluginAction, "ExportCubemap", "Execute ExportCubemap action", EUserInterfaceActionType::Button, FInputGesture());
}

#undef LOCTEXT_NAMESPACE
