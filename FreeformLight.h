#pragma once
#include <vector>
#include <d3dx9.h>


// ������ ������ ���� ����
class CFreeformLight
{
public:
	HRESULT AddLight( LPDIRECT3DDEVICE9, float x, float y );
	HRESULT RemoveLight();
	HRESULT Draw( LPDIRECT3DDEVICE9, LONG xCenter, LONG yCenter );
	HRESULT RestoreDevice( LPDIRECT3DDEVICE9, const D3DDISPLAYMODE& );
	void InvalidateDeviceObjects();
	inline void Uninitialize() { InvalidateDeviceObjects(); }

private:
	// ������ ������ ���� �ؽ�ó�� �����
	// TODO: �ؽ�ó�� ���� �� �������� ������ �����. DX9 ����� ����� �Ѽ� Ȯ���غ���. 
	//
	// pTexture: ������ �ؽ�ó�� ���� ������
	HRESULT CreateLightTextureByRenderer( LPDIRECT3DDEVICE9, LPDIRECT3DTEXTURE9* pTexture ) const;
	// �ſ� �������� ���� �Լ��� ��ĥ ������ ����Ѵ�. ���ҽ��� ������ �ҽ� ������ ���� �������� �õ�
	HRESULT CreateLightTextureByLockRect( LPDIRECT3DDEVICE9, LPDIRECT3DTEXTURE9* pTexture ) const;

	HRESULT CreateMaskMesh( LPDIRECT3DDEVICE9, LPD3DXMESH* ) const;
	HRESULT CreateMaskTexture( LPDIRECT3DDEVICE9, LPDIRECT3DTEXTURE9* ) const;
	HRESULT DrawHelper( LPDIRECT3DDEVICE9 ) const;
	HRESULT DrawMask( LPDIRECT3DDEVICE9 ) const;
	HRESULT DrawLightMask( LPDIRECT3DDEVICE9 )const;

private:
	struct CUSTOMVERTEX_MASK {
		D3DXVECTOR3 position;
		D3DXVECTOR2 uv;
	};
	// ������ ��� �Ѳ����� �׸����� ȭ�鸸�� �޽�. �� ���� ���� ȭ�鿡 ����ũó�� ������ ���� ȿ���� ����
	LPD3DXMESH m_pScreenMesh = {};
	// �ؽ�ó
	LPDIRECT3DTEXTURE9 m_pScreenTexture = {};

	// ���� ������ ���� ����
	// �ؽ�ó
	LPDIRECT3DTEXTURE9 m_pLightTexture = {};
	// ����
	LPDIRECT3DINDEXBUFFER9 m_pLightIndexBuffer = {};
	LPDIRECT3DVERTEXBUFFER9 m_pLightVertexBuffer = {};
	// ���ؽ� ����
	UINT m_lightVertexCount = {};
	UINT m_lightPrimitiveCount = {};

	const DWORD m_fvf = D3DFVF_XYZ | D3DFVF_TEX1;

	std::vector< D3DXVECTOR3 > m_topSideVertices{
		{ -1.f, +1.f, -0.f },
	};
	std::vector< D3DXVECTOR3 > m_rightSideVectices{
		{ +1.0f, +1.f, 0.f },
		{ +1.5f,  0.f, 0.f },
	};
	std::vector< D3DXVECTOR3 > m_bottomSideVertices{
		{ +1.f, -1.f, 0.f },
	};
	std::vector< D3DXVECTOR3 > m_leftSideVertices{
		{ -1.0f, -1.f, 0.f },
		{ -1.5f, -1.f, 0.f },
		{ -1.0f,  0.f, 0.f },
	};
	D3DDISPLAYMODE m_displayMode = {};
	D3DXCOLOR m_maskColor = D3DCOLOR_ARGB( 255, 255, 0, 0 );

	// ����̷� ǥ�õǴ� ����
	using HelperVertices = std::vector<D3DXVECTOR3>;
	HelperVertices m_helperVectics;
};