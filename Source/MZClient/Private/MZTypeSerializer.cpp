// Copyright Epic Games, Inc. All Rights Reserved.


#include "MZClient.h"

DEFINE_LOG_CATEGORY(LogMZProto);

TMap<FGuid, FMZClient::ResourceInfo> GetCopyList(FMZClient* fmz)
{
	std::unique_lock lock(fmz->Mutex);
	return fmz->CopyOnTick;
}

bool FMZClient::Tick(float dt)
{
	InitConnection();

	if (!CopyOnTick.Num())
	{
		return true;
	}

	ENQUEUE_RENDER_COMMAND(FMZClient_CopyOnTick)(
		[this, CopyList=GetCopyList(this)](FRHICommandListImmediate& RHICmdList)
		{
			//HANDLE eventHandle = CreateEventEx(nullptr, 0, 0, EVENT_ALL_ACCESS);
			CmdList->Reset(CmdAlloc, 0);
			std::vector<D3D12_RESOURCE_BARRIER> barriers;
			for (auto& [_, pin] : CopyList)
			{
				if (pin.Fence->GetCompletedValue() < pin.FenceValue)
				{
					pin.Fence->SetEventOnCompletion(pin.FenceValue, pin.Event);
					WaitForSingleObject(pin.Event, INFINITE);
				}

				auto barrier = D3D12_RESOURCE_BARRIER{
						.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION,
						.Transition =
						D3D12_RESOURCE_TRANSITION_BARRIER{
							.pResource = pin.SrcResource,
							.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET,
							.StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE,
						}
				};

				CmdList->ResourceBarrier(1, &barrier);
				CmdList->CopyResource(pin.DstResource, pin.SrcResource);
				barriers.push_back(D3D12_RESOURCE_BARRIER{
						.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION,
						.Transition = 
						D3D12_RESOURCE_TRANSITION_BARRIER{
							.pResource = pin.SrcResource,
							.StateBefore = D3D12_RESOURCE_STATE_COPY_SOURCE,
							.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET,
						}
					});
			}

			CmdList->ResourceBarrier(barriers.size(), barriers.data());
			CmdList->Close();
			CmdQueue->ExecuteCommandLists(1, (ID3D12CommandList**)&CmdList);
			for (auto& [_, pin] : CopyOnTick)
			{
				CmdQueue->Signal(pin.Fence, ++pin.FenceValue);
			}
		});
	return true;
}

void ClientImpl::OnNodeUpdate(mz::proto::Node const& archive)
{

	mz::proto::msg<mz::proto::Texture> tex;
	FMZClient* fmz = (FMZClient*)IMZClient::Get();
	std::unique_lock lock(fmz->Mutex);
	FString newId = archive.id().c_str();
	if (id != newId)
	{
		fmz->ClearResources();
		id = newId;
	}

	for (auto& pin : archive.pins())
	{
		FGuid out;
		if (FGuid::Parse(pin.id().c_str(), out))
		{	
			if (auto ppRes = fmz->PendingCopyQueue.Find(out))
			{
				if (mz::app::ParseFromString(tex.m_Ptr, pin.dynamic().data().c_str()))
				{
					ID3D12Resource* res = *ppRes;
					MzTextureShareInfo info = {
						.textureInfo = {
							.width = tex->width(),
							.height = tex->height(),
							.format = (MzFormat)tex->format(),
							.usage = (MzImageUsage)tex->usage(),
						},
						.pid = tex->pid(),
						.memory = tex->memory(),
						.sync = tex->sync(),
						.offset = tex->offset(),
					};
					
					FMZClient::ResourceInfo copyInfo = {
						.SrcResource = res,
						.Event = CreateEventA(0,0,0,0)
					};
					mzGetD3D12Resources(&info, fmz->Dev, &copyInfo.DstResource, &copyInfo.Fence);
					fmz->PendingCopyQueue.Remove(out);
					fmz->CopyOnTick.Add(out, copyInfo);
				}
			}
		}
	}
}

void FMZClient::InitRHI()
{
	Dev = (ID3D12Device*)GDynamicRHI->RHIGetNativeDevice();
	HRESULT re = Dev->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&CmdAlloc));
	FD3D12DynamicRHI* D3D12RHI = static_cast<FD3D12DynamicRHI*>(GDynamicRHI);
	CmdQueue = D3D12RHI->RHIGetD3DCommandQueue();
	Dev->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, CmdAlloc, 0, IID_PPV_ARGS(&CmdList));
	CmdList->Close();
}

void FMZClient::OnTextureReceived(FGuid id, mz::proto::Texture const& texture)
{
	
}

void FMZClient::QueueTextureCopy(FGuid id, ID3D12Resource* res, mz::proto::Dynamic* dyn)
{
	MzTextureInfo info = {};
	if (MZ_RESULT_SUCCESS == mzD3D12GetTextureInfo(res, &info))
	{
		{
			std::unique_lock lock(Mutex);
			PendingCopyQueue.Add(id, res);
		}

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
