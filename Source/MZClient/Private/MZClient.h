#pragma once

#include "IMZClient.h"
#include "CoreMinimal.h"
#include "Misc/MessageDialog.h"
#include "RemoteControlPreset.h"
#include "Engine/TextureRenderTarget2D.h"
#include "IRemoteControlPropertyHandle.h"
#include "RemoteControlPreset.h"
#include "D3D12RHIPrivate.h"
#include "D3D12RHI.h"
#include "D3D12Resources.h"

#include <queue>

#include "mediaz.h"

#pragma warning (disable : 4800)
#pragma warning (disable : 4668)

#include "DispelUnrealMadnessPrelude.h"
#include <d3d12.h>
#include "AppClient.h"
#include "DispelUnrealMadnessPostlude.h"

/**
 * Implements communication with the MediaZ server
 */
class MZCLIENT_API FMZClient : public IMZClient {

 public:

	 FMZClient();

	 virtual void StartupModule() override;
	 virtual void ShutdownModule() override;
	 
	 bool Connect();

	 uint32 Run();

	 virtual void OnNodeUpdateReceived(mz::proto::Node const&) override;

	 virtual void SendNodeUpdate(TMap<FGuid, MZEntity> const& entities) override;
	 virtual void SendPinRemoved(FGuid) override;
	 virtual void SendPinAdded(MZEntity) override;
	 virtual void SendPinValueChanged(MZEntity) override;
	 virtual void Disconnect() override;
	 virtual void NodeRemoved() override;
	 void ClearResources();

	 virtual void QueueTextureCopy(FGuid id, const MZEntity* entity, mz::proto::Pin* dyn) override;
	 virtual void OnTextureReceived(FGuid id, mz::proto::Texture const& texture) override;

     void InitRHI();
     
	 void InitConnection();

	 bool Tick(float dt);

	 std::atomic_bool bClientShouldDisconnect = false;

	 struct ResourceInfo
	 {
		 MZEntity SrcEntity = {};
		 ID3D12Resource* SrcResource = 0;
		 ID3D12Resource* DstResource = 0;
		 ID3D12Fence* Fence = 0;
		 void* Event = 0;
		 uint64_t FenceValue = 0;
		 void Release()
		 {
			 if (Fence && Event && (Fence->GetCompletedValue() < FenceValue))
			 {
				 WaitForSingleObject(Event, INFINITE);
			 }
			 if(SrcResource) SrcResource->Release();
			 if(DstResource) DstResource->Release();
			 if(Fence) Fence->Release();
			 if(Event) CloseHandle(Event);
			 memset(this, 0, sizeof(*this));
		 }
	 };

	 struct ID3D12Device* Dev;
	 struct ID3D12CommandAllocator* CmdAlloc;
	 struct ID3D12CommandQueue* CmdQueue;
	 struct ID3D12GraphicsCommandList* CmdList;

	 class ClientImpl* Client = 0;

	 std::mutex PendingCopyQueueMutex;
	 TMap<FGuid, MZEntity> PendingCopyQueue;

	 std::mutex CopyOnTickMutex;
	 TMap<FGuid, ResourceInfo> CopyOnTick;

	 std::mutex ResourceChangedMutex;
	 TMap<FGuid, MZEntity> ResourceChanged;
};

