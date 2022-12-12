#if WITH_EDITOR
#include "MZActorProperties.h"
#include "MZTextureShareManager.h"
#include "EditorCategoryUtils.h"
#include "ObjectEditorUtils.h"
#include "Reality/Public/RealityTrack.h"


#define CHECK_PROP_SIZE() {if (size != Property->ElementSize){UE_LOG(LogMediaZ, Error, TEXT("Property size mismatch with mediaZ (uint64)"));return;}}

bool PropertyVisibleExp(FProperty* ueproperty)
{
	return !ueproperty->HasAllPropertyFlags(CPF_DisableEditOnInstance) &&
		!ueproperty->HasAllPropertyFlags(CPF_Deprecated) &&
		//!ueproperty->HasAllPropertyFlags(CPF_EditorOnly) && //? dont know what this flag does but it hides more than necessary
		ueproperty->HasAllPropertyFlags(CPF_Edit) &&
		//ueproperty->HasAllPropertyFlags(CPF_BlueprintVisible) && //? dont know what this flag does but it hides more than necessary
		ueproperty->HasAllFlags(RF_Public);
}

MZProperty::MZProperty(UObject* container, FProperty* uproperty, FString parentCategory, uint8* structPtr, MZStructProperty* parentProperty)
{
	Property = uproperty;
	
	if (Property->HasAnyPropertyFlags(CPF_OutParm))
	{
		ReadOnly = true;
	}

	Container = container;
	StructPtr = structPtr;
	Id = FGuid::NewGuid();
	PropertyName = uproperty->GetFName().ToString();
	if (Container && Container->IsA<UActorComponent>())
	{
		PropertyName = *FString(Container->GetFName().ToString() + "" + PropertyName);
	}

	auto metaDataMap = uproperty->GetMetaDataMap();
	if (metaDataMap)
	{
		static const FName NAME_DisplayName(TEXT("DisplayName"));
		static const FName NAME_Category(TEXT("Category"));
		static const FName NAME_UIMin(TEXT("UIMin"));
		static const FName NAME_UIMax(TEXT("UIMax"));

		const auto& metaData = *metaDataMap;
		DisplayName = metaData.Contains(NAME_DisplayName) ? metaData[NAME_DisplayName] : uproperty->GetFName().ToString();
		CategoryName = (metaData.Contains(NAME_Category) ? metaData[NAME_Category] : "Default");
		UIMinString = metaData.Contains(NAME_UIMin) ? metaData[NAME_UIMin] : "";
		UIMaxString = metaData.Contains(NAME_UIMax) ? metaData[NAME_UIMax] : "";

		if (metaData.Contains(NAME_UIMin))
		{
			UE_LOG(LogTemp, Warning, TEXT("metadata is found for uimin"));
		}
		if (metaData.Contains(NAME_UIMax))
		{
			UE_LOG(LogTemp, Warning, TEXT("metadata is found for uimax"));
		}
		if (metaData.Contains("ClampMin"))
		{
			UE_LOG(LogTemp, Warning, TEXT("metadata is found for cmin"));
		}
		if (metaData.Contains("ClampMax"))
		{
			UE_LOG(LogTemp, Warning, TEXT("metadata is found for cmax"));
		}

	}
	else
	{
		//UE_LOG(LogTemp, Warning, TEXT("no_metadata_is_found " + TEXT(*uproperty->GetFName().ToString())));
		DisplayName = uproperty->GetFName().ToString();
		CategoryName = "Default";
		UIMinString = "";
		UIMaxString = "";
	}
	IsAdvanced = uproperty->HasAllPropertyFlags(CPF_AdvancedDisplay);
	
	// For properties inside a struct, add them to their own category unless they just take the name of the parent struct.  
	// In that case push them to the parent category
	FName PropertyCategoryName = FObjectEditorUtils::GetCategoryFName(Property);
	if (parentProperty && (PropertyCategoryName == parentProperty->structprop->Struct->GetFName()))
	{
		CategoryName = parentCategory;
	}
	else
	{
		if (!parentCategory.IsEmpty())
		{
			if (CategoryName.IsEmpty())
			{
				CategoryName = parentCategory;
			}
			else
			{
				CategoryName = (parentCategory + "|" + CategoryName);
			}
		}
	}

	if (parentProperty)
	{
		DisplayName = parentProperty->DisplayName + "_" + DisplayName;
	}

}



