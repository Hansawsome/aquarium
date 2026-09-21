#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"

#include <vector>

#include "aquarium/Boids.h"
#include "aquarium/Obstacles.h"

#include "FishSchoolSubsystem.generated.h"

class AFishActor;

// Supplies every fish with the two things the rules layer cannot produce on its own: the list of
// other fish, and (from Task 8) the props that intersect its swim plane. It owns no behaviour --
// separation, alignment, cohesion and obstacle steering all live in aquarium::Boids /
// aquarium::Obstacles, where Catch2 can reach them.
UCLASS()
class AQUARIUM_API UFishSchoolSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	void Register(AFishActor* Fish);
	void Unregister(AFishActor* Fish);
	int32 RegisteredCount() const { return Fishes.Num(); }

	// Every registered fish in the shared swim frame. Rebuilt at most once per frame.
	const std::vector<aquarium::BoidNeighbor>& Neighbors();

	// Dev-only per-item toggles for the performance attribution run. Public and plain bools so a
	// test can set them directly without going through the command line.
	bool bSchoolingEnabled = true;
	bool bPropAvoidanceEnabled = true;
	// Pure, so it is testable without a command line. Returns true when the flag is present.
	static bool ParseDisableFlag(const TCHAR* CmdLine, const TCHAR* Flag);

	// Obstacle discs for one swim plane, in that plane's LOCAL 2D coordinates. Derived from the
	// bounds of the actors tagged AquariumProp -- never from a radius table copied into C++.
	// PlaneHalfDepth is how far along world X a prop may be and still matter to this plane.
	void BuildObstaclesForPlane(const FVector& PlaneOrigin, float PlaneHalfDepth,
	                            std::vector<aquarium::Obstacle>& Out);
	// Name of the actor tag build_reef_m1.py puts on every prop.
	static const FName PropTag;
	// Cap on how many discs one prop is approximated by (see the .cpp for why a prop is a stack).
	static constexpr int32 MaxDiscsPerProp = 4;
	// Bounds thinner than this in either in-plane axis are treated as degenerate and skipped.
	// aquarium::SteerAroundObstacles adds a 20 cm margin to EVERY radius, so a prop whose bounds
	// collapsed to nothing would still become a 20 cm ghost obstacle that fish swerve around for
	// no visible reason. Filtering belongs here: the rules layer is handed only real discs.
	static constexpr float MinPropExtentCm = 2.f;

	// Test hook: forces the next Neighbors() call to rebuild.
	void InvalidateSnapshot() { bSnapshotValid = false; }

private:
	TArray<TWeakObjectPtr<AFishActor>> Fishes;
	std::vector<aquarium::BoidNeighbor> Snapshot;
	uint64 SnapshotFrame = 0;
	bool bSnapshotValid = false;
};
