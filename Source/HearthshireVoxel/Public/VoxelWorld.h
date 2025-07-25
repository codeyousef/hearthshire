// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "VoxelTypes.h"
#include "Engine/World.h"
#include "VoxelWorld.generated.h"

// Forward declarations
class AVoxelChunk;
class UVoxelChunkComponent;
class UVoxelMaterialSet;

/**
 * Voxel world configuration
 */
USTRUCT(BlueprintType)
struct HEARTHSHIREVOXEL_API FVoxelWorldConfig
{
    GENERATED_BODY()
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel", meta = (ClampMin = "1", ClampMax = "64"))
    int32 ChunkSize;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel", meta = (ClampMin = "1", ClampMax = "20"))
    int32 ViewDistanceInChunks;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel", meta = (ClampMin = "10", ClampMax = "200"))
    int32 ChunkPoolSize;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel")
    TArray<FVoxelLODConfig> LODConfigs;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel")
    UVoxelMaterialSet* MaterialSet;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Performance")
    bool bUseMultithreading;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Performance", meta = (ClampMin = "1", ClampMax = "8"))
    int32 MaxConcurrentChunkGenerations;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Performance", meta = (ClampMin = "400", ClampMax = "2000"))
    int32 MobileMemoryBudgetMB;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Performance", meta = (ClampMin = "800", ClampMax = "4000"))
    int32 PCMemoryBudgetMB;
    
    FVoxelWorldConfig()
    {
#if VOXEL_MOBILE_PLATFORM
        ChunkSize = 16;
        ViewDistanceInChunks = 6;
        MobileMemoryBudgetMB = 400;
#else
        ChunkSize = 32;
        ViewDistanceInChunks = 10;
        PCMemoryBudgetMB = 800;
#endif
        ChunkPoolSize = 100;
        bUseMultithreading = true;
        MaxConcurrentChunkGenerations = 4;
        MaterialSet = nullptr;
        
        // Default LOD configuration
        FVoxelLODConfig LOD0;
        LOD0.Distance = 0.0f;
        LOD0.VoxelScale = 1.0f;
        LOD0.bUseGreedyMeshing = true;
        LOD0.bGenerateCollision = true;
        LODConfigs.Add(LOD0);
        
        FVoxelLODConfig LOD1;
        LOD1.Distance = 5000.0f;
        LOD1.VoxelScale = 2.0f;
        LOD1.bUseGreedyMeshing = true;
        LOD1.bGenerateCollision = true;
        LODConfigs.Add(LOD1);
        
        FVoxelLODConfig LOD2;
        LOD2.Distance = 10000.0f;
        LOD2.VoxelScale = 4.0f;
        LOD2.bUseGreedyMeshing = true;
        LOD2.bGenerateCollision = false;
        LODConfigs.Add(LOD2);
        
        FVoxelLODConfig LOD3;
        LOD3.Distance = 20000.0f;
        LOD3.VoxelScale = 8.0f;
        LOD3.bUseGreedyMeshing = false;
        LOD3.bGenerateCollision = false;
        LODConfigs.Add(LOD3);
    }
};

/**
 * Chunk generation task for multithreading
 */
USTRUCT()
struct FVoxelChunkTask
{
    GENERATED_BODY()
    
    FIntVector ChunkPosition;
    int32 Priority;
    bool bIsRegeneration;
    
    FVoxelChunkTask()
    {
        ChunkPosition = FIntVector::ZeroValue;
        Priority = 0;
        bIsRegeneration = false;
    }
};

/**
 * Main voxel world actor - manages all chunks
 */
UCLASS()
class HEARTHSHIREVOXEL_API AVoxelWorld : public AActor
{
    GENERATED_BODY()
    
public:
    AVoxelWorld();
    
    // Actor overrides
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void Tick(float DeltaTime) override;
    
    // Configuration
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel")
    FVoxelWorldConfig Config;
    
    // Chunk management
    UFUNCTION(BlueprintCallable, Category = "Voxel")
    AVoxelChunk* GetOrCreateChunk(const FIntVector& ChunkPosition);
    
    UFUNCTION(BlueprintCallable, Category = "Voxel")
    void UnloadChunk(const FIntVector& ChunkPosition);
    
    UFUNCTION(BlueprintCallable, Category = "Voxel")
    void RegenerateChunk(const FIntVector& ChunkPosition);
    