std::vector<uint8> MZProperty::UpdatePinValue(uint8* customContainer)
{
	if (customContainer)
	{
		void* val = Property->ContainerPtrToValuePtr<void>(customContainer);
		memcpy(data.data(), val, data.size());
	}
	else if (Container)
	{
		void* val = Property->ContainerPtrToValuePtr< void >(Container);
		memcpy(data.data(), val, data.size());
	}
	else if (StructPtr)
	{
		void* val = Property->ContainerPtrToValuePtr< void >(StructPtr);
		memcpy(data.data(), val, data.size());
	}

	return data;
}

void MZProperty::MarkState()
{
	if (UActorComponent* Component = Cast<UActorComponent>(Container))
	{
		Component->MarkRenderStateDirty();
		Component->UpdateComponentToWorld();
	}
}

void MZProperty::SetPropValue(void* val, size_t size, uint8* customContainer)
{
	IsChanged = true;
	CHECK_PROP_SIZE();
	if (Container)
	{
		SetProperty_InCont(Container, val);
		MarkState();
	}
	else if (StructPtr)
	{
		SetProperty_InCont(StructPtr, val);
		MarkState();
	}
	else if (customContainer)
	{
		SetProperty_InCont(customContainer, val);
	}
}

void MZProperty::SetProperty_InCont(void* container, void* val) { return; }

std::vector<uint8> MZBoolProperty::UpdatePinValue(uint8* customContainer)
{
	if (customContainer)
	{
		auto val = !!(boolprop->GetPropertyValue_InContainer(customContainer));
		memcpy(data.data(), &val, data.size());
	}
	else if (Container)
	{
		auto val = !!(boolprop->GetPropertyValue_InContainer(Container));
		memcpy(data.data(), &val, data.size());
	}
	else if (StructPtr)
	{
		auto val = !!(boolprop->GetPropertyValue_InContainer(StructPtr));
		memcpy(data.data(), &val, data.size());
	}

	return data;
}

void MZBoolProperty::SetProperty_InCont(void* container, void* val) { boolprop->SetPropertyValue_InContainer(container, !!(*(bool*)val)); }

void MZFloatProperty::SetProperty_InCont(void* container, void* val) { floatprop->SetPropertyValue_InContainer(container, *(float*)val); }

void MZDoubleProperty::SetProperty_InCont(void* container, void* val) { doubleprop->SetPropertyValue_InContainer(container, *(double*)val); }

void MZInt8Property::SetProperty_InCont(void* container, void* val) { int8prop->SetPropertyValue_InContainer(container, *(int8*)val); }

void MZInt16Property::SetProperty_InCont(void* container, void* val) { int16prop->SetPropertyValue_InContainer(container, *(int16*)val); }

void MZIntProperty::SetProperty_InCont(void* container, void* val) { intprop->SetPropertyValue_InContainer(container, *(int*)val); }

void MZInt64Property::SetProperty_InCont(void* container, void* val) { int64prop->SetPropertyValue_InContainer(container, *(int64*)val); }

void MZByteProperty::SetProperty_InCont(void* container, void* val) { byteprop->SetPropertyValue_InContainer(container, *(uint8*)val); }

void MZUInt16Property::SetProperty_InCont(void* container, void* val) { uint16prop->SetPropertyValue_InContainer(container, *(uint16*)val); }

void MZUInt32Property::SetProperty_InCont(void* container, void* val) { uint32prop->SetPropertyValue_InContainer(container, *(uint32*)val); }

