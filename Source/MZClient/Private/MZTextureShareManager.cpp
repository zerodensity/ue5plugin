#include "MZTextureShareManager.h"

#include "HardwareInfo.h"
#include "AppTemplates.h"

#pragma warning (disable : 4800)
#pragma warning (disable : 4668)

void MemoryBarrier();
#include "Windows/AllowWindowsPlatformTypes.h"
#pragma intrinsic(_InterlockedCompareExchange64)
#define InterlockedCompareExchange64 _InterlockedCompareExchange64
#include <d3d12.h>
#include "Windows/HideWindowsPlatformTypes.h"

#include "D3D12RHIPrivate.h"
#include "D3D12RHI.h"
#include "D3D12Resources.h"
#include "ID3D12DynamicRHI.h"
#include "RHI.h"
#include "MZActorProperties.h"


//template<class T> requires((u32)mz::app::AppEventUnionTraits<T>::enum_value != 0)
//static flatbuffers::grpc::Message<mz::app::AppEvent> MakeAppEvent(MessageBuilder& b, flatbuffers::Offset<T> event)
//{
//	b.Finish(mz::app::CreateAppEvent(b, mz::app::AppEventUnionTraits<T>::enum_value, event.Union()));
//	auto msg = b.ReleaseMessage<mz::app::AppEvent>();
//	assert(msg.Verify());
//	return msg;
//}
//
//template <class T> requires((u32)mz::app::AppEventUnionTraits<T>::enum_value != 0)
//static flatbuffers::Offset<mz::app::AppEvent> CreateAppEventOffset(flatbuffers::FlatBufferBuilder& b, flatbuffers::Offset<T> event)
//{
//	return mz::app::CreateAppEvent(b, mz::app::AppEventUnionTraits<T>::enum_value, event.Union());
//}
MZTextureShareManager* MZTextureShareManager::singleton;


MzTextureInfo GetResourceInfo(MZProperty* mzprop)
{
	UObject* obj = mzprop->Container;
	FObjectProperty* prop = CastField<FObjectProperty>(mzprop->Property);

	if (!obj)
	{
		return MzTextureInfo{};
	}

	UTextureRenderTarget2D* trt2d = Cast<UTextureRenderTarget2D>(prop->GetObjectPropertyValue(prop->ContainerPtrToValuePtr<UTextureRenderTarget2D>(obj)));

	if (!trt2d)
	{
		return MzTextureInfo{};
	}

	MzTextureInfo info = {
		.width = (uint32_t)trt2d->GetSurfaceWidth(),
		.height = (uint32_t)trt2d->GetSurfaceHeight(),
		.usage = (MzImageUsage)(MZ_IMAGE_USAGE_RENDER_TARGET | MZ_IMAGE_USAGE_SAMPLED | MZ_IMAGE_USAGE_TRANSFER_SRC | MZ_IMAGE_USAGE_TRANSFER_DST),
	};

	switch (trt2d->RenderTargetFormat)
	{
	case ETextureRenderTargetFormat::RTF_R8:
		info.format = MZ_FORMAT_R8_UNORM;
		break;
	case ETextureRenderTargetFormat::RTF_RG8:
		info.format = MZ_FORMAT_R8G8_UNORM;
		break;
	case ETextureRenderTargetFormat::RTF_RGBA8:
		info.format = MZ_FORMAT_R8G8B8A8_UNORM;
		break;
	case ETextureRenderTargetFormat::RTF_RGBA8_SRGB:
		info.format = MZ_FORMAT_R8G8B8A8_SRGB;
		break;

	case ETextureRenderTargetFormat::RTF_R16f:
		info.format = MZ_FORMAT_R16_SFLOAT;
		break;
	case ETextureRenderTargetFormat::RTF_RG16f:
		info.format = MZ_FORMAT_R16G16_SFLOAT;
		break;
	case ETextureRenderTargetFormat::RTF_RGBA16f:
		info.format = MZ_FORMAT_R16G16B16A16_SFLOAT;
		break;

	case ETextureRenderTargetFormat::RTF_R32f:
		info.format = MZ_FORMAT_R32_SFLOAT;
		break;
	case ETextureRenderTargetFormat::RTF_RG32f:
		info.format = MZ_FORMAT_R32G32_SFLOAT;
		break;
	case ETextureRenderTargetFormat::RTF_RGBA32f:
		info.format = MZ_FORMAT_R32G32B32A32_SFLOAT;
		break;

	case ETextureRenderTargetFormat::RTF_RGB10A2:
		info.format = MZ_FORMAT_A2R10G10B10_UNORM_PACK32;
		break;
	}

	return info;
}

