// Copyright UET2A Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

UET2APLUGIN_API DECLARE_LOG_CATEGORY_EXTERN(LogT2A, Log, All);

class FUET2APluginModule : public IModuleInterface
{
public:
	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	/** Get this module instance */
	static FUET2APluginModule& Get()
	{
		return FModuleManager::LoadModuleChecked<FUET2APluginModule>("UET2APlugin");
	}

	static bool IsAvailable()
	{
		return FModuleManager::Get().IsModuleLoaded("UET2APlugin");
	}
};
