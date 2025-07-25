// Copyright Epic Games, Inc. All Rights Reserved.

#include "VoxelTestActor.h"
#include "VoxelWorld.h"
#include "VoxelBlueprintLibrary.h"
#include "VoxelPerformanceStats.h"
#include "Components/SceneComponent.h"
#include "Engine/World.h"
#include "HearthshireVoxelModule.h"

AVoxelTestActor::AVoxelTestActor()
{
    PrimaryActorTick.bCanEverTick = true;
    
    // Create root component
    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("RootComponent"));
    
    // Create voxel world component
    VoxelWorldComponent = CreateDefaultSubobject<UVoxelWorldComponent>(TEXT("VoxelWorldComponent"));
    
    // Set default configuration
    WorldConfig = FVoxelWorldConfig();
    
    bIsRunningPerformanceTest = false;
    PerformanceTestStartTime = 0.0f;
    ChunksGenerated = 0;
    CachedVoxelWorld = nullptr;
}

void AVoxelTestActor::BeginPlay()
{
    Super::BeginPlay();
    
    // Apply configuration to component
    if (VoxelWorldComponent)
    {
        VoxelWorldComponent->Config = WorldConfig;
        CachedVoxelWorld = VoxelWorldComponent->GetVoxelWorld();
    }
    
    // Generate test terrain by default
    GenerateTestTerrain();
}

void AVoxelTestActor::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
    
    // Update performance test if running
    if (bIsRunningPerformanceTest && CachedVoxelWorld)
    {
        float ElapsedTime = GetWorld()->GetTimeSeconds() - PerformanceTestStartTime;
        
        // Run test for 10 seconds
        if (ElapsedTime > 10.0f)
        {
            bIsRunningPerformanceTest = false;
            FVoxelPerformanceMonitor::Get().StopMonitoring();
            
            FString Results = GetPerformanceResults();
            OnPerformanceTestComplete(Results);
        }
    }
}

void AVoxelTestActor::GenerateTestTerrain()
{
    if (!CachedVoxelWorld)
    {
        UE_LOG(LogHearthshireVoxel, Warning, TEXT("No voxel world available"));
        return;
    }
    
    UE_LOG(LogHearthshireVoxel, Log, TEXT("Generating test terrain..."));
    
    // Generate simple hills
    GenerateSimpleHills();
    
    // Add some caves
    GenerateCaves();
    
    // Notify Blueprint
    OnTerrainGenerated();
}

void AVoxelTestActor::ClearTerrain()
{
    if (!CachedVoxelWorld)
    {
        return;
    }
    
    // Clear a large area around the origin
    float ClearRadius = 5000.0f; // 50m radius
    FVector MinCorner = GetActorLocation() - FVector(ClearRadius);
    FVector MaxCorner = GetActorLocation() + FVector(ClearRadius);
    
    UVoxelBlueprintLibrary::SetVoxelBox(CachedVoxelWorld, MinCorner, MaxCorner, EVoxelMaterial::Air);
}

void AVoxelTestActor::CreateSphereAt(const FVector& Location, float Radius, EVoxelMaterial Material)
{
    if (CachedVoxelWorld)
    {
        UVoxelBlueprintLibrary::SetVoxelSphere(CachedVoxelWorld, Location, Radius, Material);
    }
}

void AVoxelTestActor::CreateBoxAt(const FVector& MinCorner, const FVector& MaxCorner, EVoxelMaterial Material)
{
    if (CachedVoxelWorld)
    {
        UVoxelBlueprintLibrary::SetVoxelBox(CachedVoxelWorld, MinCorner, MaxCorner, Material);
    }
}

void AVoxelTestActor::RunPerformanceTest()
{
    if (!CachedVoxelWorld || bIsRunningPerformanceTest)
    {
        return;
    }
    
    UE_LOG(LogHearthshireVoxel, Log, TEXT("Starting performance test..."));
    
    // Start monitoring
    FVoxelPerformanceMonitor::Get().StartMonitoring();
    
    bIsRunningPerformanceTest = true;
    PerformanceTestStartTime = GetWorld()->GetTimeSeconds();
    ChunksGenerated = 0;
    
    // Clear existing terrain
    ClearTerrain();
    
    // Generate varied terrain for testing
    GenerateTestTerrain();
}