MZTextureShareManager::MZTextureShareManager()
{
	Initiate();
}

MZTextureShareManager* MZTextureShareManager::GetInstance()
{
	if (singleton == nullptr) {
		singleton = new MZTextureShareManager();
	}
	return singleton;
}

MZTextureShareManager::~MZTextureShareManager()
{
}

void MZTextureShareManager::AddTexturePin(MZProperty* mzprop, mz::fb::Texture* tex)
{
	MzTextureInfo info = GetResourceInfo(mzprop);
	{
		std::unique_lock lock(PendingCopyQueueMutex);
		PendingCopyQueue.Add(mzprop->id, mzprop);
	}
	tex->mutate_width(info.width);
	tex->mutate_height(info.height);
	tex->mutate_format(mz::fb::Format(info.format));
	tex->mutate_usage(mz::fb::ImageUsage(info.usage) | mz::fb::ImageUsage::SAMPLED);
	tex->mutate_type(0x00000040);
}


void MZTextureShareManager::UpdateTexturePin(MZProperty* mzprop, mz::fb::Texture* tex)
{
	std::unique_lock lock2(CopyOnTickMutex);
	MzTextureShareInfo info = {
	.type = tex->type(),
	.handle = tex->handle(),
	.pid = tex->pid(),
	.memory = tex->memory(),
	.offset = tex->offset(),
	.textureInfo = {
		.width = tex->width(),
		.height = tex->height(),
		.format = (MzFormat)tex->format(),
		.usage = (MzImageUsage)tex->usage(),
	},
	};

	ResourceInfo copyInfo = {
		.SrcMzp = mzprop,
		.ReadOnly = mzprop->PinShowAs == mz::fb::ShowAs::INPUT_PIN,
		.Info = info,
	};

	mzGetD3D12Resources(&info, Dev, &copyInfo.DstResource);
	PendingCopyQueue.Remove(mzprop->id);
	CopyOnTick.Add(mzprop->id, copyInfo);
}

void MZTextureShareManager::WaitCommands()
{
	if (CmdFence->GetCompletedValue() < CmdFenceValue)
	{
		CmdFence->SetEventOnCompletion(CmdFenceValue, CmdEvent);
		WaitForSingleObject(CmdEvent, INFINITE);
	}

	CmdAlloc->Reset();
	CmdList->Reset(CmdAlloc, 0);
}

void MZTextureShareManager::ExecCommands()
{
	CmdList->Close();
	CmdQueue->ExecuteCommandLists(1, (ID3D12CommandList**)&CmdList);
	CmdQueue->Signal(CmdFence, ++CmdFenceValue);
}