void MZUInt64Property::SetProperty_InCont(void* container, void* val) { uint64prop->SetPropertyValue_InContainer(container, *(uint64*)val); }

void MZVec2Property::SetProperty_InCont(void* container, void* val) { structprop->CopyCompleteValue(structprop->ContainerPtrToValuePtr<void>(container), (FVector2D*)val); }

void MZVec3Property::SetProperty_InCont(void* container, void* val) { structprop->CopyCompleteValue(structprop->ContainerPtrToValuePtr<void>(container), (FVector*)val); }

void MZVec4Property::SetProperty_InCont(void* container, void* val) { structprop->CopyCompleteValue(structprop->ContainerPtrToValuePtr<void>(container), (FVector4*)val); }

void MZTrackProperty::SetProperty_InCont(void* container, void* val) 
{
	structprop->CopyCompleteValue(structprop->ContainerPtrToValuePtr<void>(container), (FRealityTrack*)val); 
	FRealityTrack newTrack = *(FRealityTrack*)val;

	if (AActor* actor = Cast<AActor>(Container))
	{
		actor->SetActorRelativeLocation(newTrack.location);
		//actor->SetActorRelativeRotation(newTrack.rotation.Rotation());
		//actor->SetActorRelativeRotation(newTrack.rotation.Rotation());
	}
}

flatbuffers::Offset<mz::fb::Pin> MZProperty::Serialize(flatbuffers::FlatBufferBuilder& fbb)
{

	std::vector<flatbuffers::Offset<mz::fb::MetaDataEntry>> metadata = SerializeMetaData(fbb);

	if (TypeName == "mz.fb.Void" || TypeName.size() < 1)
	{
		return mz::fb::CreatePinDirect(fbb, (mz::fb::UUID*)&Id, TCHAR_TO_UTF8(*DisplayName), "mz.fb.Void", mz::fb::ShowAs::NONE, mz::fb::CanShowAs::INPUT_OUTPUT_PROPERTY, TCHAR_TO_UTF8(*CategoryName), 0, &data, 0, 0, 0, &default_val, 0, ReadOnly, IsAdvanced, transient, &metadata);
	}
	return mz::fb::CreatePinDirect(fbb, (mz::fb::UUID*)&Id, TCHAR_TO_UTF8(*DisplayName), TypeName.c_str(),  PinShowAs, mz::fb::CanShowAs::INPUT_OUTPUT_PROPERTY, TCHAR_TO_UTF8(*CategoryName), 0, &data, 0, &min_val, &max_val, &default_val, 0, ReadOnly, IsAdvanced, transient, &metadata);
}

std::vector<flatbuffers::Offset<mz::fb::MetaDataEntry>> MZProperty::SerializeMetaData(flatbuffers::FlatBufferBuilder& fbb)
{
	std::vector<flatbuffers::Offset<mz::fb::MetaDataEntry>> metadata;
	for (auto [key, value] : mzMetaDataMap)
	{
		metadata.push_back(mz::fb::CreateMetaDataEntryDirect(fbb, TCHAR_TO_UTF8(*key), TCHAR_TO_UTF8(*value)));
	}
	return metadata;
}

