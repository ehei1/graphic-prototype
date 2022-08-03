#pragma once
#include <map>
#include <vector>
#include <d3dx9.h>


// ������ ������ ���� ����
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

	// ������ �׸���
	//
	// pDevice: ������ �׸��µ� ���� ��ġ
	// pSurface: ������ �׸� ǥ��
	// x: ������ �׸� ��ġ
	// y: ������ �׸� ��ġ
	HRESULT Draw( LPDIRECT3DDEVICE9 pDevice, LPDIRECT3DSURFACE9 pSurface, float x, float y );
	HRESULT RestoreDevice( const D3DDISPLAYMODE& );
	void InvalidateDeviceObjects();
	inline void Uninitialize() { InvalidateDeviceObjects(); }
	inline bool IsVisible() const { return !!m_pLightVertexBuffer; }

	// ���� �뵵�� imgui â�� �����
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

	// ������ ������ ���� �ؽ�ó�� �����
	// TODO: �ؽ�ó�� ���� �� �������� ������ �����. DX9 ����� ����� �Ѽ� Ȯ���غ���. 
	//
	// pTexture: ������ �ؽ�ó�� ���� ������
	HRESULT CreateLightTextureByRenderer( LPDIRECT3DDEVICE9, LPDIRECT3DTEXTURE9* pTexture ) const;
	// �ſ� �������� ���� �Լ��� ��ĥ ������ ����Ѵ�. ���ҽ��� ������ �ҽ� ������ ���� �������� �õ�
	HRESULT CreateLightTextureByLockRect( LPDIRECT3DDEVICE9, LPDIRECT3DTEXTURE9* pTexture, const Setting& ) const;
	HRESULT UpdateLightVertex( WORD index, const D3DXVECTOR3& position );
	HRESULT UpdateLightVertex( const Points& );
	HRESULT AddLightVertex( LPDIRECT3DDEVICE9, size_t index, const D3DXVECTOR3& position );
	HRESULT RemoveLightVertex( LPDIRECT3DDEVICE9, size_t index );

	HRESULT CopyToMemory( LPDIRECT3DVERTEXBUFFER9 pDest, LPVOID pSrc, UINT size ) const;
	HRESULT UpdateLightVertexBuffer( LPDIRECT3DVERTEXBUFFER9* pOut, Vertices& vertices, LPDIRECT3DDEVICE9 pDevice, const Points& points, float falloff );
	// �ε��� ���۸� �����Ѵ�
	HRESULT UpdateLightIndexBuffer( LPDIRECT3DINDEXBUFFER9* pOut, Indices& indices, LPDIRECT3DDEVICE9, size_t vertexSize ) const;
	// ������ ��´�
	D3DXVECTOR3 GetCenterPoint( const Points& ) const;
	BOOL IsInsidePolygon( const Points&, const D3DXVECTOR3& point ) const;
	/*
	�־��� �� ������ y �� ���� ������ ��´�
	*/
	BOOL GetCrossPoint( D3DXVECTOR3& out, D3DXVECTOR3 p0, D3DXVECTOR3 p1, const D3DXVECTOR2& mousePosition ) const;

	Points GetPointsFromVertices( const Vertices& ) const;
	void ClearEditingStates( size_t vertexCount );

private:
	// ���� ������ ���� ����
	// �ؽ�ó
	LPDIRECT3DTEXTURE9 m_pLightTexture{};
	// ����
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