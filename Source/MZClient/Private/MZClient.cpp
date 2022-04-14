
#include "MZClient.h"
#include "HAL/RunnableThread.h"

#include "DispelUnrealMadnessPrelude.h"

#include "TextureStream.grpc.pb.h"

#define LOCTEXT_NAMESPACE "MediazClient"

FMediaZClient::FMediaZClient() {}

void FMediaZClient::Start() {
  if (Thread == nullptr) {
    Thread =
        FRunnableThread::Create(this, TEXT("MediaZClient"), 0, TPri_Lowest);
  }
}

void FMediaZClient::Stop() {

  if (Thread) {
    Thread->WaitForCompletion();
  }
}


bool FMediaZClient::Connect() {

  uint32_t iWidth = 1;
  uint32_t iHeight = 1;
  IConsoleVariable* MediaZOutputConsoleVar =
      IConsoleManager::Get().FindConsoleVariable(TEXT("r.MediaZOutput"));
  switch (MediaZOutputConsoleVar->GetInt()) {
    case 1: {
      iWidth = 1280;
      iHeight = 720;
      break;
    }

    case 3: {
      iWidth = 3840;
      iHeight = 2160;
      break;
    }

    case 2:
    default: {
      iWidth = 1920;
      iHeight = 1080;
      break;
    }
  }
  return true;
}

uint32 FMediaZClient::Run() {
  return 0;
}

#undef LOCTEXT_NAMESPACE

#include "DispelUnrealMadnessPostlude.h"