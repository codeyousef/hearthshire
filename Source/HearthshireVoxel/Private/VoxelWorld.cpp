// Copyright Epic Games, Inc. All Rights Reserved.

#include "VoxelWorld.h"
#include "VoxelChunk.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "HearthshireVoxelModule.h"
#include "Async/ParallelFor.h"

AVoxelWorld::AVoxelWorld()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.TickInterval = 0.033f; // 30Hz update
    
    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("RootComponent"));
    
    TrackedPlayer = nullptr;
    LastPlayerPosition = FVector::ZeroVector;
    ChunkUpdateTimer = 0.0f;
    MemoryCheckTimer = 0.0f;
}

void AVoxelWorld::BeginPlay()
{
    Super::BeginPlay();
    
    // Find player to track
    if (UWorld* World = GetWorld())
    {
        if (APlayerController* PC = World->GetFirstPlayerController())
        {
            TrackedPlayer = PC->GetPawn();
        }
    }
    
    // Pre-allocate chunk pool
    UE_LOG(LogHearthshireVoxel, Log, TEXT("Pre-allocating %d chunks for pool"), Config.ChunkPoolSize);
    
    for (int32 i = 0; i < Config.ChunkPoolSize; i++)
    {
        if (AVoxelChunk* NewChunk = GetWorld()->SpawnActor<AVoxelChunk>(AVoxelChunk::StaticClass()))
        {
            NewChunk->SetActorHiddenInGame(true);
            NewChunk->SetActorEnableCollision(false);
            NewChunk->SetActorTickEnabled(false);
            NewChunk->AttachToActor(this, FAttachmentTransformRules::KeepRelativeTransform);
            ChunkPool.Add(NewChunk);
        }
    }
    
    UE_LOG(LogHearthshireVoxel, Log, TEXT("VoxelWorld initialized with %d pooled chunks"), ChunkPool.Num());
}

void AVoxelWorld::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    // Clean up all chunks
    for (auto& ChunkPair : ActiveChunks)
    {
        if (ChunkPair.Value)
        {
            ChunkPair.Value->Destroy();
        }
    }
    ActiveChunks.Empty();
    
    for (AVoxelChunk* PooledChunk : ChunkPool)
    {
        if (PooledChunk)
        {
            PooledChunk->Destroy();
        }
    }
    ChunkPool.Empty();
    
    Super::EndPlay(EndPlayReason);
}

void AVoxelWorld::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
    
    // Update chunks based on player position
    ChunkUpdateTimer += DeltaTime;
    if (ChunkUpdateTimer >= ChunkUpdateInterval)
    {
        ChunkUpdateTimer = 0.0f;
        UpdateChunks();
    }
    
    // Process chunk generation tasks
    ProcessChunkTasks();
    
    // Memory management
    MemoryCheckTimer += DeltaTime;
    if (MemoryCheckTimer >= MemoryCheckInterval)
    {
        MemoryCheckTimer = 0.0f;
        UpdateMemoryUsage();
        EnforceMemoryBudget();
    }
}

AVoxelChunk* AVoxelWorld::GetOrCreateChunk(const FIntVector& ChunkPosition)
{
    // Check if chunk already exists
    if (AVoxelChunk** ExistingChunk = ActiveChunks.Find(ChunkPosition))
    {
        return *ExistingChunk;
    }
    
    // Get chunk from pool
    AVoxelChunk* NewChunk = GetChunkFromPool();
    if (!NewChunk)
    {
        // Pool exhausted, spawn new chunk
        NewChunk = GetWorld()->SpawnActor<AVoxelChunk>(AVoxelChunk::StaticClass());
        if (!NewChunk)
        {
            UE_LOG(LogHearthshireVoxel, Error, TEXT("Failed to spawn chunk at %s"), *ChunkPosition.ToString());
            return nullptr;
        }
        NewChunk->AttachToActor(this, FAttachmentTransformRules::KeepRelativeTransform);
    }
    
    // Initialize chunk
    FVoxelChunkSize ChunkSize(Config.ChunkSize);
    NewChunk->InitializeChunk(ChunkPosition, ChunkSize, this);
    
    // Set up chunk component
    if (UVoxelChunkComponent* ChunkComp = NewChunk->ChunkComponent)
    {
        ChunkComp->OnChunkGenerated.AddDynamic(this, &AVoxelWorld::OnChunkGenerated);
        
        // TODO: Generate terrain data for chunk
        // For now, create a simple test pattern
        for (int32 Z = 0; Z < ChunkSize.Z / 2; Z++)
        {
            for (int32 Y = 0; Y < ChunkSize.Y; Y++)
            {
                for (int32 X = 0; X < ChunkSize.X; X++)
                {
                    EVoxelMaterial Material = EVoxelMaterial::Stone;
                    if (Z == ChunkSize.Z / 2 - 1)
                    {
                        Material = EVoxelMaterial::Grass;
                    }
                    else if (Z > ChunkSize.Z / 2 - 4)
                    {
                        Material = EVoxelMaterial::Dirt;
                    }
                    
                    ChunkComp->SetVoxel(X, Y, Z, Material);
                }
            }
        }
    }
    
    // Add to active chunks
    ActiveChunks.Add(ChunkPosition, NewChunk);
    
    // Queue for generation
    QueueChunkGeneration(ChunkPosition, CalculateChunkPriority(ChunkPosition));
    
    OnChunkLoaded.Broadcast(ChunkPosition);
    
    return NewChunk;
}

