#pragma once

#include "CoreMinimal.h"

class FClassReflection;
class FMethodReflection;
class UBlueprint;

class UNREALCSHARPBLUEPRINTFUNCTION_API FCSharpFunctionInjector
{
public:
	static bool InjectClass(UClass* InClass);

#if WITH_EDITOR
	static bool InjectBlueprint(UBlueprint* InBlueprint);

	static void RebuildInjectableClassCache();

	static bool MayHaveInjectableFunctions(const UClass* InClass);
#endif

private:
	static bool IsInjectableMethod(const FMethodReflection* InMethod);

	static FClassReflection* GetOverrideClassReflection(UClass* InClass);

	static bool InjectFunctions(FClassReflection* InClassReflection, UClass* InClass);

#if WITH_EDITOR
private:
	static TSet<FName> InjectableClassNames;
#endif
};
