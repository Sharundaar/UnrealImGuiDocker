// Copyright Donatien Rabiller. All rights reserved.

#pragma once

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
	void WorldInitializedActors(const FActorsInitializedParams& ActorsInitializedParams);
	void TickImGui(float DeltaTime);

	UPROPERTY()
	TObjectPtr<UTexture2D> FontTexture;
	TUniquePtr<FSlateBrush> FontTextureBrush;

	bool bInImGuiFrame = false;
	TAnsiStringBuilder<512> IniFileName;
};
