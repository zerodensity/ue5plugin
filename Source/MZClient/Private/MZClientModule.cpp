// Copyright Epic Games, Inc. All Rights Reserved.

#include "Core.h"
#include "Interfaces/IPluginManager.h"
#include "Modules/ModuleManager.h"

#include "IMZClient.h"
#include "MZClient.h"

#include <vulkan/vulkan_core.h>

#include <memory>

template <class T>
using rc = std::shared_ptr<T>;

template <class T>
using box = std::unique_ptr<T>;

#include <grpcpp/grpcpp.h>

#include "DispelUnrealMadnessPrelude.h"
#include "generated/TextureStream.grpc.pb.h"

#define LOCTEXT_NAMESPACE "FMZClientModule"

struct MZClient {
  MZClient(rc<grpc::Channel> channel)
      : Channel(channel),
        Stub(mz::vk::stream::TextureStream::NewStub(channel)) {}

  rc<grpc::Channel> Channel;
  box<mz::vk::stream::TextureStream::Stub> Stub;

  uint64_t Connect(const mz::vk::stream::TextureStreamRequest* request,
                   std::vector<mz::proto::Texture>& textures) {
    grpc::ClientContext ctx;
    mz::vk::stream::TextureStreamResponse response;
    Stub->Connect(&ctx, *request, &response);

    textures.clear();

    for (mz::proto::Texture texture : response.textures()) {
      textures.emplace_back(texture);
    }

    return response.stream_handle();
  }

  uint32_t AcquireImage(uint64_t stream_handle) {
    grpc::ClientContext ctx;
    mz::vk::stream::AcquireImageResponse response;
    mz::vk::stream::AcquireImageRequest request;
    request.set_stream_handle(stream_handle);
    Stub->AcquireImage(&ctx, request, &response);
    return response.texture_index();
  }
};

struct MZStream : IMZStream {
  uint64_t Handle;
  std::vector<mz::proto::Texture> Textures;
};

class FMZClientModule : public IMZClient {
 public:
  MZClient* Client;

  std::vector<MZStream*> Streams;

  void StartupModule() override {
    Client = new MZClient(grpc::CreateChannel(
        "localhost:6969", grpc::InsecureChannelCredentials()));

    FMessageDialog::Debugf(FText::FromString("Loaded MZClient module"), 0);
  }

  IMZStream* RequestStream(uint32_t Width,
                           uint32_t Height,
                           EPixelFormat ImageFormat,
                           ETextureCreateFlags ImageUsage,
                           uint32_t ImageCount,
                           IMZStream::Type StreamType,
                           uint64_t DeviceId) override {
    mz::vk::stream::TextureStreamRequest stream;
    stream.set_width(Width);
    stream.set_width(Height);
    stream.set_image_format(ImageFormat);
    stream.set_image_count(ImageCount);
    stream.set_device_id(DeviceId);

    switch (StreamType) {
      case IMZStream::Input:
        stream.set_stream_type(mz::vk::stream::StreamType::Input);
        break;
      case IMZStream::Output:
        stream.set_stream_type(mz::vk::stream::StreamType::Output);
        break;
    }

    auto None = ETextureCreateFlags::None;
    uint32_t ImageUsageFlags = 0;
    if (None != (ImageUsage & ETextureCreateFlags::RenderTargetable))
      ImageUsageFlags |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    if (None != (ImageUsage & ETextureCreateFlags::ShaderResource))
      ImageUsageFlags |= VK_IMAGE_USAGE_SAMPLED_BIT;
    if (None != (ImageUsage & ETextureCreateFlags::UAV))
      ImageUsageFlags |= VK_IMAGE_USAGE_SAMPLED_BIT;
    if (None != (ImageUsage & ETextureCreateFlags::Presentable))
      ImageUsageFlags |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    stream.set_image_usage(ImageUsageFlags);

    MZStream* pStream = new MZStream();
    Streams.push_back(pStream);
    pStream->Handle = Client->Connect(&stream, pStream->Textures);
    return pStream;
  }

  void ShutdownModule() override {
    for (auto stream : Streams) {
      delete stream;
    }
    delete Client;
  }
};

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FMZClientModule, MZClient)
#include "DispelUnrealMadnessPostlude.h"