// Guards the failure mode that cost M4a two rounds of work: a material that fails to compile is
// silently swapped for the grey Default Material at runtime while the editor viewport still
// looks fine. Every material this project generates from Python lives under /Game/Props or
// /Game/Env, so walking those two folders covers all of them.
//
// The test does TWO things, on purpose.
//
// 1. Sampler type vs texture compression settings, checked on the material GRAPH. This is the
//    exact mistake that hit M4a twice ("Sampler type is Linear Grayscale, should be Linear
//    Color" from a roughness map imported as TC_Default instead of TC_MASKS). It needs no
//    shader compiler and no render hardware, so it is the part of this test that actually runs
//    -- and actually fails -- under the project's standard -nullrhi automation invocation.
//
// 2. Shader compile errors on the FMaterialResource, when there is one. Under -nullrhi there
//    usually is not, so this half self-reports as skipped ("checked 0 of N materials") instead
//    of passing quietly. The real gate for shader compile failures remains the log grep over a
//    non-nullrhi game run, exactly as the M4b design doc says.
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "AssetRegistry/ARFilter.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Engine/Texture.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionTextureBase.h"
#include "Materials/MaterialExpressionUtils.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAquariumPropMaterialsCompileTest,
    "Aquarium.Content.PropMaterialsCompile",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAquariumPropMaterialsCompileTest::RunTest(const FString& Parameters)
{
    FAssetRegistryModule& Registry =
        FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
    Registry.Get().SearchAllAssets(/*bSynchronousSearch=*/true);

    FARFilter Filter;
    Filter.ClassPaths.Add(UMaterial::StaticClass()->GetClassPathName());
    Filter.PackagePaths.Add(TEXT("/Game/Props"));
    Filter.PackagePaths.Add(TEXT("/Game/Env"));
    Filter.bRecursivePaths = true;

    TArray<FAssetData> Assets;
    Registry.Get().GetAssets(Filter, Assets);
    TestTrue(TEXT("at least one generated material was found"), Assets.Num() > 0);

    int32 ResourcesChecked = 0;
    int32 SamplersChecked = 0;
    for (const FAssetData& Asset : Assets)
    {
        UMaterial* Material = Cast<UMaterial>(Asset.GetAsset());
        if (Material == nullptr)
        {
            continue;
        }
        const FString Name = Asset.AssetName.ToString();
#if WITH_EDITOR
        // (1) Graph-level sampler type check. No RHI needed.
        for (const TObjectPtr<UMaterialExpression>& Expression : Material->GetExpressions())
        {
            const UMaterialExpressionTextureBase* TextureExpression =
                Cast<UMaterialExpressionTextureBase>(Expression.Get());
            if (TextureExpression == nullptr || TextureExpression->Texture == nullptr)
            {
                continue;
            }
            const UTexture* Texture = TextureExpression->Texture;
            FString ErrorMessage;
            const bool bOk = MaterialExpressionUtils::VerifySamplerType(
                Texture->GetPathName(),
                MaterialExpressionUtils::GetSamplerTypeForTexture(Texture),
                Texture->SRGB,
                TextureExpression->SamplerType,
                ErrorMessage);
            if (!bOk)
            {
                AddError(FString::Printf(TEXT("%s samples %s with the wrong sampler type: %s"),
                                         *Name, *Texture->GetName(), *ErrorMessage));
            }
            ++SamplersChecked;
        }

        // (2) Shader compile errors, when a material resource exists at all.
        // UE 5.8 takes an EShaderPlatform here, not an ERHIFeatureLevel (the plan snippet was
        // written against the older signature).
        const FMaterialResource* Resource = Material->GetMaterialResource(GMaxRHIShaderPlatform);
        if (Resource == nullptr)
        {
            AddInfo(FString::Printf(TEXT("%s: no material resource (nullrhi?), skipped"), *Name));
            continue;
        }
        const TArray<FString>& Errors = Resource->GetCompileErrors();
        TestEqual(FString::Printf(TEXT("%s compile errors"), *Name), Errors.Num(), 0);
        for (const FString& Error : Errors)
        {
            AddError(FString::Printf(TEXT("%s: %s"), *Name, *Error));
        }
        ++ResourcesChecked;
#endif
    }
    // The sampler half must never be vacuous: this project's materials all sample textures, so
    // zero texture samplers examined means the walk itself broke, not that everything is clean.
    TestTrue(TEXT("texture samplers were actually examined"), SamplersChecked > 0);
    AddInfo(FString::Printf(TEXT("checked %d texture samplers and %d of %d material resources"),
                            SamplersChecked, ResourcesChecked, Assets.Num()));
    return true;
}

#endif  // WITH_DEV_AUTOMATION_TESTS
