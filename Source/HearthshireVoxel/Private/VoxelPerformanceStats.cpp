// Copyright Epic Games, Inc. All Rights Reserved.

#include "VoxelPerformanceStats.h"
#include "HAL/PlatformFilemanager.h"
#include "Misc/FileHelper.h"
#include "Misc/DateTime.h"

// Define stats
DEFINE_STAT(STAT_VoxelMeshGeneration);
DEFINE_STAT(STAT_GreedyMeshing);
DEFINE_STAT(STAT_ChunkUpdate);
DEFINE_STAT(STAT_VoxelMemory);
DEFINE_STAT(STAT_ActiveChunks);
DEFINE_STAT(STAT_TotalTriangles);
DEFINE_STAT(STAT_TotalVertices);
DEFINE_STAT(STAT_TriangleReduction);

// Singleton instance
static FVoxelPerformanceMonitor* GVoxelPerformanceMonitor = nullptr;

FVoxelPerformanceMonitor& FVoxelPerformanceMonitor::Get()
{
    if (!GVoxelPerformanceMonitor)
    {
        GVoxelPerformanceMonitor = new FVoxelPerformanceMonitor();
    }
    return *GVoxelPerformanceMonitor;
}

FVoxelPerformanceMonitor::FVoxelPerformanceMonitor()
{
    bIsMonitoring = false;
    bCSVLoggingEnabled = false;
    
    AverageMeshGenerationMs = 0.0f;
    AverageGreedyMeshingMs = 0.0f;
    AverageTriangleReduction = 0.0f;
    PeakMemoryUsageMB = 0.0f;
    
    CurrentFrame = FPerformanceFrame();
}

FVoxelPerformanceMonitor::~FVoxelPerformanceMonitor()
{
}

void FVoxelPerformanceMonitor::StartMonitoring()
{
    FScopeLock Lock(&HistoryLock);
    
    bIsMonitoring = true;
    PerformanceHistory.Empty();
    CurrentFrame = FPerformanceFrame();
}

void FVoxelPerformanceMonitor::StopMonitoring()
{
    FScopeLock Lock(&HistoryLock);
    
    bIsMonitoring = false;
    UpdateStatistics();
}

void FVoxelPerformanceMonitor::RecordMeshGeneration(float TimeMs, int32 TriangleCount, int32 VertexCount)
{
    if (!bIsMonitoring)
        return;
    
    FScopeLock Lock(&HistoryLock);
    
    CurrentFrame.MeshGenerationMs = TimeMs;
    CurrentFrame.TriangleCount = TriangleCount;
    CurrentFrame.VertexCount = VertexCount;
    CurrentFrame.Timestamp = FPlatformTime::Seconds();
    
    SET_FLOAT_STAT(STAT_VoxelMeshGeneration, TimeMs);
    SET_DWORD_STAT(STAT_TotalTriangles, TriangleCount);
    SET_DWORD_STAT(STAT_TotalVertices, VertexCount);
}

void FVoxelPerformanceMonitor::RecordGreedyMeshing(float TimeMs, float ReductionPercent)
{
    if (!bIsMonitoring)
        return;
    
    FScopeLock Lock(&HistoryLock);
    
    CurrentFrame.GreedyMeshingMs = TimeMs;
    CurrentFrame.TriangleReductionPercent = ReductionPercent;
    
    SET_FLOAT_STAT(STAT_GreedyMeshing, TimeMs);
    SET_FLOAT_STAT(STAT_TriangleReduction, ReductionPercent);
}

void FVoxelPerformanceMonitor::RecordChunkUpdate(int32 ActiveChunks, float MemoryMB)
{
    if (!bIsMonitoring)
        return;
    
    FScopeLock Lock(&HistoryLock);
    
    CurrentFrame.ActiveChunks = ActiveChunks;
    CurrentFrame.MemoryUsageMB = MemoryMB;
    
    // Add frame to history
    PerformanceHistory.Add(CurrentFrame);
    
    // Keep only last 1000 frames
    if (PerformanceHistory.Num() > 1000)
    {
        PerformanceHistory.RemoveAt(0);
    }
    
    // Update peak memory
    if (MemoryMB > PeakMemoryUsageMB)
    {
        PeakMemoryUsageMB = MemoryMB;
    }
    
    SET_DWORD_STAT(STAT_ActiveChunks, ActiveChunks);
    SET_FLOAT_STAT(STAT_VoxelMemory, MemoryMB);
    
    // Reset current frame
    CurrentFrame = FPerformanceFrame();
}

