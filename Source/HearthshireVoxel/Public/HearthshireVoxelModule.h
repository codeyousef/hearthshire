// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

DECLARE_LOG_CATEGORY_EXTERN(LogHearthshireVoxel, Log, All);

class FHearthshireVoxelModule : public IModuleInterface
{
public:
    /** IModuleInterface implementation */
    virtual void StartupModule() override;
    virtual void ShutdownModule() override;
    
    /** Gets the singleton instance of this module */
    static inline FHearthshireVoxelModule& Get()
    {
        return FModuleManager::LoadModuleChecked<FHearthshireVoxelModule>("HearthshireVoxel");
    }
    
    /** Checks if this module is loaded and ready */
    static inline bool IsAvailable()
    {
        return FModuleManager::Get().IsModuleLoaded("HearthshireVoxel");
    }
};