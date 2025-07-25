// Copyright Epic Games, Inc. All Rights Reserved.

#include "VoxelChunk.h"
#include "ProceduralMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "DrawDebugHelpers.h"
#include "Async/Async.h"
#include "Logging/LogMacros.h"
#include "HearthshireVoxelModule.h"
#include "VoxelMeshGenerator.h"
#include "VoxelGreedyMesher.h"
#include "VoxelWorld.h"
#include "Math/UnrealMathUtility.h"
#include "KismetProceduralMeshLibrary.h"

// VoxelChunkComponent Implementation

UVoxelChunkComponent::UVoxelChunkComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
    
    ChunkState = EVoxelChunkState::Uninitialized;
    CurrentLOD = EVoxelChunkLOD::LOD0; // Default to LOD0 for editor visibility
    MaterialSet = nullptr;
    OwnerWorld = nullptr;
    bIsGeneratingMesh = false;
    WorldPosition = FVector::ZeroVector;
    
    // Initialize with default chunk size for editor-placed actors
    ChunkData.ChunkSize = FVoxelChunkSize(32); // Default 32x32x32
    ChunkData.ChunkPosition = FIntVector::ZeroValue;
    ChunkData.bIsDirty = true;
    
    // Pre-allocate voxel array with default size
    const int32 VoxelCount = ChunkData.ChunkSize.GetVoxelCount();
    ChunkData.Voxels.SetNum(VoxelCount);
    
    UE_LOG(LogHearthshireVoxel, Log, TEXT("VoxelChunkComponent constructed with default size: %dx%dx%d"), 
        ChunkData.ChunkSize.X, ChunkData.ChunkSize.Y, ChunkData.ChunkSize.Z);
}

void UVoxelChunkComponent::BeginPlay()
{
    Super::BeginPlay();
    
    // Find or create procedural mesh component
    AActor* Owner = GetOwner();
    if (Owner)
    {
        ProceduralMesh = Owner->FindComponentByClass<UProceduralMeshComponent>();
        if (!ProceduralMesh)
        {
            ProceduralMesh = NewObject<UProceduralMeshComponent>(Owner, TEXT("ProceduralMesh"));
            ProceduralMesh->RegisterComponent();
            Owner->AddInstanceComponent(ProceduralMesh);
            ProceduralMesh->AttachToComponent(Owner->GetRootComponent(), FAttachmentTransformRules::KeepRelativeTransform);
        }
        
        // Configure ProceduralMesh for opaque voxel rendering
        ProceduralMesh->bUseAsyncCooking = true;
        ProceduralMesh->bUseComplexAsSimpleCollision = false;
        ProceduralMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
        ProceduralMesh->SetCollisionResponseToAllChannels(ECR_Block);
        
        // Ensure opaque rendering
        ProceduralMesh->SetCastShadow(true);
        ProceduralMesh->bRenderCustomDepth = false;
        ProceduralMesh->bRenderInMainPass = true;
        ProceduralMesh->SetReceivesDecals(true);
        ProceduralMesh->bVisibleInReflectionCaptures = true;
        ProceduralMesh->bVisibleInRealTimeSkyCaptures = true;
        ProceduralMesh->bVisibleInRayTracing = true;
    }
    
    // Ensure chunk is initialized with proper defaults if not already done
    if (ChunkData.Voxels.Num() == 0)
    {
        UE_LOG(LogHearthshireVoxel, Warning, TEXT("Chunk data not initialized in BeginPlay, initializing with defaults"));
        Initialize(FIntVector::ZeroValue, FVoxelChunkSize(32));
    }
    
    UE_LOG(LogHearthshireVoxel, Log, TEXT("VoxelChunkComponent BeginPlay - ChunkSize: %dx%dx%d, LOD: %d"), 
        ChunkData.ChunkSize.X, ChunkData.ChunkSize.Y, ChunkData.ChunkSize.Z, (int32)CurrentLOD);
}

void UVoxelChunkComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    ClearMesh();
    Super::EndPlay(EndPlayReason);
}

void UVoxelChunkComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
}

int32 UVoxelChunkComponent::GetVoxelCount() const
{
    int32 Count = 0;
    for (const FVoxel& Voxel : ChunkData.Voxels)
    {
        if (!Voxel.IsAir())
        {
            Count++;
        }
    }
    return Count;
}

FBox UVoxelChunkComponent::GetWorldBounds() const
{
    const FVector ChunkWorldPos = FVector(ChunkData.ChunkPosition) * ChunkData.ChunkSize.X * VoxelSize;
    const FVector ChunkWorldSize = FVector(ChunkData.ChunkSize.ToIntVector()) * VoxelSize;
    return FBox(ChunkWorldPos, ChunkWorldPos + ChunkWorldSize);
}

void UVoxelChunkComponent::RegenerateMeshAsync()
{
    if (!bIsGeneratingMesh)
    {
        GenerateMeshAsync();
    }
}

void UVoxelChunkComponent::SetVoxelRange(const FIntVector& Min, const FIntVector& Max, EVoxelMaterial Material)
{
    const FIntVector ClampedMin = FIntVector(
        FMath::Max(0, Min.X),
        FMath::Max(0, Min.Y),
        FMath::Max(0, Min.Z)
    );
    
    const FIntVector ClampedMax = FIntVector(
        FMath::Min(ChunkData.ChunkSize.X - 1, Max.X),
        FMath::Min(ChunkData.ChunkSize.Y - 1, Max.Y),
        FMath::Min(ChunkData.ChunkSize.Z - 1, Max.Z)
    );
    
    for (int32 Z = ClampedMin.Z; Z <= ClampedMax.Z; Z++)
    {
        for (int32 Y = ClampedMin.Y; Y <= ClampedMax.Y; Y++)
        {
            for (int32 X = ClampedMin.X; X <= ClampedMax.X; X++)
            {
                ChunkData.SetVoxel(X, Y, Z, FVoxel(Material));
            }
        }
    }
    
    if (ChunkData.bIsDirty)
    {
        OnChunkUpdated.Broadcast(this);
    }
}

void UVoxelChunkComponent::Initialize(const FIntVector& InChunkPosition, const FVoxelChunkSize& InChunkSize)
{
    ChunkData.ChunkPosition = InChunkPosition;
    ChunkData.ChunkSize = InChunkSize;
    ChunkData.bIsDirty = true;
    
    // Allocate voxel data
    const int32 VoxelCount = InChunkSize.GetVoxelCount();
    ChunkData.Voxels.SetNum(VoxelCount);
    
    // Calculate world position
    WorldPosition = FVector(
        InChunkPosition.X * InChunkSize.X * VoxelSize,
        InChunkPosition.Y * InChunkSize.Y * VoxelSize,
        InChunkPosition.Z * InChunkSize.Z * VoxelSize
    );
    
    if (AActor* Owner = GetOwner())
    {
        Owner->SetActorLocation(WorldPosition);
    }
    
    ChunkState = EVoxelChunkState::Generating;
}

void UVoxelChunkComponent::SetVoxel(int32 X, int32 Y, int32 Z, EVoxelMaterial Material)
{
    EVoxelMaterial OldMaterial = ChunkData.GetVoxel(X, Y, Z).Material;
    ChunkData.SetVoxel(X, Y, Z, FVoxel(Material));
    
    if (OldMaterial != Material)
    {
        OnVoxelChanged.Broadcast(FIntVector(X, Y, Z), Material);
    }
    
    if (ChunkState == EVoxelChunkState::Ready)
    {
        OnChunkUpdated.Broadcast(this);
    }
}

EVoxelMaterial UVoxelChunkComponent::GetVoxel(int32 X, int32 Y, int32 Z) const
{
    return ChunkData.GetVoxel(X, Y, Z).Material;
}

void UVoxelChunkComponent::SetVoxelBatch(const TArray<FIntVector>& Positions, const TArray<EVoxelMaterial>& Materials)
{
    if (Positions.Num() != Materials.Num())
    {
        UE_LOG(LogHearthshireVoxel, Warning, TEXT("SetVoxelBatch: Position and Material arrays must have same length"));
        return;
    }
    
    for (int32 i = 0; i < Positions.Num(); i++)
    {
        const FIntVector& Pos = Positions[i];
        ChunkData.SetVoxel(Pos.X, Pos.Y, Pos.Z, FVoxel(Materials[i]));
    }
    
    if (ChunkState == EVoxelChunkState::Ready)
    {
        OnChunkUpdated.Broadcast(this);
    }
}

void UVoxelChunkComponent::GenerateMesh(bool bAsync)
{
    if (bIsGeneratingMesh)
    {
        UE_LOG(LogHearthshireVoxel, Warning, TEXT("Chunk already generating mesh"));
        return;
    }
    
    // Validate chunk data
    if (ChunkData.Voxels.Num() == 0)
    {
        UE_LOG(LogHearthshireVoxel, Error, TEXT("GenerateMesh: Chunk data not initialized!"));
        return;
    }
    
    // Count solid voxels for debugging
    int32 SolidVoxelCount = GetVoxelCount();
    UE_LOG(LogHearthshireVoxel, Log, TEXT("GenerateMesh: Starting mesh generation - Solid voxels: %d, LOD: %d"),
        SolidVoxelCount, (int32)CurrentLOD);
    
    ChunkState = EVoxelChunkState::Meshing;
    
    if (bAsync)
    {
        GenerateMeshAsync();
    }
    else
    {
        // Generate mesh based on current LOD
        switch (CurrentLOD)
        {
            case EVoxelChunkLOD::LOD0:
                GenerateLOD0Mesh();
                break;
            case EVoxelChunkLOD::LOD1:
                GenerateLOD1Mesh();
                break;
            case EVoxelChunkLOD::LOD2:
                GenerateLOD2Mesh();
                break;
            case EVoxelChunkLOD::LOD3:
                GenerateLOD3Mesh();
                break;
            case EVoxelChunkLOD::Unloaded:
                UE_LOG(LogHearthshireVoxel, Warning, TEXT("GenerateMesh: LOD is Unloaded, setting to LOD0"));
                CurrentLOD = EVoxelChunkLOD::LOD0;
                GenerateLOD0Mesh();
                break;
            default:
                UE_LOG(LogHearthshireVoxel, Error, TEXT("GenerateMesh: Unknown LOD level"));
                break;
        }
        
        ApplyMeshData();
    }
}

