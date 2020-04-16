#pragma once
#include <vector>
#include <d3dx9.h>


// 프리폼 조명을 위한 정보
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
	// 프리폼 조명을 위한 텍스처를 만든다
	// TODO: 텍스처를 만든 후 렌더러가 동작을 멈춘다. DX9 디버그 기능을 켜서 확인해보기. 
	//
	// pTexture: 생성할 텍스처를 담을 포인터
	HRESULT CreateLightTextureByRenderer( LPDIRECT3DDEVICE9, LPDIRECT3DTEXTURE9* pTexture ) const;
	// 매우 느리지만 위의 함수를 고칠 때까지 사용한다. 리소스를 가능한 소스 폴더에 넣지 않으려는 시도
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
	// 조명을 모아 한꺼번에 그리려는 화면만한 메시. 그 다음 게임 화면에 마스크처럼 씌워서 조명 효과를 낸다
	LPD3DXMESH m_pScreenMesh = {};
	// 텍스처
	LPDIRECT3DTEXTURE9 m_pScreenTexture = {};

	// 개별 프리폼 조명 정보
	// 텍스처
	LPDIRECT3DTEXTURE9 m_pLightTexture = {};
	// 버퍼
	LPDIRECT3DINDEXBUFFER9 m_pLightIndexBuffer = {};
	LPDIRECT3DVERTEXBUFFER9 m_pLightVertexBuffer = {};
	// 버텍스 개수
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

	// 도우미로 표시되는 정점
	using HelperVertices = std::vector<D3DXVECTOR3>;
	HelperVertices m_helperVectics;
};