void AVoxelWorld::UnloadChunk(const FIntVector& ChunkPosition)
{
    AVoxelChunk** ChunkPtr = ActiveChunks.Find(ChunkPosition);
    if (!ChunkPtr || !(*ChunkPtr))
    {
        return;
    }
    
    AVoxelChunk* Chunk = *ChunkPtr;
    
    // Remove from active chunks
    ActiveChunks.Remove(ChunkPosition);
    
    // Return to pool
    ReturnChunkToPool(Chunk);
    
    OnChunkUnloaded.Broadcast(ChunkPosition);
}

void AVoxelWorld::RegenerateChunk(const FIntVector& ChunkPosition)
{
    if (AVoxelChunk** ChunkPtr = ActiveChunks.Find(ChunkPosition))
    {
        if (AVoxelChunk* Chunk = *ChunkPtr)
        {
            if (UVoxelChunkComponent* ChunkComp = Chunk->ChunkComponent)
            {
                ChunkComp->GenerateMesh(Config.bUseMultithreading);
            }
        }
    }
}

void AVoxelWorld::SetVoxel(const FVector& WorldPosition, EVoxelMaterial Material)
{
    FIntVector ChunkPos = WorldToChunkPosition(WorldPosition);
    FIntVector LocalVoxel = WorldToLocalVoxel(WorldPosition, ChunkPos);
    
    AVoxelChunk* Chunk = GetOrCreateChunk(ChunkPos);
    if (Chunk && Chunk->ChunkComponent)
    {
        Chunk->ChunkComponent->SetVoxel(LocalVoxel.X, LocalVoxel.Y, LocalVoxel.Z, Material);
        
        // Check if we need to update neighboring chunks
        bool bNeedsNeighborUpdate = false;
        if (LocalVoxel.X == 0 || LocalVoxel.X == Config.ChunkSize - 1) bNeedsNeighborUpdate = true;
        if (LocalVoxel.Y == 0 || LocalVoxel.Y == Config.ChunkSize - 1) bNeedsNeighborUpdate = true;
        if (LocalVoxel.Z == 0 || LocalVoxel.Z == Config.ChunkSize - 1) bNeedsNeighborUpdate = true;
        
        if (bNeedsNeighborUpdate)
        {
            // Queue neighbor chunks for regeneration
            for (int32 DX = -1; DX <= 1; DX++)
            {
                for (int32 DY = -1; DY <= 1; DY++)
                {
                    for (int32 DZ = -1; DZ <= 1; DZ++)
                    {
                        if (DX == 0 && DY == 0 && DZ == 0) continue;
                        
                        FIntVector NeighborPos = ChunkPos + FIntVector(DX, DY, DZ);
                        if (ActiveChunks.Contains(NeighborPos))
                        {
                            QueueChunkGeneration(NeighborPos, 1, true);
                        }
                    }
                }
            }
        }
    }
}

