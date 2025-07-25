// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class HearthshireVoxel : ModuleRules
{
    public HearthshireVoxel(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        
        // Optimize for performance
        OptimizeCode = CodeOptimization.InShippingBuildsOnly;
        
        PublicIncludePaths.AddRange(
            new string[] {
                "HearthshireVoxel/Public"
            }
        );
        
        PrivateIncludePaths.AddRange(
            new string[] {
                "HearthshireVoxel/Private"
            }
        );
        
        PublicDependencyModuleNames.AddRange(
            new string[] {
                "Core",
                "CoreUObject",
                "Engine",
                "RenderCore",
                "RHI",
                "ProceduralMeshComponent"
            }
        );
        
        PrivateDependencyModuleNames.AddRange(
            new string[] {
                "Slate",
                "SlateCore"
            }
        );
        
        if (Target.bBuildEditor)
        {
            PrivateDependencyModuleNames.AddRange(
                new string[] {
                    "UnrealEd",
                    "EditorSubsystem"
                }
            );
        }
        
        // Enable SIMD optimizations
        if (Target.Platform == UnrealTargetPlatform.Win64 || 
            Target.Platform == UnrealTargetPlatform.Mac || 
            Target.Platform == UnrealTargetPlatform.Linux)
        {
            PublicDefinitions.Add("VOXEL_USE_SIMD=1");
        }
        else
        {
            PublicDefinitions.Add("VOXEL_USE_SIMD=0");
        }
        
        // Platform-specific optimizations
        if (Target.Platform == UnrealTargetPlatform.Android || 
            Target.Platform == UnrealTargetPlatform.IOS)
        {
            PublicDefinitions.Add("VOXEL_MOBILE_PLATFORM=1");
            PublicDefinitions.Add("VOXEL_DEFAULT_CHUNK_SIZE=16");
        }
        else
        {
            PublicDefinitions.Add("VOXEL_MOBILE_PLATFORM=0");
            PublicDefinitions.Add("VOXEL_DEFAULT_CHUNK_SIZE=32");
        }
        
        // Enable multithreading
        PublicDefinitions.Add("VOXEL_THREADSAFE=1");
        
        // Performance profiling
        if (Target.Configuration != UnrealTargetConfiguration.Shipping)
        {
            PublicDefinitions.Add("VOXEL_ENABLE_STATS=1");
        }
        else
        {
            PublicDefinitions.Add("VOXEL_ENABLE_STATS=0");
        }
    }
}