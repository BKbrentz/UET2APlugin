// Copyright UET2A Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"

enum class ET2APipelineStage : uint8;

class UAnimSequence;
class USkeletalMesh;
class UT2AEditorImportRunner;
struct FAssetData;

class ST2AEditorPanel : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(ST2AEditorPanel) {}
	SLATE_END_ARGS()

	~ST2AEditorPanel();
	void Construct(const FArguments& InArgs);

private:
	TSharedRef<SWidget> BuildAPIConfigSection();
	TSharedRef<SWidget> BuildPromptInputSection();
	TSharedRef<SWidget> BuildParameterSection();
	TSharedRef<SWidget> BuildTargetSection();
	TSharedRef<SWidget> BuildActionSection();
	TSharedRef<SWidget> BuildStatusSection();

	FReply OnGenerateClicked();
	FReply OnCancelClicked();
	FReply OnSaveAPIKeyClicked();

	void OnTargetSkeletalMeshChanged(const FAssetData& AssetData);
	void OnPipelineProgress(ET2APipelineStage Stage, float Progress, const FString& StatusMessage);
	void OnPipelineCompleted(UAnimSequence* Animation, const FString& RewrittenPrompt);
	void OnPipelineFailed(ET2APipelineStage FailedStage, const FString& ErrorMessage);

	FString GetTargetSkeletalMeshPath() const;

	FString APIKey;
	FString TextPrompt;
	FString ImportPath = TEXT("/Game/HunyuanMotion/Imported");
	int32 Duration = 5;
	FString StatusText = TEXT("Ready to generate and import a T2A animation asset.");
	float ProgressValue = 0.0f;
	bool bIsRunning = false;
	TWeakObjectPtr<USkeletalMesh> TargetSkeletalMesh;
	UT2AEditorImportRunner* Runner = nullptr;

	TSharedPtr<class SEditableTextBox> APIKeyInput;
	TSharedPtr<class SMultiLineEditableTextBox> PromptInput;
	TSharedPtr<class SEditableTextBox> ImportPathInput;
	TSharedPtr<class SProgressBar> ProgressBar;
	TSharedPtr<class STextBlock> StatusLabel;
};
