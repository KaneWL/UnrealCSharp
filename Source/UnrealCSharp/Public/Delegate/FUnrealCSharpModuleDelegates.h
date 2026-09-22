#pragma once

class UNREALCSHARP_API FUnrealCSharpModuleDelegates
{
public:
	DECLARE_MULTICAST_DELEGATE(FOnUnrealCSharpModuleActive);

	DECLARE_MULTICAST_DELEGATE(FOnUnrealCSharpModuleInActive);

	DECLARE_MULTICAST_DELEGATE(FOnCSharpEnvironmentInitialize);

	DECLARE_MULTICAST_DELEGATE_OneParam(FOnCSharpClassBinding, UClass*);

	static FOnUnrealCSharpModuleActive OnUnrealCSharpModuleActive;

	static FOnUnrealCSharpModuleInActive OnUnrealCSharpModuleInActive;

	static FOnCSharpEnvironmentInitialize OnCSharpEnvironmentInitialize;

	static FOnCSharpClassBinding OnCSharpClassBinding;
};