EVoxelMaterial AVoxelWorld::GetVoxel(const FVector& WorldPosition) const
{
    FIntVector ChunkPos = WorldToChunkPosition(WorldPosition);
    FIntVector LocalVoxel = WorldToLocalVoxel(WorldPosition, ChunkPos);
    
    if (AVoxelChunk* const* ChunkPtr = ActiveChunks.Find(ChunkPos))
    {
        if (AVoxelChunk* Chunk = *ChunkPtr)
        {
            if (Chunk->ChunkComponent)
            {
                return Chunk->ChunkComponent->GetVoxel(LocalVoxel.X, LocalVoxel.Y, LocalVoxel.Z);
            }
        }
    }
    
    return EVoxelMaterial::Air;
}

void AVoxelWorld::SetVoxelSphere(const FVector& Center, float Radius, EVoxelMaterial Material)
{
    // Calculate affected chunks
    FIntVector MinChunk = WorldToChunkPosition(Center - FVector(Radius));
    FIntVector MaxChunk = WorldToChunkPosition(Center + FVector(Radius));
    
    TArray<FIntVector> AffectedChunks;
    
    for (int32 X = MinChunk.X; X <= MaxChunk.X; X++)
    {
        for (int32 Y = MinChunk.Y; Y <= MaxChunk.Y; Y++)
        {
            for (int32 Z = MinChunk.Z; Z <= MaxChunk.Z; Z++)
            {
                FIntVector ChunkPos(X, Y, Z);
                AVoxelChunk* Chunk = GetOrCreateChunk(ChunkPos);
                
                if (Chunk && Chunk->ChunkComponent)
                {
                    bool bChunkModified = false;
                    
                    // Modify voxels within sphere
                    for (int32 VX = 0; VX < Config.ChunkSize; VX++)
                    {
                        for (int32 VY = 0; VY < Config.ChunkSize; VY++)
                        {
                            for (int32 VZ = 0; VZ < Config.ChunkSize; VZ++)
                            {
                                FVector VoxelWorldPos = FVector(ChunkPos * Config.ChunkSize + FIntVector(VX, VY, VZ)) * VoxelSize;
                                float Distance = FVector::Dist(VoxelWorldPos + FVector(VoxelSize * 0.5f), Center);
                                
                                if (Distance <= Radius)
                                {
                                    Chunk->ChunkComponent->SetVoxel(VX, VY, VZ, Material);
                                    bChunkModified = true;
                                }
                            }
                        }
                    }
                    
                    if (bChunkModified)
                    {
                        AffectedChunks.Add(ChunkPos);
                    }
                }
            }
        }
    }
    
    // Queue affected chunks for regeneration
    for (const FIntVector& ChunkPos : AffectedChunks)
    {
        QueueChunkGeneration(ChunkPos, 0, true);
    }
}

FIntVector AVoxelWorld::WorldToChunkPosition(const FVector& WorldPosition) const
{
    return FIntVector(
        FMath::FloorToInt(WorldPosition.X / (Config.ChunkSize * VoxelSize)),
        FMath::FloorToInt(WorldPosition.Y / (Config.ChunkSize * VoxelSize)),
        FMath::FloorToInt(WorldPosition.Z / (Config.ChunkSize * VoxelSize))
    );
}

FIntVector AVoxelWorld::WorldToLocalVoxel(const FVector& WorldPosition, const FIntVector& ChunkPosition) const
{
    FVector ChunkWorldPos = FVector(ChunkPosition) * Config.ChunkSize * VoxelSize;
    FVector LocalPos = WorldPosition - ChunkWorldPos;
    
    return FIntVector(
        FMath::FloorToInt(LocalPos.X / VoxelSize),
        FMath::FloorToInt(LocalPos.Y / VoxelSize),
        FMath::FloorToInt(LocalPos.Z / VoxelSize)
    );
}

FVoxelPerformanceStats AVoxelWorld::GetWorldStats() const
{
    return WorldStats;
}

