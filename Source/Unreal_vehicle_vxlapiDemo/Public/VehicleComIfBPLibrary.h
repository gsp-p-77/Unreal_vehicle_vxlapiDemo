// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "VehicleComIfBPLibrary.generated.h"

/**
 * 
 */
UCLASS()
class UNREAL_VEHICLE_VXLAPIDEMO_API UVehicleComIfBPLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
	public:

		UFUNCTION(BlueprintCallable, Category="Vehicle communication interface")
		static void InitTask();	

		UFUNCTION(BlueprintCallable, Category="Vehicle communication interface")
		static void DeInitTask();	
		
		UFUNCTION(BlueprintCallable, Category="Vehicle communication interface")
		static void SendVelocity(float in_Velocity);
};
