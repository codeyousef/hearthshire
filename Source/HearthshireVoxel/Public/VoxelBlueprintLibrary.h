// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "VoxelTypes.h"
#include "VoxelPerformanceStats.h"
#include "VoxelBlueprintLibrary.generated.h"

// Forward declarations
class AVoxelWorld;
class AVoxelChunk;

/**
 * Blueprint function library for voxel system
 */
UCLASS()
class HEARTHSHIREVOXEL_API UVoxelBlueprintLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
    
public:
    // World creation and management
    UFUNCTION(BlueprintCallable, Category = "Voxel|World", meta = (WorldContext = "WorldContextObject"))
    static AVoxelWorld* CreateVoxelWorld(
        UObject* WorldContextObject,
        const FTransform& Transform,
        const FVoxelWorldConfig& Config
    );
    
    UFUNCTION(BlueprintCallable, Category = "Voxel|World")
    static void DestroyVoxelWorld(AVoxelWorld* VoxelWorld);
    
    // Voxel manipulation
    UFUNCTION(BlueprintCallable, Category = "Voxel|Manipulation")
    static void SetVoxelAtWorldPosition(
        AVoxelWorld* VoxelWorld,
        const FVector& WorldPosition,
        EVoxelMaterial Material
    );
    
    UFUNCTION(BlueprintCallable, Category = "Voxel|Manipulation")
    static EVoxelMaterial GetVoxelAtWorldPosition(
        const AVoxelWorld* VoxelWorld,
        const FVector& WorldPosition
    );
    
    UFUNCTION(BlueprintCallable, Category = "Voxel|Manipulation")
    static void SetVoxelSphere(
        AVoxelWorld* VoxelWorld,
        const FVector& Center,
        float Radius,
        EVoxelMaterial Material
    );
    
    UFUNCTION(BlueprintCallable, Category = "Voxel|Manipulation")
    static void SetVoxelBox(
        AVoxelWorld* VoxelWorld,
        const FVector& MinCorner,
        const FVector& MaxCorner,
        EVoxelMaterial Material
    );
    
    // Additional batch operations
    UFUNCTION(BlueprintCallable, Category = "Voxel|Manipulation", meta = (DisplayName = "Set Voxel Cylinder"))
    static void SetVoxelCylinder(
        AVoxelWorld* VoxelWorld,
        const FVector& Base,
        const FVector& Direction,
        float Radius,
        float Height,
        EVoxelMaterial Material
    );
    
    UFUNCTION(BlueprintCallable, Category = "Voxel|Manipulation", meta = (DisplayName = "Set Voxel Cone"))
    static void SetVoxelCone(
        AVoxelWorld* VoxelWorld,
        const FVector& Base,
        const FVector& Direction,
        float BaseRadius,
        float Height,
        EVoxelMaterial Material
    );
    
    UFUNCTION(BlueprintCallable, Category = "Voxel|Manipulation", meta = (DisplayName = "Copy Voxel Region"))
    static TArray<FVoxel> CopyVoxelRegion(
        const AVoxelWorld* VoxelWorld,
        const FVector& MinCorner,
        const FVector& MaxCorner
    );
    
    UFUNCTION(BlueprintCallable, Category = "Voxel|Manipulation", meta = (DisplayName = "Paste Voxel Region"))
    static void PasteVoxelRegion(
        AVoxelWorld* VoxelWorld,
        const FVector& Position,
        const TArray<FVoxel>& VoxelData,
        const FIntVector& DataSize
    );
    
    // Chunk management
    UFUNCTION(BlueprintCallable, Category = "Voxel|Chunks")
    static AVoxelChunk* GetChunkAtWorldPosition(
        const AVoxelWorld* VoxelWorld,
        const FVector& WorldPosition
    );
    
    UFUNCTION(BlueprintCallable, Category = "Voxel|Chunks")
    static void RegenerateChunksInRadius(
        AVoxelWorld* VoxelWorld,
        const FVector& Center,
        float Radius
    );
    
    // Terrain generation helpers
    UFUNCTION(BlueprintCallable, Category = "Voxel|Generation")
    static void GenerateFlatTerrain(
        AVoxelWorld* VoxelWorld,
        const FVector& MinCorner,
        const FVector& MaxCorner,
        int32 GroundLevel,
        EVoxelMaterial GroundMaterial = EVoxelMaterial::Grass,
        EVoxelMaterial UndergroundMaterial = EVoxelMaterial::Dirt
    );
    
    UFUNCTION(BlueprintCallable, Category = "Voxel|Generation")
    static void GeneratePerlinTerrain(
        AVoxelWorld* VoxelWorld,
        const FVector& MinCorner,
        const FVector& MaxCorner,
        float NoiseScale = 0.01f,
        float HeightScale = 50.0f,
        int32 BaseHeight = 10
    );
    
    // Coordinate conversion
    UFUNCTION(BlueprintPure, Category = "Voxel|Coordinates")
    static FIntVector WorldToChunkPosition(
        const FVector& WorldPosition,
        int32 ChunkSize = 32,
        float VoxelSize = 25.0f
    );
    
    UFUNCTION(BlueprintPure, Category = "Voxel|Coordinates")
    static FIntVector WorldToVoxelPosition(
        const FVector& WorldPosition,
        float VoxelSize = 25.0f
    );
    
    UFUNCTION(BlueprintPure, Category = "Voxel|Coordinates")
    static FVector VoxelToWorldPosition(
        const FIntVector& VoxelPosition,
        float VoxelSize = 25.0f
    );
    
    // Performance monitoring
    UFUNCTION(BlueprintCallable, Category = "Voxel|Performance")
    static void StartPerformanceMonitoring();
    
    UFUNCTION(BlueprintCallable, Category = "Voxel|Performance")
    static void StopPerformanceMonitoring();
    
    UFUNCTION(BlueprintCallable, Category = "Voxel|Performance")
    static FVoxelPerformanceReport GetPerformanceReport();
    
    UFUNCTION(BlueprintCallable, Category = "Voxel|Performance")
    static void SavePerformanceReportToFile(const FString& FilePath);
    
    // Material management
    UFUNCTION(BlueprintCallable, Category = "Voxel|Materials")
    static UVoxelMaterialSet* CreateVoxelMaterialSet();
    
    UFUNCTION(BlueprintCallable, Category = "Voxel|Materials")
    static void SetVoxelMaterial(
        UVoxelMaterialSet* MaterialSet,
        EVoxelMaterial VoxelMaterial,
        UMaterialInterface* Material
    );
    
    // Utility functions
    UFUNCTION(BlueprintPure, Category = "Voxel|Utility")
    static float GetVoxelSize() { return 25.0f; }
    
    UFUNCTION(BlueprintPure, Category = "Voxel|Utility")
    static int32 GetDefaultChunkSize();
    
    UFUNCTION(BlueprintPure, Category = "Voxel|Utility")
    static bool IsMobilePlatform();
    
    UFUNCTION(BlueprintCallable, Category = "Voxel|Utility")
    static void OptimizeVoxelWorldForPlatform(
        AVoxelWorld* VoxelWorld,
        bool bForceMobileSettings = false
    );
    
    // Debug visualization
    UFUNCTION(BlueprintCallable, Category = "Voxel|Debug", meta = (WorldContext = "WorldContextObject"))
    static void DrawDebugVoxel(
        UObject* WorldContextObject,
        const FVector& VoxelPosition,
        float Size = 25.0f,
        const FLinearColor& Color = FLinearColor::White,
        float Duration = 0.0f
    );
    
    UFUNCTION(BlueprintCallable, Category = "Voxel|Debug", meta = (WorldContext = "WorldContextObject"))
    static void DrawDebugChunk(
        UObject* WorldContextObject,
        const FIntVector& ChunkPosition,
        int32 ChunkSize = 32,
        float VoxelSize = 25.0f,
        const FLinearColor& Color = FLinearColor::Green,
        float Duration = 0.0f
    );
    
    // Analysis functions
    UFUNCTION(BlueprintCallable, Category = "Voxel|Analysis", meta = (DisplayName = "Get Voxel Density In Region"))
    static float GetVoxelDensityInRegion(
        const AVoxelWorld* VoxelWorld,
        const FVector& MinCorner,
        const FVector& MaxCorner
    );
    
    UFUNCTION(BlueprintCallable, Category = "Voxel|Analysis", meta = (DisplayName = "Find Connected Voxels"))
    static TArray<FIntVector> FindConnectedVoxels(
        const AVoxelWorld* VoxelWorld,
        const FVector& StartPosition,
        EVoxelMaterial TargetMaterial,
        int32 MaxResults = 1000
    );
    
    // Visualization helpers
    UFUNCTION(BlueprintCallable, Category = "Voxel|Debug", meta = (WorldContext = "WorldContextObject", DisplayName = "Draw Debug Voxel Region"))
    static void DrawDebugVoxelRegion(
        UObject* WorldContextObject,
        const FVector& MinCorner,
        const FVector& MaxCorner,
        const FLinearColor& Color = FLinearColor::Yellow,
        float Duration = 0.0f,
        float Thickness = 2.0f
    );
    
    UFUNCTION(BlueprintCallable, Category = "Voxel|Debug", meta = (DisplayName = "Highlight Modified Chunks"))
    static void HighlightModifiedChunks(
        AVoxelWorld* VoxelWorld,
        const FLinearColor& Color = FLinearColor::Red,
        float Duration = 5.0f
    );
};