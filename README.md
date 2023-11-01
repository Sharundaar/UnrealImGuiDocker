# UnrealImGuiDocker

DearImGui docker branch integration in Unreal, tested with Unreal 5.3.

# How to Use

Simply clone the repository in your Plugins folder and compile, you can then emmit ImGui commands in any context where you have access to a Tick function. For example here's an editor subsystem that creates an ImGui window depending on a CVar :

```cpp
// UnrealDockerEditorExample.h
UCLASS()
class WEBSITESUPPORT_API UUnrealDockerEditorExample : public UEditorSubsystem, public FTickableEditorObject
{
  GENERATED_BODY()

public:
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual ETickableTickType GetTickableTickType() const override { return IsTemplate() ? ETickableTickType::Never : ETickableTickType::Always; }
};

// UnrealDockerEditorExample.cpp
#include "UnrealDockerEditorExample.h"

#include "Editor.h"
#include "imgui.h"
#include "Selection.h"

static int32 UnrealDockerShowExample = 0;
static FAutoConsoleVariableRef CVarUnrealDockerShowExample(
	TEXT("ImGui.ShowEditorDockerExample"),
	UnrealDockerShowExample,
	TEXT("0: hide, 1: show"),
	ECVF_Default
);

void UUnrealDockerEditorExample::Tick(float DeltaTime)
{
	if(!UnrealDockerShowExample)
		return;

	bool bOpened = true;
	if(ImGui::Begin("Unreal Docker Example", &bOpened))
	{
    // Write ImGui code here
	}
	ImGui::End();
	
	if(!bOpened)
	{
		UnrealDockerShowExample = 0;
	}
}

TStatId UUnrealDockerEditorExample::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UUnrealDockerEditorExample, STATGROUP_Tickables);
}

```
