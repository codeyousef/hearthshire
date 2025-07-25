// Copyright Epic Games, Inc. All Rights Reserved.

#include "VoxelWorld.h"
#include "VoxelChunk.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "HearthshireVoxelModule.h"
#include "Async/ParallelFor.h"
#include "VoxelPerformanceTest.h"
#include "VoxelBlueprintLibrary.h"
#include "VoxelWorldTemplate.h"
#include "Engine/AssetManager.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "UObject/SavePackage.h"
#include "Misc/PackageName.h"

#if WITH_EDITOR
#include "Misc/MessageDialog.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Widgets/Notifications/SNotificationList.h"
#endif

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
    
    UE_LOG(LogHearthshireVoxel, Warning, TEXT("=== VoxelWorld BeginPlay ==="));
    UE_LOG(LogHearthshireVoxel, Warning, TEXT("  bPreserveEditorChunks = %s"), bPreserveEditorChunks ? TEXT("TRUE") : TEXT("FALSE"));
    UE_LOG(LogHearthshireVoxel, Warning, TEXT("  bDisableDynamicGeneration = %s"), bDisableDynamicGeneration ? TEXT("TRUE") : TEXT("FALSE"));
    UE_LOG(LogHearthshireVoxel, Warning, TEXT("  bFlatWorldMode = %s"), bFlatWorldMode ? TEXT("TRUE") : TEXT("FALSE"));
    UE_LOG(LogHearthshireVoxel, Warning, TEXT("  ActiveChunks.Num() = %d"), ActiveChunks.Num());
    
    // Find all chunk actors that exist in the world (created in editor)
    if (bPreserveEditorChunks)
    {
        TArray<AActor*> FoundActors;
        UGameplayStatics::GetAllActorsOfClass(GetWorld(), AVoxelChunk::StaticClass(), FoundActors);
        
        UE_LOG(LogHearthshireVoxel, Warning, TEXT("=== BeginPlay: Found %d VoxelChunk actors in world ==="), FoundActors.Num());
        
        // First, destroy any chunks that aren't attached to this VoxelWorld
        TArray<AVoxelChunk*> ChunksToDestroy;
        for (AActor* Actor : FoundActors)
        {
            if (AVoxelChunk* Chunk = Cast<AVoxelChunk>(Actor))
            {
                // Check if this chunk belongs to this VoxelWorld
                if (Chunk->GetAttachParentActor() != this)
                {
                    ChunksToDestroy.Add(Chunk);
                    UE_LOG(LogHearthshireVoxel, Warning, TEXT("Found orphaned chunk, will destroy"));
                }
            }
        }
        
        // Destroy orphaned chunks
        for (AVoxelChunk* Chunk : ChunksToDestroy)
        {
            Chunk->Destroy();
        }
        
        // Now process chunks that belong to this world
        for (AActor* Actor : FoundActors)
        {
            if (AVoxelChunk* Chunk = Cast<AVoxelChunk>(Actor))
            {
                if (Chunk->GetAttachParentActor() == this && !ChunksToDestroy.Contains(Chunk))
                {
                    if (UVoxelChunkComponent* ChunkComp = Chunk->ChunkComponent)
                    {
                        FIntVector ChunkPos = ChunkComp->GetChunkPosition();
                        
                        UE_LOG(LogHearthshireVoxel, Warning, TEXT("Processing chunk at %s, HasBeenGenerated=%d"), 
                            *ChunkPos.ToString(), ChunkComp->HasBeenGenerated() ? 1 : 0);
                        
                        // Add to ActiveChunks if not already there
                        if (!ActiveChunks.Contains(ChunkPos))
                        {
                            ActiveChunks.Add(ChunkPos, Chunk);
                            UE_LOG(LogHearthshireVoxel, Warning, TEXT("Preserved editor chunk at %s"), *ChunkPos.ToString());
                        }
                        
                        // Ensure chunk is properly initialized
                        Chunk->InitializeChunk(ChunkPos, FVoxelChunkSize(Config.ChunkSize), this);
                        
                        // Mark as generated to prevent regeneration
                        ChunkComp->MarkAsGenerated();
                        
                        // Set material set if available
                        if (Config.MaterialSet)
                        {
                            ChunkComp->SetMaterialSet(Config.MaterialSet);
                        }
                    }
                }
            }
        }
        
        UE_LOG(LogHearthshireVoxel, Warning, TEXT("=== Total preserved chunks: %d ==="), ActiveChunks.Num());
        
        // Auto-detect flat world mode if all preserved chunks are at Z=0
        if (ActiveChunks.Num() > 0)
        {
            bool bAllChunksAtZ0 = true;
            for (const auto& ChunkPair : ActiveChunks)
            {
                if (ChunkPair.Key.Z != 0)
                {
                    bAllChunksAtZ0 = false;
                    break;
                }
            }
            
            if (bAllChunksAtZ0)
            {
                UE_LOG(LogHearthshireVoxel, Warning, TEXT("Auto-detected flat world (all chunks at Z=0) - enabling flat world mode and disabling dynamic generation"));
                bFlatWorldMode = true;
                bDisableDynamicGeneration = true;
            }
        }
    }
    
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
    
    // If we have preserved chunks and dynamic generation is disabled, we're done
    if (ActiveChunks.Num() > 0 && bDisableDynamicGeneration)
    {
        UE_LOG(LogHearthshireVoxel, Warning, TEXT("Skipping initial chunk generation - have %d preserved chunks and dynamic generation disabled"), 
            ActiveChunks.Num());
    }
    
    OnWorldInitialized.Broadcast();
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
    
    // Skip chunk updates if disabled or if we have preserved editor chunks
    bool bShouldUpdateChunks = !bDisableDynamicGeneration;
    
    if (bShouldUpdateChunks && bPreserveEditorChunks && ActiveChunks.Num() > 0)
    {
        // Check if any chunks are manually generated
        for (const auto& ChunkPair : ActiveChunks)
        {
            if (ChunkPair.Value && ChunkPair.Value->ChunkComponent && 
                ChunkPair.Value->ChunkComponent->HasBeenGenerated())
            {
                bShouldUpdateChunks = false;
                UE_LOG(LogHearthshireVoxel, VeryVerbose, TEXT("Skipping chunk updates - found manually generated chunks"));
                break;
            }
        }
    }
    
    // Update chunks based on player position
    ChunkUpdateTimer += DeltaTime;
    if (bShouldUpdateChunks && ChunkUpdateTimer >= ChunkUpdateInterval)
    {
        ChunkUpdateTimer = 0.0f;
        UpdateChunks();
    }
    
    // Process chunk generation tasks (skip if dynamic generation is disabled)
    if (!bDisableDynamicGeneration)
    {
        ProcessChunkTasks();
    }
    
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
        UE_LOG(LogHearthshireVoxel, VeryVerbose, TEXT("GetOrCreateChunk: Chunk already exists at %s, returning existing"), *ChunkPosition.ToString());
        return *ExistingChunk;
    }
    
    // If dynamic generation is disabled, don't create new chunks
    if (bDisableDynamicGeneration)
    {
        UE_LOG(LogHearthshireVoxel, VeryVerbose, TEXT("GetOrCreateChunk: Dynamic generation disabled, not creating chunk at %s"), *ChunkPosition.ToString());
        return nullptr;
    }
    
    // If in flat world mode, only allow chunks at Z=0
    if (bFlatWorldMode && ChunkPosition.Z != 0)
    {
        UE_LOG(LogHearthshireVoxel, VeryVerbose, TEXT("GetOrCreateChunk: Flat world mode enabled, rejecting chunk at Z=%d"), ChunkPosition.Z);
        return nullptr;
    }
    
    // Debug: log the call stack to see where this is coming from
    UE_LOG(LogHearthshireVoxel, Error, TEXT("=== CREATING NEW CHUNK at %s (Z level: %d) ==="), *ChunkPosition.ToString(), ChunkPosition.Z);
    UE_LOG(LogHearthshireVoxel, Error, TEXT("Call stack: bDisableDynamicGeneration=%d"), bDisableDynamicGeneration ? 1 : 0);
    
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
        
        // Check if we should load from template
        bool bLoadedFromTemplate = false;
        if (bUseTemplate && WorldTemplate)
        {
            FVoxelChunkData TemplateChunkData;
            if (LoadChunkFromTemplate(ChunkPosition, TemplateChunkData))
            {
                ChunkComp->SetChunkData(TemplateChunkData);
                bLoadedFromTemplate = true;
                UE_LOG(LogHearthshireVoxel, Log, TEXT("Loaded chunk %s from template"), *ChunkPosition.ToString());
            }
        }
        
        // If not loaded from template and chunk hasn't been manually generated, generate procedurally
        if (!bLoadedFromTemplate && !ChunkComp->HasBeenGenerated())
        {
            // Generate smooth rolling hills using Perlin noise
            const float NoiseScale = 0.03f; // Adjust for hill frequency
            const float HeightScale = 10.0f; // Max height variation
            const float BaseHeight = 10.0f; // Base terrain height
            
            for (int32 Y = 0; Y < ChunkSize.Y; Y++)
            {
                for (int32 X = 0; X < ChunkSize.X; X++)
                {
                    // Calculate world position for seamless noise across chunks
                    float WorldX = (ChunkPosition.X * ChunkSize.X + X) * NoiseScale;
                    float WorldY = (ChunkPosition.Y * ChunkSize.Y + Y) * NoiseScale;
                    
                    // Generate height using 2D Perlin noise
                    float NoiseValue = FMath::PerlinNoise2D(FVector2D(WorldX, WorldY));
                    // Convert from [-1, 1] to [0, 1]
                    NoiseValue = (NoiseValue + 1.0f) * 0.5f;
                    
                    // Calculate terrain height (5-15 voxels)
                    int32 TerrainHeight = FMath::FloorToInt(BaseHeight + NoiseValue * HeightScale);
                    TerrainHeight = FMath::Clamp(TerrainHeight, 5, 15);
                    
                    // Fill voxels from bottom to height
                    for (int32 Z = 0; Z < ChunkSize.Z; Z++)
                    {
                        if (Z < TerrainHeight)
                        {
                            EVoxelMaterial Material;
                            
                            // Top layer is grass
                            if (Z == TerrainHeight - 1)
                            {
                                Material = EVoxelMaterial::Grass;
                            }
                            // Next 3 layers are dirt
                            else if (Z >= TerrainHeight - 4)
                            {
                                Material = EVoxelMaterial::Dirt;
                            }
                            // Everything else is stone
                            else
                            {
                                Material = EVoxelMaterial::Stone;
                            }
                            
                            ChunkComp->SetVoxel(X, Y, Z, Material);
                        }
                        // else leave as air
                    }
                }
            }
            
            // Don't generate mesh here - let QueueChunkGeneration handle it
        }
    }
    
    // Add to active chunks
    ActiveChunks.Add(ChunkPosition, NewChunk);
    
    // Queue for generation only if chunk hasn't been manually generated
    if (NewChunk && NewChunk->ChunkComponent && !NewChunk->ChunkComponent->HasBeenGenerated())
    {
        QueueChunkGeneration(ChunkPosition, CalculateChunkPriority(ChunkPosition));
    }
    
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
    if (!TrackedPlayer || bDisableDynamicGeneration)
    {
        UE_LOG(LogHearthshireVoxel, VeryVerbose, TEXT("UpdateChunks: Skipping - TrackedPlayer=%p, bDisableDynamicGeneration=%d"), 
            TrackedPlayer, bDisableDynamicGeneration ? 1 : 0);
        return;
    }
    
    UE_LOG(LogHearthshireVoxel, Warning, TEXT("=== UpdateChunks called - creating chunks around player ==="));
    UE_LOG(LogHearthshireVoxel, Warning, TEXT("  bFlatWorldMode = %s"), bFlatWorldMode ? TEXT("TRUE") : TEXT("FALSE"));
    UE_LOG(LogHearthshireVoxel, Warning, TEXT("  bDisableDynamicGeneration = %s (should be TRUE!)"), bDisableDynamicGeneration ? TEXT("TRUE") : TEXT("FALSE"));
    
    FVector PlayerPosition = TrackedPlayer->GetActorLocation();
    FIntVector PlayerChunk = WorldToChunkPosition(PlayerPosition);
    
    // Load chunks around player
    int32 ViewDistance = Config.ViewDistanceInChunks;
    
    for (int32 X = -ViewDistance; X <= ViewDistance; X++)
    {
        for (int32 Y = -ViewDistance; Y <= ViewDistance; Y++)
        {
            // Determine Z range based on flat world mode
            int32 MinZ = bFlatWorldMode ? 0 : -2;
            int32 MaxZ = bFlatWorldMode ? 0 : 2;
            
            for (int32 Z = MinZ; Z <= MaxZ; Z++)
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
                    // Skip if chunk has been manually generated (unless it's a forced regeneration)
                    if (ChunkComp->HasBeenGenerated() && !Task.bIsRegeneration)
                    {
                        continue;
                    }
                    
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
        
        // Broadcast event only on first time exceeding
        static bool bWasUnderBudget = true;
        if (bWasUnderBudget)
        {
            OnMemoryBudgetExceeded.Broadcast();
            bWasUnderBudget = false;
        }
        
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
    else
    {
        // Reset the flag when we're back under budget
        static bool bWasUnderBudget = true;
        bWasUnderBudget = true;
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
    
    OnChunkGenerationQueued.Broadcast(ChunkPosition, Priority);
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

TArray<AVoxelChunk*> AVoxelWorld::GetAllActiveChunks() const
{
    TArray<AVoxelChunk*> Result;
    Result.Reserve(ActiveChunks.Num());
    
    for (const auto& ChunkPair : ActiveChunks)
    {
        if (ChunkPair.Value)
        {
            Result.Add(ChunkPair.Value);
        }
    }
    
    return Result;
}

void AVoxelWorld::UnloadAllChunks()
{
    TArray<FIntVector> ChunkPositions;
    ActiveChunks.GetKeys(ChunkPositions);
    
    for (const FIntVector& Position : ChunkPositions)
    {
        UnloadChunk(Position);
    }
}

void AVoxelWorld::SetMemoryBudget(int32 NewBudgetMB)
{
#if VOXEL_MOBILE_PLATFORM
    Config.MobileMemoryBudgetMB = NewBudgetMB;
#else
    Config.PCMemoryBudgetMB = NewBudgetMB;
#endif
    
    // Immediately enforce new budget
    UpdateMemoryUsage();
    EnforceMemoryBudget();
}

AVoxelChunk* AVoxelWorld::GetChunkAtPosition(const FIntVector& ChunkPosition) const
{
    if (AVoxelChunk* const* ChunkPtr = ActiveChunks.Find(ChunkPosition))
    {
        return *ChunkPtr;
    }
    return nullptr;
}

void AVoxelWorld::GenerateTestTerrain()
{
    // Generate a 5x5 grid of chunks around origin
    const int32 GridSize = 5;
    const int32 HalfGrid = GridSize / 2;
    
    for (int32 X = -HalfGrid; X <= HalfGrid; X++)
    {
        for (int32 Y = -HalfGrid; Y <= HalfGrid; Y++)
        {
            FIntVector ChunkPos(X, Y, 0);
            GetOrCreateChunk(ChunkPos);
        }
    }
    
    UE_LOG(LogHearthshireVoxel, Log, TEXT("Generated test terrain with %d chunks"), GridSize * GridSize);
}

void AVoxelWorld::GenerateFlatWorld()
{
    UE_LOG(LogHearthshireVoxel, Warning, TEXT("=== GenerateFlatWorld called ==="));
    
    // Enable flat world mode and disable dynamic generation
    bFlatWorldMode = true;
    bDisableDynamicGeneration = true;
    
    UE_LOG(LogHearthshireVoxel, Warning, TEXT("Set bFlatWorldMode = TRUE"));
    UE_LOG(LogHearthshireVoxel, Warning, TEXT("Set bDisableDynamicGeneration = TRUE"));
    
    // Only unload chunks if we're in editor mode
    // In Play mode, we want to keep existing chunks
    if (GetWorld() && !GetWorld()->IsPlayInEditor())
    {
        // First, unload all chunks to start fresh
        UnloadAllChunks();
    }
    
    // Clear any pending generation tasks
    {
        FScopeLock Lock(&TaskQueueLock);
        while (!ChunkTaskQueue.IsEmpty())
        {
            FVoxelChunkTask Task;
            ChunkTaskQueue.Dequeue(Task);
        }
    }
    GeneratingChunks.Empty();
    
    // Set a flag to prevent automatic terrain generation
    bool bOriginalUseTemplate = bUseTemplate;
    bUseTemplate = true; // This prevents automatic terrain generation in GetOrCreateChunk
    WorldTemplate = nullptr; // But with no template, it won't load anything
    
    // Create a 5x5 grid of chunks
    const int32 GridSize = 5;
    const int32 HalfGrid = GridSize / 2;
    
    // Create chunks using GetOrCreateChunk to ensure proper setup
    for (int32 ChunkX = -HalfGrid; ChunkX <= HalfGrid; ChunkX++)
    {
        for (int32 ChunkY = -HalfGrid; ChunkY <= HalfGrid; ChunkY++)
        {
            FIntVector ChunkPos(ChunkX, ChunkY, 0);
            AVoxelChunk* NewChunk = GetOrCreateChunk(ChunkPos);
            
            if (!NewChunk || !NewChunk->ChunkComponent)
            {
                UE_LOG(LogHearthshireVoxel, Error, TEXT("Failed to create chunk at %s"), *ChunkPos.ToString());
                continue;
            }
            
            // Mark as generated immediately to prevent queued generation
            NewChunk->ChunkComponent->MarkAsGenerated();
        }
    }
    
    // Restore original template setting
    bUseTemplate = bOriginalUseTemplate;
    
    // Now fill all chunks with flat terrain at height 10
    const int32 FlatHeight = 10;
    
    for (const auto& ChunkPair : ActiveChunks)
    {
        if (ChunkPair.Value && ChunkPair.Value->ChunkComponent)
        {
            UVoxelChunkComponent* ChunkComp = ChunkPair.Value->ChunkComponent;
            const FIntVector& ChunkPos = ChunkPair.Key;
            const FVoxelChunkSize& ChunkSize = ChunkComp->GetChunkSize();
            
            
            // Clear the chunk first - ensure ALL voxels are set to air
            ChunkComp->ClearChunk();
            
            // Double-check the chunk is clear by explicitly setting all voxels to air first
            for (int32 LocalX = 0; LocalX < ChunkSize.X; LocalX++)
            {
                for (int32 LocalY = 0; LocalY < ChunkSize.Y; LocalY++)
                {
                    for (int32 LocalZ = 0; LocalZ < ChunkSize.Z; LocalZ++)
                    {
                        ChunkComp->SetVoxel(LocalX, LocalY, LocalZ, EVoxelMaterial::Air);
                    }
                }
            }
            
            // Now fill with flat terrain
            for (int32 LocalX = 0; LocalX < ChunkSize.X; LocalX++)
            {
                for (int32 LocalY = 0; LocalY < ChunkSize.Y; LocalY++)
                {
                    for (int32 LocalZ = 0; LocalZ < FlatHeight; LocalZ++)
                    {
                        EVoxelMaterial Material;
                        
                        // Top layer (Z = 9) is grass
                        if (LocalZ == FlatHeight - 1)
                        {
                            Material = EVoxelMaterial::Grass;
                        }
                        // Everything below is dirt
                        else
                        {
                            Material = EVoxelMaterial::Dirt;
                        }
                        
                        ChunkComp->SetVoxel(LocalX, LocalY, LocalZ, Material);
                    }
                }
            }
            
            // Log voxel count for debugging
            int32 VoxelCount = ChunkComp->GetVoxelCount();
            int32 ExpectedCount = ChunkSize.X * ChunkSize.Y * FlatHeight;
            
            // Test a few voxel positions to ensure they're set correctly
            EVoxelMaterial TestMat1 = ChunkComp->GetVoxel(0, 0, 0); // Should be Dirt
            EVoxelMaterial TestMat2 = ChunkComp->GetVoxel(0, 0, 9); // Should be Grass
            EVoxelMaterial TestMat3 = ChunkComp->GetVoxel(0, 0, 10); // Should be Air
            
            
            // Count voxels at each Z level to debug the pyramid issue
            TArray<int32> VoxelsPerLevel;
            VoxelsPerLevel.SetNum(ChunkSize.Z);
            for (int32 Z = 0; Z < ChunkSize.Z; Z++)
            {
                int32 CountAtZ = 0;
                for (int32 Y = 0; Y < ChunkSize.Y; Y++)
                {
                    for (int32 X = 0; X < ChunkSize.X; X++)
                    {
                        if (ChunkComp->GetVoxel(X, Y, Z) != EVoxelMaterial::Air)
                        {
                            CountAtZ++;
                        }
                    }
                }
                VoxelsPerLevel[Z] = CountAtZ;
                if (CountAtZ > 0)
                {
                }
            }
            
            // Mark chunk as manually generated so it won't be regenerated
            ChunkComp->MarkAsGenerated();
            
            // Generate mesh immediately
            ChunkComp->GenerateMesh(false);
        }
    }
    
}

void AVoxelWorld::ClearAllVoxels()
{
    for (const auto& ChunkPair : ActiveChunks)
    {
        if (ChunkPair.Value && ChunkPair.Value->ChunkComponent)
        {
            ChunkPair.Value->ClearAllVoxels();
        }
    }
}

void AVoxelWorld::RunPerformanceTest()
{
    TArray<FVoxelTestResult> Results = UVoxelPerformanceTest::RunAllPerformanceTests(this);
    FString Report = UVoxelPerformanceTest::GenerateTestReport(Results);
    
    UE_LOG(LogHearthshireVoxel, Log, TEXT("Performance Test Results:\n%s"), *Report);
}

// Template Functions Implementation

void AVoxelWorld::SaveWorldAsTemplate()
{
#if WITH_EDITOR
    // If no template assigned, create a new one
    if (!WorldTemplate)
    {
        CreateNewTemplateAsset();
        if (!WorldTemplate)
        {
            return; // Failed to create template
        }
    }
    
    // Use the configured template name
    FString TemplateName = TemplateSaveName;
    if (TemplateName.IsEmpty())
    {
        TemplateName = "UnnamedTemplate";
    }
    
    // Update template metadata
    WorldTemplate->TemplateName = TemplateName;
    WorldTemplate->Description = TemplateDescription;
    
    bool bSuccess = UVoxelTemplateUtility::SaveWorldAsTemplate(this, WorldTemplate, TemplateName);
    
    if (bSuccess)
    {
        // Mark the asset as dirty so it saves
        WorldTemplate->MarkPackageDirty();
        
        // Show success notification
        FNotificationInfo Info(FText::Format(
            FText::FromString("World saved as template: {0}"),
            FText::FromString(TemplateName)
        ));
        Info.ExpireDuration = 4.0f;
        Info.bUseLargeFont = true;
        Info.bUseSuccessFailIcons = true;
        Info.Image = FSlateIcon(FSlateIcon("EditorStyle", "Icons.SuccessWithCircle").GetStyleSetName(), 
                                FSlateIcon("EditorStyle", "Icons.SuccessWithCircle").GetStyleName()).GetIcon();
        
        FSlateNotificationManager::Get().AddNotification(Info);
        
        UE_LOG(LogHearthshireVoxel, Log, TEXT("World saved to template: %s at %s"), 
            *TemplateName, *WorldTemplate->GetPathName());
    }
    else
    {
        FText Title = FText::FromString("Save Failed");
        FText Message = FText::FromString("Failed to save world to template. Check the log for details.");
        FMessageDialog::Open(EAppMsgType::Ok, Message, Title);
    }
#endif
}

void AVoxelWorld::LoadFromTemplate()
{
#if WITH_EDITOR
    if (!WorldTemplate)
    {
        FText Title = FText::FromString("No Template Asset");
        FText Message = FText::FromString("Please assign a World Template asset to load world data from.");
        FMessageDialog::Open(EAppMsgType::Ok, Message, Title);
        return;
    }
    
    // Clear existing world
    UnloadAllChunks();
    
    // Load all chunks from template
    int32 LoadedChunks = 0;
    for (const FVoxelTemplateChunk& TemplateChunk : WorldTemplate->ChunkData)
    {
        if (TemplateChunk.bHasData)
        {
            AVoxelChunk* Chunk = GetOrCreateChunk(TemplateChunk.ChunkPosition);
            if (Chunk)
            {
                UVoxelChunkComponent* ChunkComp = Chunk->FindComponentByClass<UVoxelChunkComponent>();
                if (ChunkComp)
                {
                    FVoxelChunkData ChunkData;
                    if (LoadChunkFromTemplate(TemplateChunk.ChunkPosition, ChunkData))
                    {
                        // Apply the loaded data to the chunk
                        ChunkComp->SetChunkData(ChunkData);
                        ChunkComp->GenerateMesh(false);
                        LoadedChunks++;
                    }
                }
            }
        }
    }
    
    // Spawn landmarks
    SpawnLandmarkActors();
    
    // Show success notification
    FNotificationInfo Info(FText::Format(
        FText::FromString("Loaded {0} chunks from template: {1}"),
        FText::AsNumber(LoadedChunks),
        FText::FromString(WorldTemplate->TemplateName)
    ));
    Info.ExpireDuration = 4.0f;
    Info.bUseLargeFont = true;
    Info.bUseSuccessFailIcons = true;
    Info.Image = FSlateIcon(FSlateIcon("EditorStyle", "Icons.SuccessWithCircle").GetStyleSetName(), 
                            FSlateIcon("EditorStyle", "Icons.SuccessWithCircle").GetStyleName()).GetIcon();
    
    FSlateNotificationManager::Get().AddNotification(Info);
    
    // Enable template mode
    bUseTemplate = true;
    
    UE_LOG(LogHearthshireVoxel, Log, TEXT("Successfully loaded %d chunks from template: %s"), 
        LoadedChunks, *WorldTemplate->TemplateName);
#endif
}

bool AVoxelWorld::LoadChunkFromTemplate(const FIntVector& ChunkPosition, FVoxelChunkData& OutChunkData)
{
    if (!WorldTemplate || !bUseTemplate)
    {
        return false;
    }
    
    // Load base chunk data from template
    if (!UVoxelTemplateUtility::LoadChunkFromTemplate(WorldTemplate, ChunkPosition, OutChunkData))
    {
        return false;
    }
    
    // Apply seed-based variations if enabled
    if (WorldTemplate->bAllowSeedVariations)
    {
        UVoxelTemplateUtility::ApplySeedVariations(OutChunkData, WorldTemplate, WorldSeed, ChunkPosition);
    }
    
    return true;
}

void AVoxelWorld::CreateNewTemplateAsset()
{
#if WITH_EDITOR
    // Ensure save folder exists
    FString PackagePath = FString::Printf(TEXT("/Game/%s"), *TemplateSaveFolder);
    
    // Create package name from template name
    FString AssetName = TemplateSaveName;
    if (AssetName.IsEmpty())
    {
        AssetName = FString::Printf(TEXT("WorldTemplate_%d"), FDateTime::Now().GetTicks());
    }
    
    // Sanitize the asset name
    AssetName = FPackageName::GetLongPackageAssetName(AssetName);
    
    FString PackageName = FString::Printf(TEXT("%s%s"), *PackagePath, *AssetName);
    UPackage* Package = CreatePackage(*PackageName);
    
    if (!Package)
    {
        FText Title = FText::FromString("Create Failed");
        FText Message = FText::FromString("Failed to create package for template asset.");
        FMessageDialog::Open(EAppMsgType::Ok, Message, Title);
        return;
    }
    
    // Create the template asset
    UVoxelWorldTemplate* NewTemplate = NewObject<UVoxelWorldTemplate>(Package, UVoxelWorldTemplate::StaticClass(), *AssetName, RF_Public | RF_Standalone);
    
    if (NewTemplate)
    {
        // Initialize template properties
        NewTemplate->TemplateName = TemplateSaveName;
        NewTemplate->Description = TemplateDescription;
        NewTemplate->CreationDate = FDateTime::Now();
        NewTemplate->CreatorName = FPlatformProcess::UserName();
        
        // Mark package dirty
        Package->MarkPackageDirty();
        
        // Register with asset registry
        FAssetRegistryModule::AssetCreated(NewTemplate);
        
        // Assign to world
        WorldTemplate = NewTemplate;
        
        // Show notification
        FNotificationInfo Info(FText::Format(
            FText::FromString("Created new template asset: {0}"),
            FText::FromString(AssetName)
        ));
        Info.ExpireDuration = 3.0f;
        Info.bUseLargeFont = true;
        FSlateNotificationManager::Get().AddNotification(Info);
        
        UE_LOG(LogHearthshireVoxel, Log, TEXT("Created new template asset: %s"), *PackageName);
    }
    else
    {
        FText Title = FText::FromString("Create Failed");
        FText Message = FText::FromString("Failed to create template asset.");
        FMessageDialog::Open(EAppMsgType::Ok, Message, Title);
    }
#endif
}

void AVoxelWorld::RefreshTemplate()
{
    if (bUseTemplate && WorldTemplate)
    {
        LoadFromTemplate();
    }
}

void AVoxelWorld::SpawnLandmarkActors()
{
    if (!WorldTemplate)
    {
        return;
    }
    
    // Spawn actors for each landmark
    for (const FVoxelLandmark& Landmark : WorldTemplate->Landmarks)
    {
        if (Landmark.ActorToSpawn)
        {
            FActorSpawnParameters SpawnParams;
            SpawnParams.Name = FName(*FString::Printf(TEXT("%s_Actor"), *Landmark.Name));
            SpawnParams.Owner = this;
            
            AActor* SpawnedActor = GetWorld()->SpawnActor<AActor>(
                Landmark.ActorToSpawn,
                Landmark.WorldPosition,
                FRotator::ZeroRotator,
                SpawnParams
            );
            
            if (SpawnedActor)
            {
                UE_LOG(LogHearthshireVoxel, Log, TEXT("Spawned landmark actor '%s' at %s"),
                    *Landmark.Name, *Landmark.WorldPosition.ToString());
            }
        }
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