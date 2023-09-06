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
#include "D3D12CommandContext.h"
#include "RHI.h"
#include "MZActorProperties.h"

#include "MZClient.h"

#include <Builtins_generated.h>

#include "MZGPUFailSafe.h"


MZTextureShareManager* MZTextureShareManager::singleton;

//#define FAIL_SAFE_THREAD
//#define DEBUG_FRAME_SYNC_LOG

mzTextureInfo GetResourceInfo(MZProperty* mzprop)
{
	UObject* obj = mzprop->GetRawObjectContainer();
	FObjectProperty* prop = CastField<FObjectProperty>(mzprop->Property);

	if (!obj)
	{
		return mzTextureInfo{
			.Width = 1600,
			.Height = 900,
			.Format = mzFormat::MZ_FORMAT_R16G16B16A16_SFLOAT,
			.Usage = (mzImageUsage)(MZ_IMAGE_USAGE_RENDER_TARGET | MZ_IMAGE_USAGE_SAMPLED | MZ_IMAGE_USAGE_TRANSFER_SRC | MZ_IMAGE_USAGE_TRANSFER_DST) 
		};
	}

	UTextureRenderTarget2D* trt2d = Cast<UTextureRenderTarget2D>(prop->GetObjectPropertyValue(prop->ContainerPtrToValuePtr<UTextureRenderTarget2D>(obj)));

	if (!trt2d)
	{
		return mzTextureInfo{
			.Width = 1600,
			.Height = 900,
			.Format = mzFormat::MZ_FORMAT_R16G16B16A16_SFLOAT,
			.Usage = (mzImageUsage)(MZ_IMAGE_USAGE_RENDER_TARGET | MZ_IMAGE_USAGE_SAMPLED | MZ_IMAGE_USAGE_TRANSFER_SRC | MZ_IMAGE_USAGE_TRANSFER_DST)
		};
	}

	mzTextureInfo info = {
		.Width = (uint32_t)trt2d->GetSurfaceWidth(),
		.Height = (uint32_t)trt2d->GetSurfaceHeight(),
		.Usage = (mzImageUsage)(MZ_IMAGE_USAGE_RENDER_TARGET | MZ_IMAGE_USAGE_SAMPLED | MZ_IMAGE_USAGE_TRANSFER_SRC | MZ_IMAGE_USAGE_TRANSFER_DST),
	};

	switch (trt2d->RenderTargetFormat)
	{
	case ETextureRenderTargetFormat::RTF_R8:
		info.Format = MZ_FORMAT_R8_UNORM;
		break;
	case ETextureRenderTargetFormat::RTF_RG8:
		info.Format = MZ_FORMAT_R8G8_UNORM;
		break;
	case ETextureRenderTargetFormat::RTF_RGBA8:
		info.Format = MZ_FORMAT_R8G8B8A8_UNORM;
		break;
	case ETextureRenderTargetFormat::RTF_RGBA8_SRGB:
		info.Format = MZ_FORMAT_R8G8B8A8_SRGB;
		break;

	case ETextureRenderTargetFormat::RTF_R16f:
		info.Format = MZ_FORMAT_R16_SFLOAT;
		break;
	case ETextureRenderTargetFormat::RTF_RG16f:
		info.Format = MZ_FORMAT_R16G16_SFLOAT;
		break;
	case ETextureRenderTargetFormat::RTF_RGBA16f:
		info.Format = MZ_FORMAT_R16G16B16A16_SFLOAT;
		break;

	case ETextureRenderTargetFormat::RTF_R32f:
		info.Format = MZ_FORMAT_R32_SFLOAT;
		break;
	case ETextureRenderTargetFormat::RTF_RG32f:
		info.Format = MZ_FORMAT_R32G32_SFLOAT;
		break;
	case ETextureRenderTargetFormat::RTF_RGBA32f:
		info.Format = MZ_FORMAT_R32G32B32A32_SFLOAT;
		break;

	case ETextureRenderTargetFormat::RTF_RGB10A2:
		info.Format = MZ_FORMAT_A2R10G10B10_UNORM_PACK32;
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
	ResourceInfo copyInfo;
	mz::fb::TTexture texture;

	if(!CreateTextureResource(mzprop, texture, copyInfo))
	{
		return texture;
	}
	

	{
		//start property pins as output pins
		Copies.Add(mzprop, copyInfo);
	}
	return texture;
}

bool MZTextureShareManager::CreateTextureResource(MZProperty* mzprop, mz::fb::TTexture& Texture, ResourceInfo& Resource)
	{
	mzTextureInfo info = GetResourceInfo(mzprop);
	UObject* obj = mzprop->GetRawObjectContainer();
	FObjectProperty* prop = CastField<FObjectProperty>(mzprop->Property);
	UTextureRenderTarget2D* trt2d = Cast<UTextureRenderTarget2D>(prop->GetObjectPropertyValue(prop->ContainerPtrToValuePtr<UTextureRenderTarget2D>(obj)));
	if(!trt2d)
	{
		mzprop->IsOrphan = true;
		mzprop->OrphanMessage = "No texture resource bound to property!";
		return false;
	}
	
	UTextureRenderTarget2D* NewRenderTarget2D = NewObject<UTextureRenderTarget2D>(GetTransientPackage(), *(mzprop->DisplayName +FGuid::NewGuid().ToString()), RF_MarkAsRootSet);
	check(NewRenderTarget2D);
	NewRenderTarget2D->Rename(*(mzprop->DisplayName +FGuid::NewGuid().ToString()));
	NewRenderTarget2D->RenderTargetFormat = trt2d->RenderTargetFormat;
	NewRenderTarget2D->ClearColor = trt2d->ClearColor;
	NewRenderTarget2D->bAutoGenerateMips = 0;
	NewRenderTarget2D->bCanCreateUAV = true;
	NewRenderTarget2D->bGPUSharedFlag = true;
	NewRenderTarget2D->InitAutoFormat(info.Width, info.Height);	
	NewRenderTarget2D->UpdateResourceImmediate(true);
	FlushRenderingCommands();
	
	auto rt = NewRenderTarget2D->GameThread_GetRenderTargetResource();
	//if (!rt) return false;
	auto RHIResource = rt->GetTexture2DRHI();
	if (!RHIResource || !RHIResource->IsValid())
	{
		return false;
	}
	FRHITexture* RHITexture = RHIResource;
	FD3D12Texture* Result((FD3D12Texture*)RHITexture->GetTextureBaseRHI());
	
	ID3D12Resource* DXResource = Result->GetResource()->GetResource();
    DXResource->SetName(*mzprop->DisplayName);
	
    HANDLE handle;
    MZ_D3D12_ASSERT_SUCCESS(Dev->CreateSharedHandle(DXResource, 0, GENERIC_ALL, 0, &handle));
	
	Texture.size = mz::fb::SizePreset::CUSTOM;
	Texture.width = info.Width;
	Texture.height = info.Height;
	Texture.format = mz::fb::Format(info.Format);
	Texture.usage = mz::fb::ImageUsage(info.Usage) | mz::fb::ImageUsage::SAMPLED;
	Texture.type = 0x00000040;
	Texture.memory = (u64)handle;
	Texture.pid = FPlatformProcess::GetCurrentProcessId();
	Texture.unmanaged = true;
	Texture.unscaled = true;
	Texture.offset = 0;
	Texture.handle = 0;
	Texture.semaphore = 0;

	Resource.SrcMzp = mzprop;
	Resource.DstResource = NewRenderTarget2D;
	Resource.ShowAs = mzprop->PinShowAs;
	return true;
}


void MZTextureShareManager::UpdateTexturePin(MZProperty* mzprop, mz::fb::ShowAs RealShowAs)
{
	UpdatePinShowAs(mzprop, RealShowAs);
}

bool MZTextureShareManager::UpdateTexturePin(MZProperty* MzProperty, mz::fb::TTexture& Texture)
{
	mzTextureInfo info = GetResourceInfo(MzProperty);

	auto resourceInfo = Copies.Find(MzProperty);
	if (resourceInfo == nullptr)
		return false;

	if (Texture.pid != (uint64_t)FPlatformProcess::GetCurrentProcessId())
		return false;

	bool changed = false;

	mz::fb::Format fmt = mz::fb::Format(info.Format);
	mz::fb::ImageUsage usage = mz::fb::ImageUsage(info.Usage) | mz::fb::ImageUsage::SAMPLED;

	if (Texture.width != info.Width ||
		Texture.height != info.Height ||
		Texture.format != fmt ||
		Texture.usage != usage)
	{
		changed = true;
		
		ResourcesToDelete.Enqueue({resourceInfo->DstResource, GFrameCounter});
		mz::fb::ShowAs tmp = resourceInfo->ShowAs;
		if(!CreateTextureResource(MzProperty, Texture, *resourceInfo))
		{
			return changed;
		}
		
		resourceInfo->ShowAs = tmp;
		Copies[MzProperty] = *resourceInfo;
	}

	return changed;
}

void MZTextureShareManager::UpdatePinShowAs(MZProperty* MzProperty, mz::fb::ShowAs NewShowAs)
{
	if(Copies.Contains(MzProperty))
	{
		auto resourceInfo = Copies.Find(MzProperty);
		resourceInfo->ShowAs = NewShowAs;
	}
}

void MZTextureShareManager::TextureDestroyed(MZProperty* textureProp)
{
	Copies.Remove(textureProp);
	
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

void FilterCopies(mz::fb::ShowAs FilterShowAs, TMap<MZProperty*, ResourceInfo>& Copies, TMap<UTextureRenderTarget2D*, ResourceInfo>& FilteredCopies)
{
	for (auto [mzprop, info] : Copies)
	{
		UObject* obj = mzprop->GetRawObjectContainer();
		if (!obj) continue;
		auto prop = CastField<FObjectProperty>(mzprop->Property);
		if (!prop) continue;
		auto URT = Cast<UTextureRenderTarget2D>(prop->GetObjectPropertyValue(prop->ContainerPtrToValuePtr<UTextureRenderTarget2D>(obj)));
		if (!URT) continue;
		
		 if(info.DstResource->SizeX != URT->SizeX || info.DstResource->SizeY != URT->SizeY)
		 {
		 	//todo texture is changed update it
		 	
			const mz::fb::Texture* tex = flatbuffers::GetRoot<mz::fb::Texture>(mzprop->data.data());
			mz::fb::TTexture texture;
			tex->UnPackTo(&texture);

		 	auto TextureShareManager = MZTextureShareManager::GetInstance();
			if (TextureShareManager->UpdateTexturePin(mzprop, texture))
			{
				// data = mz::Buffer::From(texture);
				flatbuffers::FlatBufferBuilder fb;
				auto offset = mz::fb::CreateTexture(fb, &texture);
				fb.Finish(offset);
				mz::Buffer buffer = fb.Release();
				mzprop->data = buffer;
				
				if (!TextureShareManager->MZClient->IsConnected() || mzprop->data.empty())
				{
					return;
				}
				
				flatbuffers::FlatBufferBuilder mb;
				auto offset2 = mz::CreatePinValueChangedDirect(mb, (mz::fb::UUID*)&mzprop->Id, &mzprop->data);
				mb.Finish(offset2);
				auto buf = mb.Release();
				auto root = flatbuffers::GetRoot<mz::PinValueChanged>(buf.data());
				TextureShareManager->MZClient->AppServiceClient->NotifyPinValueChanged(*root);
			}
		 }
			
		if(info.ShowAs == FilterShowAs)
		{
			FilteredCopies.Add(URT, info);
		}
	}
}

void MZTextureShareManager::SetupFences(FRHICommandListImmediate& RHICmdList, mz::fb::ShowAs CopyShowAs,
	TMap<ID3D12Fence*, u64>& SignalGroup)
{
	if(ExecutionState == mz::app::ExecutionState::SYNCED)
	{
		if(CopyShowAs == mz::fb::ShowAs::INPUT_PIN)
		{
			RHICmdList.EnqueueLambda([CmdQueue = CmdQueue,InputFence = InputFence, FrameCounter = FrameCounter](FRHICommandList& ExecutingCmdList)
			{
				GetID3D12DynamicRHI()->RHIWaitManualFence(ExecutingCmdList, InputFence, (2 * FrameCounter) + 1);
			});
			SignalGroup.Add(InputFence, (2 * FrameCounter) + 2);

#ifdef DEBUG_FRAME_SYNC_LOG
			UE_LOG(LogTemp, Warning, TEXT("Input pins are waiting on %d") , 2 * FrameCounter + 1);
#endif
			
		}
		else if (CopyShowAs == mz::fb::ShowAs::OUTPUT_PIN)
		{
			RHICmdList.EnqueueLambda([CmdQueue = CmdQueue, OutputFence = OutputFence, FrameCounter = FrameCounter](FRHICommandList& ExecutingCmdList)
			{
				
				GetID3D12DynamicRHI()->RHIWaitManualFence(ExecutingCmdList, OutputFence, (2 * FrameCounter));
			});
			SignalGroup.Add(OutputFence, (2 * FrameCounter) + 1);

#ifdef DEBUG_FRAME_SYNC_LOG
			UE_LOG(LogTemp, Warning, TEXT("Out pins are waiting on %d") , 2 * FrameCounter);
#endif
			FrameCounter++;
		}
	}
}

void MZTextureShareManager::ProcessCopies(mz::fb::ShowAs CopyShowAs, TMap<MZProperty*, ResourceInfo>& CopyMap)
{
	{
		if (CopyMap.IsEmpty())
		{
			return;
		}
	}
	TMap<UTextureRenderTarget2D*, ResourceInfo> CopiesFiltered;
	FilterCopies(CopyShowAs, CopyMap, CopiesFiltered);

	//auto cmdData = GetNewCommandList();
	ENQUEUE_RENDER_COMMAND(FMZClient_CopyOnTick)(
		[this, CopyShowAs, CopiesFiltered](FRHICommandListImmediate& RHICmdList)
		{
			if (CopyShowAs == mz::fb::ShowAs::OUTPUT_PIN)
			{
				FString EventLabel("MediaZ Output Copies");
				RHICmdList.PushEvent(*EventLabel, FColor::Red);
			}
			else
			{
				FString EventLabel("MediaZ Input Copies");
				RHICmdList.PushEvent(*EventLabel, FColor::Red);
			}
			TMap<ID3D12Fence*, u64> SignalGroup;
			SetupFences(RHICmdList, CopyShowAs, SignalGroup);
			for (auto& [URT, pin] : CopiesFiltered)
			{
				FRHICopyTextureInfo CopyInfo;
				CopyInfo.Size = FIntVector(pin.DstResource->SizeX, pin.DstResource->SizeY, 1);
				FRHITexture* dst = pin.DstResource->GetRenderTargetResource()->GetRenderTargetTexture();
				FRHITexture* src = URT->GetRenderTargetResource()->GetRenderTargetTexture();
				if(CopyShowAs == mz::fb::ShowAs::INPUT_PIN)
				{
					Swap(dst, src);
				}
				RHICmdList.CopyTexture(src, dst, CopyInfo);
			}
			for(auto& [fence, val] : SignalGroup)
			{
				RHICmdList.EnqueueLambda([CmdQueue = CmdQueue, fence, val](FRHICommandList& ExecutingCmdList)
				{
					GetID3D12DynamicRHI()->RHISignalManualFence(ExecutingCmdList, fence, val);
				});
			}
			RHICmdList.PopEvent();
		});
}

void MZTextureShareManager::OnBeginFrame()
{
	ProcessCopies(mz::fb::ShowAs::INPUT_PIN, Copies);
}

void MZTextureShareManager::OnEndFrame()
{
	ProcessCopies(mz::fb::ShowAs::OUTPUT_PIN, Copies);

	while(!ResourcesToDelete.IsEmpty())
	{
		TPair<TObjectPtr<UTextureRenderTarget2D>, uint32_t> resource;
		ResourcesToDelete.Peek(resource);
		if(resource.Value + 5 <= GFrameCounter) // resources are deleted after 5 frames, because we need to make sure that they are no longer in use
		{
			FlushRenderingCommands();
			resource.Key->ReleaseResource();
			resource.Key = nullptr;
			ResourcesToDelete.Pop();
		}
		else
		{
			break;
		}
	}
	// ENQUEUE_RENDER_COMMAND(FMZClient_CopyOnTick)(
	// 	[this, FrameCount = GFrameCounter](FRHICommandListImmediate& RHICmdList)
	// 	{
	// 	});
}

bool MZTextureShareManager::SwitchStateToSynced()
{
	RenewSemaphores();
	ENQUEUE_RENDER_COMMAND(FMZClient_CopyOnTick)(
		[this](FRHICommandListImmediate& RHICmdList)
		{
			ExecutionState = mz::app::ExecutionState::SYNCED;
		});

	return true;
}

void MZTextureShareManager::SwitchStateToIdle_GRPCThread(u64 LastFrameNumber)
{
	ExecutionState = mz::app::ExecutionState::IDLE;
	for(int i = 0; i < 2; i++)
	{
		if (InputFence && OutputFence)
		{
			InputFence->Signal(UINT64_MAX);
			OutputFence->Signal(UINT64_MAX);
		}
		FPlatformProcess::Sleep(0.2);
	}
	FrameCounter = 0;
}

void MZTextureShareManager::SwitchStateToIdle()
{
	if (InputFence && OutputFence)
	{
		InputFence->Signal(UINT64_MAX);
		OutputFence->Signal(UINT64_MAX);
	}
	FrameCounter = 0;
}

void MZTextureShareManager::Reset()
{
	Copies.Empty();
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
	
	
#ifdef FAIL_SAFE_THREAD 
			FailSafeRunnable = new MZGPUFailSafeRunnable(CmdQueue, Dev);
			FailSafeThread = FRunnableThread::Create(FailSafeRunnable, TEXT("MZGPUFailSafeThread"));

			FCoreDelegates::OnEnginePreExit.AddLambda([this]()
			{
				if(FailSafeRunnable && FailSafeThread)
				{
					FailSafeRunnable->Stop();
					FailSafeThread->WaitForCompletion();
				}
			});
#endif
	
	RenewSemaphores();
}

void MZTextureShareManager::RenewSemaphores()
{
	if (InputFence)
	{
		::CloseHandle(SyncSemaphoresExportHandles.InputSemaphore);
		InputFence->Release();

	}
	if (OutputFence)
	{
		::CloseHandle(SyncSemaphoresExportHandles.OutputSemaphore);
		OutputFence->Release();
	}

	FrameCounter = 0;
	
	Dev->CreateFence(0, D3D12_FENCE_FLAG_SHARED, IID_PPV_ARGS(&InputFence));
	Dev->CreateFence(0, D3D12_FENCE_FLAG_SHARED, IID_PPV_ARGS(&OutputFence));
	MZ_D3D12_ASSERT_SUCCESS(Dev->CreateSharedHandle(InputFence, 0, GENERIC_ALL, 0, &SyncSemaphoresExportHandles.InputSemaphore));
	MZ_D3D12_ASSERT_SUCCESS(Dev->CreateSharedHandle(OutputFence, 0, GENERIC_ALL, 0, &SyncSemaphoresExportHandles.OutputSemaphore));
}

	
	
