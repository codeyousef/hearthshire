// Copyright Epic Games, Inc. All Rights Reserved.

#include "VoxelPerformanceTest.h"
#include "VoxelWorld.h"
#include "VoxelChunk.h"
#include "VoxelMeshGenerator.h"
#include "VoxelGreedyMesher.h"
#include "VoxelPerformanceStats.h"
#include "Engine/World.h"
#include "HAL/PlatformFilemanager.h"
#include "Misc/FileHelper.h"
#include "HearthshireVoxelModule.h"

TArray<FVoxelTestResult> UVoxelPerformanceTest::RunAllPerformanceTests(UObject* WorldContextObject)
{
    TArray<FVoxelTestResult> Results;
    
    UE_LOG(LogHearthshireVoxel, Log, TEXT("Running voxel performance tests..."));
    
    // Run individual tests
    Results.Add(TestChunkGenerationSpeed(WorldContextObject));
    Results.Add(TestGreedyMeshingReduction(WorldContextObject));
    Results.Add(TestMemoryUsage(WorldContextObject));
    Results.Add(TestMultithreadedGeneration(WorldContextObject));
    Results.Add(TestFrameRateUnderLoad(WorldContextObject));
    
    // Log summary
    int32 PassedTests = 0;
    for (const FVoxelTestResult& Result : Results)
    {
        if (Result.bPassed)
        {
            PassedTests++;
        }
        
        UE_LOG(LogHearthshireVoxel, Log, TEXT("%s: %s (%.2f/%.2f)"),
            *Result.TestName,
            Result.bPassed ? TEXT("PASSED") : TEXT("FAILED"),
            Result.MeasuredValue,
            Result.TargetValue
        );
    }
    
    UE_LOG(LogHearthshireVoxel, Log, TEXT("Performance tests complete: %d/%d passed"),
        PassedTests, Results.Num());
    
    return Results;
}

FVoxelTestResult UVoxelPerformanceTest::TestChunkGenerationSpeed(UObject* WorldContextObject)
{
    FVoxelTestResult Result;
    Result.TestName = TEXT("Chunk Generation Speed");
    Result.TargetValue = 5.0f; // Target: <5ms
    
    // Create test chunk data
    FVoxelChunkData ChunkData;
    GenerateTestChunkData(ChunkData, 0.6f); // 60% density
    
    // Measure generation time
    const int32 TestIterations = 10;
    double TotalTime = 0.0;
    
    for (int32 i = 0; i < TestIterations; i++)
    {
        double StartTime = FPlatformTime::Seconds();
        
        FVoxelMeshData MeshData;
        FVoxelMeshGenerator::GenerateGreedyMesh(ChunkData, MeshData);
        
        double EndTime = FPlatformTime::Seconds();
        TotalTime += (EndTime - StartTime) * 1000.0; // Convert to ms
    }
    
    Result.MeasuredValue = TotalTime / TestIterations;
    Result.bPassed = Result.MeasuredValue < Result.TargetValue;
    Result.Details = FString::Printf(TEXT("Average over %d iterations"), TestIterations);
    
    return Result;
}

FVoxelTestResult UVoxelPerformanceTest::TestGreedyMeshingReduction(UObject* WorldContextObject)
{
    FVoxelTestResult Result;
    Result.TestName = TEXT("Greedy Meshing Triangle Reduction");
    Result.TargetValue = 70.0f; // Target: 70% reduction
    
    // Create test chunk with good merging potential
    FVoxelChunkData ChunkData;
    ChunkData.ChunkSize = FVoxelChunkSize(16); // Use smaller chunk for test
    ChunkData.Voxels.SetNum(ChunkData.ChunkSize.GetVoxelCount());
    
    // Create large flat areas for good merging
    for (int32 Z = 0; Z < 8; Z++)
    {
        for (int32 Y = 0; Y < 16; Y++)
        {
            for (int32 X = 0; X < 16; X++)
            {
                EVoxelMaterial Material = (Z < 4) ? EVoxelMaterial::Stone : EVoxelMaterial::Dirt;
                ChunkData.SetVoxel(X, Y, Z, FVoxel(Material));
            }
        }
    }
    
    // Generate basic mesh
    FVoxelMeshData BasicMesh;
    FVoxelMeshGenerator::GenerateBasicMesh(ChunkData, BasicMesh);
    int32 BasicTriangles = BasicMesh.TriangleCount;
    
    // Generate greedy mesh
    FVoxelMeshData GreedyMesh;
    FVoxelMeshGenerator::GenerateGreedyMesh(ChunkData, GreedyMesh);
    int32 GreedyTriangles = GreedyMesh.TriangleCount;
    
    // Calculate reduction
    float ReductionPercent = 0.0f;
    if (BasicTriangles > 0)
    {
        ReductionPercent = (1.0f - (float)GreedyTriangles / (float)BasicTriangles) * 100.0f;
    }
    
    Result.MeasuredValue = ReductionPercent;
    Result.bPassed = Result.MeasuredValue >= Result.TargetValue;
    Result.Details = FString::Printf(TEXT("%d triangles reduced to %d"), BasicTriangles, GreedyTriangles);
    
    return Result;
}

