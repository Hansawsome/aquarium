#include "FishSchoolSubsystem.h"

#include "FishActor.h"

#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

void UFishSchoolSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
#if !UE_BUILD_SHIPPING
	// Per-item toggles, so the performance run can attribute cost to ONE behaviour at a time.
	// This is the only method that survived M4b: the design-stage prediction that the translucent
	// curtains would be the expensive item was refuted by exactly this kind of measurement.
	bSchoolingEnabled = !ParseDisableFlag(FCommandLine::Get(), TEXT("AquariumNoSchooling"));
	bPropAvoidanceEnabled = !ParseDisableFlag(FCommandLine::Get(), TEXT("AquariumNoPropAvoid"));
#endif
}

bool UFishSchoolSubsystem::ParseDisableFlag(const TCHAR* CmdLine, const TCHAR* Flag)
{
	if (CmdLine == nullptr || Flag == nullptr)
	{
		return false;
	}
	return FParse::Param(CmdLine, Flag);
}

void UFishSchoolSubsystem::Register(AFishActor* Fish)
{
	if (Fish == nullptr)
	{
		return;
	}
	Fishes.AddUnique(Fish);
	bSnapshotValid = false;
}

void UFishSchoolSubsystem::Unregister(AFishActor* Fish)
{
	Fishes.RemoveAll([Fish](const TWeakObjectPtr<AFishActor>& P) { return !P.IsValid() || P.Get() == Fish; });
	bSnapshotValid = false;
}

const std::vector<aquarium::BoidNeighbor>& UFishSchoolSubsystem::Neighbors()
{
	// Rebuilt lazily by whichever fish asks first in a frame. A UTickableWorldSubsystem has no
	// ordering guarantee against actor ticks, so half the school would read a one-frame-stale
	// snapshot; this way every fish in a frame sees the same current data. The order is
	// registration order, which keeps AFishActor's determinism test meaningful.
	const uint64 Frame = GFrameCounter;
	if (bSnapshotValid && SnapshotFrame == Frame)
	{
		return Snapshot;
	}
	Fishes.RemoveAll([](const TWeakObjectPtr<AFishActor>& P) { return !P.IsValid(); });
	Snapshot.clear();
	Snapshot.reserve(static_cast<size_t>(Fishes.Num()));
	for (const TWeakObjectPtr<AFishActor>& P : Fishes)
	{
		Snapshot.push_back(P->AsNeighbor());
	}
	SnapshotFrame = Frame;
	bSnapshotValid = true;
	return Snapshot;
}