void AVoxelWorld::UpdateChunks()
{
    if (!TrackedPlayer)
    {
        return;
    }
    
    FVector PlayerPosition = TrackedPlayer->GetActorLocation();
    FIntVector PlayerChunk = WorldToChunkPosition(PlayerPosition);
    
    // Load chunks around player
    int32 ViewDistance = Config.ViewDistanceInChunks;
    
    for (int32 X = -ViewDistance; X <= ViewDistance; X++)
    {
        for (int32 Y = -ViewDistance; Y <= ViewDistance; Y++)
        {
            for (int32 Z = -2; Z <= 2; Z++) // Limited vertical range
            {
                FIntVector ChunkPos = PlayerChunk + FIntVector(X, Y, Z);
                
                if (ShouldLoadChunk(ChunkPos) && !ActiveChunks.Contains(ChunkPos))
                {
                    GetOrCreateChunk(ChunkPos);
                }
            }
        }
    }
    
    // Unload distant chunks
    TArray<FIntVector> ChunksToUnload;
    
    for (const auto& ChunkPair : ActiveChunks)
    {
        if (!ShouldLoadChunk(ChunkPair.Key))
        {
            ChunksToUnload.Add(ChunkPair.Key);
        }
    }
    
    for (const FIntVector& ChunkPos : ChunksToUnload)
    {
        UnloadChunk(ChunkPos);
    }
    
    LastPlayerPosition = PlayerPosition;
}

void AVoxelWorld::ProcessChunkTasks()
{
    if (!Config.bUseMultithreading)
    {
        return;
    }
    
    int32 TasksProcessed = 0;
    
    while (ActiveGenerations.GetValue() < Config.MaxConcurrentChunkGenerations && TasksProcessed < MaxChunksPerFrame)
    {
        FVoxelChunkTask Task;
        
        {
            FScopeLock Lock(&TaskQueueLock);
            if (!ChunkTaskQueue.Dequeue(Task))
            {
                break;
            }
        }
        
        // Check if chunk still exists and needs generation
        if (AVoxelChunk** ChunkPtr = ActiveChunks.Find(Task.ChunkPosition))
        {
            if (AVoxelChunk* Chunk = *ChunkPtr)
            {
                if (UVoxelChunkComponent* ChunkComp = Chunk->ChunkComponent)
                {
                    if (ChunkComp->GetState() != EVoxelChunkState::Ready || Task.bIsRegeneration)
                    {
                        ActiveGenerations.Increment();
                        ChunkComp->GenerateMesh(true);
                    }
                }
            }
        }
        
        TasksProcessed++;
    }
}

void AVoxelWorld::UpdateMemoryUsage()
{
    float TotalMemoryMB = 0.0f;
    int32 TotalTriangles = 0;
    int32 TotalVertices = 0;
    
    for (const auto& ChunkPair : ActiveChunks)
    {
        if (ChunkPair.Value && ChunkPair.Value->ChunkComponent)
        {
            FVoxelPerformanceStats ChunkStats = ChunkPair.Value->ChunkComponent->GetPerformanceStats();
            TotalTriangles += ChunkStats.TriangleCount;
            TotalVertices += ChunkStats.VertexCount;
        }
    }
    
    // Estimate memory usage
    TotalMemoryMB = (ActiveChunks.Num() * 0.1f) + // Chunk overhead
                    (TotalVertices * 32.0f / 1024.0f / 1024.0f) + // Vertex data
                    (TotalTriangles * 12.0f / 1024.0f / 1024.0f); // Index data
    
    WorldStats.ActiveChunks = ActiveChunks.Num();
    WorldStats.MemoryUsageMB = TotalMemoryMB;
    WorldStats.TriangleCount = TotalTriangles;
    WorldStats.VertexCount = TotalVertices;
}

void AVoxelWorld::EnforceMemoryBudget()
{
    float CurrentMemoryMB = WorldStats.MemoryUsageMB;
    float MemoryBudget = Config.PCMemoryBudgetMB;
    
#if VOXEL_MOBILE_PLATFORM
    MemoryBudget = Config.MobileMemoryBudgetMB;
#endif
    
    if (CurrentMemoryMB > MemoryBudget)
    {
        UE_LOG(LogHearthshireVoxel, Warning, TEXT("Memory usage (%.1fMB) exceeds budget (%.1fMB). Unloading distant chunks."),
            CurrentMemoryMB, MemoryBudget);
        
        // Sort chunks by distance from player and unload furthest
        TArray<TPair<float, FIntVector>> ChunkDistances;
        FVector PlayerPos = TrackedPlayer ? TrackedPlayer->GetActorLocation() : FVector::ZeroVector;
        
        for (const auto& ChunkPair : ActiveChunks)
        {
            FVector ChunkWorldPos = FVector(ChunkPair.Key) * Config.ChunkSize * VoxelSize;
            float Distance = FVector::Dist(ChunkWorldPos, PlayerPos);
            ChunkDistances.Add(TPair<float, FIntVector>(Distance, ChunkPair.Key));
        }
        
        ChunkDistances.Sort([](const TPair<float, FIntVector>& A, const TPair<float, FIntVector>& B)
        {
            return A.Key > B.Key; // Sort descending by distance
        });
        
        // Unload chunks until under budget
        int32 ChunksToUnload = FMath::Max(1, ChunkDistances.Num() / 10); // Unload 10% of chunks
        for (int32 i = 0; i < ChunksToUnload && i < ChunkDistances.Num(); i++)
        {
            UnloadChunk(ChunkDistances[i].Value);
        }
    }
}