MZStructProperty::MZStructProperty(UObject* container, FStructProperty* uproperty, FString parentCategory, uint8* StructPtr, MZStructProperty* parentProperty)
	: MZProperty(container, uproperty, parentCategory, StructPtr, parentProperty), structprop(uproperty)
{
	if (Container)
	{
		class FProperty* AProperty = structprop->Struct->PropertyLink;
		uint8* StructInst = structprop->ContainerPtrToValuePtr<uint8>(Container);
		while (AProperty != nullptr)
		{
			FName CategoryNamek = FObjectEditorUtils::GetCategoryFName(AProperty);
			UClass* Class = Container->GetClass();

			if (FEditorCategoryUtils::IsCategoryHiddenFromClass(Class, CategoryNamek.ToString()) || !PropertyVisibleExp(AProperty))
			{
				AProperty = AProperty->PropertyLinkNext;
				continue;
			}

			UE_LOG(LogTemp, Warning, TEXT("The property name in struct: %s"), *(AProperty->GetAuthoredName()));
			auto mzprop = MZPropertyFactory::CreateProperty(nullptr, AProperty, nullptr, nullptr, CategoryName + "|" + DisplayName, StructInst, this);
			if (mzprop)
			{
				childProperties.push_back(mzprop);
				for (auto it : mzprop->childProperties)
				{
					childProperties.push_back(it);
				}
			}

			AProperty = AProperty->PropertyLinkNext;
		}
	}
	else if (StructPtr)
	{
		class FProperty* AProperty = structprop->Struct->PropertyLink;
		uint8* StructInst = structprop->ContainerPtrToValuePtr<uint8>(StructPtr);
		while (AProperty != nullptr)
		{
			FName CategoryNamek = FObjectEditorUtils::GetCategoryFName(AProperty);
			UClass* Class = structprop->Struct->GetClass();

			if (FEditorCategoryUtils::IsCategoryHiddenFromClass(Class, CategoryNamek.ToString()) || !PropertyVisibleExp(AProperty))
			{
				AProperty = AProperty->PropertyLinkNext;
				continue;
			}

			UE_LOG(LogTemp, Warning, TEXT("The property name in struct: %s"), *(AProperty->GetAuthoredName()));
			auto mzprop = MZPropertyFactory::CreateProperty(nullptr, AProperty, nullptr, nullptr, CategoryName + "|" + DisplayName, StructInst, this);
			if (mzprop)
			{
				childProperties.push_back(mzprop);
				for (auto it : mzprop->childProperties)
				{
					childProperties.push_back(it);
				}
			}

			AProperty = AProperty->PropertyLinkNext;
		}
	}

	data = std::vector<uint8_t>(1, 0);
	TypeName = "mz.fb.Void";
}

void MZStructProperty::SetPropValue(void* val, size_t size, uint8* customContainer)
{
	//empty
}

MZObjectProperty::MZObjectProperty(UObject* container, FObjectProperty* uproperty, FString parentCategory, uint8* StructPtr, MZStructProperty* parentProperty)
	: MZProperty(container, uproperty, parentCategory, StructPtr, parentProperty), objectprop(uproperty)
{
	if (objectprop->PropertyClass->IsChildOf<UTextureRenderTarget2D>()) // We only support texturetarget2d from object properties
	{
		TypeName = "mz.fb.Texture"; 
		data = std::vector<uint8_t>(sizeof(mz::fb::Texture), 0);
		mz::fb::Texture tex = {};
		MZTextureShareManager::GetInstance()->AddTexturePin(this, &tex);
		memcpy(data.data(), &tex, sizeof(tex));

	}
	else
	{
		data = std::vector<uint8_t>(1, 0);
		TypeName = "mz.fb.Void";
	}
}

void MZObjectProperty::SetPropValue(void* val, size_t size, uint8* customContainer)
{
}

void MZNameProperty::SetPropValue(void* val, size_t size, uint8* customContainer)
{
	IsChanged = true;
	if (Container)
	{
		FString newval((char*)val);
		nameprop->SetPropertyValue_InContainer(Container, FName(newval));
		MarkState();
	}
	else if (StructPtr)
	{
		FString newval((char*)val);
		nameprop->SetPropertyValue_InContainer(StructPtr, FName(newval));
		MarkState();
	}
	else if (customContainer)
	{
		FString newval((char*)val);
		nameprop->SetPropertyValue_InContainer(customContainer, FName(newval));
	}
	return;

}

