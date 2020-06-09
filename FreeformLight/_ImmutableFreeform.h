#pragma once
#include "_FreeformImpl.h"
#include "_ImmutableLightImpl.h"


namespace FreeformLight
{
	class _ImmutableFreeform : public _FreeformImpl<_ImmutableLightImpl>
	{};
}