    // Voxel manipulation
    UFUNCTION(BlueprintCallable, Category = "Voxel")
    void SetVoxel(const FVector& WorldPosition, EVoxelMaterial Material);
    
    UFUNCTION(BlueprintCallable, Category = "Voxel")
    EVoxelMaterial GetVoxel(const FVector& WorldPosition) const;
    
    UFUNCTION(BlueprintCallable, Category = "Voxel")
    void SetVoxelSphere(const FVector& Center, float Radius, EVoxelMaterial Material);
    
    // World queries
    UFUNCTION(BlueprintCallable, Category = "Voxel")
    FIntVector WorldToChunkPosition(const FVector& WorldPosition) const;
    
    UFUNCTION(BlueprintCallable, Category = "Voxel")
    FIntVector WorldToLocalVoxel(const FVector& WorldPosition, const FIntVector& ChunkPosition) const;
    
    // Performance
    UFUNCTION(BlueprintCallable, Category = "Voxel")
    FVoxelPerformanceStats GetWorldStats() const;
    
    UFUNCTION(BlueprintCallable, Category = "Voxel")
    int32 GetActiveChunkCount() const { return ActiveChunks.Num(); }
    
    UFUNCTION(BlueprintCallable, Category = "Voxel")
    int32 GetPooledChunkCount() const { return ChunkPool.Num(); }
    
    // Events
    DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnChunkLoaded, const FIntVector&, ChunkPosition);
    UPROPERTY(BlueprintAssignable, Category = "Voxel")
    FOnChunkLoaded OnChunkLoaded;
    
    DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnChunkUnloaded, const FIntVector&, ChunkPosition);
    UPROPERTY(BlueprintAssignable, Category = "Voxel")
    FOnChunkUnloaded OnChunkUnloaded;
    
    // Chunk storage (made public for Blueprint access)
    UPROPERTY(BlueprintReadOnly, Category = "Voxel")
    TMap<FIntVector, AVoxelChunk*> ActiveChunks;
    
protected:
    
    UPROPERTY()
    TArray<AVoxelChunk*> ChunkPool;
    
    // Task queue for chunk generation
    TQueue<FVoxelChunkTask> ChunkTaskQueue;
    FCriticalSection TaskQueueLock;
    
    // Currently generating chunks
    TSet<FIntVector> GeneratingChunks;
    FCriticalSection GeneratingChunksLock;
    FThreadSafeCounter ActiveGenerations;
    
    // Performance tracking
    UPROPERTY()
    FVoxelPerformanceStats WorldStats;
    
    // Player tracking
    UPROPERTY()
    APawn* TrackedPlayer;
    FVector LastPlayerPosition;
    
    // Update frequencies
    float ChunkUpdateTimer;
    static constexpr float ChunkUpdateInterval = 0.1f;
    
    float MemoryCheckTimer;
    static constexpr float MemoryCheckInterval = 1.0f;
    
    // Internal functions
    void UpdateChunks();
    void ProcessChunkTasks();
    void UpdateMemoryUsage();
    void EnforceMemoryBudget();
    
    AVoxelChunk* GetChunkFromPool();
    void ReturnChunkToPool(AVoxelChunk* Chunk);
    
    void QueueChunkGeneration(const FIntVector& ChunkPosition, int32 Priority, bool bRegeneration = false);
    bool ShouldLoadChunk(const FIntVector& ChunkPosition) const;
    int32 CalculateChunkPriority(const FIntVector& ChunkPosition) const;
    
    void OnChunkGenerated(UVoxelChunkComponent* ChunkComponent);
    
private:
    // Voxel size constant (25cm)
    static constexpr float VoxelSize = 25.0f;
    
    // Maximum chunks to process per frame
    static constexpr int32 MaxChunksPerFrame = 5;
};

/**
 * Voxel world component for Blueprint usage
 */
UCLASS(ClassGroup=(Voxel), meta=(BlueprintSpawnableComponent))
class HEARTHSHIREVOXEL_API UVoxelWorldComponent : public UActorComponent
{
    GENERATED_BODY()
    
public:
    UVoxelWorldComponent();
    
    // Component overrides
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    
    // Configuration
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel")
    FVoxelWorldConfig Config;
    
    // Get the voxel world actor
    UFUNCTION(BlueprintCallable, Category = "Voxel")
    AVoxelWorld* GetVoxelWorld() const { return VoxelWorld; }
    
protected:
    UPROPERTY()
    AVoxelWorld* VoxelWorld;
};