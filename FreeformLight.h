#pragma once
#include <list>
#include <map>
#include <vector>
#include <d3dx9.h>


template<typename T> class _ImmutableFreeform;
template<typename> class _MutableFreeform;

class _ImmutableLightImpl
{
	friend _ImmutableFreeform<_ImmutableLightImpl>;

protected:
	struct CUSTOM_VERTEX {
		D3DXVECTOR3 position;
		D3DXVECTOR2 uv;
	};
	using Indices = std::vector<WORD>;
	using Points = std::vector<D3DXVECTOR3>;
	using Vertices = std::vector<CUSTOM_VERTEX>;

	struct Setting
	{
		D3DXCOLOR lightColor = D3DCOLOR_XRGB( 255, 255, 255 );
		D3DXCOLOR shadowColor = D3DCOLOR_XRGB( 255 / 2, 255 / 2, 255 / 2 );
		float intensity = 1.f;
		float falloff = 1.f;
	};

public:
	// TODO: setting을 입력받아야 함. 포인터 좌표 포함
	_ImmutableLightImpl( LPDIRECT3DDEVICE9, LPDIRECT3DPIXELSHADER9 pBlurShader, Points const& );
	virtual ~_ImmutableLightImpl();

	HRESULT Draw( LPDIRECT3DDEVICE9 );
	// TODO 생성자에서 처리해야함
	HRESULT UpdateLight( LPDIRECT3DDEVICE9, const Setting& );

	static HRESULT CreateMesh( LPDIRECT3DDEVICE9, LPD3DXMESH*, UINT width, UINT height );
	static HRESULT CreateTexture( LPDIRECT3DDEVICE9, LPDIRECT3DTEXTURE9*, UINT width, UINT height );

protected:
	// 프리폼 조명을 위한 텍스처를 만든다
	// TODO: 텍스처를 만든 후 렌더러가 동작을 멈춘다. DX9 디버그 기능을 켜서 확인해보기. 
	//
	// pTexture: 생성할 텍스처를 담을 포인터
	HRESULT CreateLightTextureByRenderer( LPDIRECT3DDEVICE9, LPDIRECT3DTEXTURE9* pTexture ) const;
	// 매우 느리지만 위의 함수를 고칠 때까지 사용한다. 리소스를 가능한 소스 폴더에 넣지 않으려는 시도
	HRESULT CreateLightTextureByLockRect( LPDIRECT3DDEVICE9, LPDIRECT3DTEXTURE9* pTexture, const Setting& ) const;

	HRESULT CopyToMemory( LPDIRECT3DVERTEXBUFFER9 pDest, LPVOID pSrc, UINT size ) const;
	HRESULT UpdateLightVertexBuffer( LPDIRECT3DVERTEXBUFFER9* pOut, Vertices& vertices, LPDIRECT3DDEVICE9 pDevice, const Points& points, float falloff );
	// 인덱스 버퍼를 갱신한다
	HRESULT UpdateLightIndexBuffer( LPDIRECT3DINDEXBUFFER9* pOut, Indices& indices, LPDIRECT3DDEVICE9, size_t vertexSize ) const;

	// 블러 처리할 마스크를 만든다
	HRESULT UpdateBlurMask( LPDIRECT3DDEVICE9, const Vertices& );

protected:
	// 개별 프리폼 조명 정보
	// 텍스처
	LPDIRECT3DTEXTURE9 m_pLightTexture{};
	// 버퍼
	LPDIRECT3DINDEXBUFFER9 m_pLightIndexBuffer{};
	LPDIRECT3DVERTEXBUFFER9 m_pLightVertexBuffer{};

	const int m_lightVertexFvf = D3DFVF_XYZ | D3DFVF_TEX1;

	Indices m_lightIndices;
	Vertices m_lightVertices;

	struct Mask
	{
		D3DXMATRIX m_worldTransform{};
		LPDIRECT3DTEXTURE9 m_pTexture{};
	}
	m_blurMask;

	// TODO: Mask 구조체에 처 넣기
	LPD3DXMESH m_pMaskMesh{};
	LPDIRECT3DPIXELSHADER9 m_pBlurPixelShader{};

	Setting m_setting;
};

class _MutableLightImpl : public _ImmutableLightImpl
{
	friend _MutableFreeform<void>;

private:
	struct Setting : _ImmutableLightImpl::Setting
	{
		inline bool operator==( const Setting& setting ) const { return !memcmp( &setting, this, sizeof( setting ) ); }
		inline bool operator!=( const Setting& setting ) const { return !( *this == setting ); }
	};

public:
	_MutableLightImpl( LPDIRECT3DDEVICE9 pDevice, LPDIRECT3DPIXELSHADER9 pBlurShader, Points const& );

	HRESULT SetSetting( LPDIRECT3DDEVICE9, const Setting& );
	inline const Setting& GetSetting() const { return m_setting; }

	HRESULT DrawVertexHelper( LPDIRECT3DDEVICE9, D3DDISPLAYMODE const&, char const* windowTitleName );

