#if WITH_EDITOR
#include "MZActorProperties.h"
#include "MZTextureShareManager.h"
#include "EditorCategoryUtils.h"
#include "ObjectEditorUtils.h"

bool PropertyVisibleExp(FProperty* ueproperty)
{
	return !ueproperty->HasAllPropertyFlags(CPF_DisableEditOnInstance) &&
		!ueproperty->HasAllPropertyFlags(CPF_Deprecated) &&
		//!ueproperty->HasAllPropertyFlags(CPF_EditorOnly) && //? dont know what this flag does but it hides more than necessary
		ueproperty->HasAllPropertyFlags(CPF_Edit) &&
		//ueproperty->HasAllPropertyFlags(CPF_BlueprintVisible) && //? dont know what this flag does but it hides more than necessary
		ueproperty->HasAllFlags(RF_Public);
}

MZProperty::MZProperty(UObject* container, FProperty* uproperty, FString parentCategory, uint8* structPtr, FStructProperty* parentProperty)
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
#if WITH_EDITOR
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
#else
	DisplayName = uproperty->GetFName().ToString();
	CategoryName = parentCategory + "Default";
	UIMinString = "";
	UIMaxString = "";
	IsAdvanced = false;
#endif
	
	
	// For properties inside a struct, add them to their own category unless they just take the name of the parent struct.  
	// In that case push them to the parent category
	FName PropertyCategoryName = FObjectEditorUtils::GetCategoryFName(Property);
	if (parentProperty && (PropertyCategoryName == parentProperty->Struct->GetFName()))
	{
		CategoryName = parentCategory;
	}
	else
	{
		if (!parentCategory.IsEmpty())
		{
			CategoryName = (parentCategory + "|" + CategoryName);
		}
	}


	if (uproperty->IsA(FFloatProperty::StaticClass())){ 
		data = std::vector<uint8_t>(4, 0);
		TypeName = "f32";
	}
	else if (uproperty->IsA(FDoubleProperty::StaticClass())){ 
		data = std::vector<uint8_t>(8, 0);
		TypeName = "f64";
	}
	else if (uproperty->IsA(FInt8Property::StaticClass())) { 
		data = std::vector<uint8_t>(1, 0);
		TypeName = "i8";
	}
	else if (uproperty->IsA(FInt16Property::StaticClass())) { 
		data = std::vector<uint8_t>(2, 0);
		TypeName = "i16";
	}
	else if (uproperty->IsA(FIntProperty::StaticClass())) { 
		data = std::vector<uint8_t>(4, 0);
		TypeName = "i32";
	}
	else if (uproperty->IsA(FInt64Property::StaticClass())){ 
		data = std::vector<uint8_t>(8, 0);
		TypeName = "i64";
	}
	else if (uproperty->IsA(FByteProperty::StaticClass())) { 
		data = std::vector<uint8_t>(1, 0);
		TypeName = "u8";
	}
	else if (uproperty->IsA(FUInt16Property::StaticClass())) { 
		data = std::vector<uint8_t>(2, 0);
		TypeName = "u16";
	}
	else if (uproperty->IsA(FUInt32Property::StaticClass())) { 
		data = std::vector<uint8_t>(4, 0);
		TypeName = "u32";
	}
	else if (uproperty->IsA(FUInt64Property::StaticClass())) { 
		data = std::vector<uint8_t>(8, 0);
		TypeName = "u64";
	}
	else if (uproperty->IsA(FBoolProperty::StaticClass())){ 
		data = std::vector<uint8_t>(1, 0);
		TypeName = "bool";
	}
	else if (uproperty->IsA(FEnumProperty::StaticClass())) { //todo data
		// how to iterate enums__??
		TypeName = "string";
	}
	else if (uproperty->IsA(FTextProperty::StaticClass())) {
		if (Container)
		{
			FString val = container ? Property->ContainerPtrToValuePtr<FText>(container)->ToString() : Property->ContainerPtrToValuePtr<FText>(StructPtr)->ToString();
			auto s = StringCast<ANSICHAR>(*val);
			data = std::vector<uint8_t>(s.Length() + 1, 0);
			memcpy(data.data(), s.Get(), s.Length());
		}
		else 
		{
			data = std::vector<uint8_t>(1, 0);
		}

		TypeName = "string";
	}
	else if (uproperty->IsA(FNameProperty::StaticClass())) { 
		if (Container)
		{
			FString val = container ? Property->ContainerPtrToValuePtr<FName>(container)->ToString() : Property->ContainerPtrToValuePtr<FName>(StructPtr)->ToString();;
			auto s = StringCast<ANSICHAR>(*val);
			data = std::vector<uint8_t>(s.Length() + 1, 0);
			memcpy(data.data(), s.Get(), s.Length());
		}
		else
		{
			data = std::vector<uint8_t>(1, 0);
		}
		TypeName = "string";
	}
	else if (uproperty->IsA(FObjectProperty::StaticClass()))
	{
		if (((FObjectProperty*)uproperty)->PropertyClass->IsChildOf<UTextureRenderTarget2D>()) // We only support texturetarget2d from object properties
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
	//else if (uproperty->IsA(FArrayProperty::StaticClass())) { //Not supported by mediaz
	//	data = std::vector<uint8_t>(1, 0);
	//	TypeName = "bool";
	//}
	else if (uproperty->IsA(FStructProperty::StaticClass())) {
		FStructProperty* StructProp = Cast<FStructProperty>(uproperty);

		if (StructProp->Struct == TBaseStructure<FVector>::Get() || StructProp->Struct == TBaseStructure<FRotator>::Get()) //todo data
		{
			data = std::vector<uint8_t>(sizeof(FVector), 0);
			TypeName = "mz.fb.vec3d";
		}
		else if (StructProp->Struct == TBaseStructure<FVector4>::Get() || StructProp->Struct == TBaseStructure<FQuat>::Get()) //todo data
		{
			data = std::vector<uint8_t>(sizeof(FVector4), 0);
			TypeName = "mz.fb.vec4d";
		}
		else if (StructProp->Struct == TBaseStructure<FVector2D>::Get()) //todo data
		{
			data = std::vector<uint8_t>(sizeof(FVector2D), 0);
			TypeName = "mz.fb.vec2d";
		}
		else if (StructProp->Struct == TBaseStructure<FTransform>::Get()) //todo everything
		{
			//TODO
		}
		else
		{
			if (Container)
			{
				class FProperty* AProperty = StructProp->Struct->PropertyLink;
				uint8* StructInst = StructProp->ContainerPtrToValuePtr<uint8>(Container);
				while (AProperty != nullptr)
				{
					FName CategoryNamek = FObjectEditorUtils::GetCategoryFName(AProperty);
					UClass* Class = Container->GetClass();

					if (FEditorCategoryUtils::IsCategoryHiddenFromClass(Class, CategoryNamek.ToString()) || !PropertyVisibleExp(AProperty))
					{
						AProperty = AProperty->PropertyLinkNext;
						continue;
					}

					UE_LOG(LogTemp, Warning, TEXT("The property name in struct: %s"), *(AProperty->GetAuthoredName()) );
					MZProperty* mzprop = new MZProperty(nullptr, AProperty, CategoryName + "|" + DisplayName, StructInst, StructProp);
					childProperties.push_back(mzprop);
					for (auto it : mzprop->childProperties)
					{
						childProperties.push_back(it);
					}
					//RegisteredProperties.Add(mzprop->id, mzprop);
					AProperty = AProperty->PropertyLinkNext;
				}
			}

			data = std::vector<uint8_t>(1, 0);
			TypeName = "mz.fb.Void";
		}
	}
	else{
		data = std::vector<uint8_t>(1, 0);
		TypeName = "mz.fb.Void";
	}


	//init data
	if (container && TypeName != "mz.fb.Void" && TypeName != "string" && TypeName != "mz.fb.Texture")
	{
		void* val = Property->ContainerPtrToValuePtr< void >(container); //: Property->ContainerPtrToValuePtr<void>(StructPtr);
		memcpy(data.data(), val, data.size());
	}

	if (structPtr && TypeName != "mz.fb.Void" && TypeName != "string" && TypeName != "mz.fb.Texture")
	{
		void* val = Property->ContainerPtrToValuePtr< void >(structPtr); //: Property->ContainerPtrToValuePtr<void>(StructPtr);
		memcpy(data.data(), val, data.size());
	}

}

