#if WITH_EDITOR
#include "MZActorProperties.h"
#include "MZTextureShareManager.h"
#include "EditorCategoryUtils.h"
#include "ObjectEditorUtils.h"
#include "MZTrack.h"


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
	id = FGuid::NewGuid();
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

void MZBoolProperty::SetProperty_InCont(void* container, void* val) { boolprop->SetPropertyValue_InContainer(container, *(bool*)val); }

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




flatbuffers::Offset<mz::fb::Pin> MZProperty::Serialize(flatbuffers::FlatBufferBuilder& fbb)
{
	if (TypeName == "mz.fb.Void" || TypeName.size() < 1)
	{
		return mz::fb::CreatePinDirect(fbb, (mz::fb::UUID*)&id, TCHAR_TO_UTF8(*DisplayName), "mz.fb.Void", mz::fb::ShowAs::NONE, mz::fb::CanShowAs::INPUT_OUTPUT_PROPERTY, TCHAR_TO_UTF8(*CategoryName), 0, &data, 0, 0, 0, 0, ReadOnly, IsAdvanced);
	}
	return mz::fb::CreatePinDirect(fbb, (mz::fb::UUID*)&id, TCHAR_TO_UTF8(*DisplayName), TypeName.c_str(),  PinShowAs, mz::fb::CanShowAs::INPUT_OUTPUT_PROPERTY, TCHAR_TO_UTF8(*CategoryName), 0, &data, 0, 0, 0, 0, ReadOnly, IsAdvanced);
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
			MZProperty* mzprop = MZPropertyFactory::CreateProperty(nullptr, AProperty, nullptr, CategoryName + "|" + DisplayName, StructInst, this);
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
			MZProperty* mzprop = MZPropertyFactory::CreateProperty(nullptr, AProperty, nullptr, CategoryName + "|" + DisplayName, StructInst, this);
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
		textprop->SetPropertyValue_InContainer(Container, FText::FromString(newval));
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



MZProperty* MZPropertyFactory::CreateProperty(UObject* container, FProperty* uproperty, TMap<FGuid, MZProperty*>* registeredProperties, FString parentCategory, uint8* StructPtr, MZStructProperty* parentProperty)
{
	MZProperty* prop = nullptr;

	//CAST THE PROPERTY ACCORDINGLY
	if (FFloatProperty* floatprop = Cast<FFloatProperty>(uproperty) ) 
	{
		prop = new MZFloatProperty(container, floatprop, parentCategory, StructPtr, parentProperty);
	}
	else if (FDoubleProperty* doubleprop = Cast<FDoubleProperty>(uproperty))
	{
		prop = new MZDoubleProperty(container, doubleprop, parentCategory, StructPtr, parentProperty);
	}
	else if (FInt8Property* int8prop = Cast<FInt8Property>(uproperty))
	{
		prop = new MZInt8Property(container, int8prop, parentCategory, StructPtr, parentProperty);
	}
	else if (FInt16Property* int16prop = Cast<FInt16Property>(uproperty))
	{
		prop = new MZInt16Property(container, int16prop, parentCategory, StructPtr, parentProperty);
	}
	else if (FIntProperty* intprop = Cast<FIntProperty>(uproperty))
	{
		prop = new MZIntProperty(container, intprop, parentCategory, StructPtr, parentProperty);
	}
	else if (FInt64Property* int64prop = Cast<FInt64Property>(uproperty))
	{
		prop = new MZInt64Property(container, int64prop, parentCategory, StructPtr, parentProperty);
	}
	else if (FByteProperty* byteprop = Cast<FByteProperty>(uproperty))
	{
		prop = new MZByteProperty(container, byteprop, parentCategory, StructPtr, parentProperty);
	}
	else if (FUInt16Property* uint16prop = Cast<FUInt16Property>(uproperty))
	{
		prop = new MZUInt16Property(container, uint16prop, parentCategory, StructPtr, parentProperty);
	}
	else if (FUInt32Property* uint32prop = Cast<FUInt32Property>(uproperty))
	{
		prop = new MZUInt32Property(container, uint32prop, parentCategory, StructPtr, parentProperty);
	}
	else if (FUInt64Property* uint64prop = Cast<FUInt64Property>(uproperty))
	{
		prop = new MZUInt64Property(container, uint64prop, parentCategory, StructPtr, parentProperty);
	}
	else if (FBoolProperty* boolprop = Cast<FBoolProperty>(uproperty))
	{
		prop = new MZBoolProperty(container, boolprop, parentCategory, StructPtr, parentProperty);
	}
	else if (FEnumProperty* enumprop = Cast<FEnumProperty>(uproperty))
	{
		prop = new MZEnumProperty(container, enumprop, parentCategory, StructPtr, parentProperty);
	}
	else if (FTextProperty* textprop = Cast<FTextProperty>(uproperty))
	{
		prop = new MZTextProperty(container, textprop, parentCategory, StructPtr, parentProperty);
	}
	else if (FNameProperty* nameprop = Cast<FNameProperty>(uproperty))
	{
		prop = new MZNameProperty(container, nameprop, parentCategory, StructPtr, parentProperty);
	}
	else if (FObjectProperty* objectprop = Cast<FObjectProperty>(uproperty))
	{
		prop = new MZObjectProperty(container, objectprop, parentCategory, StructPtr, parentProperty);
	}
	else if (FStructProperty* structprop = Cast<FStructProperty>(uproperty))
	{

//	else if (StructProp->Struct == TBaseStructure<FTransform>::Get()) //todo everything
//	{
//		//TODO
//		setVal = false;
//	}
//	else if (StructProp->Struct == FMZTrack::StaticStruct())
//	{
//		data = std::vector<uint8_t>(sizeof(mz::fb::Track), 0);
//		TypeName = "mz.fb.Track";
//		//setVal = false;
//	}

		if (structprop->Struct == TBaseStructure<FVector2D>::Get()) //vec2
		{
			prop = new MZVec2Property(container, structprop, parentCategory, StructPtr, parentProperty);
		}
		else if (structprop->Struct == TBaseStructure<FVector>::Get() || structprop->Struct == TBaseStructure<FRotator>::Get()) //vec3
		{
			prop = new MZVec3Property(container, structprop, parentCategory, StructPtr, parentProperty);
		}
		else if (structprop->Struct == TBaseStructure<FVector4>::Get() || structprop->Struct == TBaseStructure<FQuat>::Get()) //vec4
		{
			prop = new MZVec4Property(container, structprop, parentCategory, StructPtr, parentProperty);

		}
		//else if () //track
		//{

		//}
		else //auto construct
		{
			auto mzstructprop = new MZStructProperty(container, structprop, parentCategory, StructPtr, parentProperty);
			//auto list = GetAllChildList(mzstructprop);
			prop = mzstructprop;
		}
	}

	if (!prop)
	{
		return nullptr; //for properties that we do not support
	}

	prop->UpdatePinValue();
	if (registeredProperties)
	{
		registeredProperties->Add(prop->id, prop);
		for (auto it : prop->childProperties)
		{
			registeredProperties->Add(it->id, it);
		}
	}
	//prop->SetPropertyValue();

	return prop;
}
















#endif


