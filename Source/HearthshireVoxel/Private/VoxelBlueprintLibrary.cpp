// Copyright Epic Games, Inc. All Rights Reserved.

#include "VoxelBlueprintLibrary.h"
#include "VoxelWorld.h"
#include "VoxelChunk.h"
#include "Engine/World.h"
#include "DrawDebugHelpers.h"
#include "Kismet/GameplayStatics.h"
#include "HearthshireVoxelModule.h"
#include "Math/UnrealMathUtility.h"

AVoxelWorld* UVoxelBlueprintLibrary::CreateVoxelWorld(
    UObject* WorldContextObject,
    const FTransform& Transform,
    const FVoxelWorldConfig& Config)
{
    UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);
    if (!World)
    {
        return nullptr;
    }
    
    FActorSpawnParameters SpawnParams;
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    
    AVoxelWorld* VoxelWorld = World->SpawnActor<AVoxelWorld>(AVoxelWorld::StaticClass(), Transform, SpawnParams);
    if (VoxelWorld)
    {
        VoxelWorld->Config = Config;
    }
    
    return VoxelWorld;
}

void UVoxelBlueprintLibrary::DestroyVoxelWorld(AVoxelWorld* VoxelWorld)
{
    if (VoxelWorld && IsValid(VoxelWorld))
    {
        VoxelWorld->Destroy();
    }
}

void UVoxelBlueprintLibrary::SetVoxelAtWorldPosition(
    AVoxelWorld* VoxelWorld,
    const FVector& WorldPosition,
    EVoxelMaterial Material)
{
    if (VoxelWorld)
    {
        VoxelWorld->SetVoxel(WorldPosition, Material);
    }
}

EVoxelMaterial UVoxelBlueprintLibrary::GetVoxelAtWorldPosition(
    const AVoxelWorld* VoxelWorld,
    const FVector& WorldPosition)
{
    if (VoxelWorld)
    {
        return VoxelWorld->GetVoxel(WorldPosition);
    }
    
    return EVoxelMaterial::Air;
}

void UVoxelBlueprintLibrary::SetVoxelSphere(
    AVoxelWorld* VoxelWorld,
    const FVector& Center,
    float Radius,
    EVoxelMaterial Material)
{
    if (VoxelWorld)
    {
        VoxelWorld->SetVoxelSphere(Center, Radius, Material);
    }
}

void UVoxelBlueprintLibrary::SetVoxelBox(
    AVoxelWorld* VoxelWorld,
    const FVector& MinCorner,
    const FVector& MaxCorner,
    EVoxelMaterial Material)
{
    if (!VoxelWorld)
    {
        return;
    }
    
    const float VoxelSize = GetVoxelSize();
    
    // Convert world positions to voxel coordinates
    FIntVector MinVoxel = WorldToVoxelPosition(MinCorner, VoxelSize);
    FIntVector MaxVoxel = WorldToVoxelPosition(MaxCorner, VoxelSize);
    
    // Ensure min is actually min
    FIntVector ActualMin(
        FMath::Min(MinVoxel.X, MaxVoxel.X),
        FMath::Min(MinVoxel.Y, MaxVoxel.Y),
        FMath::Min(MinVoxel.Z, MaxVoxel.Z)
    );
    
    FIntVector ActualMax(
        FMath::Max(MinVoxel.X, MaxVoxel.X),
        FMath::Max(MinVoxel.Y, MaxVoxel.Y),
        FMath::Max(MinVoxel.Z, MaxVoxel.Z)
    );
    
    // Set voxels in the box
    for (int32 X = ActualMin.X; X <= ActualMax.X; X++)
    {
        for (int32 Y = ActualMin.Y; Y <= ActualMax.Y; Y++)
        {
            for (int32 Z = ActualMin.Z; Z <= ActualMax.Z; Z++)
            {
                FVector VoxelWorldPos = VoxelToWorldPosition(FIntVector(X, Y, Z), VoxelSize);
                VoxelWorld->SetVoxel(VoxelWorldPos, Material);
            }
        }
    }
}

AVoxelChunk* UVoxelBlueprintLibrary::GetChunkAtWorldPosition(
    const AVoxelWorld* VoxelWorld,
    const FVector& WorldPosition)
{
    if (!VoxelWorld)
    {
        return nullptr;
    }
    
    FIntVector ChunkPos = VoxelWorld->WorldToChunkPosition(WorldPosition);
    
    if (AVoxelChunk* const* ChunkPtr = VoxelWorld->ActiveChunks.Find(ChunkPos))
    {
        return *ChunkPtr;
    }
    
    return nullptr;
}