void MZProperty::SetValue(void* newval, size_t size, uint8* customContainer) //called from game thread*
{
	if (Container)
	{
		if (TypeName != "mz.fb.Void" && TypeName != "string")
		{
			if (TypeName == "bool")
			{
				bool nval = *(bool*)newval;
				FBoolProperty* boolProperty = Cast<FBoolProperty>(Property);
				if (boolProperty)
				{
					void* val = Property->ContainerPtrToValuePtr< void >(Container);
					boolProperty->SetPropertyValue(val, nval);
				}
			}
			else
			{
				if (Property->ElementSize == size)
				{
					void* val = Property->ContainerPtrToValuePtr< void >(Container);
					memcpy(val, newval, size);
					if (TypeName == "mz.fb.vec3d")
					{
						UE_LOG(LogTemp, Warning, TEXT("The vector value is: %s"), *(*(FVector*)val).ToString());
					}
				}
				else
				{
					UE_LOG(LogMediaZ, Error, TEXT("Property size mismatch with mediaZ"));

				}
			}
		}
		

		if (UActorComponent* Component = Cast<UActorComponent>(Container))
		{
			Component->MarkRenderStateDirty();
			Component->UpdateComponentToWorld();
		}
		
		
	}
	else if(customContainer)
	{
		if (TypeName == "bool")
		{
			bool nval = *(bool*)newval;
			FBoolProperty* boolProperty = Cast<FBoolProperty>(Property);
			if (boolProperty)
			{
				void* val = Property->ContainerPtrToValuePtr< void >(customContainer);
				boolProperty->SetPropertyValue(val, nval);
			}
		}
		else if (TypeName != "mz.fb.Void" && TypeName != "string")
		{
			void* val = Property->ContainerPtrToValuePtr< void >(customContainer);
			memcpy(val, newval, size);
		}
	}
	
}

