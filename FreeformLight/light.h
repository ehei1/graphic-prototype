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
	가변 프리폼 조명 구현. 편집 환경에서 쓰임
*/
// TODO: 주석 달기
// TODO: 조명 색을 diffuse로 적용. 현재 방식은 색깔이 바뀌면 텍스처를 만들어야 해서 편집 시 불리하다. 또한 조명 간에 마스크 텍스처를 공유할 수 있는 수단은 diffuse 이용 뿐이다.
// TODO: 블러 마스크를 비동기로 갱신

// 게임에서 쓰이는 프리폼 조명. 편집 불가
using CImmutableFreeformLight = FreeformLight::_ImmutableFreeform;
// 편집 환경에서 쓰이는 프리폼 조명. 편집 가능
using CMutableFreeformLight = FreeformLight::_MutableFreeform;