void UVoxelChunkComponent::ClearMesh()
{
    if (ProceduralMesh)
    {
        ProceduralMesh->ClearAllMeshSections();
    }
    
    MeshData.Clear();
    ChunkState = EVoxelChunkState::Uninitialized;
}

void UVoxelChunkComponent::SetLOD(EVoxelChunkLOD NewLOD)
{
    if (CurrentLOD != NewLOD)
    {
        EVoxelChunkLOD OldLOD = CurrentLOD;
        CurrentLOD = NewLOD;
        OnLODChanged.Broadcast(this, OldLOD, NewLOD);
        
        if (NewLOD == EVoxelChunkLOD::Unloaded)
        {
            ClearMesh();
        }
        else if (ChunkData.bIsDirty || ChunkState != EVoxelChunkState::Ready)
        {
            GenerateMesh(true);
        }
    }
}

void UVoxelChunkComponent::GenerateMeshAsync()
{
    if (bIsGeneratingMesh)
    {
        return;
    }
    
    bIsGeneratingMesh = true;
    OnMeshGenerationStarted.Broadcast(this);
    
    // Capture chunk data for async operation
    FVoxelChunkData AsyncChunkData = ChunkData;
    EVoxelChunkLOD AsyncLOD = CurrentLOD;
    
    Async(EAsyncExecution::ThreadPool, [this, AsyncChunkData, AsyncLOD]()
    {
        FVoxelMeshData AsyncMeshData;
        
        // Generate mesh based on LOD
        FVoxelMeshGenerator::FGenerationConfig Config;
        Config.VoxelSize = VoxelSize;
        Config.bGenerateCollision = (AsyncLOD == EVoxelChunkLOD::LOD0 || AsyncLOD == EVoxelChunkLOD::LOD1);
        Config.bGenerateTangents = true;
        Config.bOptimizeIndices = true;
        
        switch (AsyncLOD)
        {
            case EVoxelChunkLOD::LOD0:
                FVoxelMeshGenerator::GenerateGreedyMesh(AsyncChunkData, AsyncMeshData, Config);
                break;
                
            case EVoxelChunkLOD::LOD1:
            case EVoxelChunkLOD::LOD2:
            case EVoxelChunkLOD::LOD3:
                // TODO: Implement LOD generation
                FVoxelMeshGenerator::GenerateBasicMesh(AsyncChunkData, AsyncMeshData, Config);
                break;
                
            default:
                break;
        }
        
        // Return to game thread
        AsyncTask(ENamedThreads::GameThread, [this, AsyncMeshData]()
        {
            MeshData = AsyncMeshData;
            ApplyMeshData();
            bIsGeneratingMesh = false;
        });
    });
}

void UVoxelChunkComponent::ApplyMeshData()
{
    if (!ProceduralMesh)
    {
        UE_LOG(LogHearthshireVoxel, Error, TEXT("ApplyMeshData: No ProceduralMesh component!"));
        ChunkState = EVoxelChunkState::Ready;
        ChunkData.bIsDirty = false;
        OnChunkGenerated.Broadcast(this);
        OnGenerationCompleted.Broadcast(LastGenerationTimeMs);
        return;
    }
    
    UE_LOG(LogHearthshireVoxel, Log, TEXT("ApplyMeshData: Applying mesh with %d vertices, %d triangles"),
        MeshData.VertexCount, MeshData.TriangleCount);
    
    // Critical validation - check for invalid triangle indices
    bool bHasInvalidIndices = false;
    for (int32 i = 0; i < MeshData.Triangles.Num(); i++)
    {
        if (MeshData.Triangles[i] >= MeshData.Vertices.Num() || MeshData.Triangles[i] < 0)
        {
            bHasInvalidIndices = true;
            UE_LOG(LogHearthshireVoxel, Error, TEXT("ApplyMeshData: Invalid triangle index %d at position %d (max: %d)"),
                MeshData.Triangles[i], i, MeshData.Vertices.Num() - 1);
        }
    }
    
    if (bHasInvalidIndices)
    {
        UE_LOG(LogHearthshireVoxel, Error, TEXT("ApplyMeshData: ABORTING - Mesh has invalid triangle indices!"));
        ChunkState = EVoxelChunkState::Ready;
        ChunkData.bIsDirty = false;
        return;
    }
    
    // Validate mesh data before applying
    if (bShowGenerationStats)
    {
        ValidateMeshData();
        
        // Additional validation for greedy meshing
        if (bEnableGreedyMeshing)
        {
            ValidateWeldedMesh();
        }
    }
    
    // Check for vertex bounds before applying
    FBox MeshBounds(ForceInit);
    for (const FVector& Vertex : MeshData.Vertices)
    {
        MeshBounds += Vertex;
    }
    
    UE_LOG(LogHearthshireVoxel, Warning, TEXT("Mesh bounds before apply: Min=%s, Max=%s, Size=%s"), 
        *MeshBounds.Min.ToString(), *MeshBounds.Max.ToString(), *MeshBounds.GetSize().ToString());
    
    // Expected bounds for chunk with VoxelSize
    float ExpectedSize = FMath::Max(ChunkData.ChunkSize.X, FMath::Max(ChunkData.ChunkSize.Y, ChunkData.ChunkSize.Z)) * VoxelSize;
    if (MeshBounds.GetSize().GetMax() > ExpectedSize * 2.0f)
    {
        UE_LOG(LogHearthshireVoxel, Error, TEXT("MESH BOUNDS TOO LARGE! Expected ~%.1f, got %.1f"), 
            ExpectedSize, MeshBounds.GetSize().GetMax());
    }
    
    // Apply mesh data to procedural mesh component
    UVoxelMaterialSet* ActiveMaterialSet = MaterialSet ? MaterialSet : ConfiguredMaterialSet;
    if (!ActiveMaterialSet)
    {
        UE_LOG(LogHearthshireVoxel, Warning, TEXT("ApplyMeshData: No material set configured, mesh may not be visible!"));
    }
    
    FVoxelMeshGenerator::ApplyMeshToComponent(ProceduralMesh, MeshData, ActiveMaterialSet);
    
    // Update performance stats
    UpdatePerformanceStats();
    
    ChunkState = EVoxelChunkState::Ready;
    ChunkData.bIsDirty = false;
    OnChunkGenerated.Broadcast(this);
    OnGenerationCompleted.Broadcast(LastGenerationTimeMs);
    
    UE_LOG(LogHearthshireVoxel, Log, TEXT("ApplyMeshData: Mesh generation completed successfully"));
}

void UVoxelChunkComponent::GenerateLOD0Mesh()
{
    const double StartTime = FPlatformTime::Seconds();
    
    MeshData.Clear();
    
    UE_LOG(LogHearthshireVoxel, Log, TEXT("GenerateLOD0Mesh: Starting LOD0 mesh generation"));
    
    // Use configuration settings
    FVoxelMeshGenerator::FGenerationConfig Config;
    Config.VoxelSize = ConfigurableVoxelSize;
    Config.bGenerateCollision = bGenerateCollision;
    Config.bGenerateTangents = true;
    Config.bOptimizeIndices = true;
    
    if (bEnableGreedyMeshing)
    {
        UE_LOG(LogHearthshireVoxel, Log, TEXT("GenerateLOD0Mesh: Using greedy meshing"));
        FVoxelMeshGenerator::GenerateGreedyMesh(ChunkData, MeshData, Config);
    }
    else
    {
        UE_LOG(LogHearthshireVoxel, Log, TEXT("GenerateLOD0Mesh: Using basic meshing"));
        FVoxelMeshGenerator::GenerateBasicMesh(ChunkData, MeshData, Config);
    }
    
    MeshData.GenerationTimeMs = (FPlatformTime::Seconds() - StartTime) * 1000.0f;
    
    UE_LOG(LogHearthshireVoxel, Log, TEXT("GenerateLOD0Mesh: Generated %d vertices, %d triangles, %d material sections in %.2fms"),
        MeshData.VertexCount, MeshData.TriangleCount, MeshData.MaterialSections.Num(), MeshData.GenerationTimeMs);
}

void UVoxelChunkComponent::GenerateLOD1Mesh()
{
    // TODO: Implement 50cm equivalent mesh
}

void UVoxelChunkComponent::GenerateLOD2Mesh()
{
    // TODO: Implement 1m equivalent mesh
}

void UVoxelChunkComponent::GenerateLOD3Mesh()
{
    // TODO: Implement billboard/impostor mesh
}