AVoxelChunk* AVoxelWorld::GetChunkFromPool()
{
    if (ChunkPool.Num() > 0)
    {
        AVoxelChunk* Chunk = ChunkPool.Pop();
        Chunk->ResetChunk();
        return Chunk;
    }
    
    return nullptr;
}

void AVoxelWorld::ReturnChunkToPool(AVoxelChunk* Chunk)
{
    if (!Chunk)
    {
        return;
    }
    
    Chunk->ReturnToPool();
    ChunkPool.Add(Chunk);
}

void AVoxelWorld::QueueChunkGeneration(const FIntVector& ChunkPosition, int32 Priority, bool bRegeneration)
{
    FVoxelChunkTask Task;
    Task.ChunkPosition = ChunkPosition;
    Task.Priority = Priority;
    Task.bIsRegeneration = bRegeneration;
    
    FScopeLock Lock(&TaskQueueLock);
    ChunkTaskQueue.Enqueue(Task);
}

bool AVoxelWorld::ShouldLoadChunk(const FIntVector& ChunkPosition) const
{
    if (!TrackedPlayer)
    {
        return false;
    }
    
    FVector ChunkWorldPos = FVector(ChunkPosition) * Config.ChunkSize * VoxelSize;
    FVector PlayerPos = TrackedPlayer->GetActorLocation();
    
    float Distance = FVector::Dist2D(ChunkWorldPos, PlayerPos);
    float MaxDistance = Config.ViewDistanceInChunks * Config.ChunkSize * VoxelSize;
    
    return Distance <= MaxDistance;
}

int32 AVoxelWorld::CalculateChunkPriority(const FIntVector& ChunkPosition) const
{
    if (!TrackedPlayer)
    {
        return 999;
    }
    
    FVector ChunkWorldPos = FVector(ChunkPosition) * Config.ChunkSize * VoxelSize;
    FVector PlayerPos = TrackedPlayer->GetActorLocation();
    
    float Distance = FVector::Dist(ChunkWorldPos, PlayerPos);
    return FMath::Clamp(FMath::FloorToInt(Distance / 1000.0f), 0, 999);
}

void AVoxelWorld::OnChunkGenerated(UVoxelChunkComponent* ChunkComponent)
{
    ActiveGenerations.Decrement();
    
    if (ChunkComponent)
    {
        FVoxelPerformanceStats ChunkStats = ChunkComponent->GetPerformanceStats();
        
        // Update world stats
        WorldStats.MeshGenerationTimeMs = FMath::Max(WorldStats.MeshGenerationTimeMs, ChunkStats.MeshGenerationTimeMs);
        WorldStats.GreedyMeshingTimeMs = FMath::Max(WorldStats.GreedyMeshingTimeMs, ChunkStats.GreedyMeshingTimeMs);
    }
}

// UVoxelWorldComponent Implementation

UVoxelWorldComponent::UVoxelWorldComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
    VoxelWorld = nullptr;
}

void UVoxelWorldComponent::BeginPlay()
{
    Super::BeginPlay();
    
    // Create voxel world actor
    if (AActor* Owner = GetOwner())
    {
        FActorSpawnParameters SpawnParams;
        SpawnParams.Owner = Owner;
        
        VoxelWorld = GetWorld()->SpawnActor<AVoxelWorld>(AVoxelWorld::StaticClass(), Owner->GetActorTransform(), SpawnParams);
        if (VoxelWorld)
        {
            VoxelWorld->Config = Config;
            VoxelWorld->AttachToActor(Owner, FAttachmentTransformRules::KeepRelativeTransform);
        }
    }
}

void UVoxelWorldComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (VoxelWorld)
    {
        VoxelWorld->Destroy();
        VoxelWorld = nullptr;
    }
    
    Super::EndPlay(EndPlayReason);
}