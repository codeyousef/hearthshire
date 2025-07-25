// Copyright Epic Games, Inc. All Rights Reserved.

#include "VoxelWorldTemplate.h"
#include "VoxelWorld.h"
#include "VoxelChunk.h"
#include "Compression/OodleDataCompression.h"
#include "HAL/PlatformFilemanager.h"
#include "Misc/DateTime.h"
#include "Engine/World.h"
#include "HearthshireVoxelModule.h"

UVoxelWorldTemplate::UVoxelWorldTemplate()
{
    TemplateName = "Unnamed Template";
    Description = "A hand-crafted voxel world template";
    CreationDate = FDateTime::Now();
    CreatorName = "Unknown";
    ChunkSize = 32;
    MinChunkPosition = FIntVector::ZeroValue;
    MaxChunkPosition = FIntVector::ZeroValue;
    bAllowSeedVariations = true;
}

FVector UVoxelWorldTemplate::GetWorldSize() const
{
    FIntVector SizeInChunks = MaxChunkPosition - MinChunkPosition + FIntVector(1, 1, 1);
    return FVector(SizeInChunks) * ChunkSize * 25.0f; // 25cm voxel size
}

bool UVoxelWorldTemplate::HasChunkData(const FIntVector& ChunkPosition) const
{
    for (const FVoxelTemplateChunk& Chunk : ChunkData)
    {
        if (Chunk.ChunkPosition == ChunkPosition && Chunk.bHasData)
        {
            return true;
        }
    }
    return false;
}

TArray<FVoxelLandmark> UVoxelWorldTemplate::GetLandmarksInRadius(const FVector& WorldPosition, float Radius) const
{
    TArray<FVoxelLandmark> Result;
    float RadiusSq = Radius * Radius;
    
    for (const FVoxelLandmark& Landmark : Landmarks)
    {
        float DistSq = FVector::DistSquared(Landmark.WorldPosition, WorldPosition);
        if (DistSq <= RadiusSq)
        {
            Result.Add(Landmark);
        }
    }
    
    return Result;
}

bool UVoxelWorldTemplate::IsPositionProtected(const FVector& WorldPosition) const
{
    for (const FVoxelLandmark& Landmark : Landmarks)
    {
        float DistSq = FVector::DistSquared(Landmark.WorldPosition, WorldPosition);
        float ProtectionRadiusSq = Landmark.ProtectionRadius * Landmark.ProtectionRadius;
        if (DistSq <= ProtectionRadiusSq)
        {
            return true;
        }
    }
    return false;
}

// Serialization is handled automatically by the UPROPERTY system

#if WITH_EDITOR
void UVoxelWorldTemplate::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
    Super::PostEditChangeProperty(PropertyChangedEvent);
    
    // Update metadata when properties change
    if (PropertyChangedEvent.GetPropertyName() == GET_MEMBER_NAME_CHECKED(UVoxelWorldTemplate, TemplateName))
    {
        MarkPackageDirty();
    }
}
#endif

// Template Utility Implementation

bool UVoxelTemplateUtility::CompressVoxelData(const TArray<uint8>& UncompressedData, TArray<uint8>& OutCompressedData)
{
    // Use Oodle compression (built into UE5)
    // Get worst-case compressed size
    int32 CompressedSizeBound = FOodleDataCompression::CompressedBufferSizeNeeded(UncompressedData.Num());
    OutCompressedData.SetNum(CompressedSizeBound);
    
    int32 CompressedSize = FOodleDataCompression::Compress(
        OutCompressedData.GetData(),
        CompressedSizeBound,
        UncompressedData.GetData(),
        UncompressedData.Num(),
        FOodleDataCompression::ECompressor::Kraken,
        FOodleDataCompression::ECompressionLevel::SuperFast
    );
    
    if (CompressedSize > 0)
    {
        OutCompressedData.SetNum(CompressedSize);
        return true;
    }
    
    return false;
}

bool UVoxelTemplateUtility::DecompressVoxelData(const TArray<uint8>& CompressedData, TArray<uint8>& OutUncompressedData, int32 UncompressedSize)
{
    OutUncompressedData.SetNum(UncompressedSize);
    
    bool bSuccess = FOodleDataCompression::Decompress(
        OutUncompressedData.GetData(),
        UncompressedSize,
        CompressedData.GetData(),
        CompressedData.Num()
    );
    
    return bSuccess;
}

