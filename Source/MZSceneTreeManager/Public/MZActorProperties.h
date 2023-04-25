#pragma once
#include "CoreMinimal.h"
#include <vector>
#pragma warning (disable : 4800)
#pragma warning (disable : 4668)
#include "AppEvents_generated.h"
#include "RealityTrack.h"
#include "MZClient.h"

class MZStructProperty;
class MZProperty;

class MZSCENETREEMANAGER_API MZObjectReference
{
public:
	MZObjectReference(TObjectPtr<UObject> Object);
	MZObjectReference(){};
	virtual ~MZObjectReference()
	{
		
	}

	void AddProperty(FName PropertyName, TSharedPtr<MZProperty>MzProperty);
	
	TMap<FName, TSharedPtr<MZProperty>> PropertiesMap;

	UObject *GetAsObject() const;
	virtual bool UpdateObjectPointer(UObject *Object);

protected:
	UPROPERTY()
	TWeakObjectPtr<UObject> ObjectRef = nullptr;
};
class MZSCENETREEMANAGER_API MZActorReference : public MZObjectReference
{
public:
	MZActorReference(TObjectPtr<AActor> actor);
	MZActorReference();
	
	AActor* Get();
	UClass* GetActorClass() const;
	
	
	explicit operator bool() {
		return !!(Get());
	}

	AActor* operator->()
	{
		return Get();
	}

	bool UpdateClass(UClass *NewActorClass);
	void UpdateActorReference(AActor *NewActor);
	bool UpdateActorPointer(const UWorld* World);
	bool UpdateActualActorPointer();

	bool InvalidReference = false;
private:
	
	UPROPERTY()
	//TWeakObjectPtr<AActor> Actor;
	
	FGuid ActorGuid;
	UClass *ActorClass; // This hold UClass reference when Actor is assigned. All properties are collected
						// relative to this class and UE internals doesn't inform when this class is reinstanced!
};

class MZSCENETREEMANAGER_API MZComponentReference : public MZObjectReference
{

public:
	MZComponentReference(TObjectPtr<UActorComponent> actorComponent);
	MZComponentReference();

	UActorComponent* Get();
	AActor* GetOwnerActor();

	explicit operator bool() {
		return !!(Get());
	}

	UActorComponent* operator->()
	{
		return Get();
	}

	bool UpdateActualComponentPointer();
	bool InvalidReference = false;

private:
	UPROPERTY()
	//TWeakObjectPtr<UActorComponent> Component;
	
	FName ComponentProperty;
	FString PathToComponent;

};

class MZSCENETREEMANAGER_API MZProperty : public TSharedFromThis<MZProperty>
{
public:
	MZProperty(MZObjectReference* ObjectReference, FProperty* UProperty, FString ParentCategory = FString(), uint8 * StructPtr = nullptr, const MZStructProperty* parentProperty = nullptr);

	virtual void SetPropValue(void* val, size_t size, uint8* customContainer = nullptr);
	UObject* GetRawObjectContainer();
	void* GetRawContainer();
	void UpdatePropertyReference(FProperty *NewProperty);
	void RemovePortal();

	virtual std::vector<uint8> UpdatePinValue(uint8* customContainer = nullptr);
	//std::vector<uint8> GetValue(uint8* customContainer = nullptr);
	void MarkState();
	virtual flatbuffers::Offset<mz::fb::Pin> Serialize(flatbuffers::FlatBufferBuilder& fbb);
	std::vector<flatbuffers::Offset<mz::fb::MetaDataEntry>> SerializeMetaData(flatbuffers::FlatBufferBuilder& fbb);
	virtual flatbuffers::Offset<mz::fb::Visualizer> SerializeVisualizer(flatbuffers::FlatBufferBuilder& fbb) {return 0;};
	
	FProperty* Property;

	MZActorReference* ActorContainer = nullptr;
	MZComponentReference* ComponentContainer = nullptr;
	TSharedPtr<struct MZPortal> Portal = nullptr;
	UObject* ObjectPtr = nullptr;
	uint8* StructPtr = nullptr;

	FName PropertyNameAsReference;
	FString PropertyName;
	FString DisplayName;
	FString CategoryName;
	FString UIMaxString;
	FString UIMinString;
	bool IsAdvanced = false;
	bool ReadOnly = false;
	std::string TypeName;
	FGuid Id;
	std::vector<uint8_t> data; //wrt mediaZ standarts
	std::vector<uint8_t> default_val; //wrt mediaZ standarts
	std::vector<uint8_t> min_val; //wrt mediaZ standarts
	std::vector<uint8_t> max_val; //wrt mediaZ standarts
	mz::fb::ShowAs PinShowAs = mz::fb::ShowAs::PROPERTY;
	std::vector<TSharedPtr<MZProperty>> childProperties;
	TMap<FString, FString> mzMetaDataMap;
	bool transient = true;
	bool IsChanged = false;