void UVoxelChunkComponent::UpdatePerformanceStats()
{
    // Ensure mesh data counts are correct
    MeshData.VertexCount = MeshData.Vertices.Num();
    MeshData.TriangleCount = MeshData.Triangles.Num() / 3;
    
    PerformanceStats.MeshGenerationTimeMs = MeshData.GenerationTimeMs;
    PerformanceStats.TriangleCount = MeshData.TriangleCount;
    PerformanceStats.VertexCount = MeshData.VertexCount;
    
    // Update runtime properties
    RuntimeVertexCount = MeshData.VertexCount;
    RuntimeTriangleCount = MeshData.TriangleCount;
    LastGenerationTimeMs = MeshData.GenerationTimeMs;
    MemoryUsageMB = GetMemoryUsageEstimate();
    TriangleReductionPercentage = GetTriangleReductionPercentage();
    bIsCurrentlyGenerating = bIsGeneratingMesh;
    
    // Comprehensive mesh validation and debugging
    UE_LOG(LogHearthshireVoxel, Warning, TEXT("=== VOXEL MESH GENERATION DEBUG ==="));
    UE_LOG(LogHearthshireVoxel, Warning, TEXT("Chunk Size: %dx%dx%d"), ChunkData.ChunkSize.X, ChunkData.ChunkSize.Y, ChunkData.ChunkSize.Z);
    UE_LOG(LogHearthshireVoxel, Warning, TEXT("Total Voxels: %d"), ChunkData.Voxels.Num());
    UE_LOG(LogHearthshireVoxel, Warning, TEXT("Solid Voxels: %d"), GetVoxelCount());
    UE_LOG(LogHearthshireVoxel, Warning, TEXT("Vertices Generated: %d"), RuntimeVertexCount);
    UE_LOG(LogHearthshireVoxel, Warning, TEXT("Triangles: %d"), RuntimeTriangleCount);
    UE_LOG(LogHearthshireVoxel, Warning, TEXT("Material Sections: %d"), MeshData.MaterialSections.Num());
    UE_LOG(LogHearthshireVoxel, Warning, TEXT("Generation Time: %.2fms"), LastGenerationTimeMs);
    UE_LOG(LogHearthshireVoxel, Warning, TEXT("Triangle Reduction: %.1f%%"), TriangleReductionPercentage);
    
    // Validate vertex colors
    int32 TransparentVertices = 0;
    for (const FColor& Color : MeshData.VertexColors)
    {
        if (Color.A < 255)
        {
            TransparentVertices++;
        }
    }
    UE_LOG(LogHearthshireVoxel, Warning, TEXT("Vertex Colors: %d total, %d transparent (A < 255)"), 
        MeshData.VertexColors.Num(), TransparentVertices);
    
    // Log material sections
    for (const auto& MaterialSection : MeshData.MaterialSections)
    {
        UE_LOG(LogHearthshireVoxel, Warning, TEXT("  Material %d (Section %d)"), 
            (int32)MaterialSection.Key, MaterialSection.Value);
    }
    
    // Verify ProceduralMesh component bounds
    if (ProceduralMesh)
    {
        FBoxSphereBounds Bounds = ProceduralMesh->Bounds;
        UE_LOG(LogHearthshireVoxel, Warning, TEXT("Mesh Bounds: Center=(%s), Extent=(%s), Radius=%.2f"), 
            *Bounds.Origin.ToString(), *Bounds.BoxExtent.ToString(), Bounds.SphereRadius);
        
        UE_LOG(LogHearthshireVoxel, Warning, TEXT("ProceduralMesh Settings:"));
        UE_LOG(LogHearthshireVoxel, Warning, TEXT("  CastShadow: %s"), ProceduralMesh->CastShadow ? TEXT("True") : TEXT("False"));
        UE_LOG(LogHearthshireVoxel, Warning, TEXT("  RenderInMainPass: %s"), ProceduralMesh->bRenderInMainPass ? TEXT("True") : TEXT("False"));
        UE_LOG(LogHearthshireVoxel, Warning, TEXT("  Collision Enabled: %d"), (int32)ProceduralMesh->GetCollisionEnabled());
        UE_LOG(LogHearthshireVoxel, Warning, TEXT("  Num Sections: %d"), ProceduralMesh->GetNumSections());
        
        // Check material assignments
        for (int32 i = 0; i < ProceduralMesh->GetNumSections(); i++)
        {
            UMaterialInterface* Material = ProceduralMesh->GetMaterial(i);
            UE_LOG(LogHearthshireVoxel, Warning, TEXT("  Section %d Material: %s"), 
                i, Material ? *Material->GetName() : TEXT("NULL"));
        }
    }
    else
    {
        UE_LOG(LogHearthshireVoxel, Error, TEXT("ProceduralMesh component is NULL!"));
    }
    
    UE_LOG(LogHearthshireVoxel, Warning, TEXT("=== END VOXEL MESH DEBUG ==="));
}

#if WITH_EDITOR
void UVoxelChunkComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
    Super::PostEditChangeProperty(PropertyChangedEvent);
    
    if (PropertyChangedEvent.Property)
    {
        const FName PropertyName = PropertyChangedEvent.Property->GetFName();
        
        // Handle material set changes
        if (PropertyName == GET_MEMBER_NAME_CHECKED(UVoxelChunkComponent, ConfiguredMaterialSet))
        {
            MaterialSet = ConfiguredMaterialSet;
        }
        // Handle voxel size changes
        else if (PropertyName == GET_MEMBER_NAME_CHECKED(UVoxelChunkComponent, ConfigurableVoxelSize))
        {
            // Note: Changing voxel size requires chunk regeneration
            if (ChunkState == EVoxelChunkState::Ready)
            {
                GenerateMesh(bEnableAsyncGeneration);
            }
        }
    }
}
#endif

// ===== Mesh Generation Functions =====
void UVoxelChunkComponent::GenerateSimpleMesh()
{
    bEnableGreedyMeshing = false;
    GenerateMesh(bEnableAsyncGeneration);
}

void UVoxelChunkComponent::GenerateGreedyMesh()
{
    bEnableGreedyMeshing = true;
    GenerateMesh(bEnableAsyncGeneration);
}

void UVoxelChunkComponent::GenerateWithSettings(bool bUseGreedy, bool bAsync, bool bGenerateCollisionMesh)
{
    bEnableGreedyMeshing = bUseGreedy;
    bGenerateCollision = bGenerateCollisionMesh;
    GenerateMesh(bAsync);
}

// ===== Terrain Generation Functions =====
void UVoxelChunkComponent::GenerateFlatTerrain(int32 GroundLevel, EVoxelMaterial GroundMaterial, EVoxelMaterial UndergroundMaterial)
{
    const FVoxelChunkSize& Size = ChunkData.ChunkSize;
    
    for (int32 Z = 0; Z < Size.Z; Z++)
    {
        for (int32 Y = 0; Y < Size.Y; Y++)
        {
            for (int32 X = 0; X < Size.X; X++)
            {
                EVoxelMaterial Material = EVoxelMaterial::Air;
                
                if (Z < GroundLevel)
                {
                    Material = UndergroundMaterial;
                }
                else if (Z == GroundLevel)
                {
                    Material = GroundMaterial;
                }
                
                SetVoxel(X, Y, Z, Material);
            }
        }
    }
    
    GenerateMesh(bEnableAsyncGeneration);
}

void UVoxelChunkComponent::GeneratePerlinTerrain(float NoiseScale, float HeightScale, int32 Seed)
{
    const FVoxelChunkSize& Size = ChunkData.ChunkSize;
    const FIntVector& ChunkPos = ChunkData.ChunkPosition;
    
    FRandomStream RandomStream(Seed);
    
    for (int32 X = 0; X < Size.X; X++)
    {
        for (int32 Y = 0; Y < Size.Y; Y++)
        {
            // Calculate world position for noise sampling
            float WorldX = (ChunkPos.X * Size.X + X) * NoiseScale;
            float WorldY = (ChunkPos.Y * Size.Y + Y) * NoiseScale;
            
            // Generate height using Perlin noise
            float NoiseValue = FMath::PerlinNoise2D(FVector2D(WorldX, WorldY));
            int32 Height = FMath::Clamp(Size.Z / 2 + FMath::RoundToInt(NoiseValue * HeightScale), 0, Size.Z - 1);
            
            for (int32 Z = 0; Z < Size.Z; Z++)
            {
                EVoxelMaterial Material = EVoxelMaterial::Air;
                
                if (Z < Height - 3)
                {
                    Material = EVoxelMaterial::Stone;
                }
                else if (Z < Height - 1)
                {
                    Material = EVoxelMaterial::Dirt;
                }
                else if (Z < Height)
                {
                    Material = EVoxelMaterial::Grass;
                }
                
                SetVoxel(X, Y, Z, Material);
            }
        }
    }
    
    GenerateMesh(bEnableAsyncGeneration);
}

void UVoxelChunkComponent::GenerateCaveSystem(float CaveFrequency, float CaveSize)
{
    const FVoxelChunkSize& Size = ChunkData.ChunkSize;
    const FIntVector& ChunkPos = ChunkData.ChunkPosition;
    
    // First, check if chunk has any solid voxels
    bool bHasSolids = false;
    for (const FVoxel& Voxel : ChunkData.Voxels)
    {
        if (Voxel.IsSolid())
        {
            bHasSolids = true;
            break;
        }
    }
    
    if (!bHasSolids)
    {
        return; // No point carving caves in air
    }
    
    // Carve caves using 3D Perlin noise
    for (int32 X = 0; X < Size.X; X++)
    {
        for (int32 Y = 0; Y < Size.Y; Y++)
        {
            for (int32 Z = 0; Z < Size.Z; Z++)
            {
                float WorldX = (ChunkPos.X * Size.X + X) * CaveFrequency;
                float WorldY = (ChunkPos.Y * Size.Y + Y) * CaveFrequency;
                float WorldZ = (ChunkPos.Z * Size.Z + Z) * CaveFrequency;
                
                float NoiseValue = FMath::PerlinNoise3D(FVector(WorldX, WorldY, WorldZ));
                
                if (NoiseValue > CaveSize)
                {
                    SetVoxel(X, Y, Z, EVoxelMaterial::Air);
                }
            }
        }
    }
    
    GenerateMesh(bEnableAsyncGeneration);
}

// ===== Bulk Operations =====
void UVoxelChunkComponent::SetVoxelSphere(const FVector& LocalCenter, float Radius, EVoxelMaterial Material, bool bAdditive)
{
    const FVoxelChunkSize& Size = ChunkData.ChunkSize;
    const float RadiusSq = Radius * Radius;
    
    // Convert local center to voxel coordinates
    FIntVector CenterVoxel = WorldToLocalVoxel(LocalCenter);
    int32 VoxelRadius = FMath::CeilToInt(Radius / ConfigurableVoxelSize);
    
    for (int32 X = FMath::Max(0, CenterVoxel.X - VoxelRadius); X <= FMath::Min(Size.X - 1, CenterVoxel.X + VoxelRadius); X++)
    {
        for (int32 Y = FMath::Max(0, CenterVoxel.Y - VoxelRadius); Y <= FMath::Min(Size.Y - 1, CenterVoxel.Y + VoxelRadius); Y++)
        {
            for (int32 Z = FMath::Max(0, CenterVoxel.Z - VoxelRadius); Z <= FMath::Min(Size.Z - 1, CenterVoxel.Z + VoxelRadius); Z++)
            {
                FVector VoxelPos = LocalToWorldPosition(FIntVector(X, Y, Z));
                float DistSq = FVector::DistSquared(VoxelPos, LocalCenter);
                
                if (DistSq <= RadiusSq)
                {
                    if (bAdditive)
                    {
                        if (GetVoxel(X, Y, Z) == EVoxelMaterial::Air)
                        {
                            SetVoxel(X, Y, Z, Material);
                        }
                    }
                    else
                    {
                        SetVoxel(X, Y, Z, Material);
                    }
                }
            }
        }
    }
    
    GenerateMesh(bEnableAsyncGeneration);
}