bool UVoxelTemplateUtility::SaveWorldAsTemplate(AVoxelWorld* World, UVoxelWorldTemplate* Template, const FString& TemplateName)
{
    if (!World || !Template)
    {
        UE_LOG(LogHearthshireVoxel, Error, TEXT("SaveWorldAsTemplate: Invalid World or Template"));
        return false;
    }
    
    // Update template metadata
    Template->TemplateName = TemplateName;
    Template->CreationDate = FDateTime::Now();
    Template->CreatorName = FPlatformProcess::UserName();
    
    // Clear existing data
    Template->ChunkData.Empty();
    
    // Get all active chunks
    TArray<AVoxelChunk*> ActiveChunks = World->GetAllActiveChunks();
    
    if (ActiveChunks.Num() == 0)
    {
        UE_LOG(LogHearthshireVoxel, Warning, TEXT("SaveWorldAsTemplate: No active chunks to save"));
        return false;
    }
    
    // Find world bounds
    FIntVector MinPos(INT32_MAX);
    FIntVector MaxPos(INT32_MIN);
    
    for (AVoxelChunk* Chunk : ActiveChunks)
    {
        if (!Chunk) continue;
        
        UVoxelChunkComponent* ChunkComp = Chunk->FindComponentByClass<UVoxelChunkComponent>();
        if (!ChunkComp) continue;
        
        const FIntVector& ChunkPos = ChunkComp->GetChunkPosition();
        MinPos.X = FMath::Min(MinPos.X, ChunkPos.X);
        MinPos.Y = FMath::Min(MinPos.Y, ChunkPos.Y);
        MinPos.Z = FMath::Min(MinPos.Z, ChunkPos.Z);
        MaxPos.X = FMath::Max(MaxPos.X, ChunkPos.X);
        MaxPos.Y = FMath::Max(MaxPos.Y, ChunkPos.Y);
        MaxPos.Z = FMath::Max(MaxPos.Z, ChunkPos.Z);
    }
    
    Template->MinChunkPosition = MinPos;
    Template->MaxChunkPosition = MaxPos;
    Template->ChunkSize = World->Config.ChunkSize;
    
    // Save each chunk
    int32 SavedChunks = 0;
    
    for (AVoxelChunk* Chunk : ActiveChunks)
    {
        if (!Chunk) continue;
        
        UVoxelChunkComponent* ChunkComp = Chunk->FindComponentByClass<UVoxelChunkComponent>();
        if (!ChunkComp) continue;
        
        const FVoxelChunkData& ChunkData = ChunkComp->GetChunkData();
        
        // Serialize voxel data
        TArray<uint8> UncompressedData;
        UncompressedData.SetNum(ChunkData.Voxels.Num());
        
        for (int32 i = 0; i < ChunkData.Voxels.Num(); i++)
        {
            UncompressedData[i] = (uint8)ChunkData.Voxels[i].Material;
        }
        
        // Compress the data
        FVoxelTemplateChunk TemplateChunk;
        TemplateChunk.ChunkPosition = ChunkComp->GetChunkPosition();
        TemplateChunk.UncompressedSize = UncompressedData.Num();
        
        if (CompressVoxelData(UncompressedData, TemplateChunk.CompressedVoxelData))
        {
            TemplateChunk.bHasData = true;
            Template->ChunkData.Add(TemplateChunk);
            SavedChunks++;
            
            UE_LOG(LogHearthshireVoxel, Log, TEXT("Saved chunk %s (compressed %d -> %d bytes)"),
                *TemplateChunk.ChunkPosition.ToString(),
                TemplateChunk.UncompressedSize,
                TemplateChunk.CompressedVoxelData.Num());
        }
    }
    
    UE_LOG(LogHearthshireVoxel, Log, TEXT("SaveWorldAsTemplate: Saved %d chunks to template '%s'"), SavedChunks, *TemplateName);
    
#if WITH_EDITOR
    Template->MarkPackageDirty();
#endif
    
    return SavedChunks > 0;
}

