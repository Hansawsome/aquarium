#include "FishSchoolSubsystem.h"

#include "FishActor.h"

#include "Engine/StaticMeshActor.h"
#include "EngineUtils.h"
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

const FName UFishSchoolSubsystem::PropTag(TEXT("AquariumProp"));

void UFishSchoolSubsystem::BuildObstaclesForPlane(const FVector& PlaneOrigin, float PlaneHalfDepth,
                                                  std::vector<aquarium::Obstacle>& Out)
{
	Out.clear();
	UWorld* W = GetWorld();
	if (W == nullptr)
	{
		return;
	}
	for (TActorIterator<AStaticMeshActor> It(W); It; ++It)
	{
		AStaticMeshActor* Prop = *It;
		if (Prop == nullptr || !Prop->ActorHasTag(PropTag))
		{
			continue;
		}
		// The actor's own bounds already include its mesh, its scale and its tilt. That is the
		// point: the radius is DERIVED here, so it cannot drift away from what is on screen the
		// way a copied table would.
		const FBox Box = Prop->GetComponentsBoundingBox(/*bNonColliding*/ true);
		if (!Box.IsValid)
		{
			continue;
		}
		const FVector Centre = Box.GetCenter();
		const FVector Extent = Box.GetExtent();
		// A degenerate bound is not a small obstacle, it is no obstacle. Without this the margin
		// alone would turn a mesh-less or zero-scaled actor into an invisible 20 cm wall.
		if (Extent.Y < MinPropExtentCm || Extent.Z < MinPropExtentCm)
		{
			continue;
		}
		// Depth gate: a prop only matters to this plane if it actually reaches it along world X.
		if (FMath::Abs(Centre.X - PlaneOrigin.X) > Extent.X + PlaneHalfDepth)
		{
			continue;
		}
		// A prop is approximated by a STACK of discs rather than one disc. One disc forces a bad
		// choice: a radius equal to the half width leaves a tall coral's top and bottom open,
		// while a radius equal to the half height turns a 60 cm coral into a 150 cm wall that
		// fish swerve around from far away. A stack matches the silhouette and costs a few more
		// cheap circle tests.
		const float RadiusCm = static_cast<float>(Extent.Y);
		const int32 Discs = FMath::Clamp(FMath::CeilToInt(static_cast<float>(Extent.Z) / RadiusCm), 1, MaxDiscsPerProp);
		const float Span = 2.f * static_cast<float>(Extent.Z);
		for (int32 i = 0; i < Discs; ++i)
		{
			const float T = (Discs == 1) ? 0.5f : (static_cast<float>(i) + 0.5f) / static_cast<float>(Discs);
			const float WorldZ = static_cast<float>(Centre.Z - Extent.Z) + Span * T;
			aquarium::Obstacle O;
			// Plane-local: the fish's own 2D position is relative to its plane origin.
			O.center = {static_cast<float>(Centre.Y) - static_cast<float>(PlaneOrigin.Y),
			            WorldZ - static_cast<float>(PlaneOrigin.Z)};
			O.radius = RadiusCm;
			Out.push_back(O);
		}
	}
}
