#pragma once
#include "_ImmutableFreeform.h"
#include "_MutableFreeform.h"


/*
_FreeformImpl
	manage freeform light

_ImmutableFreeform
	immutable freeform. it uses for game

_MutableFreeform
	mutable freeform. it uses for editor

_FreeformImpl
	common implementation of freeform

_ImmutableLightImpl
	immutable freeform implementation. it uses for game

_MutableLightImpl
	mutable freeform implementation. it uses for editor
*/

using CImmutableFreeformLight = FreeformLight::_ImmutableFreeform;
using CMutableFreeformLight = FreeformLight::_MutableFreeform;