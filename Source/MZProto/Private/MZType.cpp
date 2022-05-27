// Copyright Epic Games, Inc. All Rights Reserved.

#include "MZType.h"
#include "Core.h"

#include "Misc/MessageDialog.h"

#include <map>

static std::map<uint64_t, MZType*> GTypeMap;

void MZType::Init(FField* Field)
{
    if (auto sprop = CastField<FStructProperty>(Field))
    {
        TArray<FField*> fields;
        sprop->GetInnerFields(fields);
        Tag = STRUCT;
        for (auto field : fields)
        {
            StructFields.Add(field->GetName(), GetType(field));
        }
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
    }
    else if (CastField<FBoolProperty>(Field))
    {
        Tag = BOOL;
        Width = 1;
    }
    else if (CastField<FStrProperty>(Field))
    {
        Tag = STRING;
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

