#pragma once
#include "_FreeformImpl.h"
#include "_ImmutableLightImpl.h"


/*
편집 불가능한 조명
*/
namespace FreeformLight
{
	class _ImmutableFreeform : public _FreeformImpl<_ImmutableLightImpl>
	{};
}