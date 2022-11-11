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
	
	//bool setVal = true;
	//if (uproperty->IsA(FFloatProperty::StaticClass())){ 
	//	data = std::vector<uint8_t>(4, 0);
	//	TypeName = "f32";
	//}
	//else if (uproperty->IsA(FDoubleProperty::StaticClass())){ 
	//	data = std::vector<uint8_t>(8, 0);
	//	TypeName = "f64";
	//}
	//else if (uproperty->IsA(FInt8Property::StaticClass())) { 
	//	data = std::vector<uint8_t>(1, 0);
	//	TypeName = "i8";
	//}
	//else if (uproperty->IsA(FInt16Property::StaticClass())) { 
	//	data = std::vector<uint8_t>(2, 0);
	//	TypeName = "i16";
	//}
	//else if (uproperty->IsA(FIntProperty::StaticClass())) { 
	//	data = std::vector<uint8_t>(4, 0);
	//	TypeName = "i32";
	//}
	//else if (uproperty->IsA(FInt64Property::StaticClass())){ 
	//	data = std::vector<uint8_t>(8, 0);
	//	TypeName = "i64";
	//}
	//else if (uproperty->IsA(FByteProperty::StaticClass())) { 
	//	data = std::vector<uint8_t>(1, 0);
	//	TypeName = "u8";
	//}
	//else if (uproperty->IsA(FUInt16Property::StaticClass())) { 
	//	data = std::vector<uint8_t>(2, 0);
	//	TypeName = "u16";
	//}
	//else if (uproperty->IsA(FUInt32Property::StaticClass())) { 
	//	data = std::vector<uint8_t>(4, 0);
	//	TypeName = "u32";
	//}
	//else if (uproperty->IsA(FUInt64Property::StaticClass())) { 
	//	data = std::vector<uint8_t>(8, 0);
	//	TypeName = "u64";
	//}
	//else if (uproperty->IsA(FBoolProperty::StaticClass())){ 
	//	data = std::vector<uint8_t>(1, 0);
	//	TypeName = "bool";
	//}
	//else if (uproperty->IsA(FEnumProperty::StaticClass())) { //todo data
	//	// how to iterate enums__??
	//	TypeName = "string";
	//}
	//else if (uproperty->IsA(FTextProperty::StaticClass())) {

	//	if (Container)
	//	{
	//		FString val = container ? Property->ContainerPtrToValuePtr<FText>(container)->ToString() : Property->ContainerPtrToValuePtr<FText>(StructPtr)->ToString();
	//		auto s = StringCast<ANSICHAR>(*val);
	//		data = std::vector<uint8_t>(s.Length() + 1, 0);
	//		memcpy(data.data(), s.Get(), s.Length());
	//	}
	//	else 
	//	{
	//		data = std::vector<uint8_t>(1, 0);
	//	}

	//	TypeName = "string";
	//	setVal = false;

	//}
	//else if (uproperty->IsA(FNameProperty::StaticClass())) { 
	//	if (Container)
	//	{
	//		FString val = container ? Property->ContainerPtrToValuePtr<FName>(container)->ToString() : Property->ContainerPtrToValuePtr<FName>(StructPtr)->ToString();;
	//		auto s = StringCast<ANSICHAR>(*val);
	//		data = std::vector<uint8_t>(s.Length() + 1, 0);
	//		memcpy(data.data(), s.Get(), s.Length());
	//	}
	//	else
	//	{
	//		data = std::vector<uint8_t>(1, 0);
	//	}
	//	TypeName = "string";
	//	setVal = false;
	//}
	//else if (uproperty->IsA(FObjectProperty::StaticClass()))
	//{
	//	if (((FObjectProperty*)uproperty)->PropertyClass->IsChildOf<UTextureRenderTarget2D>()) // We only support texturetarget2d from object properties
	//	{
	//		TypeName = "mz.fb.Texture"; 
	//		data = std::vector<uint8_t>(sizeof(mz::fb::Texture), 0);
	//		mz::fb::Texture tex = {};
	//		MZTextureShareManager::GetInstance()->AddTexturePin(this, &tex);
	//		memcpy(data.data(), &tex, sizeof(tex));

	//	}
	//	else
	//	{
	//		data = std::vector<uint8_t>(1, 0);
	//		TypeName = "mz.fb.Void";
	//	}
	//	setVal = false;
	//}
	////else if (uproperty->IsA(FArrayProperty::StaticClass())) { //Not supported by mediaz
	////	data = std::vector<uint8_t>(1, 0);
	////	TypeName = "bool";
	////}
	//else if (uproperty->IsA(FStructProperty::StaticClass())) {
	//	FStructProperty* StructProp = Cast<FStructProperty>(uproperty);

	//	if (StructProp->Struct == TBaseStructure<FVector>::Get() || StructProp->Struct == TBaseStructure<FRotator>::Get()) //todo data
	//	{
	//		data = std::vector<uint8_t>(sizeof(FVector), 0);
	//		TypeName = "mz.fb.vec3d";
	//	}
	//	else if (StructProp->Struct == TBaseStructure<FVector4>::Get() || StructProp->Struct == TBaseStructure<FQuat>::Get()) //todo data
	//	{
	//		data = std::vector<uint8_t>(sizeof(FVector4), 0);
	//		TypeName = "mz.fb.vec4d";
	//	}
	//	else if (StructProp->Struct == TBaseStructure<FVector2D>::Get()) //todo data
	//	{
	//		data = std::vector<uint8_t>(sizeof(FVector2D), 0);
	//		TypeName = "mz.fb.vec2d";
	//	}
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
	//	else
	//	{
	//		if (Container)
	//		{
	//			class FProperty* AProperty = StructProp->Struct->PropertyLink;
	//			uint8* StructInst = StructProp->ContainerPtrToValuePtr<uint8>(Container);
	//			while (AProperty != nullptr)
	//			{
	//				FName CategoryNamek = FObjectEditorUtils::GetCategoryFName(AProperty);
	//				UClass* Class = Container->GetClass();

	//				if (FEditorCategoryUtils::IsCategoryHiddenFromClass(Class, CategoryNamek.ToString()) || !PropertyVisibleExp(AProperty))
	//				{
	//					AProperty = AProperty->PropertyLinkNext;
	//					continue;
	//				}

	//				UE_LOG(LogTemp, Warning, TEXT("The property name in struct: %s"), *(AProperty->GetAuthoredName()) );
	//				MZProperty* mzprop = new MZProperty(nullptr, AProperty, CategoryName + "|" + DisplayName, StructInst, StructProp);
	//				childProperties.push_back(mzprop);
	//				for (auto it : mzprop->childProperties)
	//				{
	//					childProperties.push_back(it);
	//				}
	//				//RegisteredProperties.Add(mzprop->id, mzprop);
	//				AProperty = AProperty->PropertyLinkNext;
	//			}
	//		}

	//		data = std::vector<uint8_t>(1, 0);
	//		TypeName = "mz.fb.Void";
	//		setVal = false;
	//	}
	//}
	//else{
	//	data = std::vector<uint8_t>(1, 0);
	//	TypeName = "mz.fb.Void";
	//	setVal = false;
	//}


	////init data
	//if (container && setVal)
	//{
	//	void* val = Property->ContainerPtrToValuePtr< void >(container); //: Property->ContainerPtrToValuePtr<void>(StructPtr);
	//	memcpy(data.data(), val, data.size());
	//}

	//if (structPtr && setVal)
	//{
	//	void* val = Property->ContainerPtrToValuePtr< void >(structPtr); //: Property->ContainerPtrToValuePtr<void>(StructPtr);
	//	memcpy(data.data(), val, data.size());
	//}

}