void UVoxelChunkComponent::SetVoxelBox(const FIntVector& Min, const FIntVector& Max, EVoxelMaterial Material)
{
    SetVoxelRange(Min, Max, Material);
    GenerateMesh(bEnableAsyncGeneration);
}

void UVoxelChunkComponent::PaintVoxelSurface(const FVector& LocalCenter, float Radius, EVoxelMaterial Material)
{
    const FVoxelChunkSize& Size = ChunkData.ChunkSize;
    const float RadiusSq = Radius * Radius;
    
    FIntVector CenterVoxel = WorldToLocalVoxel(LocalCenter);
    int32 VoxelRadius = FMath::CeilToInt(Radius / ConfigurableVoxelSize);
    
    for (int32 X = FMath::Max(0, CenterVoxel.X - VoxelRadius); X <= FMath::Min(Size.X - 1, CenterVoxel.X + VoxelRadius); X++)
    {
        for (int32 Y = FMath::Max(0, CenterVoxel.Y - VoxelRadius); Y <= FMath::Min(Size.Y - 1, CenterVoxel.Y + VoxelRadius); Y++)
        {
            for (int32 Z = FMath::Max(0, CenterVoxel.Z - VoxelRadius); Z <= FMath::Min(Size.Z - 1, CenterVoxel.Z + VoxelRadius); Z++)
            {
                // Only paint surface voxels
                if (GetVoxel(X, Y, Z) != EVoxelMaterial::Air)
                {
                    // Check if it's a surface voxel (has air neighbor)
                    bool bIsSurface = false;
                    for (int32 dx = -1; dx <= 1 && !bIsSurface; dx++)
                    {
                        for (int32 dy = -1; dy <= 1 && !bIsSurface; dy++)
                        {
                            for (int32 dz = -1; dz <= 1 && !bIsSurface; dz++)
                            {
                                if (dx == 0 && dy == 0 && dz == 0) continue;
                                
                                int32 nx = X + dx;
                                int32 ny = Y + dy;
                                int32 nz = Z + dz;
                                
                                if (nx >= 0 && nx < Size.X && ny >= 0 && ny < Size.Y && nz >= 0 && nz < Size.Z)
                                {
                                    if (GetVoxel(nx, ny, nz) == EVoxelMaterial::Air)
                                    {
                                        bIsSurface = true;
                                    }
                                }
                            }
                        }
                    }
                    
                    if (bIsSurface)
                    {
                        FVector VoxelPos = LocalToWorldPosition(FIntVector(X, Y, Z));
                        float DistSq = FVector::DistSquared(VoxelPos, LocalCenter);
                        
                        if (DistSq <= RadiusSq)
                        {
                            SetVoxel(X, Y, Z, Material);
                        }
                    }
                }
            }
        }
    }
    
    GenerateMesh(bEnableAsyncGeneration);
}

// ===== Performance Functions =====
FString UVoxelChunkComponent::RunPerformanceBenchmark(int32 Iterations)
{
    if (Iterations <= 0) return TEXT("Invalid iteration count");
    
    TArray<float> SimpleTimes;
    TArray<float> GreedyTimes;
    TArray<int32> SimpleTriangles;
    TArray<int32> GreedyTriangles;
    
    // Backup current settings
    bool bOriginalGreedy = bEnableGreedyMeshing;
    bool bOriginalAsync = bEnableAsyncGeneration;
    
    // Force synchronous generation for accurate timing
    bEnableAsyncGeneration = false;
    
    // Test simple mesh generation
    bEnableGreedyMeshing = false;
    for (int32 i = 0; i < Iterations; i++)
    {
        double StartTime = FPlatformTime::Seconds();
        GenerateMesh(false);
        double EndTime = FPlatformTime::Seconds();
        
        SimpleTimes.Add((EndTime - StartTime) * 1000.0f);
        SimpleTriangles.Add(RuntimeTriangleCount);
    }
    
    // Test greedy mesh generation
    bEnableGreedyMeshing = true;
    for (int32 i = 0; i < Iterations; i++)
    {
        double StartTime = FPlatformTime::Seconds();
        GenerateMesh(false);
        double EndTime = FPlatformTime::Seconds();
        
        GreedyTimes.Add((EndTime - StartTime) * 1000.0f);
        GreedyTriangles.Add(RuntimeTriangleCount);
    }
    
    // Restore original settings
    bEnableGreedyMeshing = bOriginalGreedy;
    bEnableAsyncGeneration = bOriginalAsync;
    
    // Calculate averages
    float AvgSimpleTime = 0.0f;
    float AvgGreedyTime = 0.0f;
    int32 AvgSimpleTriangles = 0;
    int32 AvgGreedyTriangles = 0;
    
    for (int32 i = 0; i < Iterations; i++)
    {
        AvgSimpleTime += SimpleTimes[i];
        AvgGreedyTime += GreedyTimes[i];
        AvgSimpleTriangles += SimpleTriangles[i];
        AvgGreedyTriangles += GreedyTriangles[i];
    }
    
    AvgSimpleTime /= Iterations;
    AvgGreedyTime /= Iterations;
    AvgSimpleTriangles /= Iterations;
    AvgGreedyTriangles /= Iterations;
    
    float TriangleReduction = 100.0f * (1.0f - (float)AvgGreedyTriangles / (float)AvgSimpleTriangles);
    
    return FString::Printf(TEXT("Benchmark Results (%d iterations):\n"
                                "Simple Mesh: %.2fms, %d triangles\n"
                                "Greedy Mesh: %.2fms, %d triangles\n"
                                "Triangle Reduction: %.1f%%\n"
                                "Speed Difference: %.1fx"),
                           Iterations,
                           AvgSimpleTime, AvgSimpleTriangles,
                           AvgGreedyTime, AvgGreedyTriangles,
                           TriangleReduction,
                           AvgSimpleTime / AvgGreedyTime);
}

void UVoxelChunkComponent::OptimizeMesh(float WeldThreshold)
{
    // This would implement vertex welding and other optimizations
    // For now, just trigger a greedy mesh regeneration
    bool bOriginal = bEnableGreedyMeshing;
    bEnableGreedyMeshing = true;
    GenerateMesh(bEnableAsyncGeneration);
    bEnableGreedyMeshing = bOriginal;
    
    int32 OriginalTriangles = RuntimeTriangleCount;
    OnChunkOptimized.Broadcast(OriginalTriangles, RuntimeTriangleCount, GetTriangleReductionPercentage());
}

// ===== Editor Functions =====
void UVoxelChunkComponent::RegenerateInEditor()
{
    UE_LOG(LogHearthshireVoxel, Log, TEXT("RegenerateInEditor called"));
    
    // Ensure chunk is initialized
    if (ChunkData.Voxels.Num() == 0)
    {
        UE_LOG(LogHearthshireVoxel, Warning, TEXT("RegenerateInEditor: Chunk not initialized, initializing with defaults"));
        Initialize(FIntVector::ZeroValue, FVoxelChunkSize(32));
    }
    
    // Ensure we have a valid LOD
    if (CurrentLOD == EVoxelChunkLOD::Unloaded)
    {
        CurrentLOD = EVoxelChunkLOD::LOD0;
        UE_LOG(LogHearthshireVoxel, Log, TEXT("RegenerateInEditor: Setting LOD to LOD0"));
    }
    
    GenerateMesh(false);
}

void UVoxelChunkComponent::RunBenchmarkInEditor()
{
    FString Result = RunPerformanceBenchmark(5);
    UE_LOG(LogHearthshireVoxel, Log, TEXT("Benchmark Result:\n%s"), *Result);
}

void UVoxelChunkComponent::GenerateCheckerboardPattern()
{
    UE_LOG(LogHearthshireVoxel, Log, TEXT("GenerateCheckerboardPattern called"));
    
    // Ensure chunk is initialized
    if (ChunkData.Voxels.Num() == 0)
    {
        UE_LOG(LogHearthshireVoxel, Warning, TEXT("GenerateCheckerboardPattern: Chunk not initialized, initializing with defaults"));
        Initialize(FIntVector::ZeroValue, FVoxelChunkSize(32));
    }
    
    // Ensure we have a procedural mesh component
    if (!ProceduralMesh)
    {
        AActor* Owner = GetOwner();
        if (Owner)
        {
            ProceduralMesh = Owner->FindComponentByClass<UProceduralMeshComponent>();
            if (!ProceduralMesh)
            {
                ProceduralMesh = NewObject<UProceduralMeshComponent>(Owner, TEXT("ProceduralMesh"));
                ProceduralMesh->RegisterComponent();
                Owner->AddInstanceComponent(ProceduralMesh);
                ProceduralMesh->AttachToComponent(Owner->GetRootComponent(), FAttachmentTransformRules::KeepRelativeTransform);
                
                // Configure ProceduralMesh for opaque voxel rendering
                ProceduralMesh->bUseAsyncCooking = true;
                ProceduralMesh->bUseComplexAsSimpleCollision = false;
                ProceduralMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
                ProceduralMesh->SetCollisionResponseToAllChannels(ECR_Block);
                ProceduralMesh->SetCastShadow(true);
                ProceduralMesh->bRenderCustomDepth = false;
                ProceduralMesh->bRenderInMainPass = true;
                ProceduralMesh->SetReceivesDecals(true);
                ProceduralMesh->bVisibleInReflectionCaptures = true;
                ProceduralMesh->bVisibleInRealTimeSkyCaptures = true;
                ProceduralMesh->bVisibleInRayTracing = true;
                
                UE_LOG(LogHearthshireVoxel, Log, TEXT("GenerateCheckerboardPattern: Created ProceduralMesh component"));
            }
        }
    }
    
    // Ensure valid LOD
    if (CurrentLOD == EVoxelChunkLOD::Unloaded)
    {
        CurrentLOD = EVoxelChunkLOD::LOD0;
        UE_LOG(LogHearthshireVoxel, Log, TEXT("GenerateCheckerboardPattern: Setting LOD to LOD0"));
    }
    
    const FVoxelChunkSize& Size = ChunkData.ChunkSize;
    int32 SolidCount = 0;
    
    UE_LOG(LogHearthshireVoxel, Log, TEXT("GenerateCheckerboardPattern: Generating pattern for chunk size %dx%dx%d"),
        Size.X, Size.Y, Size.Z);
    
    for (int32 Z = 0; Z < Size.Z; Z++)
    {
        for (int32 Y = 0; Y < Size.Y; Y++)
        {
            for (int32 X = 0; X < Size.X; X++)
            {
                bool bIsSolid = ((X + Y + Z) % 2) == 0;
                EVoxelMaterial Material = bIsSolid ? EVoxelMaterial::Stone : EVoxelMaterial::Air;
                SetVoxel(X, Y, Z, Material);
                if (bIsSolid) SolidCount++;
            }
        }
    }
    
    UE_LOG(LogHearthshireVoxel, Log, TEXT("GenerateCheckerboardPattern: Set %d solid voxels, generating mesh..."), SolidCount);
    GenerateMesh(false);
}

