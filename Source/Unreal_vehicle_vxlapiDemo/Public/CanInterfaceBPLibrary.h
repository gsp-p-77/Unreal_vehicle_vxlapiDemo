// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CanInterfaceBPLibrary.generated.h"

/**
 * 
 */
UCLASS()
class UNREAL_VEHICLE_VXLAPIDEMO_API UCanInterfaceBPLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
	public:
		UFUNCTION(BlueprintCallable, Category="vxlapi")
		static bool vxlapiInit();

		UFUNCTION(BlueprintCallable, Category="vxlapi")
		static bool vxlapiDeInit();

		UFUNCTION(BlueprintCallable, Category="vxlapi")
		static bool vxlapiSendCanMessage(int canId, TArray<uint8> data, int len);

		UFUNCTION(BlueprintCallable, Category="vxlapi")
		static bool vxlapiPollCanRxMessageQueue(UPARAM(ref) int& out_canId, UPARAM(ref) TArray<uint8> & out_data, UPARAM(ref) int & out_len);		
};