void UVoxelBlueprintLibrary::RegenerateChunksInRadius(
    AVoxelWorld* VoxelWorld,
    const FVector& Center,
    float Radius)
{
    if (!VoxelWorld)
    {
        return;
    }
    
    const float ChunkWorldSize = VoxelWorld->Config.ChunkSize * GetVoxelSize();
    const int32 ChunkRadius = FMath::CeilToInt(Radius / ChunkWorldSize);
    
    FIntVector CenterChunk = VoxelWorld->WorldToChunkPosition(Center);
    
    for (int32 X = -ChunkRadius; X <= ChunkRadius; X++)
    {
        for (int32 Y = -ChunkRadius; Y <= ChunkRadius; Y++)
        {
            for (int32 Z = -ChunkRadius; Z <= ChunkRadius; Z++)
            {
                FIntVector ChunkPos = CenterChunk + FIntVector(X, Y, Z);
                FVector ChunkWorldPos = FVector(ChunkPos) * ChunkWorldSize;
                
                if (FVector::Dist(ChunkWorldPos, Center) <= Radius)
                {
                    VoxelWorld->RegenerateChunk(ChunkPos);
                }
            }
        }
    }
}

void UVoxelBlueprintLibrary::GenerateFlatTerrain(
    AVoxelWorld* VoxelWorld,
    const FVector& MinCorner,
    const FVector& MaxCorner,
    int32 GroundLevel,
    EVoxelMaterial GroundMaterial,
    EVoxelMaterial UndergroundMaterial)
{
    if (!VoxelWorld)
    {
        return;
    }
    
    const float VoxelSize = GetVoxelSize();
    
    // Convert to voxel coordinates
    FIntVector MinVoxel = WorldToVoxelPosition(MinCorner, VoxelSize);
    FIntVector MaxVoxel = WorldToVoxelPosition(MaxCorner, VoxelSize);
    
    // Generate flat terrain
    for (int32 X = MinVoxel.X; X <= MaxVoxel.X; X++)
    {
        for (int32 Y = MinVoxel.Y; Y <= MaxVoxel.Y; Y++)
        {
            for (int32 Z = MinVoxel.Z; Z <= FMath::Min(MaxVoxel.Z, GroundLevel); Z++)
            {
                FVector VoxelWorldPos = VoxelToWorldPosition(FIntVector(X, Y, Z), VoxelSize);
                
                EVoxelMaterial Material = (Z == GroundLevel) ? GroundMaterial : UndergroundMaterial;
                VoxelWorld->SetVoxel(VoxelWorldPos, Material);
            }
        }
    }
}

void UVoxelBlueprintLibrary::GeneratePerlinTerrain(
    AVoxelWorld* VoxelWorld,
    const FVector& MinCorner,
    const FVector& MaxCorner,
    float NoiseScale,
    float HeightScale,
    int32 BaseHeight)
{
    if (!VoxelWorld)
    {
        return;
    }
    
    const float VoxelSize = GetVoxelSize();
    
    // Convert to voxel coordinates
    FIntVector MinVoxel = WorldToVoxelPosition(MinCorner, VoxelSize);
    FIntVector MaxVoxel = WorldToVoxelPosition(MaxCorner, VoxelSize);
    
    // Generate terrain using Perlin noise
    for (int32 X = MinVoxel.X; X <= MaxVoxel.X; X++)
    {
        for (int32 Y = MinVoxel.Y; Y <= MaxVoxel.Y; Y++)
        {
            // Sample Perlin noise for height
            float NoiseValue = FMath::PerlinNoise2D(FVector2D(X * NoiseScale, Y * NoiseScale));
            int32 Height = BaseHeight + FMath::RoundToInt(NoiseValue * HeightScale);
            
            for (int32 Z = MinVoxel.Z; Z <= FMath::Min(MaxVoxel.Z, Height); Z++)
            {
                FVector VoxelWorldPos = VoxelToWorldPosition(FIntVector(X, Y, Z), VoxelSize);
                
                EVoxelMaterial Material = EVoxelMaterial::Stone;
                if (Z == Height)
                {
                    Material = EVoxelMaterial::Grass;
                }
                else if (Z > Height - 3)
                {
                    Material = EVoxelMaterial::Dirt;
                }
                
                VoxelWorld->SetVoxel(VoxelWorldPos, Material);
            }
        }
    }
}

FIntVector UVoxelBlueprintLibrary::WorldToChunkPosition(
    const FVector& WorldPosition,
    int32 ChunkSize,
    float VoxelSize)
{
    return FIntVector(
        FMath::FloorToInt(WorldPosition.X / (ChunkSize * VoxelSize)),
        FMath::FloorToInt(WorldPosition.Y / (ChunkSize * VoxelSize)),
        FMath::FloorToInt(WorldPosition.Z / (ChunkSize * VoxelSize))
    );
}

