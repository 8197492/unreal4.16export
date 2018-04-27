// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Framework/Commands/Commands.h"
#include "ExportCubemapStyle.h"

class FExportCubemapCommands : public TCommands<FExportCubemapCommands>
{
public:

	FExportCubemapCommands()
		: TCommands<FExportCubemapCommands>(TEXT("ExportCubemap"), NSLOCTEXT("Contexts", "ExportCubemap", "ExportCubemap Plugin"), NAME_None, FExportCubemapStyle::GetStyleSetName())
	{
	}

	// TCommands<> interface
	virtual void RegisterCommands() override;

public:
	TSharedPtr< FUICommandInfo > PluginAction;
};
