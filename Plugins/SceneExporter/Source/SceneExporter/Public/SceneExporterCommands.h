// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Framework/Commands/Commands.h"
#include "SceneExporterStyle.h"

class FSceneExporterCommands : public TCommands<FSceneExporterCommands>
{
public:

	FSceneExporterCommands()
		: TCommands<FSceneExporterCommands>(TEXT("SceneExporter"), NSLOCTEXT("Contexts", "SceneExporter", "SceneExporter Plugin"), NAME_None, FSceneExporterStyle::GetStyleSetName())
	{
	}

	// TCommands<> interface
	virtual void RegisterCommands() override;

public:
	TSharedPtr< FUICommandInfo > PluginAction;
};
