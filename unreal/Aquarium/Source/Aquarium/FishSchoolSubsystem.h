#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"

#include <vector>

#include "aquarium/Boids.h"

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

	// Test hook: forces the next Neighbors() call to rebuild.
	void InvalidateSnapshot() { bSnapshotValid = false; }

private:
	TArray<TWeakObjectPtr<AFishActor>> Fishes;
	std::vector<aquarium::BoidNeighbor> Snapshot;
	uint64 SnapshotFrame = 0;
	bool bSnapshotValid = false;
};
