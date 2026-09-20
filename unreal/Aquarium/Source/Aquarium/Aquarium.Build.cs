using System.IO;
using EpicGames.Core;
using UnrealBuildTool;

public class Aquarium : ModuleRules
{
	public Aquarium(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "UMG", "Slate", "SlateCore" });

		// Engine-independent rules layer lives at the repo root; compile the same sources here (no copies).
		string RepoRoot = Path.GetFullPath(Path.Combine(ModuleDirectory, "..", "..", "..", ".."));
		string RulesDir = Path.Combine(RepoRoot, "rules");
		PublicIncludePaths.Add(Path.Combine(RulesDir, "include"));
		ConditionalAddModuleDirectory(new DirectoryReference(Path.Combine(RulesDir, "src")));
		bEnableExceptions = false;

		// The module uses a flat layout (no Public/Private); let Tests/ include module headers by bare name.
		PrivateIncludePaths.Add(ModuleDirectory);

		// FishActor automation tests (Tests/FishActorTests.cpp) spawn actors into a test map via
		// FAutomationEditorCommonUtils::CreateNewMap, which lives in UnrealEd (editor builds only).
		if (Target.bBuildEditor)
		{
			PrivateDependencyModuleNames.Add("UnrealEd");
		}
	}
}
