// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class UnrealImGuiDocker : ModuleRules
{
	public UnrealImGuiDocker(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		string ImGuiPath = Path.Combine(PluginDirectory, "ThirdParty", "imgui");
		string ImPlotPath = Path.Combine(PluginDirectory, "ThirdParty", "implot");
		PublicIncludePaths.AddRange(
			new string[] {
				ImGuiPath,
				ImPlotPath
			}
		);
			
		
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				// ... add other public dependencies that you statically link with here ...
			}
			);

		bUseUnity = false;
		
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"CoreUObject",
				"Engine",
				"Slate",
				"SlateCore",
				"ApplicationCore",
				"UnrealEd",
				"InputCore", 
				"ModelingComponents",
				// ... add private dependencies that you statically link with here ...	
			}
			);
	}
}
