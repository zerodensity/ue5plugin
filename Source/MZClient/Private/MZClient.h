#pragma once

#include "IMZClient.h"
#include "CoreMinimal.h"
#include "Misc/MessageDialog.h"

#include <queue>

#include "mediaz.h"

#pragma warning (disable : 4800)
#pragma warning (disable : 4668)

#include "AppClient.h"

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
	 virtual void SendPinValueChanged(MZEntity) override;
	 virtual void Disconnect() override;

	 static size_t HashTextureParams(uint32_t width, uint32_t height, uint32_t format, uint32_t usage);
	
	 virtual void QueueTextureCopy(FGuid id, struct ID3D12Resource* res, mz::proto::Dynamic* dyn) override;
	 virtual void OnTextureReceived(FGuid id, mz::proto::Texture const& texture) override;

     void InitRHI();

     struct ID3D12Device* Dev;
     struct ID3D12CommandAllocator* CmdAlloc;
     struct ID3D12CommandQueue* CmdQueue;
     struct ID3D12GraphicsCommandList* CmdList;

	 class ClientImpl* Client;
	 TMap<FGuid, ID3D12Resource*> CopyQueue;
};