//void MZProperty::SetValue(void* newval, size_t size, uint8* customContainer) //called from game thread*
//{
//	if (Container)
//	{
//		if (TypeName != "mz.fb.Void" && TypeName != "string")
//		{
//			if (TypeName == "bool")
//			{
//				bool nval = *(bool*)newval;
//				FBoolProperty* boolProperty = Cast<FBoolProperty>(Property);
//				if (boolProperty)
//				{
//					void* val = Property->ContainerPtrToValuePtr< void >(Container);
//					boolProperty->SetPropertyValue(val, nval);
//				}
//			}
//			else
//			{
//				if (Property->ElementSize == size)
//				{
//					void* val = Property->ContainerPtrToValuePtr< void >(Container);
//					memcpy(val, newval, size);
//					if (TypeName == "mz.fb.vec3d")
//					{
//						UE_LOG(LogTemp, Warning, TEXT("The vector value is: %s"), *(*(FVector*)val).ToString());
//					}
//				}
//				else
//				{
//					UE_LOG(LogMediaZ, Error, TEXT("Property size mismatch with mediaZ"));
//
//				}
//			}
//		}
//		
//
//		if (UActorComponent* Component = Cast<UActorComponent>(Container))
//		{
//			Component->MarkRenderStateDirty();
//			Component->UpdateComponentToWorld();
//		}
//		
//		
//	}
//	else if(customContainer)
//	{
//		if (TypeName == "bool")
//		{
//			bool nval = *(bool*)newval;
//			FBoolProperty* boolProperty = Cast<FBoolProperty>(Property);
//			if (boolProperty)
//			{
//				void* val = Property->ContainerPtrToValuePtr< void >(customContainer);
//				boolProperty->SetPropertyValue(val, nval);
//			}
//		}
//		else if (TypeName != "mz.fb.Void" && TypeName != "string")
//		{
//			void* val = Property->ContainerPtrToValuePtr< void >(customContainer);
//			memcpy(val, newval, size);
//		}
//	}
//	
//}

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

