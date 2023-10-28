// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include <imgui.h>
#include <string>

#include "CoreMinimal.h"
#include "Subsystems/EngineSubsystem.h"
#include "ImGuiSubsystem.generated.h"

UCLASS()
class UNREALIMGUIDOCKER_API UImGuiSubsystem : public UEngineSubsystem
{
	GENERATED_BODY()

public:
	virtual void Deinitialize() override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	UFUNCTION(BlueprintCallable)
	static void WindowTest();
	
protected:
	void PreWorldInitialization(UWorld* World, FWorldInitializationValues WorldInitializationValues);
	void WorldInitializedActors(const FActorsInitializedParams& ActorsInitializedParams);
	void BeginFrame(float DeltaTime);
	void EndFrame(float DeltaTime);
	void LevelViewportClientListChanged() const;

	UPROPERTY()
	TObjectPtr<UTexture2D> FontTexture;
	TUniquePtr<FSlateBrush> FontTextureBrush;

	bool bInImGuiFrame = false;
	std::string IniFileName;
};