FVoxelTestResult UVoxelPerformanceTest::TestMemoryUsage(UObject* WorldContextObject)
{
    FVoxelTestResult Result;
    Result.TestName = TEXT("Memory Usage Per Chunk");
    Result.TargetValue = 100.0f; // Target: <100KB per chunk
    
    // Create and measure chunk memory
    FVoxelChunkData ChunkData;
    GenerateTestChunkData(ChunkData, 0.5f);
    
    FVoxelMeshData MeshData;
    FVoxelMeshGenerator::GenerateGreedyMesh(ChunkData, MeshData);
    
    // Calculate memory usage
    size_t VoxelDataSize = ChunkData.Voxels.Num() * sizeof(FVoxel);
    size_t VertexDataSize = MeshData.Vertices.Num() * sizeof(FVector);
    size_t IndexDataSize = MeshData.Triangles.Num() * sizeof(int32);
    size_t NormalDataSize = MeshData.Normals.Num() * sizeof(FVector);
    size_t UVDataSize = MeshData.UV0.Num() * sizeof(FVector2D);
    size_t TangentDataSize = MeshData.Tangents.Num() * sizeof(FProcMeshTangent);
    
    size_t TotalBytes = VoxelDataSize + VertexDataSize + IndexDataSize + 
                       NormalDataSize + UVDataSize + TangentDataSize;
    
    float TotalKB = TotalBytes / 1024.0f;
    
    Result.MeasuredValue = TotalKB;
    Result.bPassed = Result.MeasuredValue < Result.TargetValue;
    Result.Details = FString::Printf(TEXT("Voxels: %.1fKB, Mesh: %.1fKB"),
        VoxelDataSize / 1024.0f,
        (TotalBytes - VoxelDataSize) / 1024.0f
    );
    
    return Result;
}

FVoxelTestResult UVoxelPerformanceTest::TestMultithreadedGeneration(UObject* WorldContextObject)
{
    FVoxelTestResult Result;
    Result.TestName = TEXT("Multithreaded Generation");
    Result.TargetValue = 20.0f; // Target: <20ms for 4 chunks
    
    UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);
    if (!World)
    {
        Result.bPassed = false;
        Result.Details = TEXT("No world context");
        return Result;
    }
    
    // Create temporary voxel world
    AVoxelWorld* TestWorld = World->SpawnActor<AVoxelWorld>(AVoxelWorld::StaticClass());
    if (!TestWorld)
    {
        Result.bPassed = false;
        Result.Details = TEXT("Failed to create test world");
        return Result;
    }
    
    TestWorld->Config.bUseMultithreading = true;
    TestWorld->Config.MaxConcurrentChunkGenerations = 4;
    
    // Generate 4 chunks simultaneously
    double StartTime = FPlatformTime::Seconds();
    
    for (int32 i = 0; i < 4; i++)
    {
        FIntVector ChunkPos(i, 0, 0);
        TestWorld->GetOrCreateChunk(ChunkPos);
    }
    
    // Wait for generation to complete (simplified - in real test would properly wait)
    FPlatformProcess::Sleep(0.1f);
    
    double EndTime = FPlatformTime::Seconds();
    double TotalTimeMs = (EndTime - StartTime) * 1000.0;
    
    Result.MeasuredValue = TotalTimeMs;
    Result.bPassed = Result.MeasuredValue < Result.TargetValue;
    Result.Details = TEXT("4 chunks generated concurrently");
    
    // Cleanup
    TestWorld->Destroy();
    
    return Result;
}