//std::vector<uint8> MZProperty::GetValue(uint8* customContainer)
//{
//	if (Container)
//	{
//		if (TypeName != "mz.fb.Void" && TypeName != "string")
//		{
//			void* val = Property->ContainerPtrToValuePtr< void >(Container);
//			auto size = Property->GetSize();
//			std::vector<uint8> value(size, 0);
//			memcpy(value.data(), val, size);
//			return value;
//		}
//
//	}
//	else if (customContainer)
//	{
//		if (TypeName != "mz.fb.Void" && TypeName != "string")
//		{
//			void* val = Property->ContainerPtrToValuePtr< void >(customContainer);
//			auto size = Property->GetSize();
//			std::vector<uint8> value(size, 0);
//			memcpy(value.data(), val, size);
//			return value;
//		}
//	}
//	return std::vector<uint8>();
//}

void MZProperty::MarkState()
{
	if (UActorComponent* Component = Cast<UActorComponent>(Container))
	{
		Component->MarkRenderStateDirty();
		Component->UpdateComponentToWorld();
	}
}

flatbuffers::Offset<mz::fb::Pin> MZProperty::Serialize(flatbuffers::FlatBufferBuilder& fbb)
{
	if (TypeName == "mz.fb.Void" || TypeName.size() < 1)
	{
		return mz::fb::CreatePinDirect(fbb, (mz::fb::UUID*)&id, TCHAR_TO_UTF8(*DisplayName), "mz.fb.Void", mz::fb::ShowAs::NONE, mz::fb::CanShowAs::INPUT_OUTPUT_PROPERTY, TCHAR_TO_UTF8(*CategoryName), 0, &data, 0, 0, 0, 0, ReadOnly, IsAdvanced);
	}
	return mz::fb::CreatePinDirect(fbb, (mz::fb::UUID*)&id, TCHAR_TO_UTF8(*DisplayName), TypeName.c_str(),  PinShowAs, mz::fb::CanShowAs::INPUT_OUTPUT_PROPERTY, TCHAR_TO_UTF8(*CategoryName), 0, &data, 0, 0, 0, 0, ReadOnly, IsAdvanced);
}




