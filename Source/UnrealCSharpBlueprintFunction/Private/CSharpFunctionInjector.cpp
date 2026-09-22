#include "CSharpFunctionInjector.h"

#include "Bridge/FTypeBridge.h"
#include "Bridge/FTypeBridge.inl"
#include "Dynamic/FDynamicGenerator.h"
#include "Dynamic/FDynamicGeneratorCore.h"
#include "Log/UnrealCSharpLog.h"
#include "Reflection/FClassReflection.h"
#include "Reflection/FMethodReflection.h"
#include "Reflection/FParamReflection.h"
#include "Reflection/FReflectionRegistry.h"
#include "UObject/UObjectArray.h"

#if WITH_EDITOR
#include "BlueprintActionDatabase.h"
#include "Editor.h"
#include "Engine/Blueprint.h"

TSet<FName> FCSharpFunctionInjector::InjectableClassNames;
#endif

bool FCSharpFunctionInjector::IsInjectableMethod(const FMethodReflection* InMethod)
{
	return InMethod != nullptr && InMethod->IsUFunction() && !InMethod->IsStatic() &&
		(InMethod->HasAttribute(FReflectionRegistry::Get().GetBlueprintCallableAttributeClass()) ||
			InMethod->HasAttribute(FReflectionRegistry::Get().GetBlueprintPureAttributeClass()));
}

bool FCSharpFunctionInjector::InjectClass(UClass* InClass)
{
	if (InClass == nullptr)
	{
		return false;
	}

	const auto ClassReflection = GetOverrideClassReflection(InClass);

	if (ClassReflection == nullptr)
	{
		return false;
	}

	return InjectFunctions(ClassReflection, InClass);
}

#if WITH_EDITOR
bool FCSharpFunctionInjector::InjectBlueprint(UBlueprint* InBlueprint)
{
	if (InBlueprint == nullptr)
	{
		return false;
	}

	const auto GeneratedClass = Cast<UClass>(InBlueprint->GeneratedClass);

	if (GeneratedClass == nullptr)
	{
		return false;
	}

	const auto ClassReflection = GetOverrideClassReflection(GeneratedClass);

	if (ClassReflection == nullptr)
	{
		UE_LOG(LogUnrealCSharp, Verbose, TEXT("InjectBlueprint: %s has no override reflection, skipped"),
		       *GeneratedClass->GetName());

		return false;
	}

	auto bInjected = InjectFunctions(ClassReflection, GeneratedClass);

	const auto SkeletonClass = Cast<UClass>(InBlueprint->SkeletonGeneratedClass);

	if (SkeletonClass != nullptr && SkeletonClass != GeneratedClass)
	{
		bInjected |= InjectFunctions(ClassReflection, SkeletonClass);
	}

	UE_LOG(LogUnrealCSharp, Log, TEXT("InjectBlueprint: %s (Skeleton: %s) bInjected=%d"),
	       *GeneratedClass->GetName(),
	       SkeletonClass != nullptr ? *SkeletonClass->GetName() : TEXT("null"),
	       bInjected);

	if (bInjected && GEditor != nullptr && !IsRunningCommandlet())
	{
		auto& ActionDatabase = FBlueprintActionDatabase::Get();

		ActionDatabase.RefreshClassActions(GeneratedClass);

		if (SkeletonClass != nullptr && SkeletonClass != GeneratedClass)
		{
			ActionDatabase.RefreshClassActions(SkeletonClass);
		}
	}

	return bInjected;
}

void FCSharpFunctionInjector::RebuildInjectableClassCache()
{
	InjectableClassNames.Reset();

	FDynamicGeneratorCore::Generator(FReflectionRegistry::Get().GetOverrideAttributeClass(),
	                                 [](FClassReflection* InClassReflection)
	                                 {
		                                 for (const auto& [Pair, Method] : InClassReflection->GetMethods())
		                                 {
			                                 if (IsInjectableMethod(Method))
			                                 {
				                                 InjectableClassNames.Add(FName(InClassReflection->GetName()));

				                                 break;
			                                 }
		                                 }
	                                 });

	UE_LOG(LogUnrealCSharp, Log, TEXT("RebuildInjectableClassCache: %d class(es): %s"),
	       InjectableClassNames.Num(),
	       *FString::JoinBy(InjectableClassNames, TEXT(", "), [](const FName& InName) { return InName.ToString(); }));
}