void UVoxelChunkComponent::GenerateSpherePattern()
{
    UE_LOG(LogHearthshireVoxel, Log, TEXT("GenerateSpherePattern called"));
    
    // Ensure chunk is initialized
    if (ChunkData.Voxels.Num() == 0)
    {
        UE_LOG(LogHearthshireVoxel, Warning, TEXT("GenerateSpherePattern: Chunk not initialized, initializing with defaults"));
        Initialize(FIntVector::ZeroValue, FVoxelChunkSize(32));
    }
    
    // Ensure we have a procedural mesh component
    if (!ProceduralMesh)
    {
        AActor* Owner = GetOwner();
        if (Owner)
        {
            ProceduralMesh = Owner->FindComponentByClass<UProceduralMeshComponent>();
            if (!ProceduralMesh)
            {
                ProceduralMesh = NewObject<UProceduralMeshComponent>(Owner, TEXT("ProceduralMesh"));
                ProceduralMesh->RegisterComponent();
                Owner->AddInstanceComponent(ProceduralMesh);
                ProceduralMesh->AttachToComponent(Owner->GetRootComponent(), FAttachmentTransformRules::KeepRelativeTransform);
                
                // Configure ProceduralMesh for opaque voxel rendering
                ProceduralMesh->bUseAsyncCooking = true;
                ProceduralMesh->bUseComplexAsSimpleCollision = false;
                ProceduralMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
                ProceduralMesh->SetCollisionResponseToAllChannels(ECR_Block);
                ProceduralMesh->SetCastShadow(true);
                ProceduralMesh->bRenderCustomDepth = false;
                ProceduralMesh->bRenderInMainPass = true;
                ProceduralMesh->SetReceivesDecals(true);
                ProceduralMesh->bVisibleInReflectionCaptures = true;
                ProceduralMesh->bVisibleInRealTimeSkyCaptures = true;
                ProceduralMesh->bVisibleInRayTracing = true;
                
                UE_LOG(LogHearthshireVoxel, Log, TEXT("GenerateSpherePattern: Created ProceduralMesh component"));
            }
        }
    }
    
    // Ensure valid LOD
    if (CurrentLOD == EVoxelChunkLOD::Unloaded)
    {
        CurrentLOD = EVoxelChunkLOD::LOD0;
        UE_LOG(LogHearthshireVoxel, Log, TEXT("GenerateSpherePattern: Setting LOD to LOD0"));
    }
    
    const FVoxelChunkSize& Size = ChunkData.ChunkSize;
    FVector Center = FVector(Size.X, Size.Y, Size.Z) * 0.5f * ConfigurableVoxelSize;
    float Radius = FMath::Min3(Size.X, Size.Y, Size.Z) * 0.4f * ConfigurableVoxelSize;
    
    UE_LOG(LogHearthshireVoxel, Log, TEXT("GenerateSpherePattern: Generating sphere with radius %.1f"), Radius);
    
    // Clear first
    ClearChunk();
    
    // Generate sphere
    SetVoxelSphere(Center, Radius, EVoxelMaterial::Stone, false);
}

void UVoxelChunkComponent::FillSolid(EVoxelMaterial Material)
{
    UE_LOG(LogHearthshireVoxel, Log, TEXT("FillSolid called with material %d"), (int32)Material);
    
    // Ensure chunk is initialized
    if (ChunkData.Voxels.Num() == 0)
    {
        UE_LOG(LogHearthshireVoxel, Warning, TEXT("FillSolid: Chunk not initialized, initializing with defaults"));
        Initialize(FIntVector::ZeroValue, FVoxelChunkSize(32));
    }
    
    // Ensure we have a procedural mesh component
    if (!ProceduralMesh)
    {
        AActor* Owner = GetOwner();
        if (Owner)
        {
            ProceduralMesh = Owner->FindComponentByClass<UProceduralMeshComponent>();
            if (!ProceduralMesh)
            {
                ProceduralMesh = NewObject<UProceduralMeshComponent>(Owner, TEXT("ProceduralMesh"));
                ProceduralMesh->RegisterComponent();
                Owner->AddInstanceComponent(ProceduralMesh);
                ProceduralMesh->AttachToComponent(Owner->GetRootComponent(), FAttachmentTransformRules::KeepRelativeTransform);
                
                // Configure ProceduralMesh for opaque voxel rendering
                ProceduralMesh->bUseAsyncCooking = true;
                ProceduralMesh->bUseComplexAsSimpleCollision = false;
                ProceduralMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
                ProceduralMesh->SetCollisionResponseToAllChannels(ECR_Block);
                ProceduralMesh->SetCastShadow(true);
                ProceduralMesh->bRenderCustomDepth = false;
                ProceduralMesh->bRenderInMainPass = true;
                ProceduralMesh->SetReceivesDecals(true);
                ProceduralMesh->bVisibleInReflectionCaptures = true;
                ProceduralMesh->bVisibleInRealTimeSkyCaptures = true;
                ProceduralMesh->bVisibleInRayTracing = true;
                
                UE_LOG(LogHearthshireVoxel, Log, TEXT("FillSolid: Created ProceduralMesh component"));
            }
        }
    }
    
    // Ensure valid LOD
    if (CurrentLOD == EVoxelChunkLOD::Unloaded)
    {
        CurrentLOD = EVoxelChunkLOD::LOD0;
        UE_LOG(LogHearthshireVoxel, Log, TEXT("FillSolid: Setting LOD to LOD0"));
    }
    
    const FVoxelChunkSize& Size = ChunkData.ChunkSize;
    int32 TotalVoxels = Size.X * Size.Y * Size.Z;
    
    UE_LOG(LogHearthshireVoxel, Log, TEXT("FillSolid: Filling %d voxels with material"), TotalVoxels);
    
    for (int32 Z = 0; Z < Size.Z; Z++)
    {
        for (int32 Y = 0; Y < Size.Y; Y++)
        {
            for (int32 X = 0; X < Size.X; X++)
            {
                SetVoxel(X, Y, Z, Material);
            }
        }
    }
    
    UE_LOG(LogHearthshireVoxel, Log, TEXT("FillSolid: Voxels set, generating mesh..."));
    GenerateMesh(false);
}

void UVoxelChunkComponent::ClearChunk()
{
    UE_LOG(LogHearthshireVoxel, Log, TEXT("ClearChunk called"));
    
    // Ensure chunk is initialized
    if (ChunkData.Voxels.Num() == 0)
    {
        UE_LOG(LogHearthshireVoxel, Warning, TEXT("ClearChunk: Chunk not initialized, initializing with defaults"));
        Initialize(FIntVector::ZeroValue, FVoxelChunkSize(32));
    }
    
    const FVoxelChunkSize& Size = ChunkData.ChunkSize;
    
    UE_LOG(LogHearthshireVoxel, Log, TEXT("ClearChunk: Clearing %dx%dx%d chunk"), Size.X, Size.Y, Size.Z);
    
    for (int32 Z = 0; Z < Size.Z; Z++)
    {
        for (int32 Y = 0; Y < Size.Y; Y++)
        {
            for (int32 X = 0; X < Size.X; X++)
            {
                SetVoxel(X, Y, Z, EVoxelMaterial::Air);
            }
        }
    }
    
    ClearMesh();
    UE_LOG(LogHearthshireVoxel, Log, TEXT("ClearChunk: Chunk cleared"));
}

// ===== Helper Functions =====
float UVoxelChunkComponent::GetTriangleReductionPercentage() const
{
    // Calculate theoretical maximum triangles for simple mesh
    int32 SolidVoxels = GetVoxelCount();
    int32 MaxPossibleTriangles = SolidVoxels * 12; // 6 faces * 2 triangles per face
    
    if (MaxPossibleTriangles == 0)
    {
        return 0.0f;
    }
    
    return 100.0f * (1.0f - (float)RuntimeTriangleCount / (float)MaxPossibleTriangles);
}

float UVoxelChunkComponent::GetMemoryUsageEstimate() const
{
    // Estimate memory usage
    float MemoryMB = 0.0f;
    
    // Voxel data
    MemoryMB += (ChunkData.Voxels.Num() * sizeof(FVoxel)) / (1024.0f * 1024.0f);
    
    // Mesh data
    MemoryMB += (MeshData.Vertices.Num() * sizeof(FVector)) / (1024.0f * 1024.0f);
    MemoryMB += (MeshData.Triangles.Num() * sizeof(int32)) / (1024.0f * 1024.0f);
    MemoryMB += (MeshData.Normals.Num() * sizeof(FVector)) / (1024.0f * 1024.0f);
    MemoryMB += (MeshData.UV0.Num() * sizeof(FVector2D)) / (1024.0f * 1024.0f);
    MemoryMB += (MeshData.Tangents.Num() * sizeof(FProcMeshTangent)) / (1024.0f * 1024.0f);
    
    return MemoryMB;
}

bool UVoxelChunkComponent::IsVoxelSolid(int32 X, int32 Y, int32 Z) const
{
    return GetVoxel(X, Y, Z) != EVoxelMaterial::Air;
}

