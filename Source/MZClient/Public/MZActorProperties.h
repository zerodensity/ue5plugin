#pragma once
#if WITH_EDITOR
#include "CoreMinimal.h"
#include <vector>
#pragma warning (disable : 4800)
#pragma warning (disable : 4668)
#include "AppClient.h"

class MZStructProperty;

class MZProperty {
public:
	MZProperty(UObject* container, FProperty* uproperty, FString parentCategory = FString(), uint8* StructPtr = nullptr, MZStructProperty* parentProperty = nullptr);

	virtual void SetPropValue(void* val, size_t size, uint8* customContainer = nullptr);


	virtual std::vector<uint8> UpdatePinValue(uint8* customContainer = nullptr);
	//std::vector<uint8> GetValue(uint8* customContainer = nullptr);
	void MarkState();
	virtual flatbuffers::Offset<mz::fb::Pin> Serialize(flatbuffers::FlatBufferBuilder& fbb);
	std::vector<flatbuffers::Offset<mz::fb::MetaDataEntry>> SerializeMetaData(flatbuffers::FlatBufferBuilder& fbb);

	FProperty* Property;
	UObject* Container;
	uint8* StructPtr = nullptr;

	FString PropertyName;
	FString DisplayName;
	FString CategoryName;
	FString UIMaxString;
	FString UIMinString;
	bool IsAdvanced = false;
	bool ReadOnly = false;
	std::string TypeName;
	FGuid id;
	std::vector<uint8_t> data; //wrt mediaZ standarts
	std::vector<uint8_t> default_val; //wrt mediaZ standarts
	std::vector<uint8_t> min_val; //wrt mediaZ standarts
	std::vector<uint8_t> max_val; //wrt mediaZ standarts
	mz::fb::ShowAs PinShowAs = mz::fb::ShowAs::PROPERTY;
	std::vector<MZProperty*> childProperties;
	TMap<FString, FString> mzMetaDataMap;
	bool transient = true;

protected:
	virtual void SetProperty_InCont(void* container, void* val);
};

class MZBoolProperty : public MZProperty
{
public:
	MZBoolProperty(UObject* container, FBoolProperty* uproperty, FString parentCategory = FString(), uint8* StructPtr = nullptr, MZStructProperty* parentProperty = nullptr)
		: MZProperty(container, uproperty, parentCategory, StructPtr, parentProperty), boolprop(uproperty) 
	{
		data = std::vector<uint8_t>(1, 0);
		TypeName = "bool";
	}

	FBoolProperty* boolprop;

protected:
	virtual void SetProperty_InCont(void* container, void* val) override;
};

class MZFloatProperty : public MZProperty
{
public:
	MZFloatProperty(UObject* container, FFloatProperty* uproperty, FString parentCategory = FString(), uint8* StructPtr = nullptr, MZStructProperty* parentProperty = nullptr)
		: MZProperty(container, uproperty, parentCategory, StructPtr, parentProperty), floatprop(uproperty) 
	{
		data = std::vector<uint8_t>(4, 0);
		TypeName = "f32";
	}
	FFloatProperty* floatprop;

protected:
	virtual void SetProperty_InCont(void* container, void* val) override;
};

class MZDoubleProperty : public MZProperty
{
public:
	MZDoubleProperty(UObject* container, FDoubleProperty* uproperty, FString parentCategory = FString(), uint8* StructPtr = nullptr, MZStructProperty* parentProperty = nullptr)
		: MZProperty(container, uproperty, parentCategory, StructPtr, parentProperty), doubleprop(uproperty) 
	{
		data = std::vector<uint8_t>(8, 0);
		TypeName = "f64";
	}
	FDoubleProperty* doubleprop;

protected:
	virtual void SetProperty_InCont(void* container, void* val) override;
};

class MZInt8Property : public MZProperty
{
public:
	MZInt8Property(UObject* container, FInt8Property* uproperty, FString parentCategory = FString(), uint8* StructPtr = nullptr, MZStructProperty* parentProperty = nullptr)
		: MZProperty(container, uproperty, parentCategory, StructPtr, parentProperty), int8prop(uproperty) 
	{
		data = std::vector<uint8_t>(1, 0);
		TypeName = "i8";
	}

	FInt8Property* int8prop;

protected:
	virtual void SetProperty_InCont(void* container, void* val) override;
};

class MZInt16Property : public MZProperty
{
public:
	MZInt16Property(UObject* container, FInt16Property* uproperty, FString parentCategory = FString(), uint8* StructPtr = nullptr, MZStructProperty* parentProperty = nullptr)
		: MZProperty(container, uproperty, parentCategory, StructPtr, parentProperty), int16prop(uproperty) 
	{
		data = std::vector<uint8_t>(2, 0);
		TypeName = "i16";
	}

