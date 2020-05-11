#pragma once
#include <vector>
#include <d3dx9.h>


// 프리폼 조명을 위한 정보
class CFreeformLight
{
public:
	// 
	HRESULT AddLight( LPDIRECT3DDEVICE9, LONG x, LONG y );
	HRESULT RemoveLight();
	HRESULT UpdateLightVertex( WORD index, const D3DXVECTOR3& position );

	// 조명을 그린다
	//
	// pDevice: 조명을 그리는데 쓰는 장치
	// pSurface: 조명을 그릴 표면
	// x: 조명을 그릴 위치
	// y: 조명을 그릴 위치
	HRESULT Draw( LPDIRECT3DDEVICE9 pDevice, LPDIRECT3DSURFACE9 pSurface, float x, float y );
	HRESULT RestoreDevice( const D3DDISPLAYMODE& );
	void InvalidateDeviceObjects();
	inline void Uninitialize() { InvalidateDeviceObjects(); }
	inline bool IsVisible() const { return !!m_pLightVertexBuffer; }

	// 개발 용도의 imgui 창을 만든다
	void CreateImgui( LPDIRECT3DDEVICE9, LONG xCenter, LONG yCenter, bool& isVisible );

	struct Setting
	{
		D3DXCOLOR lightColor = D3DCOLOR_XRGB( 255, 255, 255 );
		D3DXCOLOR shadowColor = D3DCOLOR_XRGB( 0, 0, 0 );
		float intensity = 1.f;
		float fallOff = 1.f;
		bool maskOnly{};

		inline bool operator==( const Setting& setting ) const { return !memcmp( &setting, this, sizeof( setting ) ); }
		inline bool operator!=( const Setting& setting ) const { return !( *this == setting ); }
	};
	inline const Setting& GetSetting() const { return m_setting; }
	HRESULT SetSetting( LPDIRECT3DDEVICE9, const Setting& );

	static HRESULT CreateMaskMesh( LPDIRECT3DDEVICE9, LPD3DXMESH*, UINT width, UINT height );
	static HRESULT CreateMaskTexture( LPDIRECT3DDEVICE9, LPDIRECT3DTEXTURE9*, UINT width, UINT height );

private:
	// 프리폼 조명을 위한 텍스처를 만든다
	// TODO: 텍스처를 만든 후 렌더러가 동작을 멈춘다. DX9 디버그 기능을 켜서 확인해보기. 
	//
	// pTexture: 생성할 텍스처를 담을 포인터
	HRESULT CreateLightTextureByRenderer( LPDIRECT3DDEVICE9, LPDIRECT3DTEXTURE9* pTexture ) const;
	// 매우 느리지만 위의 함수를 고칠 때까지 사용한다. 리소스를 가능한 소스 폴더에 넣지 않으려는 시도
	HRESULT CreateLightTextureByLockRect( LPDIRECT3DDEVICE9, LPDIRECT3DTEXTURE9* pTexture, const Setting& ) const;

private:
	struct CUSTOM_VERTEX {
		D3DXVECTOR3 position;
		D3DXVECTOR2 uv;
	};

	// 개별 프리폼 조명 정보
	// 텍스처
	LPDIRECT3DTEXTURE9 m_pLightTexture{};
	// 버퍼
	LPDIRECT3DINDEXBUFFER9 m_pLightIndexBuffer{};
	LPDIRECT3DVERTEXBUFFER9 m_pLightVertexBuffer{};

	std::vector< D3DXVECTOR3 > m_topSideVertices{
		{ -1.f, +1.f, -0.f },
	};
	std::vector< D3DXVECTOR3 > m_rightSideVectices{
		{ +1.0f, +1.f, 0.f },
		//{ +1.5f,  0.f, 0.f },
	};
	std::vector< D3DXVECTOR3 > m_bottomSideVertices{
		{ +1.f, -1.f, 0.f },
	};
	std::vector< D3DXVECTOR3 > m_leftSideVertices{
		{ -1.0f, -1.f, 0.f },
		//{ -1.5f, -1.f, 0.f },
		//{ -1.0f,  0.f, 0.f },
	};
	D3DDISPLAYMODE m_displayMode{};

	Setting m_setting{};
	POINT m_position{};

	const int m_lightVertexFvf = D3DFVF_XYZ | D3DFVF_TEX1;

	using Indices = std::vector<WORD>;
	Indices m_lightIndices;

	using Vertices = std::vector<CUSTOM_VERTEX>;
	Vertices m_lightVertices;
};