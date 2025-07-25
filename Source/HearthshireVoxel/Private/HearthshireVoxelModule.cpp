// Copyright Epic Games, Inc. All Rights Reserved.

#include "HearthshireVoxelModule.h"
#include "Modules/ModuleManager.h"

DEFINE_LOG_CATEGORY(LogHearthshireVoxel);

#define LOCTEXT_NAMESPACE "FHearthshireVoxelModule"

void FHearthshireVoxelModule::StartupModule()
{
    UE_LOG(LogHearthshireVoxel, Log, TEXT("HearthshireVoxel module starting up"));
    
    // Initialize voxel system components here
}

void FHearthshireVoxelModule::ShutdownModule()
{
    UE_LOG(LogHearthshireVoxel, Log, TEXT("HearthshireVoxel module shutting down"));
    
    // Clean up voxel system components here
}

#undef LOCTEXT_NAMESPACE
    
IMPLEMENT_MODULE(FHearthshireVoxelModule, HearthshireVoxel)