	virtual ~MZProperty() {}
protected:
	virtual void SetProperty_InCont(void* container, void* val);

};

template<typename T, mz::tmp::StrLiteral LitType, typename CppType = T::TCppType>
requires std::is_base_of_v<FProperty, T>
class MZNumericProperty : public MZProperty {
public:
	MZNumericProperty(MZObjectReference *ObjectReference, T* uproperty, FString parentCategory = FString(), uint8* StructPtr = nullptr, MZStructProperty* parentProperty = nullptr) :
		MZProperty(ObjectReference, uproperty, parentCategory, StructPtr, parentProperty)
	{
		data = std::vector<uint8_t>(sizeof(CppType), 0);
		TypeName = LitType.val;
	}
	T* GetProperty()
	{
		return static_cast<T*>(Property);
	}
	
	virtual std::vector<uint8> UpdatePinValue(uint8* customContainer = nullptr) override 
	{
		void* container = nullptr;
		if (customContainer) container = customContainer;
		else if (ComponentContainer) container = ComponentContainer->Get();
		else if (ActorContainer) container = ActorContainer->Get();
		else if (ObjectPtr && IsValid(ObjectPtr)) container = ObjectPtr;
		else if (StructPtr) container = StructPtr;

		if (container)
		{
			CppType val = static_cast<CppType>(GetProperty()->GetPropertyValue_InContainer(container));
			memcpy(data.data(), &val, data.size());
		}

		return data;
	}

protected:
	virtual void SetProperty_InCont(void* container, void* val) override 
	{
		GetProperty()->SetPropertyValue_InContainer(container, (*(CppType*)val));
	}
};

using MZBoolProperty = MZNumericProperty<FBoolProperty, "bool">;
using MZFloatProperty = MZNumericProperty<FFloatProperty, "float">;
using MZDoubleProperty = MZNumericProperty<FDoubleProperty, "double">;
using MZInt8Property = MZNumericProperty<FInt8Property, "byte">;
using MZInt16Property = MZNumericProperty<FInt16Property, "short">;
using MZIntProperty = MZNumericProperty<FIntProperty, "int">;
using MZInt64Property = MZNumericProperty<FInt64Property, "long">;
using MZByteProperty = MZNumericProperty<FByteProperty, "ubyte">;
using MZUInt16Property = MZNumericProperty<FUInt16Property, "ushort">;
using MZUInt32Property = MZNumericProperty<FUInt32Property, "uint">;
using MZUInt64Property = MZNumericProperty<FUInt64Property, "ulong">;

class MZEnumProperty : public MZProperty
{
public:
	MZEnumProperty(MZObjectReference* ObjectReference, FProperty* OrgProperty, FString parentCategory = FString(), uint8* StructPtr = nullptr, MZStructProperty* parentProperty = nullptr)
		: MZProperty(ObjectReference, OrgProperty, parentCategory, StructPtr, parentProperty)
	{
		UObject *container = nullptr;

		if(ObjectReference && ObjectReference->GetAsObject())
		{
			container = ObjectReference->GetAsObject();
		}

		const UEnum* Enum = GetEnum();
		
		data = std::vector<uint8_t>(1, 0); 
		TypeName = "string";

		int EnumSize = Enum->NumEnums();
		for(int i = 0; i < EnumSize; i++)
		{
			NameMap.Add(Enum->GetNameByIndex(i).ToString(), Enum->GetValueByIndex(i));
		}

		
		flatbuffers::FlatBufferBuilder mb;
		std::vector<mz::fb::String256> NameList;
		for (auto [name, _]: NameMap)
		{
			mz::fb::String256 str256;
			auto val = str256.mutable_val();
			auto size = name.Len() < 256 ? name.Len() : 256;
			memcpy(val->data(), TCHAR_TO_UTF8(*name), size);
			NameList.push_back(str256);
		}
		mz::fb::String256 listName;
		strcat((char*)listName.mutable_val()->data(), TCHAR_TO_UTF8(*Enum->GetFName().ToString()));
		auto offset = mz::app::CreateUpdateStringList(mb, mz::fb::CreateString256ListDirect(mb, &listName, &NameList));
		mb.Finish(offset);
		auto buf = mb.Release();
		auto root = flatbuffers::GetRoot<mz::app::UpdateStringList>(buf.data());
		auto MZClient = &FModuleManager::LoadModuleChecked<FMZClient>("MZClient");
		MZClient->AppServiceClient->UpdateStringList(*root);
	}

