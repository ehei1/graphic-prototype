#pragma once
#include "_FreeformImpl.h"
#include "_ImmutableLightImpl.h"


/*
���� �Ұ����� ����
*/
namespace FreeformLight
{
	class _ImmutableFreeform : public _FreeformImpl<_ImmutableLightImpl>
	{};
}