#pragma once

#if WITH_EDITOR

#include "Engine/TextureRenderTarget2D.h"
#pragma warning (disable : 4800)
#pragma warning (disable : 4668)

void MemoryBarrier();
#include "Windows/AllowWindowsPlatformTypes.h"
#pragma intrinsic(_InterlockedCompareExchange64)
#define InterlockedCompareExchange64 _InterlockedCompareExchange64
#include <d3d12.h>
#include "Windows/HideWindowsPlatformTypes.h"

#include <shared_mutex>
#include "MediaZ/MediaZ.h"

#include "MZActorProperties.h"
#include "MediaZ/AppInterface.h" 
#include <mzFlatBuffersCommon.h>
#include "MZClient.h"
#include "RHI.h"

struct ResourceInfo
{
	MZProperty* SrcMzp = 0;
	ID3D12Resource* DstResource = 0;
	bool ReadOnly = true;
	MzTextureShareInfo Info = {};
	void Release()
	{
		if (DstResource) DstResource->Release();
		memset(this, 0, sizeof(*this));
	}
};

//This class manages copy operations between textures of MediaZ and unreal 2d texture target
class MZTextureShareManager
{
//protected:
public:
	MZTextureShareManager();
	static MZTextureShareManager* singleton;

	static MZTextureShareManager* GetInstance();

	~MZTextureShareManager();
	
	void AddTexturePin(MZProperty*, mz::fb::Texture*);
	void UpdateTexturePin(MZProperty*, mz::fb::Texture*);
	//void AddToCopyQueue();
	//void RemoveFromCopyQueue();
	void Reset();
	void WaitCommands();
	void ExecCommands();
	void EnqueueCommands(mz::app::IAppServiceClient* client);
	void TextureDestroyed(MZProperty* texture);


	struct ID3D12Device* Dev;
	struct ID3D12CommandAllocator* CmdAlloc;
	struct ID3D12CommandQueue* CmdQueue;
	struct ID3D12GraphicsCommandList* CmdList;
	struct ID3D12Fence* CmdFence;
	HANDLE CmdEvent;
	uint64_t CmdFenceValue = 0;

	std::mutex PendingCopyQueueMutex;
	TMap<FGuid, MZProperty*> PendingCopyQueue;

	std::shared_mutex CopyOnTickMutex;
	TMultiMap<MZProperty*, ResourceInfo> CopyOnTick;

private:
	void Initiate();
};

#endif