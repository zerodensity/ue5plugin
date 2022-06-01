#pragma once

#include "Modules/ModuleInterface.h"


namespace google::protobuf
{
	class Message;
}


namespace mz::proto
{

}


DECLARE_LOG_CATEGORY_EXTERN(LogMZProto, Log, All);
class IMZProto : public IModuleInterface {};
