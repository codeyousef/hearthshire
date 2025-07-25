// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "VoxelTypes.h"
#include "VoxelWorldTemplate.generated.h"

/**
 * Represents a landmark in the world that should be preserved
 */
USTRUCT(BlueprintType)
struct HEARTHSHIREVOXEL_API FVoxelLandmark
{
    GENERATED_BODY()
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Landmark")
    FString Name;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Landmark")
    FVector WorldPosition;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Landmark", meta = (ClampMin = "100", ClampMax = "5000"))
    float ProtectionRadius = 1000.0f; // 10 meters default
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Landmark")
    FString Description;
    
    // Optional spawn point for NPCs or objects
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Landmark")
    TSubclassOf<AActor> ActorToSpawn;
    
    FVoxelLandmark()
    {
        Name = "Unnamed Landmark";
        WorldPosition = FVector::ZeroVector;
        ProtectionRadius = 1000.0f;
    }
};

/**
 * Compressed chunk data for storage
 */
USTRUCT()
struct HEARTHSHIREVOXEL_API FVoxelTemplateChunk
{
    GENERATED_BODY()
    
    UPROPERTY()
    FIntVector ChunkPosition;
    
    UPROPERTY()
    TArray<uint8> CompressedVoxelData;
    
    UPROPERTY()
    int32 UncompressedSize;
    
    UPROPERTY()
    bool bHasData;
    
    FVoxelTemplateChunk()
    {
        ChunkPosition = FIntVector::ZeroValue;
        UncompressedSize = 0;
        bHasData = false;
    }
};

/**
 * Seed variation parameters
 */
USTRUCT(BlueprintType)
struct HEARTHSHIREVOXEL_API FVoxelVariationParams
{
    GENERATED_BODY()
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Variation", meta = (ClampMin = "0", ClampMax = "1"))
    float GrassVariation = 0.3f;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Variation", meta = (ClampMin = "0", ClampMax = "1"))
    float FlowerDensity = 0.2f;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Variation", meta = (ClampMin = "0", ClampMax = "1"))
    float TreeVariation = 0.4f;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Variation", meta = (ClampMin = "0", ClampMax = "50"))
    float TerrainNoiseScale = 10.0f;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Variation", meta = (ClampMin = "0", ClampMax = "100"))
    float TerrainNoiseHeight = 25.0f; // 25cm = 1 voxel
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Variation")
    bool bAllowPathVariation = false;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Variation")
    bool bAllowWaterVariation = false;
};

/**
 * World template data asset
 */
UCLASS(BlueprintType)
class HEARTHSHIREVOXEL_API UVoxelWorldTemplate : public UDataAsset
{
    GENERATED_BODY()
    
public:
    UVoxelWorldTemplate();
    
    // Template metadata
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Template")
    FString TemplateName;
    
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Template")
    FString Description;
    
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Template")
    FDateTime CreationDate;
    
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Template")
    FString CreatorName;
    
    // World bounds
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Template")
    FIntVector MinChunkPosition;
    
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Template")
    FIntVector MaxChunkPosition;
    
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Template")
    int32 ChunkSize = 32;
    
    // Compressed chunk data
    UPROPERTY()
    TArray<FVoxelTemplateChunk> ChunkData;
    
    // Landmarks
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Landmarks")
    TArray<FVoxelLandmark> Landmarks;
    
    // Variation parameters
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Variation")
    FVoxelVariationParams VariationParams;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Variation")
    bool bAllowSeedVariations = true;
    
    // Functions
    UFUNCTION(BlueprintCallable, Category = "Voxel Template")
    int32 GetTotalChunkCount() const { return ChunkData.Num(); }
    
    UFUNCTION(BlueprintCallable, Category = "Voxel Template")
    FVector GetWorldSize() const;
    
    UFUNCTION(BlueprintCallable, Category = "Voxel Template")
    bool HasChunkData(const FIntVector& ChunkPosition) const;
    
    UFUNCTION(BlueprintCallable, Category = "Voxel Template")
    TArray<FVoxelLandmark> GetLandmarksInRadius(const FVector& WorldPosition, float Radius) const;
    
    UFUNCTION(BlueprintCallable, Category = "Voxel Template")
    bool IsPositionProtected(const FVector& WorldPosition) const;
    
    // Serialization handled automatically by UPROPERTY system
    
#if WITH_EDITOR
    virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
};

/**
 * Template loader/saver utility
 */
UCLASS()
class HEARTHSHIREVOXEL_API UVoxelTemplateUtility : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
    
public:
    // Compression helpers
    static bool CompressVoxelData(const TArray<uint8>& UncompressedData, TArray<uint8>& OutCompressedData);
    static bool DecompressVoxelData(const TArray<uint8>& CompressedData, TArray<uint8>& OutUncompressedData, int32 UncompressedSize);
    
    // Save/Load helpers
    UFUNCTION(BlueprintCallable, Category = "Voxel Template", meta = (CallInEditor = "true"))
    static bool SaveWorldAsTemplate(AVoxelWorld* World, UVoxelWorldTemplate* Template, const FString& TemplateName);
    
    // Non-Blueprint functions
    static bool LoadChunkFromTemplate(UVoxelWorldTemplate* Template, const FIntVector& ChunkPosition, FVoxelChunkData& OutChunkData);
    static void ApplySeedVariations(FVoxelChunkData& ChunkData, UVoxelWorldTemplate* Template, int32 Seed, const FIntVector& ChunkPosition);
    
private:
    static void ApplyGrassVariation(FVoxelChunkData& ChunkData, const FVoxelVariationParams& Params, FRandomStream& Random);
    static void ApplyTreeVariation(FVoxelChunkData& ChunkData, const FVoxelVariationParams& Params, FRandomStream& Random, const TArray<FVoxelLandmark>& Landmarks, const FIntVector& ChunkWorldPosition);
    static void ApplyTerrainNoise(FVoxelChunkData& ChunkData, const FVoxelVariationParams& Params, FRandomStream& Random);
};