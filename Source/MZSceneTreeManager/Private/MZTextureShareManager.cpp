#include "MZTextureShareManager.h"

#include "HardwareInfo.h"

#pragma warning (disable : 4800)
#pragma warning (disable : 4668)

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

#include "MZClient.h"

#include "MediaZ/MediaZ.h"
#include <Builtins_generated.h>


MZTextureShareManager* MZTextureShareManager::singleton;


MzTextureInfo GetResourceInfo(MZProperty* mzprop)
{
	UObject* obj = mzprop->GetRawObjectContainer();
	FObjectProperty* prop = CastField<FObjectProperty>(mzprop->Property);

	if (!obj)
	{
		return MzTextureInfo{
			.width = 1600,
			.height = 900,
			.format = MzFormat::MZ_FORMAT_R16G16B16A16_SFLOAT,
			.usage = (MzImageUsage)(MZ_IMAGE_USAGE_RENDER_TARGET | MZ_IMAGE_USAGE_SAMPLED | MZ_IMAGE_USAGE_TRANSFER_SRC | MZ_IMAGE_USAGE_TRANSFER_DST) 
		};
	}

	UTextureRenderTarget2D* trt2d = Cast<UTextureRenderTarget2D>(prop->GetObjectPropertyValue(prop->ContainerPtrToValuePtr<UTextureRenderTarget2D>(obj)));

	if (!trt2d)
	{
		return MzTextureInfo{
			.width = 1600,
			.height = 900,
			.format = MzFormat::MZ_FORMAT_R16G16B16A16_SFLOAT,
			.usage = (MzImageUsage)(MZ_IMAGE_USAGE_RENDER_TARGET | MZ_IMAGE_USAGE_SAMPLED | MZ_IMAGE_USAGE_TRANSFER_SRC | MZ_IMAGE_USAGE_TRANSFER_DST)
		};
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

mz::fb::TTexture MZTextureShareManager::AddTexturePin(MZProperty* mzprop)
{
	MzTextureInfo info = GetResourceInfo(mzprop);
	{
		std::unique_lock lock(PendingCopyQueueMutex);
		PendingCopyQueue.Add(mzprop->Id, mzprop);
	}
	mz::fb::TTexture tex;
	tex.size = mz::fb::SizePreset::CUSTOM;
	tex.width = info.width;
	tex.height = info.height;
	tex.format = mz::fb::Format(info.format);
	tex.usage = mz::fb::ImageUsage(info.usage) | mz::fb::ImageUsage::SAMPLED;
	tex.type = 0x00000040;
	// return tex;
	UObject* obj = mzprop->GetRawObjectContainer();
	FObjectProperty* prop = CastField<FObjectProperty>(mzprop->Property);
	UTextureRenderTarget2D* trt2d = Cast<UTextureRenderTarget2D>(prop->GetObjectPropertyValue(prop->ContainerPtrToValuePtr<UTextureRenderTarget2D>(obj)));
	auto TargetFormat = static_cast<DXGI_FORMAT>(GPixelFormats[trt2d->GetFormat()].PlatformFormat);
	// Shared heaps are not supported on CPU-accessible heaps
	D3D12_RESOURCE_DESC desc{};
	desc.Dimension        = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	desc.Alignment        = 0;
	desc.Width            = info.width;
	desc.Height           = info.height;
	desc.DepthOrArraySize = 1;
	desc.MipLevels        = 1;
	desc.Format           = TargetFormat;
	desc.SampleDesc.Count = 1;
	desc.Layout           = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	desc.Flags            = D3D12_RESOURCE_FLAG_ALLOW_SIMULTANEOUS_ACCESS | D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS | D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

	ID3D12Resource* res;
	HANDLE handle;
	
	D3D12_HEAP_PROPERTIES props = {.Type = D3D12_HEAP_TYPE_DEFAULT};

	auto state = D3D12_RESOURCE_STATE_COMMON;

	MZ_D3D12_ASSERT_SUCCESS(Dev->CreateCommittedResource(&props, D3D12_HEAP_FLAG_SHARED, &desc, state, 0, IID_PPV_ARGS(&res)));
	
	MZ_D3D12_ASSERT_SUCCESS(Dev->CreateSharedHandle(res, 0, GENERIC_ALL, 0, &handle));
	
	res->Release();

	tex.memory = (u64)handle;
	tex.pid = FPlatformProcess::GetCurrentProcessId();
	tex.unmanaged = true;
	tex.offset = 0;
	tex.handle = 0;
	return tex;
}

void MZTextureShareManager::UpdateTexturePin(MZProperty* mzprop, mz::fb::ShowAs RealShowAs, mz::fb::Texture const* tex)
{
	auto curtex = flatbuffers::GetRoot<mz::fb::Texture>(mzprop->data.data());
	if(tex->handle() == curtex->handle() && tex->memory() == curtex->memory() && tex->offset() == curtex->offset() && !PendingCopyQueue.Contains(mzprop->Id))
	{
		return;
	}
	
	mzprop->data = mz::Buffer::From(*tex);
	//std::unique_lock lock(CopyOnTickMutex);
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
		.ReadOnly = RealShowAs == mz::fb::ShowAs::INPUT_PIN,
		.Info = info,
	};

	if (MzResult::MZ_RESULT_SUCCESS != FMediaZ::GetD3D12Resources(&info, Dev, &copyInfo.DstResource))
	{
		abort();
	}

	UObject* obj = mzprop->GetRawObjectContainer();
	if (!obj) return;
	auto prop = CastField<FObjectProperty>(mzprop->Property);
	if (!prop) return;
	auto URT = Cast<UTextureRenderTarget2D>(prop->GetObjectPropertyValue(prop->ContainerPtrToValuePtr<UTextureRenderTarget2D>(obj)));
	if (!URT) return;
	{
		std::unique_lock lock(CopyOnTickMutex);
		if(PendingCopyQueue.Contains(mzprop->Id))
		{
			PendingCopyQueue.Remove(mzprop->Id);
			CopyOnTick.Add(mzprop, copyInfo);
		}
		else if(CopyOnTick.Contains(mzprop))
		{
			auto resource = CopyOnTick.FindRef(mzprop).DstResource;
			if(resource && copyInfo.DstResource != resource)
			{
				std::unique_lock reslock(ResourcesToDeleteMutex);
				ResourcesToDelete.Add(resource);
			}
			CopyOnTick.Add(mzprop, copyInfo);
		}
	}
}

void MZTextureShareManager::UpdatePinShowAs(MZProperty* MzProperty, mz::fb::ShowAs NewShowAs)
{
	std::unique_lock lock(CopyOnTickMutex);
	if(CopyOnTick.Contains(MzProperty))
	{
		auto info = CopyOnTick.Find(MzProperty);
		info->ReadOnly = NewShowAs == mz::fb::ShowAs::INPUT_PIN;
	}
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

void MZTextureShareManager::TextureDestroyed(MZProperty* textureProp)
{
	{
		std::unique_lock lock(CopyOnTickMutex);
		if(CopyOnTick.Contains(textureProp))
		{
			auto CopyInfo = CopyOnTick.FindRef(textureProp);
			if(CopyInfo.DstResource)
			{
				CopyInfo.DstResource->Release();
			}
			CopyOnTick.Remove(textureProp);
		}
	}
	{
		std::unique_lock lock(PendingCopyQueueMutex);
		PendingCopyQueue.Remove(textureProp->Id);
		auto tex = flatbuffers::GetRoot<mz::fb::Texture>(textureProp->data.data());
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
		if(!info.handle)
		{
			return;
		}
		ID3D12Resource* TextureResource = 0;
		if (MzResult::MZ_RESULT_SUCCESS != FMediaZ::GetD3D12Resources(&info, Dev, &TextureResource))
		{
			abort();
		}
		if(TextureResource)
		{
			TextureResource->Release();
		}
	}
}

void MZTextureShareManager::Reset()
{
	std::unique_lock lock(CopyOnTickMutex);
	CopyOnTick.Empty();
	PendingCopyQueue.Empty();
}

void MZTextureShareManager::EnqueueCommands(mz::app::IAppServiceClient* Client)
{
	{
		std::shared_lock lock(CopyOnTickMutex);
		if (CopyOnTick.IsEmpty())
		{
			return;
		}
	}

	TMap<UTextureRenderTarget2D*, ResourceInfo> CopyOnTickFiltered;
	{
		std::unique_lock lock(CopyOnTickMutex);
		for (auto [mzprop, info] : CopyOnTick)
		{
			UObject* obj = mzprop->GetRawObjectContainer();
			if (!obj) return;
			auto prop = CastField<FObjectProperty>(mzprop->Property);
			if (!prop) return;
			auto URT = Cast<UTextureRenderTarget2D>(prop->GetObjectPropertyValue(prop->ContainerPtrToValuePtr<UTextureRenderTarget2D>(obj)));
			if (!URT) return;

			CopyOnTickFiltered.Add(URT, info);
		}
	}

	ENQUEUE_RENDER_COMMAND(FMZClient_CopyOnTick)(
		[this, Client, CopyOnTickFiltered](FRHICommandListImmediate& RHICmdList)
		{
			//std::shared_lock lock(CopyOnTickMutex);
			WaitCommands();
			{
				std::unique_lock lock(ResourcesToDeleteMutex);
				for(auto Resource : ResourcesToDelete)
				{
					Resource->Release();
				}
				ResourcesToDelete.Empty();
			}
			TArray<D3D12_RESOURCE_BARRIER> barriers;
			flatbuffers::FlatBufferBuilder fbb;
			std::vector<flatbuffers::Offset<mz::app::AppEvent>> events;
			for (auto& [URT, pin] : CopyOnTickFiltered)
			{
				auto rt = URT->GetRenderTargetResource();
				if (!rt) return;
				auto RHIResource = rt->GetTexture2DRHI();

				if (!RHIResource || !RHIResource->IsValid())
				{
					continue;
				}
				//auto* tex = flatbuffers::GetRoot<mz::fb::Texture>(pin.SrcMzp->data.data());
				//std::cout << tex << std::endl;
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
					// EPixelFormat format = PF_Unknown;
					// ETextureSourceFormat sourceFormat = TSF_Invalid;
					// ETextureRenderTargetFormat rtFormat = RTF_RGBA16f;
					// for (auto& fmt : GPixelFormats)
					// {
					// 	if (fmt.PlatformFormat == DstDesc.Format)
					// 	{
					// 		format = fmt.UnrealFormat;
					// 		sourceFormat = (fmt.BlockBytes == 8) ? TSF_RGBA16F : TSF_BGRA8;
					// 		rtFormat = (fmt.BlockBytes == 8) ? RTF_RGBA16f : RTF_RGBA8;
					// 		break;
					// 	}
					// }
					//
					// ETextureCreateFlags Flags = ETextureCreateFlags::ShaderResource;
					//
					// if (DstDesc.Flags & D3D12_RESOURCE_FLAG_ALLOW_SIMULTANEOUS_ACCESS) Flags |= ETextureCreateFlags::Shared;
					// if (DstDesc.Flags & D3D12_RESOURCE_FLAG_ALLOW_CROSS_ADAPTER)       Flags |= ETextureCreateFlags::Shared;
					// if (DstDesc.Flags & D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET)       Flags |= ETextureCreateFlags::RenderTargetable;
					// if (DstDesc.Flags & D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS)    Flags |= ETextureCreateFlags::UAV;
					// if (DstDesc.Flags & D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE)      Flags ^= ETextureCreateFlags::ShaderResource;
					//
					//
					// FTexture2DRHIRef Texture2DRHI = GetID3D12DynamicRHI()->RHICreateTexture2DFromResource(format, Flags, FClearValueBinding::Black, DstResource);
					// URT->RenderTargetFormat = rtFormat;
					// URT->SizeX = DstDesc.Width;
					// URT->SizeY = DstDesc.Height;
					// URT->ClearColor = FLinearColor::Black;
					// URT->bGPUSharedFlag = 1;
					// RHIUpdateTextureReference(URT->TextureReference.TextureReferenceRHI, Texture2DRHI);
					// URT->GetResource()->TextureRHI = Texture2DRHI;
					// URT->GetResource()->SetTextureReference(URT->TextureReference.TextureReferenceRHI);
					// continue;
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

				if (!pin.ReadOnly)
				{
					auto id = pin.SrcMzp->Id;
					events.push_back(mz::CreateAppEventOffset(fbb, mz::app::CreatePinDirtied(fbb, (mz::fb::UUID*)&id)));
				}
			}
			CmdList->ResourceBarrier(barriers.Num(), barriers.GetData());
			ExecCommands();

			if (!events.empty() && Client && Client->IsConnected())
			{
				Client->Send(mz::CreateAppEvent(fbb, mz::app::CreateBatchAppEventDirect(fbb, &events)));
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