FIntVector UVoxelBlueprintLibrary::WorldToVoxelPosition(
    const FVector& WorldPosition,
    float VoxelSize)
{
    return FIntVector(
        FMath::FloorToInt(WorldPosition.X / VoxelSize),
        FMath::FloorToInt(WorldPosition.Y / VoxelSize),
        FMath::FloorToInt(WorldPosition.Z / VoxelSize)
    );
}

FVector UVoxelBlueprintLibrary::VoxelToWorldPosition(
    const FIntVector& VoxelPosition,
    float VoxelSize)
{
    return FVector(VoxelPosition) * VoxelSize;
}

void UVoxelBlueprintLibrary::StartPerformanceMonitoring()
{
    FVoxelPerformanceMonitor::Get().StartMonitoring();
}

void UVoxelBlueprintLibrary::StopPerformanceMonitoring()
{
    FVoxelPerformanceMonitor::Get().StopMonitoring();
}

FVoxelPerformanceReport UVoxelBlueprintLibrary::GetPerformanceReport()
{
    FVoxelPerformanceReport Report;
    
    FString ReportString = FVoxelPerformanceMonitor::Get().GetPerformanceReport();
    Report.PerformanceSummary = ReportString;
    
    // TODO: Parse individual values from the monitor
    
    return Report;
}

void UVoxelBlueprintLibrary::SavePerformanceReportToFile(const FString& FilePath)
{
    FVoxelPerformanceMonitor::Get().DumpCSVData(FilePath);
}

UVoxelMaterialSet* UVoxelBlueprintLibrary::CreateVoxelMaterialSet()
{
    return NewObject<UVoxelMaterialSet>();
}

void UVoxelBlueprintLibrary::SetVoxelMaterial(
    UVoxelMaterialSet* MaterialSet,
    EVoxelMaterial VoxelMaterial,
    UMaterialInterface* Material)
{
    if (MaterialSet)
    {
        if (FVoxelMaterialConfig* Config = MaterialSet->Materials.Find(VoxelMaterial))
        {
            Config->Material = Material;
        }
        else
        {
            FVoxelMaterialConfig NewConfig;
            NewConfig.Material = Material;
            MaterialSet->Materials.Add(VoxelMaterial, NewConfig);
        }
    }
}

int32 UVoxelBlueprintLibrary::GetDefaultChunkSize()
{
#if VOXEL_MOBILE_PLATFORM
    return 16;
#else
    return 32;
#endif
}

bool UVoxelBlueprintLibrary::IsMobilePlatform()
{
#if VOXEL_MOBILE_PLATFORM
    return true;
#else
    return false;
#endif
}

void UVoxelBlueprintLibrary::OptimizeVoxelWorldForPlatform(
    AVoxelWorld* VoxelWorld,
    bool bForceMobileSettings)
{
    if (!VoxelWorld)
    {
        return;
    }
    
    if (bForceMobileSettings || IsMobilePlatform())
    {
        // Mobile settings
        VoxelWorld->Config.ChunkSize = 16;
        VoxelWorld->Config.ViewDistanceInChunks = 6;
        VoxelWorld->Config.MaxConcurrentChunkGenerations = 2;
        VoxelWorld->Config.ChunkPoolSize = 50;
    }
    else
    {
        // PC settings
        VoxelWorld->Config.ChunkSize = 32;
        VoxelWorld->Config.ViewDistanceInChunks = 10;
        VoxelWorld->Config.MaxConcurrentChunkGenerations = 4;
        VoxelWorld->Config.ChunkPoolSize = 100;
    }
}

void UVoxelBlueprintLibrary::DrawDebugVoxel(
    UObject* WorldContextObject,
    const FVector& VoxelPosition,
    float Size,
    const FLinearColor& Color,
    float Duration)
{
    UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);
    if (!World)
    {
        return;
    }
    
    FVector Center = VoxelPosition + FVector(Size * 0.5f);
    DrawDebugBox(World, Center, FVector(Size * 0.5f), Color.ToFColor(true), false, Duration);
}

void UVoxelBlueprintLibrary::DrawDebugChunk(
    UObject* WorldContextObject,
    const FIntVector& ChunkPosition,
    int32 ChunkSize,
    float VoxelSize,
    const FLinearColor& Color,
    float Duration)
{
    UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);
    if (!World)
    {
        return;
    }
    
    FVector ChunkWorldPos = FVector(ChunkPosition) * ChunkSize * VoxelSize;
    FVector ChunkExtent = FVector(ChunkSize) * VoxelSize * 0.5f;
    FVector Center = ChunkWorldPos + ChunkExtent;
    
    DrawDebugBox(World, Center, ChunkExtent, Color.ToFColor(true), false, Duration, 0, 2.0f);
}