#pragma once
#include "_FreeformImpl.h"
#include "_ImmutableLightImpl.h"


/*
it's unchangable light
*/
namespace FreeformLight
{
	class _ImmutableFreeform : public _FreeformImpl<_ImmutableLightImpl>
	{};
}