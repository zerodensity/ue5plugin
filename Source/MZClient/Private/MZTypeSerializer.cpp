// Copyright Epic Games, Inc. All Rights Reserved.


#include "MZClient.h"
#include "MZType.h"
#include "IRemoteControlPropertyHandle.h"
#include "RemoteControlPreset.h"


#include "Engine/TextureRenderTarget2D.h"

#include "D3D12RHIPrivate.h"
#include "D3D12RHI.h"
#include "D3D12Resources.h"

#include "DispelUnrealMadnessPrelude.h"
#include <d3d12.h>

#include "google/protobuf/message.h"

#include <Arena.h>
#include "AppClient.h"

#include "DispelUnrealMadnessPostlude.h"

#include "mediaz.h"

#undef INT
#undef FLOAT

DEFINE_LOG_CATEGORY(LogMZProto);


void ClientImpl::OnNodeUpdate(mz::proto::Node const& archive)
{
	id = archive.id();
	mz::proto::msg<mz::proto::Texture> tex;
	FMZClient* fmz = (FMZClient*)IMZClient::Get();
	for (auto& pin : archive.pins())
	{
		FGuid out;
		if (FGuid::Parse(pin.id().c_str(), out))
		{
			if (auto ppRes = fmz->CopyQueue.Find(out))
			{
				if (mz::app::ParseFromString(tex.m_Ptr, pin.dynamic().data().c_str()))
				{
					ID3D12Resource* res = *ppRes;
					fmz->CopyQueue.Remove(out);
					fmz->CmdList->Reset(fmz->CmdAlloc, 0);
					mzCopyD3D12Resource(res, fmz->CmdList, fmz->CmdQueue, tex->pid(), tex->memory(), tex->sync());
				}
			}
		}
	}
}

void FMZClient::InitRHI()
{
	Dev = (ID3D12Device*)GDynamicRHI->RHIGetNativeDevice();
	HRESULT re = Dev->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&CmdAlloc));

	D3D12_COMMAND_QUEUE_DESC queueDesc = {
		.Type = D3D12_COMMAND_LIST_TYPE_DIRECT,
		.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL,
		.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE,
	};

	re = Dev->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&CmdQueue));
	Dev->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, CmdAlloc, 0, IID_PPV_ARGS(&CmdList));
	CmdList->Close();
}

void FMZClient::OnTextureReceived(FGuid id, mz::proto::Texture const& texture)
{
	if (auto ppRes = CopyQueue.Find(id))
	{
		ID3D12Resource* res = *ppRes;
		CopyQueue.Remove(id);
		CmdList->Reset(CmdAlloc, 0);
		mzCopyD3D12Resource(res, CmdList, CmdQueue, texture.pid(), texture.memory(), texture.sync());
	}
}

void FMZClient::QueueTextureCopy(FGuid id, ID3D12Resource* res, mz::proto::Dynamic* dyn)
{
	MzTextureInfo info = {};
	if (MZ_RESULT_SUCCESS == mzD3D12GetTextureInfo(res, &info))
	{
		CopyQueue.Add(id, res);

		mz::proto::msg<mz::proto::Texture> tex;
		tex->set_width(info.width);
		tex->set_height(info.height);
		tex->set_format(info.format);
		tex->set_usage(info.usage | MZ_IMAGE_USAGE_SAMPLED);
		
		mz::app::SetDyn(dyn, tex.m_Ptr);
	}
}

template<class T>
static void SetValue(mz::proto::Dynamic* dyn, IRemoteControlPropertyHandle* p)
{
	using ValueType = decltype(T{}.val());

	mz::proto::msg<T> m;
	ValueType val;

	p->GetValue(val);
	m->set_val(val);

	mz::app::SetDyn(dyn, m.m_Ptr);
}

#pragma optimize( "", off )
void MZType::SerializeToProto(mz::proto::Dynamic* dyn, MZEntity* e)
{
	switch (Tag)
	{
	case BOOL:	 
		SetValue<mz::proto::Bool>(dyn, e->Property.Get());
		break;

	case INT:	 
		switch (Width)
		{
		case 32: SetValue<mz::proto::i32>(dyn, e->Property.Get()); break;
		case 64: SetValue<mz::proto::i64>(dyn, e->Property.Get()); break;
		}
		break;
	case FLOAT:
		switch (Width)
		{
		case 32: SetValue<mz::proto::f32>(dyn, e->Property.Get()); break;
		case 64: SetValue<mz::proto::f64>(dyn, e->Property.Get()); break;
		}
		break;
	
	case STRING: {
		FString val;
		mz::proto::msg<mz::proto::String> m;
		e->Property.Get()->GetValue(val);

		mz::app::SetField(m.m_Ptr,  m->kValFieldNumber, TCHAR_TO_UTF8(*val));
		mz::app::SetDyn(dyn, m.m_Ptr);
	}
	break;
	case STRUCT:
	{
		break;
	}
	case TRT2D:
	{
		UObject* obj = e->Entity->GetBoundObject();
		FObjectProperty* prop = CastField<FObjectProperty>(e->Property->GetProperty());
		UTextureRenderTarget2D* val = Cast<UTextureRenderTarget2D>(prop->GetObjectPropertyValue(prop->ContainerPtrToValuePtr<UTextureRenderTarget2D>(obj)));

		ENQUEUE_RENDER_COMMAND(TRT2D_GetRenderTargetResource)(
			[val, dyn, e](FRHICommandListImmediate& RHICmdList)
			{
				auto res = val->GetRenderTargetResource()->GetTextureRenderTarget2DResource();
				FRHITexture2D* rhi = res->GetTexture2DRHI();
				FString RHIName = GDynamicRHI->GetName();

				if (RHIName == "D3D12")
				{
					ID3D12Resource* handle = (ID3D12Resource*)rhi->GetNativeResource();
					ID3D12Device* dev = 0;
					HRESULT re = handle->GetDevice(__uuidof(ID3D12Device), (void**)&dev);
					D3D12_RESOURCE_DESC desc = handle->GetDesc();
					FMZClient::Get()->QueueTextureCopy(e->Entity->GetId(), handle, dyn);
				}
			});

		FlushRenderingCommands();

		break;
	}
	}
}
#pragma optimize( "", on )
void MZEntity::SerializeToProto(mz::proto::Dynamic* req)
{
	if (Type)
	{
		Type->SerializeToProto(req, this);
	}
	else
	{
		UE_LOG(LogMZProto, Error, TEXT("Unknown Type"));
	}
}
