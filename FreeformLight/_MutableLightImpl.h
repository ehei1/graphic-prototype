#pragma once
#include <map>
#include "_ImmutableLightImpl.h"


namespace FreeformLight
{
	class _MutableFreeform;

	class _MutableLightImpl : public _ImmutableLightImpl
	{
		friend _MutableFreeform;

	private:
		struct Setting : _ImmutableLightImpl::Setting
		{
			inline bool operator==( const Setting& setting ) const { return !memcmp( &setting, this, sizeof( setting ) ); }
			inline bool operator!=( const Setting& setting ) const { return !( *this == setting ); }
		};

	public:
		_MutableLightImpl( LPDIRECT3DDEVICE9 pDevice, LPDIRECT3DPIXELSHADER9 pBlurShader, Points const& );
		virtual ~_MutableLightImpl() {}

		// 조명 설정을 바꾼다
		HRESULT SetSetting( LPDIRECT3DDEVICE9, const Setting& );
		// 조명 설정을 얻는다
		inline const Setting& GetSetting() const { return m_setting; }

		// 모든 조명을 그린다
		virtual HRESULT Draw( LPDIRECT3DDEVICE9 ) override final;
		// 조명 설정에 필요한 도우미를 그린다
		HRESULT DrawHelper( LPDIRECT3DDEVICE9, D3DDISPLAYMODE const&, char const* windowTitleName );

	private:
		HRESULT UpdateLight( LPDIRECT3DDEVICE9, const Setting& );

		// 정점을 편집한다
		HRESULT UpdateLightVertex( LPDIRECT3DDEVICE9, WORD index, const D3DXVECTOR3& position );
		HRESULT UpdateLightVertex( LPDIRECT3DDEVICE9, const Points& );
		HRESULT AddLightVertex( LPDIRECT3DDEVICE9, size_t index, const D3DXVECTOR3& position );
		HRESULT RemoveLightVertex( LPDIRECT3DDEVICE9, size_t index );

		// 중점을 얻는다
		bool IsInsidePolygon( const Points&, const D3DXVECTOR3& point ) const;

		// 주어진 두 선과의 y 축 간의 교점을 얻는다
		static bool GetCrossPoint( D3DXVECTOR3& out, D3DXVECTOR3 p0, D3DXVECTOR3 p1, const D3DXVECTOR2& mousePosition, D3DDISPLAYMODE const& );

		// 정점 정보로부터 위치 값을 얻어낸다
		Points GetPointsFromVertices( const Vertices& ) const;

		// 정점들의 중점을 얻는다
		template<class _InIt>
		D3DXVECTOR3 GetCenterPoint( _InIt _First, _InIt _Last ) const;

		// 기본 설정을 얻는다
		Setting GetDefaultSetting() const;

		// 편집 상태를 해제한다
		inline void ClearEditingStates( size_t vertexCount ) { m_vertexEditingStates.clear(); m_vertexEditingStates.resize( vertexCount ); }

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
}