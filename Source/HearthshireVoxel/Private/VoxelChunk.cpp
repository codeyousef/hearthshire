// Copyright Epic Games, Inc. All Rights Reserved.

#include "VoxelChunk.h"
#include "ProceduralMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "DrawDebugHelpers.h"
#include "Async/Async.h"
#include "HearthshireVoxelModule.h"
#include "VoxelMeshGenerator.h"
#include "VoxelGreedyMesher.h"

// VoxelChunkComponent Implementation

UVoxelChunkComponent::UVoxelChunkComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
    
    ChunkState = EVoxelChunkState::Uninitialized;
    CurrentLOD = EVoxelChunkLOD::Unloaded;
    MaterialSet = nullptr;
    OwnerWorld = nullptr;
    bIsGeneratingMesh = false;
    WorldPosition = FVector::ZeroVector;
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
        
        ProceduralMesh->bUseAsyncCooking = true;
        ProceduralMesh->bUseComplexAsSimpleCollision = false;
        ProceduralMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
        ProceduralMesh->SetCollisionResponseToAllChannels(ECR_Block);
    }
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
    ChunkData.SetVoxel(X, Y, Z, FVoxel(Material));
    
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
            default:
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
        CurrentLOD = NewLOD;
        
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
        ChunkState = EVoxelChunkState::Ready;
        ChunkData.bIsDirty = false;
        OnChunkGenerated.Broadcast(this);
        return;
    }
    
    // Apply mesh data to procedural mesh component
    FVoxelMeshGenerator::ApplyMeshToComponent(ProceduralMesh, MeshData, MaterialSet);
    
    // Update performance stats
    UpdatePerformanceStats();
    
    ChunkState = EVoxelChunkState::Ready;
    ChunkData.bIsDirty = false;
    OnChunkGenerated.Broadcast(this);
}

void UVoxelChunkComponent::GenerateLOD0Mesh()
{
    const double StartTime = FPlatformTime::Seconds();
    
    MeshData.Clear();
    
    // Use greedy meshing for LOD0
    FVoxelMeshGenerator::FGenerationConfig Config;
    Config.VoxelSize = VoxelSize;
    Config.bGenerateCollision = true;
    Config.bGenerateTangents = true;
    Config.bOptimizeIndices = true;
    
    FVoxelMeshGenerator::GenerateGreedyMesh(ChunkData, MeshData, Config);
    
    MeshData.GenerationTimeMs = (FPlatformTime::Seconds() - StartTime) * 1000.0f;
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
    PerformanceStats.MeshGenerationTimeMs = MeshData.GenerationTimeMs;
    PerformanceStats.TriangleCount = MeshData.TriangleCount;
    PerformanceStats.VertexCount = MeshData.VertexCount;
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
        // Only draw a subset of voxels to avoid performance impact
        const int32 Step = FMath::Max(1, ChunkSize.X / 8);
        
        for (int32 X = 0; X < ChunkSize.X; X += Step)
        {
            for (int32 Y = 0; Y < ChunkSize.Y; Y += Step)
            {
                for (int32 Z = 0; Z < ChunkSize.Z; Z += Step)
                {
                    if (ChunkComponent->GetVoxel(X, Y, Z) != EVoxelMaterial::Air)
                    {
                        const FVector VoxelPos = ChunkWorldPos + FVector(X, Y, Z) * UVoxelChunkComponent::VoxelSize;
                        DrawDebugPoint(GetWorld(), VoxelPos, 5.0f, FColor::Red, false, -1.0f);
                    }
                }
            }
        }
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