std::vector<uint8> MZProperty::GetValue(uint8* customContainer)
{
	if (Container)
	{
		if (TypeName != "mz.fb.Void" && TypeName != "string")
		{
			void* val = Property->ContainerPtrToValuePtr< void >(Container);
			auto size = Property->GetSize();
			std::vector<uint8> value(size, 0);
			memcpy(value.data(), val, size);
			return value;
		}

	}
	else if (customContainer)
	{
		if (TypeName != "mz.fb.Void" && TypeName != "string")
		{
			void* val = Property->ContainerPtrToValuePtr< void >(customContainer);
			auto size = Property->GetSize();
			std::vector<uint8> value(size, 0);
			memcpy(value.data(), val, size);
			return value;
		}
	}
	return std::vector<uint8>();
}

flatbuffers::Offset<mz::fb::Pin> MZProperty::Serialize(flatbuffers::FlatBufferBuilder& fbb)
{
	if (TypeName == "mz.fb.Void")
	{
		return mz::fb::CreatePinDirect(fbb, (mz::fb::UUID*)&id, TCHAR_TO_UTF8(*DisplayName), TypeName.c_str(), mz::fb::ShowAs::NONE, mz::fb::CanShowAs::INPUT_OUTPUT_PROPERTY, TCHAR_TO_UTF8(*CategoryName), 0, &data, 0, 0, 0, 0, ReadOnly, IsAdvanced);
	}
	return mz::fb::CreatePinDirect(fbb, (mz::fb::UUID*)&id, TCHAR_TO_UTF8(*DisplayName), TypeName.c_str(),  PinShowAs, mz::fb::CanShowAs::INPUT_OUTPUT_PROPERTY, TCHAR_TO_UTF8(*CategoryName), 0, &data, 0, 0, 0, 0, ReadOnly, IsAdvanced);
}







#endif