bool FCSharpFunctionInjector::MayHaveInjectableFunctions(const UClass* InClass)
{
	return InClass != nullptr && InjectableClassNames.Contains(InClass->GetFName());
}
#endif

FClassReflection* FCSharpFunctionInjector::GetOverrideClassReflection(UClass* InClass)
{
	if (InClass->IsChildOf<UInterface>())
	{
		return nullptr;
	}

	if (FDynamicGenerator::IsDynamicClass(InClass))
	{
		return nullptr;
	}

	const auto ClassReflection = FReflectionRegistry::Get().GetClass(TWeakObjectPtr<UField>(InClass));

	return ClassReflection != nullptr && ClassReflection->IsOverride() ? ClassReflection : nullptr;
}

bool FCSharpFunctionInjector::InjectFunctions(FClassReflection* InClassReflection, UClass* InClass)
{
	auto bInjected = false;

	for (const auto& [Pair, Method] : InClassReflection->GetMethods())
	{
		if (!IsInjectableMethod(Method))
		{
			continue;
		}

		const FName FunctionName(Pair.Get<0>());

		if (InClass->FindFunctionByName(FunctionName) != nullptr)
		{
			continue;
		}

		const auto Function = NewObject<UFunction>(InClass, FunctionName, RF_Public | RF_Transient);

		Function->MinAlignment = 1;

		auto bHasOutParms = false;

		if (const auto Return = Method->GetReturn())
		{
			if (const auto Property = FTypeBridge::Factory<true>(Return, Function, "", RF_Public | RF_Transient))
			{
				Property->SetPropertyFlags(CPF_Parm | CPF_OutParm | CPF_ReturnParm);

				Function->AddCppProperty(Property);

				bHasOutParms = true;
			}
		}

		const auto& Params = Method->GetParams();

		for (auto Index = Method->GetParamCount() - 1; Index >= 0; --Index)
		{
			const auto Property = FTypeBridge::Factory<true>(
				Params[Index]->GetReflectionType(),
				Function,
				FName(Params[Index]->GetName()),
				RF_Public | RF_Transient);

			Property->SetPropertyFlags(CPF_Parm);

			if (Params[Index]->IsRef())
			{
				Property->SetPropertyFlags(CPF_OutParm | CPF_ReferenceParm);

				bHasOutParms = true;
			}

			Function->AddCppProperty(Property);
		}

		FDynamicGeneratorCore::SetFlags(Function, Method);

		Function->FunctionFlags |= FUNC_Public | FUNC_Event | FUNC_BlueprintEvent;

		Function->FunctionFlags &= ~FUNC_Native;

		if (bHasOutParms)
		{
			Function->FunctionFlags |= FUNC_HasOutParms;
		}

		Function->Bind();

		Function->StaticLink(true);

#if WITH_EDITOR
		FDynamicGeneratorCore::SetMetaData(Function, Method);

		Function->SetMetaData(TEXT("CSharpFunctionInjected"), TEXT("true"));
#endif

		if (InClass->Children == nullptr)
		{
			InClass->Children = Function;
		}
		else
		{
			auto Tail = InClass->Children;

			while (Tail->Next != nullptr)
			{
				Tail = Tail->Next;
			}

			Tail->Next = Function;
		}

		InClass->AddFunctionToFunctionMap(Function, FName(Method->GetName()));

		if (InClass->HasAnyInternalFlags(EInternalObjectFlags::RootSet) || GUObjectArray.IsDisregardForGC(InClass))
		{
			Function->AddToRoot();
		}

		UE_LOG(LogUnrealCSharp, Log, TEXT("InjectFunctions: %s.%s (Flags=0x%08x)"),
		       *InClass->GetName(), *Function->GetName(), static_cast<uint32>(Function->FunctionFlags));

		bInjected = true;
	}

	return bInjected;
}