	UEnum* GetEnum() const
	{
		UEnum* Enum = nullptr;
		if(const FEnumProperty* EnumProp = GetEnumProperty())
		{
			if(const FNumericProperty* NumericProp = EnumProp->GetUnderlyingProperty())
			{
				Enum = EnumProp->GetEnum();
			}
		}else if(const FNumericProperty* NumericProperty = GetNumericProperty())
		{
			Enum = NumericProperty->GetIntPropertyEnum();
		}

		return Enum;
	}

	FEnumProperty* GetEnumProperty() const
	{
		return CastField<FEnumProperty>(Property);
	}

	/**
	 * @brief This NumericProperty depends on the type of originating FProperty, so instead of holding a reference, we get
	 * it from originating property
	 * @return 
	 */
	FNumericProperty* GetNumericProperty() const
	{
		FNumericProperty *Ret = nullptr;
		if(FEnumProperty* EnumProperty = CastField<FEnumProperty>(Property))
		{
			Ret =  EnumProperty->GetUnderlyingProperty();
		}else if(CastField<FNumericProperty>(Property) && CastField<FNumericProperty>(Property)->IsEnum())
		{
			Ret = CastField<FNumericProperty>(Property);
		}
		return Ret;
	}

	FString MediaZListName;
	TMap<FString, int64> NameMap;
	FString CurrentName;
	int64 CurrentValue;
	
	virtual flatbuffers::Offset<mz::fb::Pin> Serialize(flatbuffers::FlatBufferBuilder& fbb) override;
	virtual flatbuffers::Offset<mz::fb::Visualizer> SerializeVisualizer(flatbuffers::FlatBufferBuilder& fbb) override;
	virtual void SetPropValue(void* val, size_t size, uint8* customContainer = nullptr) override;
	virtual std::vector<uint8> UpdatePinValue(uint8* customContainer = nullptr) override; 

};

class MZTextProperty : public MZProperty
{
public:
	MZTextProperty(MZObjectReference* ObjectReference, FTextProperty* uproperty, FString parentCategory = FString(), uint8* StructPtr = nullptr, MZStructProperty* parentProperty = nullptr)
		: MZProperty(ObjectReference, uproperty, parentCategory, StructPtr, parentProperty)
	{
		data = std::vector<uint8_t>(1, 0);
		TypeName = "string";
	}

	FTextProperty* GetTextProperty() const;
	virtual void SetPropValue(void* val, size_t size, uint8* customContainer = nullptr) override;
	virtual std::vector<uint8> UpdatePinValue(uint8* customContainer = nullptr) override;

};

class MZNameProperty : public MZProperty
{
public:
	MZNameProperty(MZObjectReference* ObjectReference, FNameProperty* uproperty, FString parentCategory = FString(), uint8* StructPtr = nullptr, MZStructProperty* parentProperty = nullptr)
		: MZProperty(ObjectReference, uproperty, parentCategory, StructPtr, parentProperty) 
	{
		data = std::vector<uint8_t>(1, 0);
		TypeName = "string";
	}

	FNameProperty* GetNameProperty() const;
	virtual void SetPropValue(void* val, size_t size, uint8* customContainer = nullptr) override;
	virtual std::vector<uint8> UpdatePinValue(uint8* customContainer = nullptr) override;

};

class MZStringProperty : public MZProperty
{
public:
	MZStringProperty(MZObjectReference* ObjectReference, FStrProperty* uproperty, FString parentCategory = FString(), uint8* StructPtr = nullptr, MZStructProperty* parentProperty = nullptr)
		: MZProperty(ObjectReference, uproperty, parentCategory, StructPtr, parentProperty)
	{
		data = std::vector<uint8_t>(1, 0);
		TypeName = "string";
	}

	FStrProperty* GetStringProperty() const;
	virtual void SetPropValue(void* val, size_t size, uint8* customContainer = nullptr) override;
	virtual std::vector<uint8> UpdatePinValue(uint8* customContainer = nullptr) override;

};

class MZObjectProperty : public MZProperty
{
public:
	MZObjectProperty(MZObjectReference* ObjectReference, FObjectProperty* uproperty, FString parentCategory = FString(), uint8* StructPtr = nullptr, MZStructProperty* parentProperty = nullptr);
	

