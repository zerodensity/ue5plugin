// Copyright MediaZ AS. All Rights Reserved.

#include "MZLicenseManager.h"
#include "MZClient.h"
#include "MZSceneTreeManager.h"

FMZLicenseManager::FMZLicenseManager()
{
}

void FMZLicenseManager::StartupModule()
{
}

void FMZLicenseManager::ShutdownModule()
{
}

bool FMZLicenseManager::UpdateFeature(bool registerFeature, AActor* actor, USceneComponent* component, FProperty* property,
	FString featureName, uint32_t count, FString message, uint64_t buildTime)
{
	auto& MZClient = FModuleManager::LoadModuleChecked<FMZClient>("MZClient");
	auto& MZSceneTreeManager = FModuleManager::LoadModuleChecked<FMZSceneTreeManager>("MZSceneTreeManager");
	void* container = component ? (void*)component : (void*)actor;

	if (!container || !MZClient.AppServiceClient)
	{
		return false;
	}
	
	if (MZSceneTreeManager.MZPropertyManager.PropertiesByPropertyAndContainer.Contains({property, container}))
	{
		auto mzprop = MZSceneTreeManager.MZPropertyManager.PropertiesByPropertyAndContainer.FindRef({property, container});
		if(MZSceneTreeManager.MZPropertyManager.PropertyToPortalPin.Contains(mzprop->Id))
		{
			auto portalId = MZSceneTreeManager.MZPropertyManager.PropertyToPortalPin.FindRef(mzprop->Id);
			flatbuffers::FlatBufferBuilder mb;
			//// TODO: find a way to get a build time
			auto offset = mz::CreateAppEventOffset(mb, mz::app::CreateFeatureRegistrationUpdateDirect(mb, (mz::fb::UUID*)&portalId, TCHAR_TO_UTF8(*featureName), !registerFeature, count, TCHAR_TO_UTF8(*message), buildTime));
			mb.Finish(offset);
			auto buf = mb.Release();
			auto root = flatbuffers::GetRoot<mz::app::AppEvent>(buf.data());
			MZClient.AppServiceClient->Send(*root);
			return true;
		}
	}
	
	return false;
}
bool FMZLicenseManager::RegisterFeature(AActor* actor, USceneComponent* component, FProperty* property,
                                        FString featureName, uint32_t count, FString message, uint64_t buildTime)
{
	return UpdateFeature(true, actor, component, property, featureName, count, message, buildTime);
}

bool FMZLicenseManager::UnregisterFeature(AActor* actor, USceneComponent* component, FProperty* property, FString featureName)
{
	return UpdateFeature(false, actor, component, property, featureName);
}


IMPLEMENT_MODULE(FMZLicenseManager, MZLicenseManager)
