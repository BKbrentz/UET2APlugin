// Copyright UET2A Team. All Rights Reserved.

#include "ST2AEditorPanel.h"
#include "T2AEditorImportRunner.h"
#include "UET2APlugin.h"
#include "AssetRegistry/AssetData.h"
#include "Engine/SkeletalMesh.h"
#include "PropertyCustomizationHelpers.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SMultiLineEditableTextBox.h"
#include "Widgets/Input/SSpinBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/Notifications/SProgressBar.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "ST2AEditorPanel"

ST2AEditorPanel::~ST2AEditorPanel()
{
	if (Runner)
	{
		Runner->Cancel();
		Runner->RemoveFromRoot();
		Runner = nullptr;
	}
}

void ST2AEditorPanel::Construct(const FArguments& InArgs)
{
	Runner = NewObject<UT2AEditorImportRunner>(GetTransientPackage());
	Runner->AddToRoot();
	Runner->OnProgress.AddSP(SharedThis(this), &ST2AEditorPanel::OnPipelineProgress);
	Runner->OnCompleted.AddSP(SharedThis(this), &ST2AEditorPanel::OnPipelineCompleted);
	Runner->OnFailed.AddSP(SharedThis(this), &ST2AEditorPanel::OnPipelineFailed);

	ChildSlot
	[
		SNew(SScrollBox)
		+ SScrollBox::Slot()
		.Padding(8.0f)
		[
			SNew(SVerticalBox)

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0, 0, 0, 8)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("Title", "Hunyuan Text-to-Animation Asset Importer"))
				.Font(FCoreStyle::GetDefaultFontStyle("Bold", 16))
			]

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

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0, 0, 0, 4)
			[
				BuildPromptInputSection()
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0, 0, 0, 4)
			[
				BuildParameterSection()
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0, 0, 0, 4)
			[
				BuildTargetSection()
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0, 4)
			[
				SNew(SSeparator)
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0, 0, 0, 4)
			[
				BuildActionSection()
			]

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
				.Text(LOCTEXT("SaveKey", "Apply"))
				.OnClicked(this, &ST2AEditorPanel::OnSaveAPIKeyClicked)
			]
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0, 2, 0, 0)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("APIKeyNote", "面板会在编辑器态直接调用混元生成并把动画导入到项目资源目录，不需要先启动 PIE。"))
			.AutoWrapText(true)
			.ColorAndOpacity(FSlateColor(FLinearColor(0.65f, 0.65f, 0.65f)))
		];
}

TSharedRef<SWidget> ST2AEditorPanel::BuildPromptInputSection()
{
	return SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(STextBlock)
			.Text(LOCTEXT("PromptLabel", "Animation Prompt"))
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
	return SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(0, 0, 16, 0)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(STextBlock)
					.Text(LOCTEXT("DurationLabel", "Duration (s)"))
				]
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(SSpinBox<int32>)
					.MinValue(1)
					.MaxValue(12)
					.Value(Duration)
					.OnValueChanged_Lambda([this](int32 Value)
					{
						Duration = Value;
					})
				]
			]
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0, 8, 0, 0)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("ImportPathLabel", "Import Path"))
			.Font(FCoreStyle::GetDefaultFontStyle("Bold", 10))
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0, 2, 0, 0)
		[
			SAssignNew(ImportPathInput, SEditableTextBox)
			.Text(FText::FromString(ImportPath))
			.HintText(LOCTEXT("ImportPathHint", "/Game/HunyuanMotion/Imported"))
			.OnTextChanged_Lambda([this](const FText& Text)
			{
				ImportPath = Text.ToString();
			})
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0, 2, 0, 0)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("ImportPathNote", "填写 Unreal 资源路径，例如 /Game/HunyuanMotion/Imported。若路径不存在，导入器会按该目录创建并保存动画资源。"))
			.AutoWrapText(true)
			.ColorAndOpacity(FSlateColor(FLinearColor(0.65f, 0.65f, 0.65f)))
		];
}

TSharedRef<SWidget> ST2AEditorPanel::BuildTargetSection()
{
	return SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(STextBlock)
			.Text(LOCTEXT("TargetLabel", "Target SkeletalMesh"))
			.Font(FCoreStyle::GetDefaultFontStyle("Bold", 10))
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0, 2, 0, 0)
		[
			SNew(SObjectPropertyEntryBox)
			.AllowedClass(USkeletalMesh::StaticClass())
			.ObjectPath(this, &ST2AEditorPanel::GetTargetSkeletalMeshPath)
			.DisplayUseSelected(true)
			.DisplayBrowse(true)
			.AllowClear(true)
			.OnObjectChanged(this, &ST2AEditorPanel::OnTargetSkeletalMeshChanged)
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0, 4, 0, 0)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("TargetNote", "如果选择了 Target SkeletalMesh，导入器会复用它的 Skeleton 来创建动画资源；不选择时会按 FBX 骨架创建新的 Skeleton 资源。"))
			.AutoWrapText(true)
			.ColorAndOpacity(FSlateColor(FLinearColor(0.85f, 0.85f, 0.85f)))
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
			.Text(LOCTEXT("GenerateAndImport", "Generate and Import Asset"))
			.HAlign(HAlign_Center)
			.IsEnabled_Lambda([this]()
			{
				return !bIsRunning;
			})
			.OnClicked(this, &ST2AEditorPanel::OnGenerateClicked)
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		[
			SNew(SButton)
			.Text(LOCTEXT("Cancel", "Cancel"))
			.IsEnabled_Lambda([this]()
			{
				return bIsRunning;
			})
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
			.Percent_Lambda([this]()
			{
				return ProgressValue;
			})
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SAssignNew(StatusLabel, STextBlock)
			.Text_Lambda([this]()
			{
				return FText::FromString(StatusText);
			})
			.AutoWrapText(true)
		];
}

