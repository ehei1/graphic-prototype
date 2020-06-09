#pragma once
#include "_ImmutableFreeform.h"
#include "_MutableFreeform.h"


/*
_FreeformImpl
	프리폼 조명을 관리한다

_ImmutableFreeform
	불변 프리폼 조명. 게임 환경에서 쓰임

_MutableFreeform
	가변 프리폼 조명. 편집 환경에서 쓰임

_FreeformImpl
	프리폼 조명의 공통 구현

_ImmutableLightImpl
	프리폼 기본 구현. 게임 환경에서 쓰임

_MutableLightImpl
	가변 프리폼 조명 구현. 편집 환경에서 으미
*/
// 게임에서 쓰이는 프리폼 조명. 편집 불가
using CImmutableFreeformLight = FreeformLight::_ImmutableFreeform;
// 편집 환경에서 쓰이는 프리폼 조명. 편집 가능
using CMutableFreeformLight = FreeformLight::_MutableFreeform;