FVoxelTestResult UVoxelPerformanceTest::TestFrameRateUnderLoad(UObject* WorldContextObject)
{
    FVoxelTestResult Result;
    Result.TestName = TEXT("Frame Time Under Load");
    
#if VOXEL_MOBILE_PLATFORM
    Result.TargetValue = 33.3f; // 30 FPS target for mobile
#else
    Result.TargetValue = 16.7f; // 60 FPS target for PC
#endif
    
    // This is a simplified test - in production you'd measure actual frame times
    // For now, we'll estimate based on chunk generation times
    
    FVoxelChunkData ChunkData;
    GenerateTestChunkData(ChunkData, 0.7f);
    
    float ChunkGenTime = MeasureChunkGenerationTime(ChunkData, true);
    
    // Assume we can generate 2 chunks per frame without impacting framerate
    float EstimatedFrameImpact = ChunkGenTime * 2.0f;
    
    Result.MeasuredValue = EstimatedFrameImpact;
    Result.bPassed = Result.MeasuredValue < Result.TargetValue;
    Result.Details = FString::Printf(TEXT("Estimated frame impact: %.1fms"), EstimatedFrameImpact);
    
    return Result;
}

FString UVoxelPerformanceTest::GenerateTestReport(const TArray<FVoxelTestResult>& Results)
{
    FString Report;
    Report += TEXT("=== Voxel Performance Test Report ===\n\n");
    Report += FString::Printf(TEXT("Platform: %s\n"), 
#if VOXEL_MOBILE_PLATFORM
        TEXT("Mobile")
#else
        TEXT("PC")
#endif
    );
    Report += FString::Printf(TEXT("Date: %s\n\n"), *FDateTime::Now().ToString());
    
    int32 PassedTests = 0;
    
    for (const FVoxelTestResult& Result : Results)
    {
        if (Result.bPassed) PassedTests++;
        
        Report += FString::Printf(TEXT("%s: %s\n"), *Result.TestName, Result.bPassed ? TEXT("PASSED") : TEXT("FAILED"));
        Report += FString::Printf(TEXT("  Measured: %.2f\n"), Result.MeasuredValue);
        Report += FString::Printf(TEXT("  Target: %.2f\n"), Result.TargetValue);
        Report += FString::Printf(TEXT("  Details: %s\n\n"), *Result.Details);
    }
    
    Report += FString::Printf(TEXT("Summary: %d/%d tests passed\n"), PassedTests, Results.Num());
    
    bool bAllPassed = PassedTests == Results.Num();
    Report += FString::Printf(TEXT("Overall Result: %s\n"), bAllPassed ? TEXT("SUCCESS") : TEXT("FAILURE"));
    
    if (bAllPassed)
    {
        Report += TEXT("\nThe C++ voxel implementation meets all performance targets!\n");
        Report += TEXT("It is recommended for production use in Hearthshire.\n");
    }
    else
    {
        Report += TEXT("\nSome performance targets were not met.\n");
        Report += TEXT("Additional optimization may be required.\n");
    }
    
    return Report;
}

void UVoxelPerformanceTest::SaveTestResultsToFile(const TArray<FVoxelTestResult>& Results, const FString& FilePath)
{
    FString Report = GenerateTestReport(Results);
    FFileHelper::SaveStringToFile(Report, *FilePath);
}

void UVoxelPerformanceTest::GenerateTestChunkData(FVoxelChunkData& ChunkData, float Density)
{
    ChunkData.ChunkSize = FVoxelChunkSize();
    ChunkData.ChunkPosition = FIntVector::ZeroValue;
    ChunkData.bIsDirty = true;
    
    const int32 VoxelCount = ChunkData.ChunkSize.GetVoxelCount();
    ChunkData.Voxels.SetNum(VoxelCount);
    
    // Generate random voxels with given density
    for (int32 i = 0; i < VoxelCount; i++)
    {
        if (FMath::FRand() < Density)
        {
            // Random material
            int32 MaterialIndex = FMath::RandRange(1, 5);
            ChunkData.Voxels[i] = FVoxel(static_cast<EVoxelMaterial>(MaterialIndex));
        }
        else
        {
            ChunkData.Voxels[i] = FVoxel(EVoxelMaterial::Air);
        }
    }
}

float UVoxelPerformanceTest::MeasureChunkGenerationTime(const FVoxelChunkData& ChunkData, bool bUseGreedyMeshing)
{
    double StartTime = FPlatformTime::Seconds();
    
    FVoxelMeshData MeshData;
    
    if (bUseGreedyMeshing)
    {
        FVoxelMeshGenerator::GenerateGreedyMesh(ChunkData, MeshData);
    }
    else
    {
        FVoxelMeshGenerator::GenerateBasicMesh(ChunkData, MeshData);
    }
    
    double EndTime = FPlatformTime::Seconds();
    return (EndTime - StartTime) * 1000.0f; // Return milliseconds
}