int32 UVoxelChunkComponent::GetSurfaceVoxelCount() const
{
    const FVoxelChunkSize& Size = ChunkData.ChunkSize;
    int32 SurfaceCount = 0;
    
    for (int32 Z = 0; Z < Size.Z; Z++)
    {
        for (int32 Y = 0; Y < Size.Y; Y++)
        {
            for (int32 X = 0; X < Size.X; X++)
            {
                if (GetVoxel(X, Y, Z) != EVoxelMaterial::Air)
                {
                    // Check if any neighbor is air
                    bool bIsSurface = false;
                    
                    // Check 6 face neighbors
                    if ((X > 0 && GetVoxel(X-1, Y, Z) == EVoxelMaterial::Air) ||
                        (X < Size.X-1 && GetVoxel(X+1, Y, Z) == EVoxelMaterial::Air) ||
                        (Y > 0 && GetVoxel(X, Y-1, Z) == EVoxelMaterial::Air) ||
                        (Y < Size.Y-1 && GetVoxel(X, Y+1, Z) == EVoxelMaterial::Air) ||
                        (Z > 0 && GetVoxel(X, Y, Z-1) == EVoxelMaterial::Air) ||
                        (Z < Size.Z-1 && GetVoxel(X, Y, Z+1) == EVoxelMaterial::Air))
                    {
                        bIsSurface = true;
                    }
                    
                    if (bIsSurface)
                    {
                        SurfaceCount++;
                    }
                }
            }
        }
    }
    
    return SurfaceCount;
}

FIntVector UVoxelChunkComponent::WorldToLocalVoxel(const FVector& WorldPos) const
{
    FVector LocalPos = WorldPos - WorldPosition;
    return FIntVector(
        FMath::FloorToInt(LocalPos.X / ConfigurableVoxelSize),
        FMath::FloorToInt(LocalPos.Y / ConfigurableVoxelSize),
        FMath::FloorToInt(LocalPos.Z / ConfigurableVoxelSize)
    );
}

FVector UVoxelChunkComponent::LocalToWorldPosition(const FIntVector& LocalVoxel) const
{
    return WorldPosition + FVector(LocalVoxel) * ConfigurableVoxelSize + FVector(ConfigurableVoxelSize * 0.5f);
}

// AVoxelChunk Implementation

AVoxelChunk::AVoxelChunk()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.TickInterval = 0.1f; // Reduced tick rate
    
    // Create root component
    USceneComponent* SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
    RootComponent = SceneRoot;
    
    // Create chunk component
    ChunkComponent = CreateDefaultSubobject<UVoxelChunkComponent>(TEXT("ChunkComponent"));
    
    // Create procedural mesh component
    UProceduralMeshComponent* ProcMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("ProceduralMesh"));
    ProcMesh->SetupAttachment(RootComponent);
    
    // Default settings
    bIsPooled = false;
    bShowDebugInfo = false;
    bShowChunkBounds = false;
    bShowVoxelGrid = false;
    OwnerWorld = nullptr;
    CachedPlayerPawn = nullptr;
    LastLODUpdateTime = 0.0f;
}

void AVoxelChunk::BeginPlay()
{
    Super::BeginPlay();
    
    // Cache player pawn
    if (UWorld* World = GetWorld())
    {
        if (APlayerController* PC = World->GetFirstPlayerController())
        {
            CachedPlayerPawn = PC->GetPawn();
        }
    }
}

void AVoxelChunk::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (ChunkComponent)
    {
        ChunkComponent->ClearMesh();
    }
    
    Super::EndPlay(EndPlayReason);
}

void AVoxelChunk::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
    
    // Update LOD based on distance
    const float CurrentTime = GetWorld()->GetTimeSeconds();
    if (CurrentTime - LastLODUpdateTime > LODUpdateInterval)
    {
        UpdateLODBasedOnDistance();
        LastLODUpdateTime = CurrentTime;
    }
    
    // Debug rendering
    if (bShowDebugInfo || bShowChunkBounds || bShowVoxelGrid)
    {
        DrawDebugInfo();
    }
}

void AVoxelChunk::InitializeChunk(const FIntVector& ChunkPosition, const FVoxelChunkSize& ChunkSize, AVoxelWorld* World)
{
    OwnerWorld = World;
    bIsPooled = false;
    
    if (ChunkComponent)
    {
        ChunkComponent->Initialize(ChunkPosition, ChunkSize);
    }
}

void AVoxelChunk::ReturnToPool()
{
    bIsPooled = true;
    SetActorHiddenInGame(true);
    SetActorEnableCollision(false);
    SetActorTickEnabled(false);
    
    if (ChunkComponent)
    {
        ChunkComponent->ClearMesh();
    }
}

void AVoxelChunk::ResetChunk()
{
    bIsPooled = false;
    SetActorHiddenInGame(false);
    SetActorEnableCollision(true);
    SetActorTickEnabled(true);
}

float AVoxelChunk::GetDistanceToPlayer() const
{
    if (!CachedPlayerPawn)
    {
        return FLT_MAX;
    }
    
    return FVector::Dist(GetActorLocation(), CachedPlayerPawn->GetActorLocation());
}

bool AVoxelChunk::ShouldBeLoaded(float MaxDistance) const
{
    return GetDistanceToPlayer() <= MaxDistance;
}

void AVoxelChunk::ToggleDebugRendering()
{
    bShowDebugInfo = !bShowDebugInfo;
}

void AVoxelChunk::FillWithTestPattern()
{
    if (!ChunkComponent)
    {
        return;
    }
    
    const FVoxelChunkSize ChunkSize = ChunkComponent->GetChunkSize();
    
    // Create a checkerboard pattern
    for (int32 Z = 0; Z < ChunkSize.Z; Z++)
    {
        for (int32 Y = 0; Y < ChunkSize.Y; Y++)
        {
            for (int32 X = 0; X < ChunkSize.X; X++)
            {
                bool bIsSolid = ((X + Y + Z) % 2) == 0;
                EVoxelMaterial Material = bIsSolid ? EVoxelMaterial::Stone : EVoxelMaterial::Air;
                ChunkComponent->SetVoxel(X, Y, Z, Material);
            }
        }
    }
    
    ChunkComponent->GenerateMesh(false);
}

void AVoxelChunk::ClearAllVoxels()
{
    if (!ChunkComponent)
    {
        return;
    }
    
    const FVoxelChunkSize ChunkSize = ChunkComponent->GetChunkSize();
    
    for (int32 Z = 0; Z < ChunkSize.Z; Z++)
    {
        for (int32 Y = 0; Y < ChunkSize.Y; Y++)
        {
            for (int32 X = 0; X < ChunkSize.X; X++)
            {
                ChunkComponent->SetVoxel(X, Y, Z, EVoxelMaterial::Air);
            }
        }
    }
    
    ChunkComponent->ClearMesh();
}

void AVoxelChunk::ForceRegenerateMesh()
{
    if (ChunkComponent)
    {
        ChunkComponent->GenerateMesh(false);
    }
}

void AVoxelChunk::DrawDebugInfo() const
{
    if (!ChunkComponent)
    {
        return;
    }
    
    const FVector ChunkWorldPos = GetActorLocation();
    const FVoxelChunkSize ChunkSize = ChunkComponent->GetChunkSize();
    const FVector ChunkExtent = FVector(ChunkSize.X, ChunkSize.Y, ChunkSize.Z) * UVoxelChunkComponent::VoxelSize * 0.5f;
    
    // Draw chunk bounds
    if (bShowChunkBounds)
    {
        DrawDebugBox(GetWorld(), ChunkWorldPos + ChunkExtent, ChunkExtent, FColor::Green, false, -1.0f, 0, 2.0f);
    }
    
    // Draw debug info
    if (bShowDebugInfo)
    {
        const FString InfoText = FString::Printf(
            TEXT("Chunk: %s\nLOD: %d\nState: %d\nDistance: %.1fm"),
            *ChunkComponent->GetChunkPosition().ToString(),
            (int32)ChunkComponent->GetCurrentLOD(),
            (int32)ChunkComponent->GetState(),
            GetDistanceToPlayer() / 100.0f
        );
        
        DrawDebugString(GetWorld(), ChunkWorldPos + FVector(0, 0, ChunkExtent.Z * 2.0f), InfoText, nullptr, FColor::White, 0.0f, true);
    }
    
    // Draw voxel grid (expensive - use sparingly)
    if (bShowVoxelGrid && ChunkComponent->GetCurrentLOD() == EVoxelChunkLOD::LOD0)
    {
        // Use configured step size
        const int32 Step = FMath::Max(1, GridDisplayStep);
        
        for (int32 X = 0; X < ChunkSize.X; X += Step)
        {
            for (int32 Y = 0; Y < ChunkSize.Y; Y += Step)
            {
                for (int32 Z = 0; Z < ChunkSize.Z; Z += Step)
                {
                    if (ChunkComponent->GetVoxel(X, Y, Z) != EVoxelMaterial::Air)
                    {
                        const FVector VoxelPos = ChunkWorldPos + FVector(X, Y, Z) * ChunkComponent->ConfigurableVoxelSize;
                        DrawDebugPoint(GetWorld(), VoxelPos, 5.0f, ChunkComponent->DebugDrawColor.ToFColor(true), false, -1.0f);
                    }
                }
            }
        }
    }
    
    // Draw performance stats if enabled
    if (bShowPerformanceStats && ChunkComponent)
    {
        const FString PerfText = FString::Printf(
            TEXT("Verts: %d\nTris: %d\nGen Time: %.2fms\nMemory: %.2fMB\nReduction: %.1f%%"),
            ChunkComponent->RuntimeVertexCount,
            ChunkComponent->RuntimeTriangleCount,
            ChunkComponent->LastGenerationTimeMs,
            ChunkComponent->MemoryUsageMB,
            ChunkComponent->TriangleReductionPercentage
        );
        
        DrawDebugString(GetWorld(), ChunkWorldPos + FVector(0, 0, -20.0f), PerfText, nullptr, FColor::Yellow, 0.0f, true);
    }
}

void AVoxelChunk::UpdateLODBasedOnDistance()
{
    if (!ChunkComponent || !OwnerWorld)
    {
        return;
    }
    
    const float Distance = GetDistanceToPlayer();
    
    // Determine appropriate LOD based on distance
    EVoxelChunkLOD TargetLOD = EVoxelChunkLOD::Unloaded;
    
    if (Distance < 5000.0f) // 50m
    {
        TargetLOD = EVoxelChunkLOD::LOD0;
    }
    else if (Distance < 10000.0f) // 100m
    {
        TargetLOD = EVoxelChunkLOD::LOD1;
    }
    else if (Distance < 20000.0f) // 200m
    {
        TargetLOD = EVoxelChunkLOD::LOD2;
    }
    else if (Distance < 30000.0f) // 300m
    {
        TargetLOD = EVoxelChunkLOD::LOD3;
    }
    
    ChunkComponent->SetLOD(TargetLOD);
}