void MZTextureShareManager::EnqueueCommands(mz::app::AppClient* client)
{
	if (CopyOnTick.IsEmpty())
	{
		return;
	}
	ENQUEUE_RENDER_COMMAND(FMZClient_CopyOnTick)(
		[this, client](FRHICommandListImmediate& RHICmdList)
		{
			std::unique_lock lock(CopyOnTickMutex);
			WaitCommands();
			TArray<D3D12_RESOURCE_BARRIER> barriers;
			flatbuffers::grpc::MessageBuilder fbb;
			std::vector<flatbuffers::Offset<mz::app::AppEvent>> events;
			for (auto& [id, pin] : CopyOnTick)
			{
				UObject* obj = pin.SrcMzp->Container;
				if (!obj) continue;
				auto prop = CastField<FObjectProperty>(pin.SrcMzp->Property);
				if (!prop) continue;
				auto URT = Cast<UTextureRenderTarget2D>(prop->GetObjectPropertyValue(prop->ContainerPtrToValuePtr<UTextureRenderTarget2D>(obj)));
				if (!URT) continue;
				auto rt = URT->GetRenderTargetResource();
				if (!rt) continue;
				auto RHIResource = rt->GetTexture2DRHI();

				if (!RHIResource)
				{
					continue;
				}

				//FD3D12Texture* Base = GetD3D12TextureFromRHITexture(RHIResource);
				
				FRHITexture* RHITexture = RHIResource;
				//if (RHITexture && EnumHasAnyFlags(RHITexture->GetFlags(), TexCreate_Presentable))
				//{
				//	FD3D12BackBufferReferenceTexture2D* BufferBufferReferenceTexture = (FD3D12BackBufferReferenceTexture2D*)RHITexture;
				//	RHITexture = BufferBufferReferenceTexture->GetBackBufferTexture();
				//}
				FD3D12Texture* Result((FD3D12Texture*)RHITexture->GetTextureBaseRHI());
				FD3D12Texture* Base = Result;

				ID3D12Resource* SrcResource = Base->GetResource()->GetResource();
				ID3D12Resource* DstResource = pin.DstResource;
				D3D12_RESOURCE_DESC SrcDesc = SrcResource->GetDesc();
				D3D12_RESOURCE_DESC DstDesc = DstResource->GetDesc();

				if (pin.ReadOnly && SrcDesc != DstDesc)
				{
					EPixelFormat format = PF_Unknown;
					ETextureSourceFormat sourceFormat = TSF_Invalid;
					ETextureRenderTargetFormat rtFormat = RTF_RGBA16f;
					for (auto& fmt : GPixelFormats)
					{
						if (fmt.PlatformFormat == DstDesc.Format)
						{
							format = fmt.UnrealFormat;
							sourceFormat = (fmt.BlockBytes == 8) ? TSF_RGBA16F : TSF_BGRA8;
							rtFormat = (fmt.BlockBytes == 8) ? RTF_RGBA16f : RTF_RGBA8;
							break;
						}
					}

					ETextureCreateFlags Flags = ETextureCreateFlags::ShaderResource;

					if (DstDesc.Flags & D3D12_RESOURCE_FLAG_ALLOW_SIMULTANEOUS_ACCESS) Flags |= ETextureCreateFlags::Shared;
					if (DstDesc.Flags & D3D12_RESOURCE_FLAG_ALLOW_CROSS_ADAPTER)       Flags |= ETextureCreateFlags::Shared;
					if (DstDesc.Flags & D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET)       Flags |= ETextureCreateFlags::RenderTargetable;
					if (DstDesc.Flags & D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS)    Flags |= ETextureCreateFlags::UAV;
					if (DstDesc.Flags & D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE)      Flags ^= ETextureCreateFlags::ShaderResource;

					
					//FTexture2DRHIRef Texture2DRHI = GetID3D12DynamicRHI()->RHICreateTexture2DFromResource(format, Flags, FClearValueBinding::Black, DstResource);
					URT->RenderTargetFormat = rtFormat;
					URT->SizeX = DstDesc.Width;
					URT->SizeY = DstDesc.Height;
					URT->ClearColor = FLinearColor::Black;
					URT->bGPUSharedFlag = 1;
					//RHIUpdateTextureReference(URT->TextureReference.TextureReferenceRHI, Texture2DRHI);
					//URT->Resource->TextureRHI = Texture2DRHI;
					URT->Resource->SetTextureReference(URT->TextureReference.TextureReferenceRHI);
					continue;
				}

				if (SrcResource == DstResource)
				{
					continue;
				}

				D3D12_RESOURCE_BARRIER barrier = {
					.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION,
					.Transition = {
						.pResource = SrcResource,
						.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,
						.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET,
						.StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE,
					}
				};

				if (pin.ReadOnly)
				{
					barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_DEST;
					Swap(SrcResource, DstResource);
				}

				CmdList->ResourceBarrier(1, &barrier);
				CmdList->CopyResource(DstResource, SrcResource);
				Swap(barrier.Transition.StateBefore, barrier.Transition.StateAfter);
				barriers.Add(barrier);

				tbl<mz::app::AppEvent> msg;
				if (!pin.ReadOnly)
				{
					events.push_back(CreateAppEventOffset(fbb, mz::app::CreatePinDirtied(fbb, (mz::fb::UUID*)&id)));
				}
			}
			CmdList->ResourceBarrier(barriers.Num(), barriers.GetData());
			ExecCommands();

			if (!events.empty() && client)
			{
				client->Write(MakeAppEvent(fbb, mz::app::CreateBatchAppEventDirect(fbb, &events)));
			}
		});

}

void MZTextureShareManager::Initiate()
{
	auto hwinfo = FHardwareInfo::GetHardwareInfo(NAME_RHI);
	if ("D3D12" != hwinfo)
	{
		return;
	}

	
	//Dev = (ID3D12Device*)GDynamicRHI->RHIGetNativeDevice();
	Dev = (ID3D12Device*)GetID3D12DynamicRHI()->RHIGetNativeDevice();
	HRESULT re = Dev->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&CmdAlloc));

	CmdQueue = GetID3D12DynamicRHI()->RHIGetCommandQueue();


	//FD3D12DynamicRHI* D3D12RHI = static_cast<FD3D12DynamicRHI*>(GDynamicRHI);
	//CmdQueue = D3D12RHI->RHIGetCommandQueue();
	CmdQueue->AddRef();
	Dev->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, CmdAlloc, 0, IID_PPV_ARGS(&CmdList));
	Dev->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&CmdFence));
	CmdEvent = CreateEventA(0, 0, 0, 0);
	CmdList->Close();
}