std::vector<uint8> MZNameProperty::UpdatePinValue(uint8* customContainer)
{
	FString val(" ");
	if (customContainer)
	{
		val = nameprop->GetPropertyValue_InContainer(customContainer).ToString();
	}
	else if (Container)
	{
		val = nameprop->GetPropertyValue_InContainer(Container).ToString();
		
	}
	else if(StructPtr)
	{
		val = nameprop->GetPropertyValue_InContainer(StructPtr).ToString();
	}

	auto s = StringCast<ANSICHAR>(*val);
	data = std::vector<uint8_t>(s.Length() + 1, 0);
	memcpy(data.data(), s.Get(), s.Length());
	
	return data;
}

void MZTextProperty::SetPropValue(void* val, size_t size, uint8* customContainer)
{
	IsChanged = true;
	if (Container)
	{
		FString newval((char*)val);
		textprop->SetPropertyValue_InContainer(Container, FText::FromString(newval));
		MarkState();
	}
	else if (StructPtr)
	{
		FString newval((char*)val);
		textprop->SetPropertyValue_InContainer(StructPtr, FText::FromString(newval));
		MarkState();
	}
	else if (customContainer)
	{
		FString newval((char*)val);
		textprop->SetPropertyValue_InContainer(customContainer, FText::FromString(newval));
	}
	return;
}

std::vector<uint8> MZTextProperty::UpdatePinValue(uint8* customContainer)
{
	FString val(" ");
	if (customContainer)
	{
		val = textprop->GetPropertyValue_InContainer(customContainer).ToString();
	}
	else if (Container)
	{
		val = textprop->GetPropertyValue_InContainer(Container).ToString();

	}
	else if (StructPtr)
	{
		val = textprop->GetPropertyValue_InContainer(StructPtr).ToString();
	}

	auto s = StringCast<ANSICHAR>(*val);
	data = std::vector<uint8_t>(s.Length() + 1, 0);
	memcpy(data.data(), s.Get(), s.Length());

	return data;
}

void MZEnumProperty::SetPropValue(void* val, size_t size, uint8* customContainer)
{
	//TODO
}

