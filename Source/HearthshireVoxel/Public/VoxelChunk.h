// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "VoxelTypes.h"
#include "Components/ActorComponent.h"
#include "VoxelChunk.generated.h"

// Forward declarations
class UProceduralMeshComponent;
class AVoxelWorld;
class UVoxelMaterialSet;

/**
 * Chunk LOD state
 */
UENUM(BlueprintType)
enum class EVoxelChunkLOD : uint8
{
    Unloaded = 0,
    LOD3 = 1,       // Billboard/Impostor
    LOD2 = 2,       // 1m equivalent
    LOD1 = 3,       // 50cm equivalent
    LOD0 = 4        // Full 25cm detail
};

/**
 * Chunk generation state
 */
UENUM(BlueprintType)
enum class EVoxelChunkState : uint8
{
    Uninitialized = 0,
    Generating = 1,
    Generated = 2,
    Meshing = 3,
    Ready = 4,
    Unloading = 5
};

/**
 * Voxel chunk component - handles mesh generation and rendering
 */
UCLASS(ClassGroup=(Voxel), meta=(BlueprintSpawnableComponent))
class HEARTHSHIREVOXEL_API UVoxelChunkComponent : public UActorComponent
{
    GENERATED_BODY()
    
public:
    UVoxelChunkComponent();
    
    // Component overrides
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
    
    // Chunk initialization
    UFUNCTION(BlueprintCallable, Category = "Voxel")
    void Initialize(const FIntVector& InChunkPosition, const FVoxelChunkSize& InChunkSize);
    
    // Voxel manipulation
    UFUNCTION(BlueprintCallable, Category = "Voxel")
    void SetVoxel(int32 X, int32 Y, int32 Z, EVoxelMaterial Material);
    
    UFUNCTION(BlueprintCallable, Category = "Voxel")
    EVoxelMaterial GetVoxel(int32 X, int32 Y, int32 Z) const;
    
    UFUNCTION(BlueprintCallable, Category = "Voxel")
    void SetVoxelBatch(const TArray<FIntVector>& Positions, const TArray<EVoxelMaterial>& Materials);
    
    // Mesh generation
    UFUNCTION(BlueprintCallable, Category = "Voxel")
    void GenerateMesh(bool bAsync = true);
    
    UFUNCTION(BlueprintCallable, Category = "Voxel")
    void ClearMesh();
    
    // LOD management
    UFUNCTION(BlueprintCallable, Category = "Voxel")
    void SetLOD(EVoxelChunkLOD NewLOD);
    
    UFUNCTION(BlueprintCallable, Category = "Voxel")
    EVoxelChunkLOD GetCurrentLOD() const { return CurrentLOD; }
    
    // State management
    UFUNCTION(BlueprintCallable, Category = "Voxel")
    EVoxelChunkState GetState() const { return ChunkState; }
    
    UFUNCTION(BlueprintCallable, Category = "Voxel")
    bool IsReady() const { return ChunkState == EVoxelChunkState::Ready; }
    
    // Chunk data access
    UFUNCTION(BlueprintCallable, Category = "Voxel")
    FIntVector GetChunkPosition() const { return ChunkData.ChunkPosition; }
    
    UFUNCTION(BlueprintCallable, Category = "Voxel")
    FVoxelChunkSize GetChunkSize() const { return ChunkData.ChunkSize; }
    
    // Performance stats
    UFUNCTION(BlueprintCallable, Category = "Voxel")
    FVoxelPerformanceStats GetPerformanceStats() const { return PerformanceStats; }
    
    // Events
    DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnChunkGenerated, UVoxelChunkComponent*, Chunk);
    UPROPERTY(BlueprintAssignable, Category = "Voxel")
    FOnChunkGenerated OnChunkGenerated;
    
    DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnChunkUpdated, UVoxelChunkComponent*, Chunk);
    UPROPERTY(BlueprintAssignable, Category = "Voxel")
    FOnChunkUpdated OnChunkUpdated;
    
protected:
    // Chunk data
    UPROPERTY()
    FVoxelChunkData ChunkData;
    
    // Mesh data
    UPROPERTY()
    FVoxelMeshData MeshData;
    
    // Current state
    UPROPERTY()
    EVoxelChunkState ChunkState;
    
    // Current LOD
    UPROPERTY()
    EVoxelChunkLOD CurrentLOD;
    
    // Procedural mesh component
    UPROPERTY()
    UProceduralMeshComponent* ProceduralMesh;
    
    // Material set
    UPROPERTY()
    UVoxelMaterialSet* MaterialSet;
    
    // Performance tracking
    UPROPERTY()
    FVoxelPerformanceStats PerformanceStats;
    
    // Owner world
    UPROPERTY()
    AVoxelWorld* OwnerWorld;
    
    // Async mesh generation
    void GenerateMeshAsync();
    void ApplyMeshData();
    
    // LOD mesh generation
    void GenerateLOD0Mesh();
    void GenerateLOD1Mesh();
    void GenerateLOD2Mesh();
    void GenerateLOD3Mesh();
    
    // Helper functions
    void UpdatePerformanceStats();
    
public:
    // Voxel size in world units (25cm = 25.0f)
    static constexpr float VoxelSize = 25.0f;
    
private:
    // Thread-safe mesh generation flag
    FThreadSafeBool bIsGeneratingMesh;
    
    // Cached world position
    FVector WorldPosition;
};

/**
 * Voxel chunk actor - manages a single chunk in the world
 */
UCLASS()
class HEARTHSHIREVOXEL_API AVoxelChunk : public AActor
{
    GENERATED_BODY()
    
public:
    AVoxelChunk();
    
    // Actor overrides
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void Tick(float DeltaTime) override;
    
    // Chunk component
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Voxel", meta = (AllowPrivateAccess = "true"))
    UVoxelChunkComponent* ChunkComponent;
    
    // Initialization
    UFUNCTION(BlueprintCallable, Category = "Voxel")
    void InitializeChunk(const FIntVector& ChunkPosition, const FVoxelChunkSize& ChunkSize, AVoxelWorld* World);
    
    // Pool management
    UFUNCTION(BlueprintCallable, Category = "Voxel")
    void ReturnToPool();
    
    UFUNCTION(BlueprintCallable, Category = "Voxel")
    void ResetChunk();
    
    // Distance calculations
    UFUNCTION(BlueprintCallable, Category = "Voxel")
    float GetDistanceToPlayer() const;
    
    UFUNCTION(BlueprintCallable, Category = "Voxel")
    bool ShouldBeLoaded(float MaxDistance) const;
    
    // Debug rendering
    UFUNCTION(BlueprintCallable, Category = "Voxel", CallInEditor)
    void ToggleDebugRendering();
    
protected:
    // Pool management
    UPROPERTY()
    bool bIsPooled;
    
    // Debug visualization
    UPROPERTY(EditAnywhere, Category = "Debug")
    bool bShowDebugInfo;
    
    UPROPERTY(EditAnywhere, Category = "Debug")
    bool bShowChunkBounds;
    
    UPROPERTY(EditAnywhere, Category = "Debug")
    bool bShowVoxelGrid;
    
    // Owner world reference
    UPROPERTY()
    AVoxelWorld* OwnerWorld;
    
    // Helper functions
    void DrawDebugInfo() const;
    void UpdateLODBasedOnDistance();
    
private:
    // Cached player reference
    APawn* CachedPlayerPawn;
    
    // Last update time
    float LastLODUpdateTime;
    
    // LOD update frequency
    static constexpr float LODUpdateInterval = 0.5f;
};