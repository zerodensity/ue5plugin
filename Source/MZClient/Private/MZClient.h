#pragma once

#include "IMZClient.h"
#include "CoreMinimal.h"

/**
 * Implements communication with the MediaZ server
 */
class MZCLIENT_API FMZClient : public IMZClient {
 public:
	 FMZClient();

	 virtual void StartupModule() override;
	 virtual void ShutdownModule() override;

 private:
	 bool Connect();

	 uint32 Run();
	 virtual void SendNodeUpdate(MZEntity) override;
 private:
  struct ClientImpl* Client;
};
