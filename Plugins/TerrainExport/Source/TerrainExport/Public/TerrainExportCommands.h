// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Framework/Commands/Commands.h"
#include "TerrainExportStyle.h"

class FTerrainExportCommands : public TCommands<FTerrainExportCommands>
{
public:

	FTerrainExportCommands()
		: TCommands<FTerrainExportCommands>(TEXT("TerrainExport"), NSLOCTEXT("Contexts", "TerrainExport", "TerrainExport Plugin"), NAME_None, FTerrainExportStyle::GetStyleSetName())
	{
	}

	// TCommands<> interface
	virtual void RegisterCommands() override;

public:
	TSharedPtr< FUICommandInfo > PluginAction;
};
