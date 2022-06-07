#pragma once

#include "IMZClient.h"
#include "CoreMinimal.h"

#include <queue>

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
	 virtual void Disconnect() override;


	 static size_t HashTextureParams(uint32_t width, uint32_t height, uint32_t format, uint32_t usage);
	
	 virtual void QueueTextureCopy(struct ID3D12Resource* res) override;
	 virtual void OnTextureReceived(mz::proto::Texture const& texture) override;
 private:
	 class ClientImpl* Client;
	 std::unordered_map<size_t, std::queue<struct ID3D12Resource*>> CopyQueue;
};
