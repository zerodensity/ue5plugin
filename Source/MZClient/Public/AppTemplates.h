#pragma once

#include "AppClient.h"
#include <mzFlatBuffersCommon.h>

using MessageBuilder = flatbuffers::grpc::MessageBuilder;

template<class T> requires((u32)mz::app::AppEventUnionTraits<T>::enum_value != 0)
static flatbuffers::grpc::Message<mz::app::AppEvent> MakeAppEvent(MessageBuilder& b, flatbuffers::Offset<T> event)
{
	b.Finish(mz::app::CreateAppEvent(b, mz::app::AppEventUnionTraits<T>::enum_value, event.Union()));
	auto msg = b.ReleaseMessage<mz::app::AppEvent>();
	assert(msg.Verify());
	return msg;
}

template <class T> requires((u32)mz::app::AppEventUnionTraits<T>::enum_value != 0)
static flatbuffers::Offset<mz::app::AppEvent> CreateAppEventOffset(flatbuffers::FlatBufferBuilder& b, flatbuffers::Offset<T> event)
{
	return mz::app::CreateAppEvent(b, mz::app::AppEventUnionTraits<T>::enum_value, event.Union());
}
