// Copyright UET2A Team. All Rights Reserved.

#include "UET2APlugin.h"

#define LOCTEXT_NAMESPACE "FUET2APluginModule"

DEFINE_LOG_CATEGORY(LogT2A);

void FUET2APluginModule::StartupModule()
{
	UE_LOG(LogT2A, Log, TEXT("UET2APlugin Runtime Module started."));
}

void FUET2APluginModule::ShutdownModule()
{
	UE_LOG(LogT2A, Log, TEXT("UET2APlugin Runtime Module shut down."));
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FUET2APluginModule, UET2APlugin)