FString FVoxelPerformanceMonitor::GetPerformanceReport() const
{
    FScopeLock Lock(&HistoryLock);
    
    const_cast<FVoxelPerformanceMonitor*>(this)->UpdateStatistics();
    
    FString Report;
    Report += TEXT("=== Voxel Performance Report ===\n");
    Report += FString::Printf(TEXT("Monitoring Duration: %.1f seconds\n"), 
        PerformanceHistory.Num() > 0 ? PerformanceHistory.Last().Timestamp - PerformanceHistory[0].Timestamp : 0.0);
    Report += FString::Printf(TEXT("Frames Recorded: %d\n\n"), PerformanceHistory.Num());
    
    Report += TEXT("Average Performance:\n");
    Report += FString::Printf(TEXT("  Mesh Generation: %.2f ms\n"), AverageMeshGenerationMs);
    Report += FString::Printf(TEXT("  Greedy Meshing: %.2f ms\n"), AverageGreedyMeshingMs);
    Report += FString::Printf(TEXT("  Triangle Reduction: %.1f%%\n"), AverageTriangleReduction);
    Report += FString::Printf(TEXT("\n"));
    
    Report += TEXT("Memory Usage:\n");
    Report += FString::Printf(TEXT("  Current: %.1f MB\n"), 
        PerformanceHistory.Num() > 0 ? PerformanceHistory.Last().MemoryUsageMB : 0.0f);
    Report += FString::Printf(TEXT("  Peak: %.1f MB\n"), PeakMemoryUsageMB);
    Report += FString::Printf(TEXT("\n"));
    
    // Find best and worst frames
    if (PerformanceHistory.Num() > 0)
    {
        float BestMeshTime = FLT_MAX;
        float WorstMeshTime = 0.0f;
        
        for (const FPerformanceFrame& Frame : PerformanceHistory)
        {
            if (Frame.MeshGenerationMs > 0)
            {
                BestMeshTime = FMath::Min(BestMeshTime, Frame.MeshGenerationMs);
                WorstMeshTime = FMath::Max(WorstMeshTime, Frame.MeshGenerationMs);
            }
        }
        
        Report += TEXT("Mesh Generation Times:\n");
        Report += FString::Printf(TEXT("  Best: %.2f ms\n"), BestMeshTime);
        Report += FString::Printf(TEXT("  Worst: %.2f ms\n"), WorstMeshTime);
        Report += FString::Printf(TEXT("\n"));
    }
    
    // Platform-specific targets
#if VOXEL_MOBILE_PLATFORM
    Report += TEXT("Platform: MOBILE\n");
    Report += TEXT("Target: <5ms greedy mesh, <400MB memory\n");
    
    bool bMeetsTarget = AverageGreedyMeshingMs < 5.0f && PeakMemoryUsageMB < 400.0f;
    Report += FString::Printf(TEXT("Status: %s\n"), bMeetsTarget ? TEXT("PASS") : TEXT("FAIL"));
#else
    Report += TEXT("Platform: PC\n");
    Report += TEXT("Target: <5ms greedy mesh, <800MB memory\n");
    
    bool bMeetsTarget = AverageGreedyMeshingMs < 5.0f && PeakMemoryUsageMB < 800.0f;
    Report += FString::Printf(TEXT("Status: %s\n"), bMeetsTarget ? TEXT("PASS") : TEXT("FAIL"));
#endif
    
    return Report;
}

void FVoxelPerformanceMonitor::EnableCSVLogging(bool bEnable)
{
    bCSVLoggingEnabled = bEnable;
}

void FVoxelPerformanceMonitor::DumpCSVData(const FString& FilePath) const
{
    FScopeLock Lock(&HistoryLock);
    
    FString CSVContent;
    CSVContent += TEXT("Timestamp,MeshGenerationMs,GreedyMeshingMs,TriangleCount,VertexCount,TriangleReduction%,ActiveChunks,MemoryMB\n");
    
    for (const FPerformanceFrame& Frame : PerformanceHistory)
    {
        CSVContent += FString::Printf(TEXT("%.3f,%.2f,%.2f,%d,%d,%.1f,%d,%.1f\n"),
            Frame.Timestamp,
            Frame.MeshGenerationMs,
            Frame.GreedyMeshingMs,
            Frame.TriangleCount,
            Frame.VertexCount,
            Frame.TriangleReductionPercent,
            Frame.ActiveChunks,
            Frame.MemoryUsageMB
        );
    }
    
    FFileHelper::SaveStringToFile(CSVContent, *FilePath);
}

void FVoxelPerformanceMonitor::UpdateStatistics()
{
    if (PerformanceHistory.Num() == 0)
        return;
    
    float TotalMeshGen = 0.0f;
    float TotalGreedyMesh = 0.0f;
    float TotalReduction = 0.0f;
    int32 ValidFrames = 0;
    
    for (const FPerformanceFrame& Frame : PerformanceHistory)
    {
        if (Frame.MeshGenerationMs > 0)
        {
            TotalMeshGen += Frame.MeshGenerationMs;
            TotalGreedyMesh += Frame.GreedyMeshingMs;
            TotalReduction += Frame.TriangleReductionPercent;
            ValidFrames++;
        }
    }
    
    if (ValidFrames > 0)
    {
        AverageMeshGenerationMs = TotalMeshGen / ValidFrames;
        AverageGreedyMeshingMs = TotalGreedyMesh / ValidFrames;
        AverageTriangleReduction = TotalReduction / ValidFrames;
    }
}