// Copyright MediaZ AS. All Rights Reserved.

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
	//res->Release();


	ID3D12Fence* fence;
	HANDLE fenceHandle;
	Dev->CreateFence(0, D3D12_FENCE_FLAG_SHARED, IID_PPV_ARGS(&fence));
	MZ_D3D12_ASSERT_SUCCESS(Dev->CreateSharedHandle(fence, 0, GENERIC_ALL, 0, &fenceHandle));
	
	mz::fb::TTexture tex;
	tex.size = mz::fb::SizePreset::CUSTOM;
	tex.width = info.width;
	tex.height = info.height;
	tex.format = mz::fb::Format(info.format);
	tex.usage = mz::fb::ImageUsage(info.usage) | mz::fb::ImageUsage::SAMPLED;
	tex.type = 0x00000040;
	tex.memory = (u64)handle;
	tex.pid = FPlatformProcess::GetCurrentProcessId();
	tex.unmanaged = true;
	tex.offset = 0;
	tex.handle = 0;
	tex.semaphore = (u64)fenceHandle;
	
	ResourceInfo copyInfo = {
		.SrcMzp = mzprop,
		.DstResource = res,
		.Fence = fence,
		.FenceValue = 0,
	};
	
	{
		//start property pins as output pins
		OutputCopies.Add(mzprop, copyInfo);
	}
	return tex;
}

void MZTextureShareManager::UpdateTexturePin(MZProperty* mzprop, mz::fb::ShowAs RealShowAs, void* data, uint32_t size)
{
	UpdatePinShowAs(mzprop, RealShowAs);

	if(OutputCopies.Contains(mzprop))
	{
		auto CopyInfo = OutputCopies.FindRef(mzprop);

		//set real fence value
		CopyInfo.FenceValue = 0;
	}
	else if(InputCopies.Contains(mzprop))
	{
		auto CopyInfo = InputCopies.FindRef(mzprop);

		//set real fence value
		CopyInfo.FenceValue = 0;
	}
	
	return;
	//TODO better update handling (texture size etc.)
	mz::fb::Texture const* tex = flatbuffers::GetRoot<mz::fb::Texture>(data);
	auto curtex = flatbuffers::GetRoot<mz::fb::Texture>(mzprop->data.data());
	auto pid = tex->pid();
	if(pid != (uint64_t)FPlatformProcess::GetCurrentProcessId() || (tex->handle() == curtex->handle() && tex->memory() == curtex->memory() && tex->offset() == curtex->offset() && !PendingCopyQueue.Contains(mzprop->Id)))
	{
		return;
	}
	

	mzprop->data.resize(size);
	memcpy(mzprop->data.data(), data, size);
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
		// .Info = info,
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
				ResourcesToDelete.Add(resource);
			}
			CopyOnTick.Add(mzprop, copyInfo);
		}
	}
}

void MZTextureShareManager::UpdatePinShowAs(MZProperty* MzProperty, mz::fb::ShowAs NewShowAs)
{
	if(NewShowAs == mz::fb::ShowAs::INPUT_PIN)
	{
		if(OutputCopies.Contains(MzProperty))
		{
			auto Info = OutputCopies.FindRef(MzProperty);
			OutputCopies.Remove(MzProperty);
			InputCopies.Add(MzProperty, Info);
		}
	}
	else if(NewShowAs == mz::fb::ShowAs::OUTPUT_PIN)
	{
		if(InputCopies.Contains(MzProperty))
		{
			auto Info = InputCopies.FindRef(MzProperty);
			InputCopies.Remove(MzProperty);
			OutputCopies.Add(MzProperty, Info);
		}
	}
	
}

void MZTextureShareManager::WaitCommands()
{
	// if (CmdFence->GetCompletedValue() < CmdFenceValue)
	// {
	// 	CmdFence->SetEventOnCompletion(CmdFenceValue, CmdEvent);
	// 	WaitForSingleObject(CmdEvent, INFINITE);
	// }
	//
	// CmdAlloc->Reset();
	// CmdList->Reset(CmdAlloc, 0);
}


