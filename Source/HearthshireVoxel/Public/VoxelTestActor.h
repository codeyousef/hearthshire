// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "VoxelTypes.h"
#include "VoxelWorld.h"
#include "VoxelTestActor.generated.h"

// Forward declarations
class UVoxelWorldComponent;

/**
 * Test actor for demonstrating voxel system usage
 * This can be used as a base for Blueprint actors
 */
UCLASS(Blueprintable)
class HEARTHSHIREVOXEL_API AVoxelTestActor : public AActor
{
    GENERATED_BODY()
    
public:
    AVoxelTestActor();
    
    // Actor overrides
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaTime) override;
    
    // Voxel world component
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Voxel")
    UVoxelWorldComponent* VoxelWorldComponent;
    
    // Configuration
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel")
    FVoxelWorldConfig WorldConfig;
    
    // Test functions exposed to Blueprint
    UFUNCTION(BlueprintCallable, Category = "Voxel|Test")
    void GenerateTestTerrain();
    
    UFUNCTION(BlueprintCallable, Category = "Voxel|Test")
    void ClearTerrain();
    
    UFUNCTION(BlueprintCallable, Category = "Voxel|Test")
    void CreateSphereAt(const FVector& Location, float Radius, EVoxelMaterial Material);
    
    UFUNCTION(BlueprintCallable, Category = "Voxel|Test")
    void CreateBoxAt(const FVector& MinCorner, const FVector& MaxCorner, EVoxelMaterial Material);
    
    UFUNCTION(BlueprintCallable, Category = "Voxel|Test")
    void RunPerformanceTest();
    
    UFUNCTION(BlueprintCallable, Category = "Voxel|Test")
    FString GetPerformanceResults() const;
    
    // Blueprint events
    UFUNCTION(BlueprintImplementableEvent, Category = "Voxel")
    void OnTerrainGenerated();
    
    UFUNCTION(BlueprintImplementableEvent, Category = "Voxel")
    void OnPerformanceTestComplete(const FString& Results);
    
protected:
    // Performance test state
    bool bIsRunningPerformanceTest;
    float PerformanceTestStartTime;
    int32 ChunksGenerated;
    
    // Helper functions
    void GenerateSimpleHills();
    void GenerateCaves();
    
private:
    // Cached reference to voxel world
    UPROPERTY()
    class AVoxelWorld* CachedVoxelWorld;
};