flatbuffers::Offset<mz::fb::Pin> MZStructProperty::Serialize(flatbuffers::FlatBufferBuilder& fbb)
{
	if (TypeName == "mz.fb.Void" || TypeName.size() < 1)
	{
		return mz::fb::CreatePinDirect(fbb, (mz::fb::UUID*)&id, TCHAR_TO_UTF8(*DisplayName), "mz.fb.Void", mz::fb::ShowAs::NONE, mz::fb::CanShowAs::INPUT_OUTPUT_PROPERTY, TCHAR_TO_UTF8(*CategoryName), 0, &data, 0, 0, 0, 0, ReadOnly, IsAdvanced);
	}
	return mz::fb::CreatePinDirect(fbb, (mz::fb::UUID*)&id, TCHAR_TO_UTF8(*DisplayName), TypeName.c_str(), PinShowAs, mz::fb::CanShowAs::INPUT_OUTPUT_PROPERTY, TCHAR_TO_UTF8(*CategoryName), 0, &data, 0, 0, 0, 0, ReadOnly, IsAdvanced);
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
			childProperties.push_back(mzprop);
			//if (registeredProperties)
			//{
			//	registeredProperties->Add(mzprop->id, mzprop);
			//}
			if (mzprop->Property->IsA(FStructProperty::StaticClass()))
			{
				for (auto it : ((MZStructProperty*)mzprop)->childProperties)
				{
					childProperties.push_back(it);
				}
			}
			//RegisteredProperties.Add(mzprop->id, mzprop);
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
			MZProperty* mzkprop = MZPropertyFactory::CreateProperty(nullptr, AProperty, nullptr, CategoryName + "|" + DisplayName, StructInst, this);
			childProperties.push_back(mzkprop);
			//if (registeredProperties)
			//{
			//	registeredProperties->Add(mzkprop->id, mzkprop);
			//}
			if (mzkprop->Property->IsA(FStructProperty::StaticClass()))
			{
				for (auto it : ((MZStructProperty*)mzkprop)->childProperties)
				{
					childProperties.push_back(it);
				}
			}
			//RegisteredProperties.Add(mzprop->id, mzprop);
			AProperty = AProperty->PropertyLinkNext;
		}
	}

	//data = std::vector<uint8_t>(1, 0);
	//TypeName = "mz.fb.Void";
}



void MZStructProperty::SetPropValue(void* val, size_t size, uint8* customContainer)
{
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
	else if (customContainer)
	{
		FString newval((char*)val);
		nameprop->SetPropertyValue(Container, FName(newval));
	}
	return;

}