	FObjectProperty* objectprop;
	virtual void SetPropValue(void* val, size_t size, uint8* customContainer = nullptr) override;
	virtual std::vector<uint8> UpdatePinValue(uint8* customContainer = nullptr) override { return std::vector<uint8>(); }

};

class MZStructProperty : public MZProperty
{
public:
	MZStructProperty(MZObjectReference* ObjectReference, FStructProperty* uproperty, FString parentCategory = FString(), uint8* StructPtr = nullptr, MZStructProperty* parentProperty = nullptr);

	FStructProperty* GetStructProperty() const;
	virtual void SetPropValue(void* val, size_t size, uint8* customContainer = nullptr) override;
	virtual std::vector<uint8> UpdatePinValue(uint8* customContainer = nullptr) override { return std::vector<uint8>(); }

};

template<typename T, mz::tmp::StrLiteral LitType>
class MZCustomStructProperty : public MZProperty 
{
public:
	MZCustomStructProperty(MZObjectReference* ObjectReference, FStructProperty* uproperty, FString parentCategory = FString(), uint8* StructPtr = nullptr, MZStructProperty* parentProperty = nullptr)
		: MZProperty(ObjectReference, uproperty, parentCategory, StructPtr, parentProperty)
	{
		data = std::vector<uint8_t>(sizeof(T), 0);
		TypeName = LitType.val;
	}

	FStructProperty* GetStructProperty() const
	{
		return static_cast<FStructProperty*>(Property);
	}
protected:
	
	virtual void SetProperty_InCont(void* container, void* val) override 
	{
		GetStructProperty()->CopyCompleteValue(GetStructProperty()->ContainerPtrToValuePtr<void>(container), (T*)val);
	}
};

using MZVec2Property = MZCustomStructProperty<FVector2D, "mz.fb.vec2d">;
using MZVec3Property = MZCustomStructProperty<FVector, "mz.fb.vec3d">;
using MZVec4Property = MZCustomStructProperty < FVector4, "mz.fb.vec4d">;

class MZRotatorProperty : public MZProperty
{
public:
	MZRotatorProperty(MZObjectReference* ObjectReference, FStructProperty* uproperty, FString parentCategory = FString(), uint8* StructPtr = nullptr, MZStructProperty* parentProperty = nullptr)
		: MZProperty(ObjectReference, uproperty, parentCategory, StructPtr, parentProperty)
	{
		data = std::vector<uint8_t>(sizeof(FVector), 0);
		TypeName = "mz.fb.vec3d";
	}
	virtual std::vector<uint8> UpdatePinValue(uint8* customContainer = nullptr) override;

protected:
	FStructProperty* GetStructProperty() const;
	virtual void SetProperty_InCont(void* container, void* val) override;
};



class MZTrackProperty : public MZProperty
{
public:
	MZTrackProperty(MZObjectReference* ObjectReference, FStructProperty* uproperty, FString parentCategory = FString(), uint8* StructPtr = nullptr, MZStructProperty* parentProperty = nullptr)
		: MZProperty(ObjectReference, uproperty, parentCategory, StructPtr, parentProperty)
	{
		
		data = std::vector<uint8_t>(1, 0);
		TypeName = "mz.fb.Track";
	}
	virtual std::vector<uint8> UpdatePinValue(uint8* customContainer = nullptr) override;

	//virtual flatbuffers::Offset<mz::fb::Pin> Serialize(flatbuffers::FlatBufferBuilder& fbb) override;
	virtual void SetPropValue(void* val, size_t size, uint8* customContainer = nullptr) override;

protected:
	FStructProperty* GetStructProperty() const;
	virtual void SetProperty_InCont(void* container, void* val) override;
};

class  MZSCENETREEMANAGER_API  MZPropertyFactory
{
public:
	static TSharedPtr<MZProperty> CreateProperty(MZObjectReference* ObjectReference, 
		FProperty* UProperty, 
		TMap<FGuid, TSharedPtr<MZProperty>>* RegisteredProperties = nullptr,
		TMap<FProperty*, TSharedPtr<MZProperty>>* PropertiesMap = nullptr,
		FString ParentCategory = FString(), 
		uint8* StructPtr = nullptr, 
		MZStructProperty* ParentProperty = nullptr);

	static TFunction<void(MZObjectReference* ObjectReference, TSharedPtr<MZProperty> MzProperty)> OnPropertyCreatedCallback;
};