// ===== Debug Function Implementations =====

void UVoxelChunkComponent::DebugMeshInfo()
{
    UE_LOG(LogHearthshireVoxel, Warning, TEXT("=== DEBUG MESH INFO ==="));
    
    if (!ProceduralMesh)
    {
        UE_LOG(LogHearthshireVoxel, Error, TEXT("ProceduralMesh is NULL!"));
        return;
    }
    
    UE_LOG(LogHearthshireVoxel, Warning, TEXT("ProceduralMesh Component:"));
    UE_LOG(LogHearthshireVoxel, Warning, TEXT("  Is Registered: %s"), ProceduralMesh->IsRegistered() ? TEXT("Yes") : TEXT("No"));
    UE_LOG(LogHearthshireVoxel, Warning, TEXT("  Is Visible: %s"), ProceduralMesh->IsVisible() ? TEXT("Yes") : TEXT("No"));
    UE_LOG(LogHearthshireVoxel, Warning, TEXT("  Num Sections: %d"), ProceduralMesh->GetNumSections());
    
    // Check each section
    for (int32 i = 0; i < ProceduralMesh->GetNumSections(); i++)
    {
        FProcMeshSection* Section = ProceduralMesh->GetProcMeshSection(i);
        if (Section)
        {
            UE_LOG(LogHearthshireVoxel, Warning, TEXT("  Section %d:"), i);
            UE_LOG(LogHearthshireVoxel, Warning, TEXT("    Vertices: %d"), Section->ProcVertexBuffer.Num());
            UE_LOG(LogHearthshireVoxel, Warning, TEXT("    Triangles: %d"), Section->ProcIndexBuffer.Num() / 3);
            
            // Check material
            UMaterialInterface* Material = ProceduralMesh->GetMaterial(i);
            UE_LOG(LogHearthshireVoxel, Warning, TEXT("    Material: %s"), Material ? *Material->GetName() : TEXT("NULL"));
            
            // Check vertex colors
            int32 TransparentCount = 0;
            for (const FProcMeshVertex& Vertex : Section->ProcVertexBuffer)
            {
                if (Vertex.Color.A < 255)
                {
                    TransparentCount++;
                }
            }
            if (TransparentCount > 0)
            {
                UE_LOG(LogHearthshireVoxel, Warning, TEXT("    WARNING: %d vertices with alpha < 255!"), TransparentCount);
            }
        }
    }
    
    // Component settings
    UE_LOG(LogHearthshireVoxel, Warning, TEXT("Rendering Settings:"));
    UE_LOG(LogHearthshireVoxel, Warning, TEXT("  Cast Shadow: %s"), ProceduralMesh->CastShadow ? TEXT("Yes") : TEXT("No"));
    UE_LOG(LogHearthshireVoxel, Warning, TEXT("  Render In Main Pass: %s"), ProceduralMesh->bRenderInMainPass ? TEXT("Yes") : TEXT("No"));
    UE_LOG(LogHearthshireVoxel, Warning, TEXT("  Render Custom Depth: %s"), ProceduralMesh->bRenderCustomDepth ? TEXT("Yes") : TEXT("No"));
    
    UE_LOG(LogHearthshireVoxel, Warning, TEXT("=== END DEBUG INFO ==="));
}

void UVoxelChunkComponent::ForceOpaqueRendering()
{
    if (!ProceduralMesh)
    {
        UE_LOG(LogHearthshireVoxel, Error, TEXT("ForceOpaqueRendering: ProceduralMesh is NULL!"));
        return;
    }
    
    UE_LOG(LogHearthshireVoxel, Warning, TEXT("Forcing opaque rendering settings..."));
    
    // Force opaque rendering settings
    ProceduralMesh->SetRenderCustomDepth(false);
    ProceduralMesh->bRenderInMainPass = true;
    ProceduralMesh->SetCastShadow(true);
    ProceduralMesh->bReceivesDecals = true;
    ProceduralMesh->bVisibleInReflectionCaptures = true;
    ProceduralMesh->bVisibleInRealTimeSkyCaptures = true;
    ProceduralMesh->bVisibleInRayTracing = true;
    
    // Force update all vertex colors to be opaque
    for (int32 i = 0; i < ProceduralMesh->GetNumSections(); i++)
    {
        FProcMeshSection* Section = ProceduralMesh->GetProcMeshSection(i);
        if (Section)
        {
            bool bNeedsUpdate = false;
            for (FProcMeshVertex& Vertex : Section->ProcVertexBuffer)
            {
                if (Vertex.Color.A < 255)
                {
                    Vertex.Color.A = 255;
                    bNeedsUpdate = true;
                }
            }
            
            if (bNeedsUpdate)
            {
                // UpdateMeshSection requires rebuilding the section with corrected data
                TArray<FVector> Vertices;
                TArray<int32> Triangles;
                TArray<FVector> Normals;
                TArray<FVector2D> UV0;
                TArray<FColor> VertexColors;
                TArray<FProcMeshTangent> Tangents;
                
                // Extract data from section
                for (const FProcMeshVertex& Vertex : Section->ProcVertexBuffer)
                {
                    Vertices.Add(Vertex.Position);
                    Normals.Add(Vertex.Normal);
                    UV0.Add(Vertex.UV0);
                    VertexColors.Add(FColor(255, 255, 255, 255)); // Force opaque white
                    Tangents.Add(Vertex.Tangent);
                }
                
                // Copy triangle indices (convert from uint32 to int32)
                Triangles.Reserve(Section->ProcIndexBuffer.Num());
                for (uint32 Index : Section->ProcIndexBuffer)
                {
                    Triangles.Add(static_cast<int32>(Index));
                }
                
                // Update the section with opaque colors
                ProceduralMesh->UpdateMeshSection(i, Vertices, Normals, UV0, VertexColors, Tangents);
                UE_LOG(LogHearthshireVoxel, Warning, TEXT("Updated section %d vertex colors to full opacity"), i);
            }
        }
    }
    
    ProceduralMesh->MarkRenderStateDirty();
    UE_LOG(LogHearthshireVoxel, Warning, TEXT("Opaque rendering settings applied"));
}

void UVoxelChunkComponent::ValidateMeshData()
{
    UE_LOG(LogHearthshireVoxel, Warning, TEXT("=== VALIDATING MESH DATA ==="));
    
    if (MeshData.Vertices.Num() == 0)
    {
        UE_LOG(LogHearthshireVoxel, Warning, TEXT("ValidateMeshData: No vertices in mesh data"));
        return;
    }
    
    // Validate mesh data arrays
    UE_LOG(LogHearthshireVoxel, Warning, TEXT("Mesh Data Arrays:"));
    UE_LOG(LogHearthshireVoxel, Warning, TEXT("  Vertices: %d"), MeshData.Vertices.Num());
    UE_LOG(LogHearthshireVoxel, Warning, TEXT("  Triangles: %d (indices: %d)"), MeshData.Triangles.Num() / 3, MeshData.Triangles.Num());
    UE_LOG(LogHearthshireVoxel, Warning, TEXT("  Normals: %d"), MeshData.Normals.Num());
    UE_LOG(LogHearthshireVoxel, Warning, TEXT("  UVs: %d"), MeshData.UV0.Num());
    UE_LOG(LogHearthshireVoxel, Warning, TEXT("  Vertex Colors: %d"), MeshData.VertexColors.Num());
    UE_LOG(LogHearthshireVoxel, Warning, TEXT("  Tangents: %d"), MeshData.Tangents.Num());
    UE_LOG(LogHearthshireVoxel, Warning, TEXT("  Material Sections: %d"), MeshData.MaterialSections.Num());
    
    // Check for duplicate vertices
    TSet<FVector> UniqueVerts;
    int32 DuplicateCount = 0;
    
    for (int32 i = 0; i < MeshData.Vertices.Num(); i++)
    {
        const FVector& Vert = MeshData.Vertices[i];
        if (UniqueVerts.Contains(Vert))
        {
            DuplicateCount++;
            if (DuplicateCount < 10) // Only log first 10 duplicates
            {
                UE_LOG(LogHearthshireVoxel, Warning, TEXT("  Duplicate vertex at index %d: %s"), 
                    i, *Vert.ToString());
            }
        }
        else
        {
            UniqueVerts.Add(Vert);
        }
    }
    
    // Validate triangle indices
    int32 InvalidIndices = 0;
    for (int32 i = 0; i < MeshData.Triangles.Num(); i++)
    {
        if (MeshData.Triangles[i] >= MeshData.Vertices.Num() || MeshData.Triangles[i] < 0)
        {
            InvalidIndices++;
            UE_LOG(LogHearthshireVoxel, Error, TEXT("  Invalid triangle index at %d: %d (max: %d)"), 
                i, MeshData.Triangles[i], MeshData.Vertices.Num() - 1);
        }
    }
    
    // Validate array sizes match
    bool bArrayMismatch = false;
    if (MeshData.Vertices.Num() != MeshData.Normals.Num())
    {
        UE_LOG(LogHearthshireVoxel, Error, TEXT("ERROR: Vertex count doesn't match normal count!"));
        bArrayMismatch = true;
    }
    if (MeshData.Vertices.Num() != MeshData.UV0.Num())
    {
        UE_LOG(LogHearthshireVoxel, Error, TEXT("ERROR: Vertex count doesn't match UV count!"));
        bArrayMismatch = true;
    }
    if (MeshData.Vertices.Num() != MeshData.VertexColors.Num())
    {
        UE_LOG(LogHearthshireVoxel, Error, TEXT("ERROR: Vertex count doesn't match color count!"));
        bArrayMismatch = true;
    }
    
    // Calculate expected vs actual triangles
    int32 SolidVoxels = GetVoxelCount();
    int32 MaxPossibleTriangles = SolidVoxels * 12; // 6 faces * 2 triangles per face
    int32 ActualTriangles = MeshData.Triangles.Num() / 3;
    float ReductionPercent = MaxPossibleTriangles > 0 ? 
        100.0f * (1.0f - (float)ActualTriangles / (float)MaxPossibleTriangles) : 0.0f;
    
    // Validate normals
    int32 ZeroNormals = 0;
    for (const FVector& Normal : MeshData.Normals)
    {
        if (Normal.IsNearlyZero())
        {
            ZeroNormals++;
        }
    }
    if (ZeroNormals > 0)
    {
        UE_LOG(LogHearthshireVoxel, Warning, TEXT("WARNING: %d zero normals found!"), ZeroNormals);
    }
    
    // Check material sections
    UE_LOG(LogHearthshireVoxel, Warning, TEXT("Material Sections: %d"), MeshData.MaterialSections.Num());
    for (const auto& MatSection : MeshData.MaterialSections)
    {
        UE_LOG(LogHearthshireVoxel, Warning, TEXT("  Material %d -> Section %d"), 
            (int32)MatSection.Key, MatSection.Value);
    }
    
    // Check for vertex welding effectiveness
    int32 ExpectedVerticesWithoutWelding = ActualTriangles * 2; // Each triangle needs 3 vertices, but quads share 2
    float WeldingEfficiency = 100.0f * (1.0f - (float)MeshData.Vertices.Num() / (float)ExpectedVerticesWithoutWelding);
    
    // Summary
    UE_LOG(LogHearthshireVoxel, Warning, TEXT("=== Mesh Validation Summary ==="));
    UE_LOG(LogHearthshireVoxel, Warning, TEXT("  Total Vertices: %d (Unique: %d, Duplicates: %d)"), 
        MeshData.Vertices.Num(), UniqueVerts.Num(), DuplicateCount);
    UE_LOG(LogHearthshireVoxel, Warning, TEXT("  Total Triangles: %d (Invalid indices: %d)"), 
        ActualTriangles, InvalidIndices);
    UE_LOG(LogHearthshireVoxel, Warning, TEXT("  Material Sections: %d"), MeshData.MaterialSections.Num());
    UE_LOG(LogHearthshireVoxel, Warning, TEXT("  Triangle Reduction: %.1f%% (Max possible: %d, Actual: %d)"),
        ReductionPercent, MaxPossibleTriangles, ActualTriangles);
    UE_LOG(LogHearthshireVoxel, Warning, TEXT("  Vertex Welding Efficiency: %.1f%% (Expected without welding: %d)"),
        WeldingEfficiency, ExpectedVerticesWithoutWelding);
    UE_LOG(LogHearthshireVoxel, Warning, TEXT("  Array Consistency: %s"), bArrayMismatch ? TEXT("FAILED") : TEXT("OK"));
    UE_LOG(LogHearthshireVoxel, Warning, TEXT("  Zero Normals: %d"), ZeroNormals);
    UE_LOG(LogHearthshireVoxel, Warning, TEXT("=============================="));
}