bool UVoxelTemplateUtility::LoadChunkFromTemplate(UVoxelWorldTemplate* Template, const FIntVector& ChunkPosition, FVoxelChunkData& OutChunkData)
{
    if (!Template)
    {
        return false;
    }
    
    // Find the chunk in template
    const FVoxelTemplateChunk* TemplateChunk = nullptr;
    for (const FVoxelTemplateChunk& Chunk : Template->ChunkData)
    {
        if (Chunk.ChunkPosition == ChunkPosition && Chunk.bHasData)
        {
            TemplateChunk = &Chunk;
            break;
        }
    }
    
    if (!TemplateChunk)
    {
        return false;
    }
    
    // Decompress the data
    TArray<uint8> UncompressedData;
    if (!DecompressVoxelData(TemplateChunk->CompressedVoxelData, UncompressedData, TemplateChunk->UncompressedSize))
    {
        UE_LOG(LogHearthshireVoxel, Error, TEXT("LoadChunkFromTemplate: Failed to decompress chunk data"));
        return false;
    }
    
    // Convert back to voxel data
    OutChunkData.ChunkPosition = ChunkPosition;
    OutChunkData.ChunkSize = FVoxelChunkSize(Template->ChunkSize);
    OutChunkData.Voxels.SetNum(UncompressedData.Num());
    
    for (int32 i = 0; i < UncompressedData.Num(); i++)
    {
        OutChunkData.Voxels[i] = FVoxel(static_cast<EVoxelMaterial>(UncompressedData[i]));
    }
    
    OutChunkData.bIsDirty = true;
    
    UE_LOG(LogHearthshireVoxel, Log, TEXT("LoadChunkFromTemplate: Loaded chunk %s"), *ChunkPosition.ToString());
    return true;
}

void UVoxelTemplateUtility::ApplySeedVariations(FVoxelChunkData& ChunkData, UVoxelWorldTemplate* Template, int32 Seed, const FIntVector& ChunkPosition)
{
    if (!Template || !Template->bAllowSeedVariations)
    {
        return;
    }
    
    FRandomStream Random(Seed ^ GetTypeHash(ChunkPosition));
    const FVoxelVariationParams& Params = Template->VariationParams;
    
    // Get landmarks in this chunk
    FVector ChunkWorldPos = FVector(ChunkPosition) * ChunkData.ChunkSize.X * 25.0f;
    float ChunkRadius = ChunkData.ChunkSize.X * 25.0f * 1.5f; // Check slightly beyond chunk bounds
    TArray<FVoxelLandmark> NearbyLandmarks = Template->GetLandmarksInRadius(ChunkWorldPos, ChunkRadius);
    
    // Apply variations in order
    ApplyTerrainNoise(ChunkData, Params, Random);
    ApplyGrassVariation(ChunkData, Params, Random);
    ApplyTreeVariation(ChunkData, Params, Random, NearbyLandmarks, ChunkPosition);
}

void UVoxelTemplateUtility::ApplyGrassVariation(FVoxelChunkData& ChunkData, const FVoxelVariationParams& Params, FRandomStream& Random)
{
    if (Params.GrassVariation <= 0.0f)
    {
        return;
    }
    
    const FVoxelChunkSize& Size = ChunkData.ChunkSize;
    
    // Add grass patches and flowers
    for (int32 Y = 0; Y < Size.Y; Y++)
    {
        for (int32 X = 0; X < Size.X; X++)
        {
            // Find top surface
            for (int32 Z = Size.Z - 1; Z >= 0; Z--)
            {
                FVoxel CurrentVoxel = ChunkData.GetVoxel(X, Y, Z);
                
                if (CurrentVoxel.Material == EVoxelMaterial::Grass)
                {
                    // Check if there's air above
                    if (Z < Size.Z - 1)
                    {
                        FVoxel AboveVoxel = ChunkData.GetVoxel(X, Y, Z + 1);
                        if (AboveVoxel.IsAir())
                        {
                            // Randomly add flowers or tall grass
                            if (Random.FRand() < Params.FlowerDensity)
                            {
                                // Add a flower (using Leaves material as placeholder)
                                ChunkData.SetVoxel(X, Y, Z + 1, FVoxel(EVoxelMaterial::Leaves));
                            }
                        }
                    }
                    break; // Move to next XY position
                }
            }
        }
    }
}