	FInt16Property* int16prop;

protected:
	virtual void SetProperty_InCont(void* container, void* val) override;
};

class MZIntProperty : public MZProperty
{
public:
	MZIntProperty(UObject* container, FIntProperty* uproperty, FString parentCategory = FString(), uint8* StructPtr = nullptr, MZStructProperty* parentProperty = nullptr)
		: MZProperty(container, uproperty, parentCategory, StructPtr, parentProperty), intprop(uproperty) 
	{
		data = std::vector<uint8_t>(4, 0);
		TypeName = "i32";
	}

	FIntProperty* intprop;

protected:
	virtual void SetProperty_InCont(void* container, void* val) override;
};

class MZInt64Property : public MZProperty
{
public:
	MZInt64Property(UObject* container, FInt64Property* uproperty, FString parentCategory = FString(), uint8* StructPtr = nullptr, MZStructProperty* parentProperty = nullptr)
		: MZProperty(container, uproperty, parentCategory, StructPtr, parentProperty), int64prop(uproperty) 
	{
		data = std::vector<uint8_t>(8, 0);
		TypeName = "i64";
	}

	FInt64Property* int64prop;

protected:
	virtual void SetProperty_InCont(void* container, void* val) override;
};

class MZByteProperty : public MZProperty
{
public:
	MZByteProperty(UObject* container, FByteProperty* uproperty, FString parentCategory = FString(), uint8* StructPtr = nullptr, MZStructProperty* parentProperty = nullptr)
		: MZProperty(container, uproperty, parentCategory, StructPtr, parentProperty), byteprop(uproperty) 
	{
		data = std::vector<uint8_t>(1, 0);
		TypeName = "u8";
	}

	FByteProperty* byteprop;

protected:
	virtual void SetProperty_InCont(void* container, void* val) override;
};

class MZUInt16Property : public MZProperty
{
public:
	MZUInt16Property(UObject* container, FUInt16Property* uproperty, FString parentCategory = FString(), uint8* StructPtr = nullptr, MZStructProperty* parentProperty = nullptr)
		: MZProperty(container, uproperty, parentCategory, StructPtr, parentProperty), uint16prop(uproperty) 
	{
		data = std::vector<uint8_t>(2, 0);
		TypeName = "u16";
	}

	FUInt16Property* uint16prop;

protected:
	virtual void SetProperty_InCont(void* container, void* val) override;
};

class MZUInt32Property : public MZProperty
{
public:
	MZUInt32Property(UObject* container, FUInt32Property* uproperty, FString parentCategory = FString(), uint8* StructPtr = nullptr, MZStructProperty* parentProperty = nullptr)
		: MZProperty(container, uproperty, parentCategory, StructPtr, parentProperty), uint32prop(uproperty) 
	{
		data = std::vector<uint8_t>(4, 0);
		TypeName = "u32";
	}

	FUInt32Property* uint32prop;

protected:
	virtual void SetProperty_InCont(void* container, void* val) override;
};

class MZUInt64Property : public MZProperty
{
public:
	MZUInt64Property(UObject* container, FUInt64Property* uproperty, FString parentCategory = FString(), uint8* StructPtr = nullptr, MZStructProperty* parentProperty = nullptr)
		: MZProperty(container, uproperty, parentCategory, StructPtr, parentProperty), uint64prop(uproperty) 
	{
		data = std::vector<uint8_t>(8, 0);
		TypeName = "u64";
	}

	FUInt64Property* uint64prop;

protected:
	virtual void SetProperty_InCont(void* container, void* val) override;
};

class MZEnumProperty : public MZProperty
{
public:
	MZEnumProperty(UObject* container, FEnumProperty* uproperty, FString parentCategory = FString(), uint8* StructPtr = nullptr, MZStructProperty* parentProperty = nullptr)
		: MZProperty(container, uproperty, parentCategory, StructPtr, parentProperty), enumprop(uproperty) 
	{
		data = std::vector<uint8_t>(1, 0); //TODO
		TypeName = "mz.fb.Void";
	}

	FEnumProperty* enumprop;
	virtual void SetPropValue(void* val, size_t size, uint8* customContainer = nullptr) override;
	virtual std::vector<uint8> UpdatePinValue(uint8* customContainer = nullptr) override { return std::vector<uint8>(); }

};

class MZTextProperty : public MZProperty
{
public:
	MZTextProperty(UObject* container, FTextProperty* uproperty, FString parentCategory = FString(), uint8* StructPtr = nullptr, MZStructProperty* parentProperty = nullptr)
		: MZProperty(container, uproperty, parentCategory, StructPtr, parentProperty), textprop(uproperty) 
	{
		data = std::vector<uint8_t>(1, 0);
		TypeName = "string";
	}

