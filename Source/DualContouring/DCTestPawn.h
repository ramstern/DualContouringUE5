// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/DefaultPawn.h"
#include "DCTestPawn.generated.h"

/**
 * 
 */
class UInputMappingContext; class UInputAction;

UCLASS()
class DUALCONTOURING_API ADCTestPawn : public ADefaultPawn
{
	GENERATED_BODY()
	
public:
	virtual void BeginPlay() override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

	UPROPERTY(EditDefaultsOnly, Category = "Input") UInputMappingContext* IMC_fly = nullptr;
	UPROPERTY(EditDefaultsOnly, Category = "Input") UInputAction* IA_raycast = nullptr;
	UPROPERTY(EditAnywhere, Category = "Trace") float ray_length = 10000.f;
private:

	void RaycastOnce();
	bool DoViewRaycast(FHitResult& OutHit) const;
};
