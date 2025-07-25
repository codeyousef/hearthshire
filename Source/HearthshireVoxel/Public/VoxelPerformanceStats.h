// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Stats/Stats.h"
#include "VoxelPerformanceStats.generated.h"

// Stat group for voxel performance
DECLARE_STATS_GROUP(TEXT("VoxelSystem"), STATGROUP_VoxelSystem, STATCAT_Advanced);

// Individual stat declarations
DECLARE_CYCLE_STAT_EXTERN(TEXT("Voxel Mesh Generation"), STAT_VoxelMeshGeneration, STATGROUP_VoxelSystem, HEARTHSHIREVOXEL_API);
DECLARE_CYCLE_STAT_EXTERN(TEXT("Greedy Meshing"), STAT_GreedyMeshing, STATGROUP_VoxelSystem, HEARTHSHIREVOXEL_API);
DECLARE_CYCLE_STAT_EXTERN(TEXT("Chunk Update"), STAT_ChunkUpdate, STATGROUP_VoxelSystem, HEARTHSHIREVOXEL_API);
DECLARE_CYCLE_STAT_EXTERN(TEXT("Voxel Memory"), STAT_VoxelMemory, STATGROUP_VoxelSystem, HEARTHSHIREVOXEL_API);

DECLARE_DWORD_COUNTER_STAT_EXTERN(TEXT("Active Chunks"), STAT_ActiveChunks, STATGROUP_VoxelSystem, HEARTHSHIREVOXEL_API);
DECLARE_DWORD_COUNTER_STAT_EXTERN(TEXT("Total Triangles"), STAT_TotalTriangles, STATGROUP_VoxelSystem, HEARTHSHIREVOXEL_API);
DECLARE_DWORD_COUNTER_STAT_EXTERN(TEXT("Total Vertices"), STAT_TotalVertices, STATGROUP_VoxelSystem, HEARTHSHIREVOXEL_API);
DECLARE_FLOAT_COUNTER_STAT_EXTERN(TEXT("Triangle Reduction %"), STAT_TriangleReduction, STATGROUP_VoxelSystem, HEARTHSHIREVOXEL_API);

/**
 * Performance monitoring utilities
 */
class HEARTHSHIREVOXEL_API FVoxelPerformanceMonitor
{
public:
    // Singleton access
    static FVoxelPerformanceMonitor& Get();
    
    // Start/stop monitoring
    void StartMonitoring();
    void StopMonitoring();
    bool IsMonitoring() const { return bIsMonitoring; }
    
    // Record performance data
    void RecordMeshGeneration(float TimeMs, int32 TriangleCount, int32 VertexCount);
    void RecordGreedyMeshing(float TimeMs, float ReductionPercent);
    void RecordChunkUpdate(int32 ActiveChunks, float MemoryMB);
    
    // Get performance report
    FString GetPerformanceReport() const;
    
    // CSV logging
    void EnableCSVLogging(bool bEnable);
    void DumpCSVData(const FString& FilePath) const;
    
private:
    FVoxelPerformanceMonitor();
    ~FVoxelPerformanceMonitor();
    
    bool bIsMonitoring;
    bool bCSVLoggingEnabled;
    
    // Performance history
    struct FPerformanceFrame
    {
        double Timestamp;
        float MeshGenerationMs;
        float GreedyMeshingMs;
        int32 TriangleCount;
        int32 VertexCount;
        float TriangleReductionPercent;
        int32 ActiveChunks;
        float MemoryUsageMB;
    };
    
    TArray<FPerformanceFrame> PerformanceHistory;
    mutable FCriticalSection HistoryLock;
    
    // Current frame data
    FPerformanceFrame CurrentFrame;
    
    // Statistics
    float AverageMeshGenerationMs;
    float AverageGreedyMeshingMs;
    float AverageTriangleReduction;
    float PeakMemoryUsageMB;
    
    void UpdateStatistics();
};

/**
 * Blueprint-exposed performance data
 */
USTRUCT(BlueprintType)
struct HEARTHSHIREVOXEL_API FVoxelPerformanceReport
{
    GENERATED_BODY()
    
    UPROPERTY(BlueprintReadOnly, Category = "Performance")
    float AverageMeshGenerationMs;
    
    UPROPERTY(BlueprintReadOnly, Category = "Performance")
    float AverageGreedyMeshingMs;
    
    UPROPERTY(BlueprintReadOnly, Category = "Performance")
    float AverageTriangleReduction;
    
    UPROPERTY(BlueprintReadOnly, Category = "Performance")
    float CurrentMemoryUsageMB;
    
    UPROPERTY(BlueprintReadOnly, Category = "Performance")
    float PeakMemoryUsageMB;
    
    UPROPERTY(BlueprintReadOnly, Category = "Performance")
    int32 TotalChunksGenerated;
    
    UPROPERTY(BlueprintReadOnly, Category = "Performance")
    FString PerformanceSummary;
    
    FVoxelPerformanceReport()
    {
        AverageMeshGenerationMs = 0.0f;
        AverageGreedyMeshingMs = 0.0f;
        AverageTriangleReduction = 0.0f;
        CurrentMemoryUsageMB = 0.0f;
        PeakMemoryUsageMB = 0.0f;
        TotalChunksGenerated = 0;
    }
};