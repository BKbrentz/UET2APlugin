// Copyright UET2A Team. All Rights Reserved.

#include "ST2AEditorPanel.h"
#include "T2AAnimationSubsystem.h"
#include "UET2APlugin.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SMultiLineEditableTextBox.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SSpinBox.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Notifications/SProgressBar.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/Text/STextBlock.h"
#include "Editor.h"
#include "Engine/GameInstance.h"

#define LOCTEXT_NAMESPACE "ST2AEditorPanel"

void ST2AEditorPanel::Construct(const FArguments& InArgs)
{
	ChildSlot
	[
		SNew(SScrollBox)
		+ SScrollBox::Slot()
		.Padding(8.0f)
		[
			SNew(SVerticalBox)

			// Title
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0, 0, 0, 8)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("Title", "Hunyuan Text-to-Animation"))
				.Font(FCoreStyle::GetDefaultFontStyle("Bold", 16))
			]

			// API Key Config
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0, 0, 0, 4)
			[
				BuildAPIConfigSection()
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0, 4)
			[
				SNew(SSeparator)
			]

			// Prompt Input
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0, 0, 0, 4)
			[
				BuildPromptInputSection()
			]

			// Parameters
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0, 0, 0, 4)
			[
				BuildParameterSection()
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0, 4)
			[
				SNew(SSeparator)
			]

			// Action Buttons
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0, 0, 0, 4)
			[
				BuildActionSection()
			]

			// Status
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0, 0, 0, 4)
			[
				BuildStatusSection()
			]
		]
	];
}

TSharedRef<SWidget> ST2AEditorPanel::BuildAPIConfigSection()
{
	return SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(STextBlock)
			.Text(LOCTEXT("APIKeyLabel", "API Key"))
			.Font(FCoreStyle::GetDefaultFontStyle("Bold", 10))
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0, 2, 0, 0)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			[
				SAssignNew(APIKeyInput, SEditableTextBox)
				.HintText(LOCTEXT("APIKeyHint", "Enter your Hunyuan API Key..."))
				.IsPassword(true)
				.OnTextCommitted_Lambda([this](const FText& Text, ETextCommit::Type)
				{
					APIKey = Text.ToString();
				})
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(4, 0, 0, 0)
			[
				SNew(SButton)
				.Text(LOCTEXT("SaveKey", "Save"))
				.OnClicked(this, &ST2AEditorPanel::OnSaveAPIKeyClicked)
			]
		];
}

TSharedRef<SWidget> ST2AEditorPanel::BuildPromptInputSection()
{
	return SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(STextBlock)
			.Text(LOCTEXT("PromptLabel", "Animation Description"))
			.Font(FCoreStyle::GetDefaultFontStyle("Bold", 10))
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0, 2, 0, 0)
		[
			SAssignNew(PromptInput, SMultiLineEditableTextBox)
			.HintText(LOCTEXT("PromptHint", "Describe the animation you want to generate...\ne.g. \"A person walks slowly forward then waves hello\""))
			.AutoWrapText(true)
			.OnTextChanged_Lambda([this](const FText& Text)
			{
				TextPrompt = Text.ToString();
			})
		];
}

TSharedRef<SWidget> ST2AEditorPanel::BuildParameterSection()
{
	return SNew(SHorizontalBox)
		// Duration
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(0, 0, 16, 0)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(STextBlock).Text(LOCTEXT("DurationLabel", "Duration (s)"))
			]
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(SSpinBox<int32>)
				.MinValue(1)
				.MaxValue(12)
				.Value(Duration)
				.OnValueChanged_Lambda([this](int32 Value) { Duration = Value; })
			]
		]
		// Disable Rewrite
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Bottom)
		[
			SNew(SCheckBox)
			.IsChecked(bDisableRewrite ? ECheckBoxState::Checked : ECheckBoxState::Unchecked)
			.OnCheckStateChanged_Lambda([this](ECheckBoxState State) 
			{ 
				bDisableRewrite = (State == ECheckBoxState::Checked); 
			})
			[
				SNew(STextBlock).Text(LOCTEXT("DisableRewrite", "Disable Prompt Rewrite"))
			]
		];
}

TSharedRef<SWidget> ST2AEditorPanel::BuildActionSection()
{
	return SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.FillWidth(1.0f)
		.Padding(0, 0, 4, 0)
		[
			SNew(SButton)
			.Text(LOCTEXT("Generate", "Generate Animation"))
			.HAlign(HAlign_Center)
			.IsEnabled_Lambda([this]() { return !bIsRunning; })
			.OnClicked(this, &ST2AEditorPanel::OnGenerateClicked)
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		[
			SNew(SButton)
			.Text(LOCTEXT("Cancel", "Cancel"))
			.IsEnabled_Lambda([this]() { return bIsRunning; })
			.OnClicked(this, &ST2AEditorPanel::OnCancelClicked)
		];
}

TSharedRef<SWidget> ST2AEditorPanel::BuildStatusSection()
{
	return SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0, 4)
		[
			SAssignNew(ProgressBar, SProgressBar)
			.Percent_Lambda([this]() { return ProgressValue; })
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SAssignNew(StatusLabel, STextBlock)
			.Text_Lambda([this]() { return FText::FromString(StatusText); })
		];
}

