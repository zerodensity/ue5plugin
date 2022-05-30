// Copyright Epic Games, Inc. All Rights Reserved.

#include "MZType.h"
#include "Core.h"

#include "Misc/MessageDialog.h"

#include <map>

static std::map<uint64_t, MZType*> GTypeMap;


void MZType::Init(FField* Field)
{
    //switch (*Field->GetClass()->GetFName().ToEName())
    //{

    //}

    if (auto sprop = CastField<FStructProperty>(Field))
    {
        TArray<FField*> fields;
        sprop->GetInnerFields(fields);
        Tag = STRUCT;
        for (auto field : fields)
        {
            StructFields.Add(field->GetName(), GetType(field));
        }

        sprop->GetClass()->GetFName().ToEName();
    }
    else if (auto aprop = CastField<FArrayProperty>(Field))
    {
        Tag = ARRAY;
        ElementCount = aprop->ArrayDim;
        ElementType = GetType(aprop->Inner);
    }
    else if (auto nprop = CastField<FNumericProperty>(Field))
    {
        Tag = (nprop->IsFloatingPoint() ? FLOAT : INT);
        Width = nprop->ElementSize * 8;
        switch (Tag)
        {
            case FLOAT:
                switch (Width)
                {
                case 32: Name = "float";
                case 64: Name = "double";
                }break;
            case INT:
                switch (Width)
                {
                case 8:  Name = "int8";
                case 16: Name = "int16";
                case 32: Name = "int32";
                case 64: Name = "int64";
                }break;
        }
    }
    else if (CastField<FBoolProperty>(Field))
    {
        Tag = BOOL;
        Width = 1;
        Name = "bool";
    }
    else if (CastField<FStrProperty>(Field))
    {
        Tag = STRING;
        Name = "string";
    }
}

MZType* MZType::GetType(FField* Field)
{
    MZType*& ty = GTypeMap[Field->GetClass()->GetId()];

    if (!ty)
    {
        ty = new MZType();
        ty->Init(Field);
    }

    return ty;
}

