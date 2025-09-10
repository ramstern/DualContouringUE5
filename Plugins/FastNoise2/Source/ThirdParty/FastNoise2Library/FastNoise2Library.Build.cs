// Fill out your copyright notice in the Description page of Project Settings.

using System.IO;
using UnrealBuildTool;

public class FastNoise2Library : ModuleRules
{
	public FastNoise2Library(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;
		PublicSystemIncludePaths.Add("$(ModuleDir)/Public");

		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			bool bUseDebugLibs = false;//Target.Configuration == UnrealTargetConfiguration.Debug || Target.Configuration == UnrealTargetConfiguration.DebugGame;
            string LibConfigFolder = bUseDebugLibs ? "Debug" : "Release";

			string TargetName = bUseDebugLibs ? "FastNoiseD" : "FastNoise";

            // Add the import library
            PublicDefinitions.Add("FASTNOISE_STATIC_LIB=1");
            PublicAdditionalLibraries.Add(Path.Combine(ModuleDirectory, "x64", LibConfigFolder, "FastSIMD.lib"));
            PublicAdditionalLibraries.Add(Path.Combine(ModuleDirectory, "x64", LibConfigFolder, TargetName + ".lib"));
        }
		else if (Target.Platform == UnrealTargetPlatform.Mac)
		{
			PublicDelayLoadDLLs.Add(Path.Combine(ModuleDirectory, "Mac", "Release", "libExampleLibrary.dylib"));
			RuntimeDependencies.Add("$(PluginDir)/Source/ThirdParty/FastNoise2Library/Mac/Release/libExampleLibrary.dylib");
		}
		else if (Target.Platform == UnrealTargetPlatform.Linux)
		{
			string ExampleSoPath = Path.Combine("$(PluginDir)", "Binaries", "ThirdParty", "FastNoise2Library", "Linux", "x86_64-unknown-linux-gnu", "libExampleLibrary.so");
			PublicAdditionalLibraries.Add(ExampleSoPath);
			PublicDelayLoadDLLs.Add(ExampleSoPath);
			RuntimeDependencies.Add(ExampleSoPath);
		}
	}
}
