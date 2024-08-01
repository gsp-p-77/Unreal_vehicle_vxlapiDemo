// Fill out your copyright notice in the Description page of Project Settings.
using System.IO;
using UnrealBuildTool;

public class Unreal_vehicle_vxlapiDemo : ModuleRules
{
   public Unreal_vehicle_vxlapiDemo(ReadOnlyTargetRules Target) : base(Target)
   {
      PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
   
      PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore" });

      PrivateDependencyModuleNames.AddRange(new string[] {  });

      PublicAdditionalLibraries.Add(Path.Combine(ModuleDirectory, ".", "ThirdParty", "vxlapi64.lib"));
      // Uncomment if you are using Slate UI
      // PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });
      
      // Uncomment if you are using online features
      // PrivateDependencyModuleNames.Add("OnlineSubsystem");

      // To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
   }
}
