// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "VoxelTypes.h"
#include "VoxelPerformanceTest.generated.h"

/**
 * Automated performance test results
 */
USTRUCT(BlueprintType)
struct HEARTHSHIREVOXEL_API FVoxelTestResult
{
    GENERATED_BODY()
    
    UPROPERTY(BlueprintReadOnly)
    FString TestName;
    
    UPROPERTY(BlueprintReadOnly)
    bool bPassed;
    
    UPROPERTY(BlueprintReadOnly)
    float MeasuredValue;
    
    UPROPERTY(BlueprintReadOnly)
    float TargetValue;
    
    UPROPERTY(BlueprintReadOnly)
    FString Details;
    
    FVoxelTestResult()
    {
        bPassed = false;
        MeasuredValue = 0.0f;
        TargetValue = 0.0f;
    }
};

/**
 * Automated performance test runner
 */
UCLASS()
class HEARTHSHIREVOXEL_API UVoxelPerformanceTest : public UObject
{
    GENERATED_BODY()
    
public:
    // Run all performance tests
    UFUNCTION(BlueprintCallable, Category = "Voxel|Testing", meta = (WorldContext = "WorldContextObject"))
    static TArray<FVoxelTestResult> RunAllPerformanceTests(UObject* WorldContextObject);
    
    // Individual tests
    UFUNCTION(BlueprintCallable, Category = "Voxel|Testing", meta = (WorldContext = "WorldContextObject"))
    static FVoxelTestResult TestChunkGenerationSpeed(UObject* WorldContextObject);
    
    UFUNCTION(BlueprintCallable, Category = "Voxel|Testing", meta = (WorldContext = "WorldContextObject"))
    static FVoxelTestResult TestGreedyMeshingReduction(UObject* WorldContextObject);
    
    UFUNCTION(BlueprintCallable, Category = "Voxel|Testing", meta = (WorldContext = "WorldContextObject"))
    static FVoxelTestResult TestMemoryUsage(UObject* WorldContextObject);
    
    UFUNCTION(BlueprintCallable, Category = "Voxel|Testing", meta = (WorldContext = "WorldContextObject"))
    static FVoxelTestResult TestMultithreadedGeneration(UObject* WorldContextObject);
    
    UFUNCTION(BlueprintCallable, Category = "Voxel|Testing", meta = (WorldContext = "WorldContextObject"))
    static FVoxelTestResult TestFrameRateUnderLoad(UObject* WorldContextObject);
    
    // Utility functions
    UFUNCTION(BlueprintCallable, Category = "Voxel|Testing")
    static FString GenerateTestReport(const TArray<FVoxelTestResult>& Results);
    
    UFUNCTION(BlueprintCallable, Category = "Voxel|Testing")
    static void SaveTestResultsToFile(const TArray<FVoxelTestResult>& Results, const FString& FilePath);
    
private:
    // Helper functions
    static void GenerateTestChunkData(FVoxelChunkData& ChunkData, float Density = 0.5f);
    static float MeasureChunkGenerationTime(const FVoxelChunkData& ChunkData, bool bUseGreedyMeshing);
};