	FTextProperty* textprop;
	virtual void SetPropValue(void* val, size_t size, uint8* customContainer = nullptr) override;
	virtual std::vector<uint8> UpdatePinValue(uint8* customContainer = nullptr) override;

};

class MZNameProperty : public MZProperty
{
public:
	MZNameProperty(UObject* container, FNameProperty* uproperty, FString parentCategory = FString(), uint8* StructPtr = nullptr, MZStructProperty* parentProperty = nullptr)
		: MZProperty(container, uproperty, parentCategory, StructPtr, parentProperty), nameprop(uproperty) 
	{
		data = std::vector<uint8_t>(1, 0);
		TypeName = "string";
	}

	FNameProperty* nameprop;
	virtual void SetPropValue(void* val, size_t size, uint8* customContainer = nullptr) override;
	virtual std::vector<uint8> UpdatePinValue(uint8* customContainer = nullptr) override;

};

class MZObjectProperty : public MZProperty
{
public:
	MZObjectProperty(UObject* container, FObjectProperty* uproperty, FString parentCategory = FString(), uint8* StructPtr = nullptr, MZStructProperty* parentProperty = nullptr);
	

	FObjectProperty* objectprop;
	virtual void SetPropValue(void* val, size_t size, uint8* customContainer = nullptr) override;
	virtual std::vector<uint8> UpdatePinValue(uint8* customContainer = nullptr) override { return std::vector<uint8>(); }

};

class MZStructProperty : public MZProperty
{
public:
	MZStructProperty(UObject* container, FStructProperty* uproperty, FString parentCategory = FString(), uint8* StructPtr = nullptr, MZStructProperty* parentProperty = nullptr);

	FStructProperty* structprop;
	virtual void SetPropValue(void* val, size_t size, uint8* customContainer = nullptr) override;
	virtual std::vector<uint8> UpdatePinValue(uint8* customContainer = nullptr) override { return std::vector<uint8>(); }
};

class MZVec2Property : public MZProperty
{
public:
	MZVec2Property(UObject* container, FStructProperty* uproperty, FString parentCategory = FString(), uint8* StructPtr = nullptr, MZStructProperty* parentProperty = nullptr)
		: MZProperty(container, uproperty, parentCategory, StructPtr, parentProperty), structprop(uproperty)
	{
		data = std::vector<uint8_t>(sizeof(FVector2D), 0);
		TypeName = "mz.fb.vec2d";
	}

	FStructProperty* structprop;
protected:
	virtual void SetProperty_InCont(void* container, void* val) override;
};

class MZVec3Property : public MZProperty
{
public:
	MZVec3Property(UObject* container, FStructProperty* uproperty, FString parentCategory = FString(), uint8* StructPtr = nullptr, MZStructProperty* parentProperty = nullptr)
		: MZProperty(container, uproperty, parentCategory, StructPtr, parentProperty), structprop(uproperty)
	{
		data = std::vector<uint8_t>(sizeof(FVector), 0);
		TypeName = "mz.fb.vec3d";
	}

	FStructProperty* structprop;
protected:
	virtual void SetProperty_InCont(void* container, void* val) override;
};

class MZVec4Property : public MZProperty
{
public:
	MZVec4Property(UObject* container, FStructProperty* uproperty, FString parentCategory = FString(), uint8* StructPtr = nullptr, MZStructProperty* parentProperty = nullptr)
		: MZProperty(container, uproperty, parentCategory, StructPtr, parentProperty), structprop(uproperty)
	{
		data = std::vector<uint8_t>(sizeof(FVector4), 0);
		TypeName = "mz.fb.vec4d";
	}

	FStructProperty* structprop; 
protected:
		virtual void SetProperty_InCont(void* container, void* val) override;
};

class MZTrackProperty : public MZProperty
{
public:
	MZTrackProperty(UObject* container, FStructProperty* uproperty, FString parentCategory = FString(), uint8* StructPtr = nullptr, MZStructProperty* parentProperty = nullptr)
		: MZProperty(container, uproperty, parentCategory, StructPtr, parentProperty), structprop(uproperty)
	{
		data = std::vector<uint8_t>(sizeof(mz::fb::Track), 0);
		TypeName = "mz.fb.Track";
	}

	FStructProperty* structprop;
protected:
	virtual void SetProperty_InCont(void* container, void* val) override;
};

class MZPropertyFactory
{
public:
	static MZProperty* CreateProperty(UObject* container, FProperty* uproperty, TMap<FGuid, MZProperty*>* registeredProperties = nullptr, FString parentCategory = FString(), uint8* StructPtr = nullptr, MZStructProperty* parentProperty = nullptr);
};



#endif