void MZTextureShareManager::ExecCommands(CmdStruct* cmdData, bool bIsInput, TMap<ID3D12Fence*, u64>& SignalGroup)
{
	cmdData->CmdList->Close();
	CmdQueue->ExecuteCommandLists(1, (ID3D12CommandList**)&cmdData->CmdList);
	CmdQueue->Signal(cmdData->CmdFence, ++cmdData->CmdFenceValue);
	
	for(auto& [fence, val] : SignalGroup)
	{
		CmdQueue->Signal(fence, val);
	}
	
	// cmdData->CmdList->Reset(CmdAlloc, 0);
	cmdData->State = CmdState::Running;
#if 0
	{
		if (cmdData->CmdFence->GetCompletedValue() < cmdData->CmdFenceValue)
		{
			HANDLE CmdEvent = CreateEventA(0, 0, 0, 0);
			cmdData->CmdFence->SetEventOnCompletion(cmdData->CmdFenceValue, CmdEvent);
			WaitForSingleObject(CmdEvent, INFINITE);
		}
		// cmdData->CmdAlloc->Reset();
		// cmdData->CmdList->Reset(cmdData->CmdAlloc, 0);
	}
#endif
}

void MZTextureShareManager::TextureDestroyed(MZProperty* textureProp)
{
	OutputCopies.Remove(textureProp);
	InputCopies.Remove(textureProp);
	
	//TODO delete real resource	
}

static HANDLE DupeHandle(uint64_t pid, HANDLE handle)
{
    HANDLE re = 0;
    HANDLE src = OpenProcess(GENERIC_ALL, false, pid);
    HANDLE cur = GetCurrentProcess();
    if (!DuplicateHandle(src, handle, cur, &re, GENERIC_ALL, 0, DUPLICATE_SAME_ACCESS))
    {
        return 0;
    }
    CloseHandle(src);
    return re;
}

static bool ImportSharedFence(uint64_t pid, HANDLE handle, ID3D12Device* pDevice, ID3D12Fence** pFence)
{
       HANDLE xmemory = DupeHandle(pid, handle);
       if (FAILED(pDevice->OpenSharedHandle(xmemory, IID_PPV_ARGS(pFence))))
       {
               return false;
       }
       CloseHandle(xmemory);
       return true;
}

void MZTextureShareManager::AllocateCommandLists()
{
	for(size_t i = 0; i < CommandListCount; i ++)
	{
		ID3D12GraphicsCommandList* cmdList;
		ID3D12Fence* cmdFence;
		ID3D12CommandAllocator* cmdAlloc;
		HRESULT re = Dev->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&cmdAlloc));
		Dev->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, cmdAlloc, 0, IID_PPV_ARGS(&cmdList));
		Dev->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&cmdFence));
		cmdList->Close();
		// cmdList->Reset(CmdAlloc, 0);
		auto cmdData = new CmdStruct;
		cmdData->CmdAlloc = cmdAlloc;
		cmdData->CmdList = cmdList;
		cmdData->CmdFence = cmdFence;
		cmdData->State = CmdState::Pending;
		cmdData->CmdFenceValue = 0;
		Cmds.push_back(cmdData);
	}
}

CmdStruct* MZTextureShareManager::GetNewCommandList()
{
	static int i = 0;
	i = (i + 1) % CommandListCount;
	
	if(Cmds[i]->State == CmdState::Pending)
	{
		Cmds[i]->CmdList->Reset(Cmds[i]->CmdAlloc, 0);
		Cmds[i]->State = Recording;
		return Cmds[i];
	}
	else if(Cmds[i]->State == CmdState::Running)
	{
		if (Cmds[i]->CmdFence->GetCompletedValue() < Cmds[i]->CmdFenceValue)
		{
			HANDLE CmdEvent = CreateEventA(0, 0, 0, 0);
			Cmds[i]->CmdFence->SetEventOnCompletion(Cmds[i]->CmdFenceValue, CmdEvent);
			WaitForSingleObject(CmdEvent, INFINITE);
		}
		Cmds[i]->CmdAlloc->Reset();
		Cmds[i]->CmdList->Reset(Cmds[i]->CmdAlloc, 0);
		Cmds[i]->State = CmdState::Recording;
		return Cmds[i];
	}
	else
	{
		abort();
		return NULL;
	}
}