// ==================== Callbacks ====================

FReply ST2AEditorPanel::OnGenerateClicked()
{
	UT2AAnimationSubsystem* Subsystem = GetSubsystem();
	if (!Subsystem)
	{
		StatusText = TEXT("Error: T2A Subsystem not available. Start PIE first.");
		return FReply::Handled();
	}

	if (TextPrompt.IsEmpty())
	{
		StatusText = TEXT("Please enter a text prompt.");
		return FReply::Handled();
	}

	// Configure pipeline
	FT2APipelineConfig Config;
	Config.TextPrompt = TextPrompt;
	Config.Duration = Duration;
	Config.bDisableRewrite = bDisableRewrite;
	Config.bAutoPlay = false; // In editor, don't auto-play
	Config.TargetCharacter = nullptr;

	bIsRunning = true;
	ProgressValue = 0.0f;
	StatusText = TEXT("Submitting...");

	BindSubsystemDelegates(Subsystem);
	Subsystem->RunPipeline(Config);

	return FReply::Handled();
}

FReply ST2AEditorPanel::OnCancelClicked()
{
	UT2AAnimationSubsystem* Subsystem = GetSubsystem();
	if (Subsystem)
	{
		Subsystem->CancelPipeline();
	}
	bIsRunning = false;
	StatusText = TEXT("Cancelled");
	ProgressValue = 0.0f;
	return FReply::Handled();
}

FReply ST2AEditorPanel::OnSaveAPIKeyClicked()
{
	if (APIKeyInput.IsValid())
	{
		APIKey = APIKeyInput->GetText().ToString();
	}

	UT2AAnimationSubsystem* Subsystem = GetSubsystem();
	if (Subsystem)
	{
		Subsystem->SetAPIKey(APIKey);
		StatusText = TEXT("API Key saved.");
	}
	else
	{
		StatusText = TEXT("API Key stored. Will apply when PIE starts.");
	}
	return FReply::Handled();
}

void ST2AEditorPanel::BindSubsystemDelegates(UT2AAnimationSubsystem* Subsystem)
{
	if (!Subsystem)
	{
		return;
	}

	UnbindSubsystemDelegates(Subsystem);

	TSharedRef<ST2AEditorPanel> SharedPanel = SharedThis(this);
	PipelineProgressHandle = Subsystem->OnPipelineProgressNative.AddSP(SharedPanel, &ST2AEditorPanel::OnPipelineProgress);
	PipelineCompletedHandle = Subsystem->OnPipelineCompletedNative.AddSP(SharedPanel, &ST2AEditorPanel::OnPipelineCompleted);
	PipelineFailedHandle = Subsystem->OnPipelineFailedNative.AddSP(SharedPanel, &ST2AEditorPanel::OnPipelineFailed);
}

void ST2AEditorPanel::UnbindSubsystemDelegates(UT2AAnimationSubsystem* Subsystem)
{
	if (!Subsystem)
	{
		return;
	}

	if (PipelineProgressHandle.IsValid())
	{
		Subsystem->OnPipelineProgressNative.Remove(PipelineProgressHandle);
		PipelineProgressHandle = FDelegateHandle();
	}

	if (PipelineCompletedHandle.IsValid())
	{
		Subsystem->OnPipelineCompletedNative.Remove(PipelineCompletedHandle);
		PipelineCompletedHandle = FDelegateHandle();
	}

	if (PipelineFailedHandle.IsValid())
	{
		Subsystem->OnPipelineFailedNative.Remove(PipelineFailedHandle);
		PipelineFailedHandle = FDelegateHandle();
	}
}

void ST2AEditorPanel::OnPipelineProgress(ET2APipelineStage Stage, float Progress, const FString& StatusMessage)
{
	static_cast<void>(Stage);
	ProgressValue = Progress;
	StatusText = StatusMessage;
}

void ST2AEditorPanel::OnPipelineCompleted(UAnimSequence* Animation, const FString& RewrittenPrompt)
{
	static_cast<void>(Animation);
	bIsRunning = false;
	ProgressValue = 1.0f;
	StatusText = RewrittenPrompt.IsEmpty()
		? TEXT("Done! Animation ready.")
		: FString::Printf(TEXT("Done! Animation ready. Prompt: %s"), *RewrittenPrompt);
}

void ST2AEditorPanel::OnPipelineFailed(ET2APipelineStage FailedStage, const FString& ErrorMessage)
{
	static_cast<void>(FailedStage);
	bIsRunning = false;
	StatusText = FString::Printf(TEXT("Failed: %s"), *ErrorMessage);
}

UT2AAnimationSubsystem* ST2AEditorPanel::GetSubsystem() const
{
	if (GEditor)
	{
		UWorld* PIEWorld = GEditor->GetEditorWorldContext().World();
		// Try to get PIE world
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			if (Context.WorldType == EWorldType::PIE && Context.World())
			{
				UGameInstance* GI = Context.World()->GetGameInstance();
				if (GI)
				{
					return GI->GetSubsystem<UT2AAnimationSubsystem>();
				}
			}
		}
	}
	return nullptr;
}

#undef LOCTEXT_NAMESPACE