TSharedPtr<MZProperty> MZPropertyFactory::CreateProperty(UObject* container,
	FProperty* uproperty, 
	TMap<FGuid, TSharedPtr<MZProperty>>* registeredProperties, 
	TMap<FProperty*, TSharedPtr<MZProperty>>* propertiesMap,
	FString parentCategory, 
	uint8* StructPtr, 
	MZStructProperty* parentProperty)
{
	TSharedPtr<MZProperty> prop = nullptr;

	//CAST THE PROPERTY ACCORDINGLY
	uproperty->GetClass();
	if (FFloatProperty* floatprop = Cast<FFloatProperty>(uproperty) ) 
	{
		prop = TSharedPtr<MZProperty>(new MZFloatProperty(container, floatprop, parentCategory, StructPtr, parentProperty));
	}
	else if (FDoubleProperty* doubleprop = Cast<FDoubleProperty>(uproperty))
	{
		prop = TSharedPtr<MZProperty>(new MZDoubleProperty(container, doubleprop, parentCategory, StructPtr, parentProperty));
	}
	else if (FInt8Property* int8prop = Cast<FInt8Property>(uproperty))
	{
		prop = TSharedPtr<MZProperty>(new MZInt8Property(container, int8prop, parentCategory, StructPtr, parentProperty));
	}
	else if (FInt16Property* int16prop = Cast<FInt16Property>(uproperty))
	{
		prop = TSharedPtr<MZProperty>(new MZInt16Property(container, int16prop, parentCategory, StructPtr, parentProperty));
	}
	else if (FIntProperty* intprop = Cast<FIntProperty>(uproperty))
	{
		prop = TSharedPtr<MZProperty>(new MZIntProperty(container, intprop, parentCategory, StructPtr, parentProperty));
	}
	else if (FInt64Property* int64prop = Cast<FInt64Property>(uproperty))
	{
		prop = TSharedPtr<MZProperty>(new MZInt64Property(container, int64prop, parentCategory, StructPtr, parentProperty));
	}
	else if (FByteProperty* byteprop = Cast<FByteProperty>(uproperty))
	{
		prop = TSharedPtr<MZProperty>(new MZByteProperty(container, byteprop, parentCategory, StructPtr, parentProperty));
	}
	else if (FUInt16Property* uint16prop = Cast<FUInt16Property>(uproperty))
	{
		prop = TSharedPtr<MZProperty>(new MZUInt16Property(container, uint16prop, parentCategory, StructPtr, parentProperty));
	}
	else if (FUInt32Property* uint32prop = Cast<FUInt32Property>(uproperty))
	{
		prop = TSharedPtr<MZProperty>(new MZUInt32Property(container, uint32prop, parentCategory, StructPtr, parentProperty));
	}
	else if (FUInt64Property* uint64prop = Cast<FUInt64Property>(uproperty))
	{
		prop = TSharedPtr<MZProperty>(new MZUInt64Property(container, uint64prop, parentCategory, StructPtr, parentProperty));
	}
	else if (FBoolProperty* boolprop = Cast<FBoolProperty>(uproperty))
	{
		prop = TSharedPtr<MZProperty>(new MZBoolProperty(container, boolprop, parentCategory, StructPtr, parentProperty));
	}
	else if (FEnumProperty* enumprop = Cast<FEnumProperty>(uproperty))
	{
		prop = TSharedPtr<MZProperty>(new MZEnumProperty(container, enumprop, parentCategory, StructPtr, parentProperty));
	}
	else if (FTextProperty* textprop = Cast<FTextProperty>(uproperty))
	{
		prop = TSharedPtr<MZProperty>(new MZTextProperty(container, textprop, parentCategory, StructPtr, parentProperty));
	}
	else if (FNameProperty* nameprop = Cast<FNameProperty>(uproperty))
	{
		prop = TSharedPtr<MZProperty>(new MZNameProperty(container, nameprop, parentCategory, StructPtr, parentProperty));
	}
	else if (FObjectProperty* objectprop = Cast<FObjectProperty>(uproperty))
	{
		prop = TSharedPtr<MZProperty>(new MZObjectProperty(container, objectprop, parentCategory, StructPtr, parentProperty));
	}
	else if (FStructProperty* structprop = Cast<FStructProperty>(uproperty))
	{

		//TODO ADD SUPPORT FOR FTRANSFORM
		if (structprop->Struct == TBaseStructure<FVector2D>::Get()) //vec2
		{
			prop = TSharedPtr<MZProperty>(new MZVec2Property(container, structprop, parentCategory, StructPtr, parentProperty));
		}
		else if (structprop->Struct == TBaseStructure<FVector>::Get()) //vec3
		{
			prop = TSharedPtr<MZProperty>(new MZVec3Property(container, structprop, parentCategory, StructPtr, parentProperty));
		}
		else if (structprop->Struct == TBaseStructure<FRotator>::Get())
		{
			prop = TSharedPtr<MZProperty>(new MZVec3Property(container, structprop, parentCategory, StructPtr, parentProperty));
			FVector min(0, 0, 0);
			FVector max(359.999, 359.999, 359.999);
			prop->min_val = prop->data;
			prop->max_val = prop->data;
			memcpy(prop->min_val.data(), &min, sizeof(FVector));
			memcpy(prop->max_val.data(), &max, sizeof(FVector));
		}
		else if (structprop->Struct == TBaseStructure<FVector4>::Get() || structprop->Struct == TBaseStructure<FQuat>::Get()) //vec4
		{
			prop = TSharedPtr<MZProperty>(new MZVec4Property(container, structprop, parentCategory, StructPtr, parentProperty));

		}
		else if (structprop->Struct == FRealityTrack::StaticStruct()) //track
		{
			prop = TSharedPtr<MZProperty>(new MZTrackProperty(container, structprop, parentCategory, StructPtr, parentProperty));
		}
		else //auto construct
		{
			prop = TSharedPtr<MZProperty>(new MZStructProperty(container, structprop, parentCategory, StructPtr, parentProperty));
		}
	}

	if (!prop)
	{
		return nullptr; //for properties that we do not support
	}

	prop->UpdatePinValue();
	prop->default_val = prop->data;
	if (registeredProperties)
	{
		registeredProperties->Add(prop->Id, prop);
		for (auto& it : prop->childProperties)
		{
			registeredProperties->Add(it->Id, it);
		}
	}
	if (propertiesMap)
	{
		propertiesMap->Add(prop->Property, prop);
		for (auto& it : prop->childProperties)
		{
			propertiesMap->Add(it->Property, it);
		}
	}
	if (prop->TypeName == "mz.fb.Void")
	{
		prop->data.clear();
		prop->default_val.clear();
	}
#if 0 //default properties from objects
	if (prop->TypeName == "mz.fb.Void")
	{
		prop->data.clear();
		prop->default_val.clear();
	}
	else if (auto actor = Cast<AActor>(container))
	{
		if (prop->TypeName != "bool")
		{
			auto defobj = actor->GetClass()->GetDefaultObject();
			if (defobj)
			{
				auto val = uproperty->ContainerPtrToValuePtr<void>(defobj);
				if (prop->default_val.size() != uproperty->GetSize())
				{
					prop->default_val = std::vector<uint8>(uproperty->GetSize(), 0);	
				}
				memcpy(prop->default_val.data(), val, uproperty->GetSize());
			}

		}
		else
		{
			auto defobj = actor->GetClass()->GetDefaultObject();
			if (defobj)
			{
				auto val = !!( *uproperty->ContainerPtrToValuePtr<bool>(defobj) );
				if (prop->default_val.size() != uproperty->GetSize())
				{
					prop->default_val = std::vector<uint8>(uproperty->GetSize(), 0);
				}
				memcpy(prop->default_val.data(), &val, uproperty->GetSize());
			}

		}
			//uproperty->ContainerPtrToValuePtrForDefaults()
	}
	else if (auto sceneComponent = Cast<USceneComponent>(container))
	{
		if (prop->TypeName != "bool")
		{
			auto defobj = sceneComponent->GetClass()->GetDefaultObject();
			if (defobj)
			{
				auto val = uproperty->ContainerPtrToValuePtr<void>(defobj);
				if (prop->default_val.size() != uproperty->GetSize())
				{
					prop->default_val = std::vector<uint8>(uproperty->GetSize(), 0);
				}
				memcpy(prop->default_val.data(), val, uproperty->GetSize());
			}
		}
		else
		{
			auto defobj = sceneComponent->GetClass()->GetDefaultObject();
			if (defobj)
			{
				auto val = !!( *uproperty->ContainerPtrToValuePtr<bool>(defobj) );

				if (prop->default_val.size() != uproperty->GetSize())
				{
					prop->default_val = std::vector<uint8>(uproperty->GetSize(), 0);
				}
				memcpy(prop->default_val.data(), &val, uproperty->GetSize());
			}

		}
		//uproperty->ContainerPtrToValuePtrForDefaults()
	}
#endif

	//update metadata
	prop->mzMetaDataMap.Add("property", uproperty->GetFName().ToString());
	if (auto component = Cast<USceneComponent>(container))
	{
		prop->mzMetaDataMap.Add("component", component->GetFName().ToString());
		if (auto actor = component->GetOwner())
		{
			prop->mzMetaDataMap.Add("actorId", actor->GetActorGuid().ToString());
		}
	}
	else if (auto actor = Cast<AActor>(container))
	{
		prop->mzMetaDataMap.Add("actorId", actor->GetActorGuid().ToString());
	}

	//prop->mzMetaDataMap.Add("component", prop->PropertyName);
	//	mz::fb::CreateMetaDataEntryDirect(fbb, "propertyPath", TCHAR_TO_UTF8(*Property->GetPathName())),
	//	mz::fb::CreateMetaDataEntryDirect(fbb, "property", TCHAR_TO_UTF8(*Property->GetName())) };

	return prop;
}

#endif


