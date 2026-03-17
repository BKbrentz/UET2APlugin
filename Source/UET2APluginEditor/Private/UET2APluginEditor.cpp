// Copyright UET2A Team. All Rights Reserved.

#include "UET2APluginEditor.h"
#include "UET2APlugin.h"
#include "ST2AEditorPanel.h"
#include "ToolMenus.h"
#include "WorkspaceMenuStructure.h"
#include "WorkspaceMenuStructureModule.h"
#include "Widgets/Docking/SDockTab.h"
#include "Framework/Docking/TabManager.h"
#include "Styling/AppStyle.h"

#define LOCTEXT_NAMESPACE "FUET2APluginEditorModule"

static const FName T2AEditorTabName("T2AEditorPanel");

void FUET2APluginEditorModule::StartupModule()
{
	UE_LOG(LogT2A, Log, TEXT("UET2APlugin Editor Module started."));

	// Register the editor tab spawner
	FGlobalTabmanager::Get()->RegisterNomadTabSpawner(T2AEditorTabName,
		FOnSpawnTab::CreateLambda([](const FSpawnTabArgs& SpawnTabArgs) -> TSharedRef<SDockTab>
		{
			return SNew(SDockTab)
				.TabRole(NomadTab)
				.Label(LOCTEXT("T2ATabTitle", "Text-to-Animation"))
				[
					SNew(ST2AEditorPanel)
				];
		}))
		.SetDisplayName(LOCTEXT("T2ATabDisplayName", "Hunyuan Text-to-Animation"))
		.SetTooltipText(LOCTEXT("T2ATabTooltip", "Generate skeletal animations from text descriptions using Hunyuan Motion API"))
		.SetGroup(WorkspaceMenu::GetMenuStructure().GetToolsCategory())
		.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.Tabs.Viewports"));

	RegisterMenus();
}

void FUET2APluginEditorModule::ShutdownModule()
{
	FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(T2AEditorTabName);
	UToolMenus::UnRegisterStartupCallback(this);
	UToolMenus::UnregisterOwner(this);

	UE_LOG(LogT2A, Log, TEXT("UET2APlugin Editor Module shut down."));
}

void FUET2APluginEditorModule::RegisterMenus()
{
	FToolMenuOwnerScoped OwnerScoped(this);

	// Add to Window menu
	UToolMenu* Menu = UToolMenus::Get()->ExtendMenu("LevelEditor.MainMenu.Window");
	if (Menu)
	{
		FToolMenuSection& Section = Menu->FindOrAddSection("WindowLayout");
		Section.AddMenuEntry(
			"OpenT2APanel",
			LOCTEXT("OpenT2APanel", "Text-to-Animation"),
			LOCTEXT("OpenT2APanelTooltip", "Open the Hunyuan Text-to-Animation panel"),
			FSlateIcon(),
			FUIAction(FExecuteAction::CreateLambda([]()
			{
				FGlobalTabmanager::Get()->TryInvokeTab(T2AEditorTabName);
			}))
		);
	}
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FUET2APluginEditorModule, UET2APluginEditor)
