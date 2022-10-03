#pragma once


#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"


class IMZClient : public IModuleInterface {

  virtual void Disconnect() = 0;
  virtual void NodeRemoved() = 0;
  virtual bool IsConnected() = 0;

};