bool MZCopyTexture_RenderThread(bool bIsInputPin, UTextureRenderTarget2D* RenderTarget, ID3D12Resource* PinTexture, ID3D12Fence* Fence, ID3D12GraphicsCommandList* CmdList, TArray<D3D12_RESOURCE_BARRIER>& Barriers)
{
	auto rt = RenderTarget->GetRenderTargetResource();
	if (!rt) return false;
	auto RHIResource = rt->GetTexture2DRHI();

	if (!RHIResource || !RHIResource->IsValid())
	{
		return false;
	}
	FRHITexture* RHITexture = RHIResource;
	FD3D12Texture* Result((FD3D12Texture*)RHITexture->GetTextureBaseRHI());
	FD3D12Texture* Base = Result;

	ID3D12Resource* SrcResource = Base->GetResource()->GetResource();
	ID3D12Resource* DstResource = PinTexture;
	D3D12_RESOURCE_DESC SrcDesc = SrcResource->GetDesc();
	D3D12_RESOURCE_DESC DstDesc = DstResource->GetDesc();

	if (SrcResource == DstResource)
	{
		return false;
	}


	if(bIsInputPin)
	{
		Swap(SrcResource, DstResource);
		D3D12_RESOURCE_BARRIER barrier = {
			.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION,
			.Transition = {
				.pResource = SrcResource,
				.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,
				.StateBefore = D3D12_RESOURCE_STATE_COMMON,
				.StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE,
			}
		};
		CmdList->ResourceBarrier(1, &barrier);

		D3D12_RESOURCE_BARRIER barrier2 = {
                			.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION,
                			.Transition = {
                				.pResource = DstResource,
                				.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,
                				.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET,
                				.StateAfter = D3D12_RESOURCE_STATE_COPY_DEST,
                			}
                		};
		CmdList->ResourceBarrier(1, &barrier2);
		CmdList->CopyResource(DstResource, SrcResource);
		D3D12_RESOURCE_BARRIER barrier3 = {
        			.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION,
        			.Transition = {
        				.pResource = DstResource,
        				.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,
        				.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST,
        				.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET,
        			}
        		};
		
		CmdList->ResourceBarrier(1, &barrier3);
		// Swap(barrier.Transition.StateBefore, barrier.Transition.StateAfter);
		// Barriers.Add(barrier);
	}
	else
	{
		D3D12_RESOURCE_BARRIER barrier = {
			.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION,
			.Transition = {
				.pResource = SrcResource,
				.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,
				.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET,
				.StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE,
			}
		};
		CmdList->ResourceBarrier(1, &barrier);
		CmdList->CopyResource(DstResource, SrcResource);
		D3D12_RESOURCE_BARRIER barrier2 = {
        			.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION,
        			.Transition = {
        				.pResource = DstResource,
        				.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,
        				.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST,
        				.StateAfter = D3D12_RESOURCE_STATE_COMMON,
        			}
        		};
		
		CmdList->ResourceBarrier(1, &barrier2);
		
		
		D3D12_RESOURCE_BARRIER barrier3 = {
			.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION,
			.Transition = {
				.pResource = SrcResource,
				.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,
				.StateBefore = D3D12_RESOURCE_STATE_COPY_SOURCE,
				.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET,
			}
		};
		
		CmdList->ResourceBarrier(1, &barrier3);
	}
	// D3D12_RESOURCE_BARRIER barrier = {
	// 	.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION,
	// 	.Transition = {
	// 		.pResource = SrcResource,
	// 		.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,
	// 		.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET,
	// 		.StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE,
	// 	}
	// };
	//
	//
	//
	// if (bIsInputPin)
	// {
	// 	Swap(SrcResource, DstResource);
	// 	barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_DEST;
	// }
	// CmdList->ResourceBarrier(1, &barrier);
	// CmdList->CopyResource(DstResource, SrcResource);
	// Swap(barrier.Transition.StateBefore, barrier.Transition.StateAfter);
	// Barriers.Add(barrier);

	return true;
}

