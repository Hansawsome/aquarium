#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "aquarium/Nickname.h"
#include "aquarium/Steering.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAquariumRulesLinkTest, "Aquarium.Rules.LinksIntoModule",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FAquariumRulesLinkTest::RunTest(const FString& Parameters)
{
    const aquarium::NicknameResult r = aquarium::ValidateNickname("  nemo ");
    TestTrue(TEXT("nickname valid"), r.ok);
    TestEqual(TEXT("trimmed"), FString(r.value.c_str()), FString(TEXT("nemo")));
    const aquarium::Vec2 v = aquarium::SteeringVector({true, false, false, true});
    TestTrue(TEXT("diagonal normalized"), FMath::IsNearlyEqual(v.Length(), 1.f, 1e-4f));
    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