FString AVoxelTestActor::GetPerformanceResults() const
{
    return FVoxelPerformanceMonitor::Get().GetPerformanceReport();
}

void AVoxelTestActor::GenerateSimpleHills()
{
    if (!CachedVoxelWorld)
    {
        return;
    }
    
    // Generate terrain in a 100m x 100m area
    float TerrainSize = 10000.0f;
    FVector BasePosition = GetActorLocation();
    FVector MinCorner = BasePosition - FVector(TerrainSize * 0.5f, TerrainSize * 0.5f, 1000.0f);
    FVector MaxCorner = BasePosition + FVector(TerrainSize * 0.5f, TerrainSize * 0.5f, 2000.0f);
    
    // Use Perlin noise for natural terrain
    UVoxelBlueprintLibrary::GeneratePerlinTerrain(
        CachedVoxelWorld,
        MinCorner,
        MaxCorner,
        0.005f,  // Noise scale
        30.0f,   // Height scale (30 voxels = 7.5m variation)
        20       // Base height (20 voxels = 5m)
    );
    
    // Add some trees (simple cylinders)
    for (int32 i = 0; i < 20; i++)
    {
        float X = FMath::RandRange(-TerrainSize * 0.4f, TerrainSize * 0.4f);
        float Y = FMath::RandRange(-TerrainSize * 0.4f, TerrainSize * 0.4f);
        FVector TreeBase = BasePosition + FVector(X, Y, 0);
        
        // Find ground level
        for (int32 Z = 50; Z >= -50; Z--)
        {
            FVector TestPos = TreeBase + FVector(0, 0, Z * 25.0f);
            if (UVoxelBlueprintLibrary::GetVoxelAtWorldPosition(CachedVoxelWorld, TestPos) != EVoxelMaterial::Air)
            {
                // Place tree trunk
                FVector TrunkMin = TestPos + FVector(-25, -25, 25);
                FVector TrunkMax = TestPos + FVector(25, 25, 200);
                UVoxelBlueprintLibrary::SetVoxelBox(CachedVoxelWorld, TrunkMin, TrunkMax, EVoxelMaterial::Wood);
                
                // Place leaves
                FVector LeavesCenter = TestPos + FVector(0, 0, 250);
                UVoxelBlueprintLibrary::SetVoxelSphere(CachedVoxelWorld, LeavesCenter, 150.0f, EVoxelMaterial::Leaves);
                break;
            }
        }
    }
}

void AVoxelTestActor::GenerateCaves()
{
    if (!CachedVoxelWorld)
    {
        return;
    }
    
    // Generate a few cave systems using spheres
    FVector BasePosition = GetActorLocation();
    
    for (int32 i = 0; i < 3; i++)
    {
        // Random cave starting position
        float X = FMath::RandRange(-3000.0f, 3000.0f);
        float Y = FMath::RandRange(-3000.0f, 3000.0f);
        float Z = FMath::RandRange(-500.0f, 0.0f);
        
        FVector CaveStart = BasePosition + FVector(X, Y, Z);
        
        // Create cave path with connected spheres
        FVector CurrentPos = CaveStart;
        for (int32 j = 0; j < 10; j++)
        {
            // Carve out cave section
            float Radius = FMath::RandRange(100.0f, 300.0f);
            UVoxelBlueprintLibrary::SetVoxelSphere(CachedVoxelWorld, CurrentPos, Radius, EVoxelMaterial::Air);
            
            // Move to next position
            FVector Direction = FVector(
                FMath::RandRange(-1.0f, 1.0f),
                FMath::RandRange(-1.0f, 1.0f),
                FMath::RandRange(-0.3f, 0.3f)
            ).GetSafeNormal();
            
            CurrentPos += Direction * FMath::RandRange(200.0f, 400.0f);
        }
    }
}