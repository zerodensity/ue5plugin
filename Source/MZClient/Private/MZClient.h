#pragma once
#include "CoreMinimal.h"
#include "HAL/Runnable.h"

/**
 * Implements communication with the MediaZ server
 */
class FMediaZClient : public FRunnable {
 public:
  FMediaZClient();

  void Start();

  void Stop();

 private:
  bool Connect();

  uint32 Run();

 private:
  class FRunnableThread* Thread;
};
