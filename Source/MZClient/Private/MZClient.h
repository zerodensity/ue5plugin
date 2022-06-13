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
	 virtual void SendNodeUpdate(MZEntity) override;
	 virtual void SendPinRemoved(FGuid) override;
	 virtual void SendPinValueChanged(MZEntity) override;
	 virtual void Disconnect() override;
	 void ClearResources();

	 static size_t HashTextureParams(uint32_t width, uint32_t height, uint32_t format, uint32_t usage);
	
	 virtual void QueueTextureCopy(FGuid id, struct ID3D12Resource* res, mz::proto::Dynamic* dyn) override;
	 virtual void OnTextureReceived(FGuid id, mz::proto::Texture const& texture) override;

     void InitRHI();
     void InitConnection();
	 bool Tick(float dt);


	 struct ResourceInfo
	 {
		 ID3D12Resource* SrcResource;
		 ID3D12Resource* DstResource;
		 ID3D12Fence* Fence;
		 void* Event;
		 uint64_t FenceValue;
	 };

	 struct ID3D12Device* Dev;
	 struct ID3D12CommandAllocator* CmdAlloc;
	 struct ID3D12CommandQueue* CmdQueue;
	 struct ID3D12GraphicsCommandList* CmdList;

	 class ClientImpl* Client = 0;

	 std::mutex Mutex;
	 TMap<FGuid, ID3D12Resource*> PendingCopyQueue;
	 TMap<FGuid, ResourceInfo> CopyOnTick;
};

