// Fill out your copyright notice in the Description page of Project Settings.


#include "DCTestPawn.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"

void ADCTestPawn::BeginPlay()
{
	Super::BeginPlay();

	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		if (ULocalPlayer* LP = PC->GetLocalPlayer())
			if (auto* Subsys = LP->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
				if (IMC_fly) Subsys->AddMappingContext(IMC_fly, 0);
	}
}

void ADCTestPawn::SetupPlayerInputComponent(UInputComponent* player_input_component)
{
	Super::SetupPlayerInputComponent(player_input_component);

	if (auto* EIC = Cast<UEnhancedInputComponent>(player_input_component))
	{
		EIC->BindAction(IA_raycast, ETriggerEvent::Started, this, &ADCTestPawn::RaycastOnce);
	}
}

bool ADCTestPawn::DoViewRaycast(FHitResult& OutHit) const
{
	const APlayerController* PC = Cast<APlayerController>(GetController());
	if (!PC) return false;

	FVector CamLoc; FRotator CamRot;
	PC->GetPlayerViewPoint(CamLoc, CamRot);

	const FVector Start = CamLoc;
	const FVector End = Start + CamRot.Vector() * ray_length;

	FCollisionQueryParams Params(SCENE_QUERY_STAT(ViewRay), false, this);
	return GetWorld()->LineTraceSingleByChannel(OutHit, Start, End, ECC_Visibility, Params);
}

void ADCTestPawn::RaycastOnce()
{
	FHitResult hit_result;
	const bool hit = DoViewRaycast(hit_result);

	UE_LOG(LogTemp, Display, TEXT("%i"), hit);
}