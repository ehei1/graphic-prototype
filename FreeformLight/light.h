#pragma once
#include "_ImmutableFreeform.h"
#include "_MutableFreeform.h"


/*
_FreeformImpl
	������ ������ �����Ѵ�

_ImmutableFreeform
	�Һ� ������ ����. ���� ȯ�濡�� ����

_MutableFreeform
	���� ������ ����. ���� ȯ�濡�� ����

_FreeformImpl
	������ ������ ���� ����

_ImmutableLightImpl
	������ �⺻ ����. ���� ȯ�濡�� ����

_MutableLightImpl
	���� ������ ���� ����. ���� ȯ�濡�� ����
*/
// TODO: �ּ� �ޱ�
// TODO: ���� ���� diffuse�� ����. ���� ����� ������ �ٲ�� �ؽ�ó�� ������ �ؼ� ���� �� �Ҹ��ϴ�. ���� ���� ���� ����ũ �ؽ�ó�� ������ �� �ִ� ������ diffuse �̿� ���̴�.
// TODO: �� ����ũ�� �񵿱�� ����

// ���ӿ��� ���̴� ������ ����. ���� �Ұ�
using CImmutableFreeformLight = FreeformLight::_ImmutableFreeform;
// ���� ȯ�濡�� ���̴� ������ ����. ���� ����
using CMutableFreeformLight = FreeformLight::_MutableFreeform;