FReply ST2AEditorPanel::OnGenerateClicked()
{
	if (!Runner)
	{
		StatusText = TEXT("Editor import runner is unavailable.");
		return FReply::Handled();
	}

	if (APIKeyInput.IsValid())
	{
		APIKey = APIKeyInput->GetText().ToString().TrimStartAndEnd();
	}

	TextPrompt = TextPrompt.TrimStartAndEnd();
	ImportPath = ImportPath.TrimStartAndEnd();

	if (APIKey.IsEmpty())
	{
		StatusText = TEXT("Please enter an API Key.");
		return FReply::Handled();
	}

	if (TextPrompt.IsEmpty())
	{
		StatusText = TEXT("Please enter an animation prompt.");
		return FReply::Handled();
	}

	if (ImportPath.IsEmpty())
	{
		StatusText = TEXT("Please enter an import path.");
		return FReply::Handled();
	}

	if (ImportPathInput.IsValid())
	{
		ImportPathInput->SetText(FText::FromString(ImportPath));
	}

	Runner->SetAPIKey(APIKey);

	bIsRunning = true;
	ProgressValue = 0.0f;
	StatusText = FString::Printf(TEXT("Submitting animation generation for import path %s..."), *ImportPath);
	Runner->RunImport(TextPrompt, Duration, TargetSkeletalMesh.Get(), ImportPath);

	return FReply::Handled();
}

FReply ST2AEditorPanel::OnCancelClicked()
{
	if (Runner)
	{
		Runner->Cancel();
	}

	bIsRunning = false;
	ProgressValue = 0.0f;
	StatusText = TEXT("Cancelled.");
	return FReply::Handled();
}

FReply ST2AEditorPanel::OnSaveAPIKeyClicked()
{
	if (APIKeyInput.IsValid())
	{
		APIKey = APIKeyInput->GetText().ToString().TrimStartAndEnd();
	}

	if (APIKey.IsEmpty())
	{
		StatusText = TEXT("Please enter an API Key first.");
		return FReply::Handled();
	}

	if (Runner)
	{
		Runner->SetAPIKey(APIKey);
	}

	StatusText = TEXT("API Key applied to editor import pipeline.");
	return FReply::Handled();
}

void ST2AEditorPanel::OnTargetSkeletalMeshChanged(const FAssetData& AssetData)
{
	TargetSkeletalMesh = Cast<USkeletalMesh>(AssetData.GetAsset());

	if (TargetSkeletalMesh.IsValid())
	{
		StatusText = FString::Printf(TEXT("Selected Target SkeletalMesh: %s"), *TargetSkeletalMesh->GetPathName());
	}
	else
	{
		StatusText = TEXT("Target SkeletalMesh cleared. Importer will create a new skeleton asset from the FBX.");
	}
}

void ST2AEditorPanel::OnPipelineProgress(ET2APipelineStage Stage, float Progress, const FString& StatusMessage)
{
	bIsRunning = Stage != ET2APipelineStage::Completed && Stage != ET2APipelineStage::Failed;
	ProgressValue = FMath::Clamp(Progress, 0.0f, 1.0f);
	StatusText = StatusMessage;
}

void ST2AEditorPanel::OnPipelineCompleted(UAnimSequence* Animation, const FString& RewrittenPrompt)
{
	bIsRunning = false;
	ProgressValue = 1.0f;

	StatusText = Runner ? Runner->GetLastCompletionSummary() : TEXT("Done! Animation asset imported successfully.");
	if (StatusText.IsEmpty())
	{
		StatusText = Animation
			? FString::Printf(TEXT("Done! Imported animation: %s"), *Animation->GetName())
			: TEXT("Done! Animation asset imported successfully.");
	}

	if (!RewrittenPrompt.IsEmpty())
	{
		StatusText += FString::Printf(TEXT(" Prompt: %s"), *RewrittenPrompt);
	}
}

void ST2AEditorPanel::OnPipelineFailed(ET2APipelineStage FailedStage, const FString& ErrorMessage)
{
	static_cast<void>(FailedStage);
	bIsRunning = false;
	StatusText = FString::Printf(TEXT("Failed: %s"), *ErrorMessage);
}

FString ST2AEditorPanel::GetTargetSkeletalMeshPath() const
{
	return TargetSkeletalMesh.IsValid() ? TargetSkeletalMesh->GetPathName() : FString();
}

#undef LOCTEXT_NAMESPACE