void FilterCopies(TMap<MZProperty*, ResourceInfo>& Copies, TMap<UTextureRenderTarget2D*, ResourceInfo>& FilteredCopies)
{
	for (auto [mzprop, info] : Copies)
	{
		UObject* obj = mzprop->GetRawObjectContainer();
		if (!obj) continue;
		auto prop = CastField<FObjectProperty>(mzprop->Property);
		if (!prop) continue;
		auto URT = Cast<UTextureRenderTarget2D>(prop->GetObjectPropertyValue(prop->ContainerPtrToValuePtr<UTextureRenderTarget2D>(obj)));
		if (!URT) continue;

		FilteredCopies.Add(URT, info);
	}
}

void MZTextureShareManager::ProcessCopies(bool bIsInput,  TMap<MZProperty*, ResourceInfo>& CopyMap)
{
	{
		if (CopyMap.IsEmpty())
		{
			return;
		}
	}
	TMap<UTextureRenderTarget2D*, ResourceInfo> CopiesFiltered;
	FilterCopies(CopyMap, CopiesFiltered);
	auto cmdData = GetNewCommandList();
	ENQUEUE_RENDER_COMMAND(FMZClient_CopyOnTick)(
		[this, bIsInput, CopiesFiltered, cmdData](FRHICommandListImmediate& RHICmdList)
		{
			TArray<D3D12_RESOURCE_BARRIER> barriers;
			std::vector<flatbuffers::Offset<mz::app::AppEvent>> events;
			TMap<ID3D12Fence*, u64> SignalGroup;
			flatbuffers::FlatBufferBuilder fbb;
			for (auto& [URT, pin] : CopiesFiltered)
			{
				if(bIsInput)
				{
					CmdQueue->Wait(pin.Fence, pin.FenceValue);
				}
				else
				{
					CmdQueue->Wait(pin.Fence, (2 * pin.FenceValue));
					SignalGroup.Add(pin.Fence, (2 * pin.FenceValue) + 1);
				}
				if(MZCopyTexture_RenderThread(bIsInput, URT, pin.DstResource, pin.Fence, cmdData->CmdList, barriers) && !bIsInput)
				{
					events.push_back(mz::CreateAppEventOffset(fbb, mz::app::CreatePinDirtied(fbb, (mz::fb::UUID*)&pin.SrcMzp->Id, pin.FenceValue)));
				}
			}
			cmdData->CmdList->ResourceBarrier(barriers.Num(), barriers.GetData());
			ExecCommands(cmdData, bIsInput, SignalGroup);
			if (!events.empty() && MZClient && MZClient->IsConnected())
			{
				MZClient->AppServiceClient->Send(mz::CreateAppEvent(fbb, mz::app::CreateBatchAppEventDirect(fbb, &events)));
			}
		});
}

void MZTextureShareManager::OnBeginFrame()
{
	ProcessCopies(true, InputCopies);
}

void MZTextureShareManager::OnEndFrame()
{
	ProcessCopies(false, OutputCopies);
	for(auto& [_, info] : OutputCopies)
	{
		info.FenceValue += 1;
	}
}

void MZTextureShareManager::Reset()
{
	CopyOnTick.Empty();
	PendingCopyQueue.Empty();
}

void MZTextureShareManager::Initiate()
{
	auto hwinfo = FHardwareInfo::GetHardwareInfo(NAME_RHI);
	if ("D3D12" != hwinfo)
	{
		return;
	}

	MZClient = &FModuleManager::LoadModuleChecked<FMZClient>("MZClient");
	
	// Create DX resources
	Dev = (ID3D12Device*)GetID3D12DynamicRHI()->RHIGetNativeDevice();

	CmdQueue = GetID3D12DynamicRHI()->RHIGetCommandQueue();
	CmdQueue->AddRef();
	
	AllocateCommandLists();
}