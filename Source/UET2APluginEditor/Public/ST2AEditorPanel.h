// Copyright UET2A Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "T2AAnimationSubsystem.h"

/**
 * Editor debug panel for the T2A plugin.
 * Provides UI for text input, parameter configuration, and pipeline monitoring.
 * Only available in editor builds.
 */
class ST2AEditorPanel : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(ST2AEditorPanel) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:
	// UI building
	TSharedRef<SWidget> BuildAPIConfigSection();
	TSharedRef<SWidget> BuildPromptInputSection();
	TSharedRef<SWidget> BuildParameterSection();
	TSharedRef<SWidget> BuildTargetSection();
	TSharedRef<SWidget> BuildActionSection();
	TSharedRef<SWidget> BuildStatusSection();

	// Button callbacks
	FReply OnGenerateClicked();
	FReply OnCancelClicked();
	FReply OnSaveAPIKeyClicked();

	void BindSubsystemDelegates(UT2AAnimationSubsystem* Subsystem);
	void UnbindSubsystemDelegates(UT2AAnimationSubsystem* Subsystem);

	// State
	void OnPipelineProgress(ET2APipelineStage Stage, float Progress, const FString& StatusMessage);
	void OnPipelineCompleted(UAnimSequence* Animation, const FString& RewrittenPrompt);
	void OnPipelineFailed(ET2APipelineStage FailedStage, const FString& ErrorMessage);

	UT2AAnimationSubsystem* GetSubsystem() const;

	// UI State
	FString APIKey;
	FString TextPrompt;
	int32 Duration = 5;
	bool bDisableRewrite = false;
	FString StatusText = TEXT("Ready");
	float ProgressValue = 0.0f;
	bool bIsRunning = false;

	// Widget refs for updates
	TSharedPtr<class SEditableTextBox> APIKeyInput;
	TSharedPtr<class SMultiLineEditableTextBox> PromptInput;
	TSharedPtr<class SProgressBar> ProgressBar;
	TSharedPtr<class STextBlock> StatusLabel;

	FDelegateHandle PipelineProgressHandle;
	FDelegateHandle PipelineCompletedHandle;
	FDelegateHandle PipelineFailedHandle;
};
