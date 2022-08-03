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

		// ���� ������ �ٲ۴�
		HRESULT SetSetting( LPDIRECT3DDEVICE9, const Setting& );
		// ���� ������ ��´�
		inline const Setting& GetSetting() const { return m_setting; }

		// ��� ������ �׸���
		virtual HRESULT Draw( LPDIRECT3DDEVICE9 ) override final;
		// ���� ������ �ʿ��� ����̸� �׸���
		HRESULT DrawHelper( LPDIRECT3DDEVICE9, D3DDISPLAYMODE const&, char const* windowTitleName );

	private:
		HRESULT UpdateLight( LPDIRECT3DDEVICE9, const Setting& );

		// ������ �����Ѵ�
		HRESULT UpdateLightVertex( LPDIRECT3DDEVICE9, WORD index, const D3DXVECTOR3& position );
		HRESULT UpdateLightVertex( LPDIRECT3DDEVICE9, const Points& );
		HRESULT AddLightVertex( LPDIRECT3DDEVICE9, size_t index, const D3DXVECTOR3& position );
		HRESULT RemoveLightVertex( LPDIRECT3DDEVICE9, size_t index );

		// ������ ��´�
		bool IsInsidePolygon( const Points&, const D3DXVECTOR3& point ) const;

		// �־��� �� ������ y �� ���� ������ ��´�
		static bool GetCrossPoint( D3DXVECTOR3& out, D3DXVECTOR3 p0, D3DXVECTOR3 p1, const D3DXVECTOR2& mousePosition, D3DDISPLAYMODE const& );

		// ���� �����κ��� ��ġ ���� ����
		Points GetPointsFromVertices( const Vertices& ) const;

		// �������� ������ ��´�
		template<class _InIt>
		D3DXVECTOR3 GetCenterPoint( _InIt _First, _InIt _Last ) const;

		// �⺻ ������ ��´�
		Setting GetDefaultSetting() const;

		// ���� ���¸� �����Ѵ�
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