void UVoxelChunkComponent::ValidateWeldedMesh()
{
    UE_LOG(LogHearthshireVoxel, Log, TEXT("=== Validating Welded Mesh ==="));
    
    if (MeshData.Vertices.Num() == 0)
    {
        UE_LOG(LogHearthshireVoxel, Warning, TEXT("ValidateWeldedMesh: No vertices to validate"));
        return;
    }
    
    // Check for duplicate vertices at same position with tolerance
    TMap<FVector, TArray<int32>> PositionToIndices;
    const float Tolerance = 0.01f; // 0.01 units tolerance
    
    for (int32 i = 0; i < MeshData.Vertices.Num(); i++)
    {
        // Round to nearest 0.01 for grouping
        FVector RoundedPos = FVector(
            FMath::RoundToFloat(MeshData.Vertices[i].X / Tolerance) * Tolerance,
            FMath::RoundToFloat(MeshData.Vertices[i].Y / Tolerance) * Tolerance,
            FMath::RoundToFloat(MeshData.Vertices[i].Z / Tolerance) * Tolerance
        );
        
        PositionToIndices.FindOrAdd(RoundedPos).Add(i);
    }
    
    // Count duplicate positions
    int32 DuplicatePositions = 0;
    int32 TotalDuplicateVertices = 0;
    
    for (const auto& Pair : PositionToIndices)
    {
        if (Pair.Value.Num() > 1)
        {
            DuplicatePositions++;
            TotalDuplicateVertices += (Pair.Value.Num() - 1);
            
            if (DuplicatePositions < 5) // Log first few duplicates
            {
                UE_LOG(LogHearthshireVoxel, Warning, TEXT("  Found %d vertices at position %s"), 
                    Pair.Value.Num(), *Pair.Key.ToString());
            }
        }
    }
    
    // Calculate welding statistics
    int32 UniquePositions = PositionToIndices.Num();
    float DuplicatePercentage = MeshData.Vertices.Num() > 0 ? 
        100.0f * TotalDuplicateVertices / MeshData.Vertices.Num() : 0.0f;
    
    // Check triangle connectivity
    TSet<TPair<int32, int32>> Edges;
    for (int32 i = 0; i < MeshData.Triangles.Num(); i += 3)
    {
        int32 V0 = MeshData.Triangles[i];
        int32 V1 = MeshData.Triangles[i + 1];
        int32 V2 = MeshData.Triangles[i + 2];
        
        // Add edges (smaller index first for consistency)
        Edges.Add(TPair<int32, int32>(FMath::Min(V0, V1), FMath::Max(V0, V1)));
        Edges.Add(TPair<int32, int32>(FMath::Min(V1, V2), FMath::Max(V1, V2)));
        Edges.Add(TPair<int32, int32>(FMath::Min(V2, V0), FMath::Max(V2, V0)));
    }
    
    // Summary
    UE_LOG(LogHearthshireVoxel, Log, TEXT("=== Welded Mesh Validation Results ==="));
    UE_LOG(LogHearthshireVoxel, Log, TEXT("  Total Vertices: %d"), MeshData.Vertices.Num());
    UE_LOG(LogHearthshireVoxel, Log, TEXT("  Unique Positions: %d"), UniquePositions);
    UE_LOG(LogHearthshireVoxel, Log, TEXT("  Duplicate Positions: %d (containing %d extra vertices)"), 
        DuplicatePositions, TotalDuplicateVertices);
    UE_LOG(LogHearthshireVoxel, Log, TEXT("  Duplicate Percentage: %.1f%%"), DuplicatePercentage);
    UE_LOG(LogHearthshireVoxel, Log, TEXT("  Unique Edges: %d"), Edges.Num());
    
    if (DuplicatePositions == 0)
    {
        UE_LOG(LogHearthshireVoxel, Log, TEXT("  Result: PROPERLY WELDED - No duplicate vertices found!"));
    }
    else
    {
        UE_LOG(LogHearthshireVoxel, Warning, TEXT("  Result: WELDING ISSUES - Found %d positions with duplicate vertices"), 
            DuplicatePositions);
    }
    
    UE_LOG(LogHearthshireVoxel, Log, TEXT("====================================="));
}

void UVoxelChunkComponent::FixMeshNormals()
{
    if (!ProceduralMesh)
    {
        UE_LOG(LogHearthshireVoxel, Error, TEXT("FixMeshNormals: ProceduralMesh is NULL!"));
        return;
    }
    
    UE_LOG(LogHearthshireVoxel, Warning, TEXT("Fixing mesh normals..."));
    
    // Recalculate normals for each section
    for (int32 i = 0; i < ProceduralMesh->GetNumSections(); i++)
    {
        FProcMeshSection* Section = ProceduralMesh->GetProcMeshSection(i);
        if (Section && Section->ProcVertexBuffer.Num() > 0)
        {
            // Calculate smooth normals
            TArray<FVector> SmoothNormals;
            SmoothNormals.SetNumZeroed(Section->ProcVertexBuffer.Num());
            
            // Accumulate face normals for each vertex
            for (int32 TriIdx = 0; TriIdx < Section->ProcIndexBuffer.Num(); TriIdx += 3)
            {
                int32 I0 = Section->ProcIndexBuffer[TriIdx];
                int32 I1 = Section->ProcIndexBuffer[TriIdx + 1];
                int32 I2 = Section->ProcIndexBuffer[TriIdx + 2];
                
                const FVector& V0 = Section->ProcVertexBuffer[I0].Position;
                const FVector& V1 = Section->ProcVertexBuffer[I1].Position;
                const FVector& V2 = Section->ProcVertexBuffer[I2].Position;
                
                // Calculate face normal (ensure clockwise winding)
                FVector Edge1 = V1 - V0;
                FVector Edge2 = V2 - V0;
                FVector FaceNormal = Edge1.Cross(Edge2).GetSafeNormal();
                
                // Add to each vertex
                SmoothNormals[I0] += FaceNormal;
                SmoothNormals[I1] += FaceNormal;
                SmoothNormals[I2] += FaceNormal;
            }
            
            // Build updated arrays with fixed normals
            TArray<FVector> Vertices;
            TArray<int32> Triangles;
            TArray<FVector> Normals;
            TArray<FVector2D> UV0;
            TArray<FColor> VertexColors;
            TArray<FProcMeshTangent> Tangents;
            
            // Extract data with corrected normals
            for (int32 VertIdx = 0; VertIdx < Section->ProcVertexBuffer.Num(); VertIdx++)
            {
                const FProcMeshVertex& Vertex = Section->ProcVertexBuffer[VertIdx];
                Vertices.Add(Vertex.Position);
                Normals.Add(SmoothNormals[VertIdx].GetSafeNormal());
                UV0.Add(Vertex.UV0);
                VertexColors.Add(FColor(255, 255, 255, 255)); // Ensure opaque
                Tangents.Add(Vertex.Tangent);
            }
            
            // Copy triangle indices (convert from uint32 to int32)
            Triangles.Reserve(Section->ProcIndexBuffer.Num());
            for (uint32 Index : Section->ProcIndexBuffer)
            {
                Triangles.Add(static_cast<int32>(Index));
            }
            
            // Update the section with fixed normals
            ProceduralMesh->UpdateMeshSection(i, Vertices, Normals, UV0, VertexColors, Tangents);
            UE_LOG(LogHearthshireVoxel, Warning, TEXT("Fixed normals for section %d"), i);
        }
    }
    
    ProceduralMesh->MarkRenderStateDirty();
    UE_LOG(LogHearthshireVoxel, Warning, TEXT("Mesh normals fixed"));
}