	inline void ClearEditingStates( size_t vertexCount ) { m_vertexEditingStates.clear(); m_vertexEditingStates.resize( vertexCount ); }

private:
	// 정점을 편집한다
	// TODO: seperate to _EditableLightImpl 
	HRESULT UpdateLightVertex( LPDIRECT3DDEVICE9, WORD index, const D3DXVECTOR3& position );
	HRESULT UpdateLightVertex( LPDIRECT3DDEVICE9, const Points& );
	HRESULT AddLightVertex( LPDIRECT3DDEVICE9, size_t index, const D3DXVECTOR3& position );
	HRESULT RemoveLightVertex( LPDIRECT3DDEVICE9, size_t index );

	// 중점을 얻는다
	bool IsInsidePolygon( const Points&, const D3DXVECTOR3& point ) const;

	// 주어진 두 선과의 y 축 간의 교점을 얻는다
	bool GetCrossPoint( D3DXVECTOR3& out, D3DXVECTOR3 p0, D3DXVECTOR3 p1, const D3DXVECTOR2& mousePosition, D3DDISPLAYMODE const& ) const;

	Points GetPointsFromVertices( const Vertices& ) const;

	template<class _InIt>
	D3DXVECTOR3 GetCenterPoint( _InIt _First, _InIt _Last ) const;

private:
	Setting m_setting;

	std::vector<bool> m_vertexEditingStates;

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
	using Cache = _Cache<typename _MutableLightImpl::Points, D3DXVECTOR3>;
	using LinePointsCaches = std::map<PointCacheKey, Cache>;
	LinePointsCaches m_linePointsCaches;
};

template<typename LIGHT_IMPL>
class _ImmutableFreeform
{
public:
	_ImmutableFreeform( LPDIRECT3DPIXELSHADER9 pBlurShader ) : m_pBlurPixelShader{ pBlurShader }
	{}
	_ImmutableFreeform( _ImmutableFreeform& ) = delete;

	HRESULT Draw( LPDIRECT3DDEVICE9 pDevice, float x, float y );

	inline void SetBlurPixelShader( LPDIRECT3DPIXELSHADER9 shader ) { m_pBlurPixelShader = shader; }

	inline HRESULT CreateMesh( LPDIRECT3DDEVICE9 pDevice, LPD3DXMESH* pOutMesh, UINT width, UINT height ) const {
		return LIGHT_IMPL::CreateMesh( pDevice, pOutMesh, width, height );
	};
	inline HRESULT CreateTexture( LPDIRECT3DDEVICE9 pDevice, LPDIRECT3DTEXTURE9* pOutTexture, UINT width, UINT height ) const { return LIGHT_IMPL::CreateTexture( pDevice, pOutTexture, width, height ); }

protected:
	std::vector<std::shared_ptr<LIGHT_IMPL>> m_lightImpls;
	LPDIRECT3DPIXELSHADER9 m_pBlurPixelShader{};
};

// TODO change to general template
template<typename>
class _MutableFreeform : public _ImmutableFreeform<_MutableLightImpl>
{
	using PARENT = _ImmutableFreeform<_MutableLightImpl>;

public:
	_MutableFreeform( LPDIRECT3DPIXELSHADER9 pBlurShader, const D3DDISPLAYMODE& displayMode ) : PARENT{ pBlurShader }, m_displayMode{ displayMode }
	{}

	HRESULT Draw( LPDIRECT3DDEVICE9 pDevice, float x, float y );
	// 개발 용도의 imgui 창을 만든다
	HRESULT DrawImgui( LPDIRECT3DDEVICE9, LONG xCenter, LONG yCenter, bool isAmbientMode, bool* pIsVisible );

	inline bool IsMaskInvisible() const { return !m_setting.maskVisible; }
	inline D3DXCOLOR GetAmbientColor() const { return m_setting.ambient;  }

private:
	HRESULT AddLight( LPDIRECT3DDEVICE9, LONG x, LONG y );
	HRESULT RemoveLight( size_t index );
	_MutableLightImpl::Points GetDefaultPoints( D3DDISPLAYMODE const&, LONG x, LONG y ) const;

private:
	struct Setting
	{
		bool maskVisible{};
		bool meshVisible{};
		bool helper{};

		D3DXCOLOR ambient{ D3DCOLOR_XRGB( 125, 125, 125 ) };
	} m_setting;

	struct Tab
	{
		bool m_opened;
		std::string m_name;

		Tab( bool opened, std::string& name ) : m_opened{ opened }, m_name{ name }
		{}

		Tab( Tab&& tab ) {
			m_opened = tab.m_opened;
			m_name = std::move( tab.m_name );
		}

		Tab& operator=( const Tab& tab ) {
			m_opened = tab.m_opened;
			m_name = tab.m_name;

			return *this;
		}
	};

	std::vector<Tab> m_tabs;
	D3DDISPLAYMODE m_displayMode{};
};

using ImmutableFreeform = _ImmutableFreeform<_ImmutableLightImpl>;
using MutableFreeform = _MutableFreeform<void>;