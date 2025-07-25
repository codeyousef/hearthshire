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
    
#if WITH_EDITOR
    virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
    
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
    
    // Get chunk data (const reference) - Not exposed to Blueprint
    const FVoxelChunkData& GetChunkData() const { return ChunkData; }
    
    // Set chunk data (for loading from templates) - Not exposed to Blueprint
    void SetChunkData(const FVoxelChunkData& NewChunkData);
    
    // Check if chunk has been generated
    UFUNCTION(BlueprintCallable, Category = "Voxel")
    bool HasBeenGenerated() const { return bHasBeenGenerated; }
    
    // Mark chunk as generated
    UFUNCTION(BlueprintCallable, Category = "Voxel")
    void MarkAsGenerated() { bHasBeenGenerated = true; }
    
    // Set material set
    void SetMaterialSet(UVoxelMaterialSet* InMaterialSet) { MaterialSet = InMaterialSet; }
    
    // Additional utility functions
    UFUNCTION(BlueprintCallable, Category = "Voxel", meta = (DisplayName = "Get Voxel Count"))
    int32 GetVoxelCount() const;
    
    UFUNCTION(BlueprintCallable, Category = "Voxel", meta = (DisplayName = "Get World Bounds"))
    FBox GetWorldBounds() const;
    
    UFUNCTION(BlueprintCallable, Category = "Voxel", meta = (DisplayName = "Regenerate Mesh Async"))
    void RegenerateMeshAsync();
    
    UFUNCTION(BlueprintCallable, Category = "Voxel", meta = (DisplayName = "Set Voxel Range"))
    void SetVoxelRange(const FIntVector& Min, const FIntVector& Max, EVoxelMaterial Material);
    
    // Mesh statistics
    UFUNCTION(BlueprintCallable, Category = "Voxel|Stats", meta = (DisplayName = "Get Triangle Count"))
    int32 GetTriangleCount() const { return MeshData.TriangleCount; }
    
    UFUNCTION(BlueprintCallable, Category = "Voxel|Stats", meta = (DisplayName = "Get Vertex Count"))
    int32 GetVertexCount() const { return MeshData.VertexCount; }
    
    // ===== Mesh Generation Functions =====
    UFUNCTION(BlueprintCallable, Category = "Voxel|Generation", meta = (DisplayName = "Generate Simple Mesh"))
    void GenerateSimpleMesh();
    
    UFUNCTION(BlueprintCallable, Category = "Voxel|Generation", meta = (DisplayName = "Generate Greedy Mesh"))
    void GenerateGreedyMesh();
    
    UFUNCTION(BlueprintCallable, Category = "Voxel|Generation", meta = (DisplayName = "Generate With Settings"))
    void GenerateWithSettings(bool bUseGreedy, bool bAsync, bool bGenerateCollisionMesh);
    
    // ===== Terrain Generation Functions =====
    UFUNCTION(BlueprintCallable, Category = "Voxel|Terrain", meta = (DisplayName = "Generate Flat Terrain"))
    void GenerateFlatTerrain(int32 GroundLevel, EVoxelMaterial GroundMaterial, EVoxelMaterial UndergroundMaterial);
    
    UFUNCTION(BlueprintCallable, Category = "Voxel|Terrain", meta = (DisplayName = "Generate Perlin Terrain"))
    void GeneratePerlinTerrain(float NoiseScale = 0.01f, float HeightScale = 10.0f, int32 Seed = 12345);
    
    UFUNCTION(BlueprintCallable, Category = "Voxel|Terrain", meta = (DisplayName = "Generate Cave System"))
    void GenerateCaveSystem(float CaveFrequency = 0.05f, float CaveSize = 0.3f);
    
    // ===== Bulk Operations =====
    UFUNCTION(BlueprintCallable, Category = "Voxel|Operations", meta = (DisplayName = "Set Voxel Sphere"))
    void SetVoxelSphere(const FVector& LocalCenter, float Radius, EVoxelMaterial Material, bool bAdditive = false);
    
    UFUNCTION(BlueprintCallable, Category = "Voxel|Operations", meta = (DisplayName = "Set Voxel Box"))
    void SetVoxelBox(const FIntVector& Min, const FIntVector& Max, EVoxelMaterial Material);
    
    UFUNCTION(BlueprintCallable, Category = "Voxel|Operations", meta = (DisplayName = "Paint Voxel Surface"))
    void PaintVoxelSurface(const FVector& LocalCenter, float Radius, EVoxelMaterial Material);
    
    // ===== Performance Functions =====
    UFUNCTION(BlueprintCallable, Category = "Voxel|Performance", meta = (DisplayName = "Run Performance Benchmark"))
    FString RunPerformanceBenchmark(int32 Iterations = 10);
    
    UFUNCTION(BlueprintCallable, Category = "Voxel|Performance", meta = (DisplayName = "Optimize Mesh"))
    void OptimizeMesh(float WeldThreshold = 0.01f);
    
    // ===== Editor Functions =====
    UFUNCTION(BlueprintCallable, Category = "Voxel|Editor", CallInEditor, meta = (DisplayName = "Regenerate In Editor"))
    void RegenerateInEditor();
    
    UFUNCTION(BlueprintCallable, Category = "Voxel|Editor", CallInEditor, meta = (DisplayName = "Run Benchmark"))
    void RunBenchmarkInEditor();
    
    UFUNCTION(BlueprintCallable, Category = "Voxel|Editor", CallInEditor, meta = (DisplayName = "Generate Checkerboard"))
    void GenerateCheckerboardPattern();
    
    UFUNCTION(BlueprintCallable, Category = "Voxel|Editor", CallInEditor, meta = (DisplayName = "Generate Sphere"))
    void GenerateSpherePattern();
    
    UFUNCTION(BlueprintCallable, Category = "Voxel|Editor", CallInEditor, meta = (DisplayName = "Fill Solid"))
    void FillSolid(EVoxelMaterial Material = EVoxelMaterial::Stone);
    
    UFUNCTION(BlueprintCallable, Category = "Voxel|Editor", CallInEditor, meta = (DisplayName = "Clear Chunk"))
    void ClearChunk();
    
    // ===== Helper Functions =====
    UFUNCTION(BlueprintPure, Category = "Voxel|Helpers", meta = (DisplayName = "Get Triangle Reduction"))
    float GetTriangleReductionPercentage() const;
    
    UFUNCTION(BlueprintPure, Category = "Voxel|Helpers", meta = (DisplayName = "Get Memory Estimate"))
    float GetMemoryUsageEstimate() const;
    
    UFUNCTION(BlueprintPure, Category = "Voxel|Helpers", meta = (DisplayName = "Is Voxel Solid"))
    bool IsVoxelSolid(int32 X, int32 Y, int32 Z) const;
    
    // ===== Component Access =====
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Voxel|Components", meta = (DisplayName = "Get Procedural Mesh Component"))
    UProceduralMeshComponent* GetProceduralMeshComponent() const { return ProceduralMesh; }
    
    // ===== Debug Functions =====
    UFUNCTION(BlueprintCallable, Category = "Voxel|Debug", CallInEditor, meta = (DisplayName = "Debug Mesh Info"))
    void DebugMeshInfo();
    
    UFUNCTION(BlueprintCallable, Category = "Voxel|Debug", meta = (DisplayName = "Force Opaque Rendering"))
    void ForceOpaqueRendering();
    
    UFUNCTION(BlueprintCallable, Category = "Voxel|Debug", meta = (DisplayName = "Validate Mesh Data"))
    void ValidateMeshData();
    
    UFUNCTION(BlueprintCallable, Category = "Voxel|Debug", meta = (DisplayName = "Validate Welded Mesh"))
    void ValidateWeldedMesh();
    
    UFUNCTION(BlueprintCallable, Category = "Voxel|Debug", CallInEditor, meta = (DisplayName = "Fix Mesh Normals"))
    void FixMeshNormals();
    
    UFUNCTION(BlueprintPure, Category = "Voxel|Helpers", meta = (DisplayName = "Get Surface Voxel Count"))
    int32 GetSurfaceVoxelCount() const;
    
    UFUNCTION(BlueprintPure, Category = "Voxel|Helpers", meta = (DisplayName = "World To Local Position"))
    FIntVector WorldToLocalVoxel(const FVector& WorldPos) const;
    
    UFUNCTION(BlueprintPure, Category = "Voxel|Helpers", meta = (DisplayName = "Local To World Position"))
    FVector LocalToWorldPosition(const FIntVector& LocalVoxel) const;
    
    // Events
    DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnChunkGenerated, UVoxelChunkComponent*, Chunk);
    UPROPERTY(BlueprintAssignable, Category = "Voxel")
    FOnChunkGenerated OnChunkGenerated;
    
    DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnChunkUpdated, UVoxelChunkComponent*, Chunk);
    UPROPERTY(BlueprintAssignable, Category = "Voxel")
    FOnChunkUpdated OnChunkUpdated;
    
    // Additional events
    DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnMeshGenerationStarted, UVoxelChunkComponent*, Chunk);
    UPROPERTY(BlueprintAssignable, Category = "Voxel")
    FOnMeshGenerationStarted OnMeshGenerationStarted;
    
    DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnMeshGenerationProgress, UVoxelChunkComponent*, Chunk, float, Progress);
    UPROPERTY(BlueprintAssignable, Category = "Voxel")
    FOnMeshGenerationProgress OnMeshGenerationProgress;
    
    DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnLODChanged, UVoxelChunkComponent*, Chunk, EVoxelChunkLOD, OldLOD, EVoxelChunkLOD, NewLOD);
    UPROPERTY(BlueprintAssignable, Category = "Voxel")
    FOnLODChanged OnLODChanged;
    
    // Additional events
    DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnVoxelChanged, const FIntVector&, Position, EVoxelMaterial, NewMaterial);
    UPROPERTY(BlueprintAssignable, Category = "Voxel|Events")
    FOnVoxelChanged OnVoxelChanged;
    
    DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnChunkOptimized, int32, OriginalTriangles, int32, OptimizedTriangles, float, ReductionPercent);
    UPROPERTY(BlueprintAssignable, Category = "Voxel|Events")
    FOnChunkOptimized OnChunkOptimized;
    
    DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnGenerationCompleted, float, GenerationTimeMs);
    UPROPERTY(BlueprintAssignable, Category = "Voxel|Events")
    FOnGenerationCompleted OnGenerationCompleted;
    
    // ===== Configuration Properties =====
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel|Configuration", meta = (DisplayName = "Voxel Size (cm)", ClampMin = "10.0", ClampMax = "100.0"))
    float ConfigurableVoxelSize = 25.0f;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel|Configuration|Mobile", meta = (DisplayName = "Mobile Chunk Size", ClampMin = "8", ClampMax = "32"))
    int32 MobileChunkSize = 16;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel|Configuration|Desktop", meta = (DisplayName = "Desktop Chunk Size", ClampMin = "16", ClampMax = "64"))
    int32 DesktopChunkSize = 32;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel|Performance", meta = (DisplayName = "Enable Greedy Meshing"))
    bool bEnableGreedyMeshing = true;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel|Performance", meta = (DisplayName = "Enable Mobile Optimizations"))
    bool bEnableMobileOptimizations = false;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel|Performance", meta = (DisplayName = "Enable Async Generation"))
    bool bEnableAsyncGeneration = true;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel|Performance", meta = (DisplayName = "Generate Collision"))
    bool bGenerateCollision = true;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel|Materials", meta = (DisplayName = "Material Set"))
    UVoxelMaterialSet* ConfiguredMaterialSet;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel|Debug", meta = (DisplayName = "Show Generation Stats"))
    bool bShowGenerationStats = false;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel|Debug", meta = (DisplayName = "Show Memory Usage"))
    bool bShowMemoryUsage = false;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel|Debug", meta = (DisplayName = "Debug Draw Color", HideAlphaChannel))
    FLinearColor DebugDrawColor = FLinearColor::Green;
    
    // ===== Runtime Properties =====
    UPROPERTY(BlueprintReadOnly, Category = "Voxel|Stats", meta = (DisplayName = "Current Vertex Count"))
    int32 RuntimeVertexCount = 0;
    
    UPROPERTY(BlueprintReadOnly, Category = "Voxel|Stats", meta = (DisplayName = "Current Triangle Count"))
    int32 RuntimeTriangleCount = 0;
    
    UPROPERTY(BlueprintReadOnly, Category = "Voxel|Stats", meta = (DisplayName = "Last Generation Time (ms)"))
    float LastGenerationTimeMs = 0.0f;
    
    UPROPERTY(BlueprintReadOnly, Category = "Voxel|Stats", meta = (DisplayName = "Memory Usage (MB)"))
    float MemoryUsageMB = 0.0f;
    
    UPROPERTY(BlueprintReadOnly, Category = "Voxel|Stats", meta = (DisplayName = "Triangle Reduction %"))
    float TriangleReductionPercentage = 0.0f;
    
    UPROPERTY(BlueprintReadOnly, Category = "Voxel|Stats", meta = (DisplayName = "Is Generating"))
    bool bIsCurrentlyGenerating = false;
    
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
    UPROPERTY(BlueprintReadOnly, Category = "Voxel|Components", meta = (AllowPrivateAccess = "true"))
    UProceduralMeshComponent* ProceduralMesh;
    
    // Flag to indicate if chunk has been manually generated
    UPROPERTY()
    bool bHasBeenGenerated;
    
    // Material set
    UPROPERTY(BlueprintReadOnly, Category = "Voxel|Components", meta = (AllowPrivateAccess = "true"))
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
    UFUNCTION(BlueprintCallable, Category = "Voxel", CallInEditor, meta = (DisplayName = "Toggle Debug Rendering"))
    void ToggleDebugRendering();
    
    // Additional editor functions
    UFUNCTION(BlueprintCallable, Category = "Voxel", CallInEditor, meta = (DisplayName = "Fill With Test Pattern"))
    void FillWithTestPattern();
    
    UFUNCTION(BlueprintCallable, Category = "Voxel", CallInEditor, meta = (DisplayName = "Clear All Voxels"))
    void ClearAllVoxels();
    
    UFUNCTION(BlueprintCallable, Category = "Voxel", CallInEditor, meta = (DisplayName = "Force Regenerate Mesh"))
    void ForceRegenerateMesh();
    
protected:
    // Pool management
    UPROPERTY()
    bool bIsPooled;
    
    // Debug visualization
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel|Debug", meta = (DisplayName = "Show Debug Info"))
    bool bShowDebugInfo;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel|Debug", meta = (DisplayName = "Show Chunk Bounds"))
    bool bShowChunkBounds;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel|Debug", meta = (DisplayName = "Show Voxel Grid"))
    bool bShowVoxelGrid;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel|Debug", meta = (DisplayName = "Grid Display Step", ClampMin = "1", ClampMax = "8"))
    int32 GridDisplayStep = 4;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel|Debug", meta = (DisplayName = "Show Performance Stats"))
    bool bShowPerformanceStats;
    
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