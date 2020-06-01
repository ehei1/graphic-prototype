#pragma once
#include <map>
#include <vector>
#include <d3dx9.h>


// 프리폼 조명을 위한 정보
class CFreeformLight
{
public:
	struct Setting
	{
		D3DXCOLOR lightColor = D3DCOLOR_XRGB( 255, 255, 255 );
		D3DXCOLOR shadowColor = D3DCOLOR_XRGB( 255 / 2, 255 / 2, 255 / 2 );
		float intensity = 1.f;
		float falloff = 1.f;
		bool maskOnly{};
		bool helper{};

		inline bool operator==( const Setting& setting ) const { return !memcmp( &setting, this, sizeof( setting ) ); }
		inline bool operator!=( const Setting& setting ) const { return !( *this == setting ); }
	};

	CFreeformLight();
	CFreeformLight( CFreeformLight& ) = delete;
	
	HRESULT AddLight( LPDIRECT3DDEVICE9, LONG x, LONG y );
	HRESULT RemoveLight();
	HRESULT UpdateLight( LPDIRECT3DDEVICE9, const Setting& );

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
	HRESULT CreateImgui( LPDIRECT3DDEVICE9, LONG xCenter, LONG yCenter, bool isAmbientMode, bool* pIsVisible );
	inline const Setting& GetSetting() const { return m_setting; }
	HRESULT SetSetting( LPDIRECT3DDEVICE9, const Setting& );

	static HRESULT CreateMesh( LPDIRECT3DDEVICE9, LPD3DXMESH*, UINT width, UINT height );
	static HRESULT CreateTexture( LPDIRECT3DDEVICE9, LPDIRECT3DTEXTURE9*, UINT width, UINT height );

private:
	struct CUSTOM_VERTEX {
		D3DXVECTOR3 position;
		D3DXVECTOR2 uv;
	};
	using Indices = std::vector<WORD>;
	using Points = std::vector<D3DXVECTOR3>;
	using Vertices = std::vector<CUSTOM_VERTEX>;

	// 프리폼 조명을 위한 텍스처를 만든다
	// TODO: 텍스처를 만든 후 렌더러가 동작을 멈춘다. DX9 디버그 기능을 켜서 확인해보기. 
	//
	// pTexture: 생성할 텍스처를 담을 포인터
	HRESULT CreateLightTextureByRenderer( LPDIRECT3DDEVICE9, LPDIRECT3DTEXTURE9* pTexture ) const;
	// 매우 느리지만 위의 함수를 고칠 때까지 사용한다. 리소스를 가능한 소스 폴더에 넣지 않으려는 시도
	HRESULT CreateLightTextureByLockRect( LPDIRECT3DDEVICE9, LPDIRECT3DTEXTURE9* pTexture, const Setting& ) const;
	HRESULT UpdateLightVertex( WORD index, const D3DXVECTOR3& position );
	HRESULT UpdateLightVertex( const Points& );
	HRESULT AddLightVertex( LPDIRECT3DDEVICE9, size_t index, const D3DXVECTOR3& position );
	HRESULT RemoveLightVertex( LPDIRECT3DDEVICE9, size_t index );

	HRESULT CopyToMemory( LPDIRECT3DVERTEXBUFFER9 pDest, LPVOID pSrc, UINT size ) const;
	HRESULT UpdateLightVertexBuffer( LPDIRECT3DVERTEXBUFFER9* pOut, Vertices& vertices, LPDIRECT3DDEVICE9 pDevice, const Points& points, float falloff );
	// 인덱스 버퍼를 갱신한다
	HRESULT UpdateLightIndexBuffer( LPDIRECT3DINDEXBUFFER9* pOut, Indices& indices, LPDIRECT3DDEVICE9, size_t vertexSize ) const;
	// 중점을 얻는다
	D3DXVECTOR3 GetCenterPoint( const Points& ) const;
	BOOL IsInsidePolygon( const Points&, const D3DXVECTOR3& point ) const;
	/*
	주어진 두 선과의 y 축 간의 교점을 얻는다
	*/
	BOOL GetCrossPoint( D3DXVECTOR3& out, D3DXVECTOR3 p0, D3DXVECTOR3 p1, const D3DXVECTOR2& mousePosition ) const;

	Points GetPointsFromVertices( const Vertices& ) const;
	void ClearEditingStates( size_t vertexCount );

private:
	// 개별 프리폼 조명 정보
	// 텍스처
	LPDIRECT3DTEXTURE9 m_pLightTexture{};
	// 버퍼
	LPDIRECT3DINDEXBUFFER9 m_pLightIndexBuffer{};
	LPDIRECT3DVERTEXBUFFER9 m_pLightVertexBuffer{};

	std::vector< D3DXVECTOR3 > m_leftTopSideVertices{
		{ -1.0f, -1.f, 0.f },
	};
	std::vector< D3DXVECTOR3 > m_rightTopSideVertices{
		{ +1.f, -1.f, 0.f },
	};
	std::vector< D3DXVECTOR3 > m_rightBottomVertices{
		{ +1.0f, +1.f, 0.f },
	};
	std::vector< D3DXVECTOR3 > m_leftBottomVertices{
		{ -1.f, +1.f, 0.f },
	};
	D3DDISPLAYMODE m_displayMode{};

	Setting m_setting{};
	POINT m_position{};

	const int m_lightVertexFvf = D3DFVF_XYZ | D3DFVF_TEX1;

	Indices m_lightIndices;
	Vertices m_lightVertices;

	std::vector<bool> m_vertexEditingStates;
	
	D3DXMATRIX m_rotationMatrix{};

	using PointCacheKey = std::pair<size_t, size_t>;

	template<class POINTS, class VECTOR>
	struct _Cache {
		const POINTS m_points;
		const VECTOR m_from{};
		const VECTOR m_to{};

		_Cache() = default;
		_Cache( _Cache const& ) = delete;

		_Cache( POINTS&& points, VECTOR&& from, VECTOR&& to ) : m_points{ std::forward<POINTS>( points ) }, m_from{ std::forward<VECTOR>( from ) }, m_to{ std::forward<VECTOR>( to ) }
		{}

		_Cache( _Cache&& c ) : m_points{ std::move( c.m_points ) }, m_from{ c.m_from }, m_to{ c.m_to }
		{}
	};
	using Cache = _Cache<Points, D3DXVECTOR3>;
	using LinePointsCaches = std::map<PointCacheKey, Cache>;
	LinePointsCaches m_linePointsCaches;
};