std::vector<uint8> MZNameProperty::UpdatePinValue(uint8* customContainer)
{
	FString val(" ");
	if (customContainer)
	{
		val = nameprop->GetPropertyValue(customContainer).ToString();
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
	else if (customContainer)
	{
		FString newval((char*)val);
		textprop->SetPropertyValue(Container, FText::FromString(newval));
	}
	return;
}

std::vector<uint8> MZTextProperty::UpdatePinValue(uint8* customContainer)
{
	FString val(" ");
	if (customContainer)
	{
		val = textprop->GetPropertyValue(customContainer).ToString();
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

void MZUInt64Property::SetPropValue(void* val, size_t size, uint8* customContainer)
{
	CHECK_PROP_SIZE();
	if (Container)
	{
		uint64prop->SetPropertyValue_InContainer(Container, *(uint64*)val);
		MarkState();
	}
	else if (customContainer)
	{
		uint64prop->SetPropertyValue(customContainer, *(uint64*)val);
	}
	return;
}

void MZUInt32Property::SetPropValue(void* val, size_t size, uint8* customContainer)
{
	CHECK_PROP_SIZE();
	if (Container)
	{
		uint32prop->SetPropertyValue_InContainer(Container, *(uint32*)val);
		MarkState();
	}
	else if (customContainer)
	{
		uint32prop->SetPropertyValue(customContainer, *(uint32*)val);
	}
	return;
}

void MZUInt16Property::SetPropValue(void* val, size_t size, uint8* customContainer)
{
	CHECK_PROP_SIZE();
	if (Container)
	{
		uint16prop->SetPropertyValue_InContainer(Container, *(uint16*)val);
		MarkState();
	}
	else if (customContainer)
	{
		uint16prop->SetPropertyValue(customContainer, *(uint16*)val);
	}
	return;
}

void MZByteProperty::SetPropValue(void* val, size_t size, uint8* customContainer)
{
	CHECK_PROP_SIZE();
	if (Container)
	{
		byteprop->SetPropertyValue_InContainer(Container, *(uint8*)val);
		MarkState();
	}
	else if (customContainer)
	{
		byteprop->SetPropertyValue(customContainer, *(uint8*)val);
	}
	return;
}

void MZInt64Property::SetPropValue(void* val, size_t size, uint8* customContainer)
{
	CHECK_PROP_SIZE();
	if (Container)
	{
		int64prop->SetPropertyValue_InContainer(Container, *(int64*)val);
		MarkState();
	}
	else if (customContainer)
	{
		int64prop->SetPropertyValue(customContainer, *(int64*)val);
	}
	return;
}

void MZIntProperty::SetPropValue(void* val, size_t size, uint8* customContainer)
{
	CHECK_PROP_SIZE();
	if (Container)
	{
		intprop->SetPropertyValue_InContainer(Container, *(int32*)val);
		MarkState();
	}
	else if (customContainer)
	{
		intprop->SetPropertyValue(customContainer, *(int32*)val);
	}
	return;
}

void MZInt16Property::SetPropValue(void* val, size_t size, uint8* customContainer)
{
	CHECK_PROP_SIZE();
	if (Container)
	{
		int16prop->SetPropertyValue_InContainer(Container, *(int16*)val);
		MarkState();
	}
	else if (customContainer)
	{
		int16prop->SetPropertyValue(customContainer, *(int16*)val);
	}
	return;
}

void MZInt8Property::SetPropValue(void* val, size_t size, uint8* customContainer)
{
	CHECK_PROP_SIZE();
	if (Container)
	{
		int8prop->SetPropertyValue_InContainer(Container, *(int8*)val);
		MarkState();
	}
	else if (customContainer)
	{
		int8prop->SetPropertyValue(customContainer, *(int8*)val);
	}
	return;
}

void MZFloatProperty::SetPropValue(void* val, size_t size, uint8* customContainer)
{
	CHECK_PROP_SIZE();
	if (Container)
	{
		floatprop->SetPropertyValue_InContainer(Container, *(float*)val);
		MarkState();
	}
	else if (customContainer)
	{
		floatprop->SetPropertyValue(customContainer, *(float*)val);
	}
	return;
}

void MZBoolProperty::SetPropValue(void* val, size_t size, uint8* customContainer)
{
	CHECK_PROP_SIZE();
	if (Container)
	{
		boolprop->SetPropertyValue_InContainer(Container, *(bool*)val);
		MarkState();
	}
	else if (customContainer)
	{
		boolprop->SetPropertyValue(customContainer, *(bool*)val);
	}
	return;
}

void MZDoubleProperty::SetPropValue(void* val, size_t size, uint8* customContainer)
{
	CHECK_PROP_SIZE();
	if (Container)
	{
		doubleprop->SetPropertyValue_InContainer(Container, *(double*)val);
		MarkState();
	}
	else if (StructPtr)
	{
		doubleprop->SetPropertyValue_InContainer(StructPtr, *(double*)val);
		MarkState();
	}
	else if (customContainer)
	{
		doubleprop->SetPropertyValue(customContainer, *(double*)val);
	}
	return;
}


MZProperty* MZPropertyFactory::CreateProperty(UObject* container, FProperty* uproperty, TMap<FGuid, MZProperty*>* registeredProperties, FString parentCategory, uint8* StructPtr, MZStructProperty* parentProperty)
{
	MZProperty* prop = nullptr;

	//CAST THE PROPERTY ACCORDINGLY
	std::vector<MZProperty*> childProperties;
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
		auto mzstructprop = new MZStructProperty(container, structprop, parentCategory, StructPtr, parentProperty);
		//auto list = GetAllChildList(mzstructprop);
		prop = mzstructprop;
		childProperties = mzstructprop->childProperties;
	}

	if (!prop)
	{
		return nullptr;
	}

	prop->UpdatePinValue();
	if (registeredProperties)
	{
		registeredProperties->Add(prop->id, prop);
		for (auto it : childProperties)
		{
			registeredProperties->Add(it->id, it);
		}
	}
	//prop->SetPropertyValue();

	return prop;
}
















#endif
