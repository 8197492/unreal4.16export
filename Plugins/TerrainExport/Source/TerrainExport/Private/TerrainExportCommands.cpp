// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "TerrainExportCommands.h"

#define LOCTEXT_NAMESPACE "FTerrainExportModule"

void FTerrainExportCommands::RegisterCommands()
{
	UI_COMMAND(PluginAction, "TerrainExport", "Execute TerrainExport action", EUserInterfaceActionType::Button, FInputGesture());
}

#undef LOCTEXT_NAMESPACE
