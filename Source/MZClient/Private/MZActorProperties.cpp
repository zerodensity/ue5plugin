#include "MZActorProperties.h"
#include "UObject/UnrealType.h"
#include "UObject/UnrealTypePrivate.h"

MZProperty::MZProperty(UObject* container, FProperty* uproperty, uint8* structPtr)
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
		CategoryName = metaData.Contains(NAME_Category) ? metaData[NAME_Category] : "Default";
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
	CategoryName = "Default";
	UIMinString = "";
	UIMaxString = "";
	IsAdvanced = false;
#endif
	

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
			data = std::vector<uint8_t>(1, 0);
			TypeName = "mz.fb.Void";
		}
	}
	else{
		data = std::vector<uint8_t>(1, 0);
		TypeName = "mz.fb.Void";
	}

	if (container && TypeName != "mz.fb.Void" && TypeName != "string")
	{
		void* val = Property->ContainerPtrToValuePtr< void >(container); //: Property->ContainerPtrToValuePtr<void>(StructPtr);
		memcpy(data.data(), val, data.size());
	}

}

void MZProperty::SetValue(void* newval, size_t size, uint8* customContainer) //called from game thread*
{
	if (Container)
	{
		if (TypeName != "mz.fb.Void" && TypeName != "string")
		{
			void* val = Property->ContainerPtrToValuePtr< void >(Container);
			memcpy(val, newval, size);
		}

		if (UActorComponent* Component = Cast<UActorComponent>(Container))
		{
			Component->MarkRenderStateDirty();
			Component->UpdateComponentToWorld();
		}
	}
	else if(customContainer)
	{
		if (TypeName != "mz.fb.Void" && TypeName != "string")
		{
			void* val = Property->ContainerPtrToValuePtr< void >(customContainer);
			memcpy(val, newval, size);
		}
	}
}

flatbuffers::Offset<mz::fb::Pin> MZProperty::Serialize(flatbuffers::FlatBufferBuilder& fbb)
{
	return mz::fb::CreatePinDirect(fbb, (mz::fb::UUID*)&id, TCHAR_TO_UTF8(*DisplayName), TypeName.c_str(),  PinShowAs, mz::fb::CanShowAs::INPUT_OUTPUT_PROPERTY, TCHAR_TO_UTF8(*CategoryName), 0, &data, 0, 0, 0, 0, ReadOnly, IsAdvanced);
}