void UVoxelTemplateUtility::ApplyTreeVariation(FVoxelChunkData& ChunkData, const FVoxelVariationParams& Params, FRandomStream& Random, const TArray<FVoxelLandmark>& Landmarks, const FIntVector& ChunkWorldPosition)
{
    if (Params.TreeVariation <= 0.0f)
    {
        return;
    }
    
    const FVoxelChunkSize& Size = ChunkData.ChunkSize;
    const float VoxelSize = 25.0f;
    
    // Try to place a few trees in this chunk
    int32 TreeAttempts = FMath::RoundToInt(Params.TreeVariation * 5);
    
    for (int32 Attempt = 0; Attempt < TreeAttempts; Attempt++)
    {
        // Random position in chunk
        int32 X = Random.RandRange(3, Size.X - 4); // Leave margin for tree size
        int32 Y = Random.RandRange(3, Size.Y - 4);
        
        // Check if position is protected by landmark
        FVector WorldPos = FVector(ChunkWorldPosition) * Size.X * VoxelSize + FVector(X, Y, 0) * VoxelSize;
        bool bProtected = false;
        
        for (const FVoxelLandmark& Landmark : Landmarks)
        {
            if (FVector::Dist(WorldPos, Landmark.WorldPosition) < Landmark.ProtectionRadius)
            {
                bProtected = true;
                break;
            }
        }
        
        if (bProtected)
        {
            continue;
        }
        
        // Find ground level
        int32 GroundZ = -1;
        for (int32 Z = Size.Z - 1; Z >= 0; Z--)
        {
            FVoxel Voxel = ChunkData.GetVoxel(X, Y, Z);
            if (Voxel.Material == EVoxelMaterial::Grass || Voxel.Material == EVoxelMaterial::Dirt)
            {
                GroundZ = Z;
                break;
            }
        }
        
        if (GroundZ < 0 || GroundZ > Size.Z - 8) // Need space for tree
        {
            continue;
        }
        
        // Place a simple tree (trunk + leaves)
        int32 TrunkHeight = Random.RandRange(4, 6);
        
        // Trunk
        for (int32 Z = 1; Z <= TrunkHeight; Z++)
        {
            ChunkData.SetVoxel(X, Y, GroundZ + Z, FVoxel(EVoxelMaterial::Wood));
        }
        
        // Leaves (simple sphere)
        int32 LeafRadius = 2;
        for (int32 DX = -LeafRadius; DX <= LeafRadius; DX++)
        {
            for (int32 DY = -LeafRadius; DY <= LeafRadius; DY++)
            {
                for (int32 DZ = -LeafRadius; DZ <= LeafRadius; DZ++)
                {
                    int32 LX = X + DX;
                    int32 LY = Y + DY;
                    int32 LZ = GroundZ + TrunkHeight + DZ;
                    
                    if (LX >= 0 && LX < Size.X && LY >= 0 && LY < Size.Y && LZ >= 0 && LZ < Size.Z)
                    {
                        float Dist = FMath::Sqrt((float)(DX*DX + DY*DY + DZ*DZ));
                        if (Dist <= LeafRadius)
                        {
                            FVoxel CurrentVoxel = ChunkData.GetVoxel(LX, LY, LZ);
                            if (CurrentVoxel.IsAir())
                            {
                                ChunkData.SetVoxel(LX, LY, LZ, FVoxel(EVoxelMaterial::Leaves));
                            }
                        }
                    }
                }
            }
        }
    }
}

void UVoxelTemplateUtility::ApplyTerrainNoise(FVoxelChunkData& ChunkData, const FVoxelVariationParams& Params, FRandomStream& Random)
{
    if (Params.TerrainNoiseHeight <= 0.0f)
    {
        return;
    }
    
    // This is a placeholder - in a real implementation, you'd want to:
    // 1. Analyze the existing terrain height
    // 2. Apply small variations that don't break paths or structures
    // 3. Smooth the results to maintain terrain flow
    
    // For now, we'll skip this to avoid breaking hand-crafted terrain
    UE_LOG(LogHearthshireVoxel, Log, TEXT("Terrain noise variation not yet implemented"));
}