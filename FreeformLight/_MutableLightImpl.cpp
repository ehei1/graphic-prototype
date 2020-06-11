#include "stdafx.h"
#include <string>
#include "_MutableLightImpl.h"

//#define DEBUG_LINE
//#define DEBUG_BLUR_MASK


namespace FreeformLight
{
	_MutableLightImpl::_MutableLightImpl( LPDIRECT3DDEVICE9 pDevice, LPDIRECT3DPIXELSHADER9 pBlurShader, Points const& points ) : _ImmutableLightImpl{ pDevice, pBlurShader, points, GetDefaultSetting() }, m_setting{ GetDefaultSetting() }
	{
		ClearEditingStates( points.size() );
	}

	HRESULT _MutableLightImpl::SetSetting( LPDIRECT3DDEVICE9 pDevice, const Setting& setting )
	{
		if ( m_setting != setting ) {
			UpdateLight( pDevice, setting );

			m_setting = setting;
		}

		return S_OK;
	}

	HRESULT _MutableLightImpl::UpdateLightVertex( LPDIRECT3DDEVICE9 pDevice, WORD updatingIndex, const D3DXVECTOR3& position )
	{
		for ( auto index : m_lightIndices ) {
			if ( updatingIndex == index ) {
				m_lightVertices[index].position = position;

				// 지렛대의 원리로 찾는 중점
				// https://blog.naver.com/dbtkdwh0/90085488219
				// 중점 수정이 아니면 자동으로 설정해준다
				if ( updatingIndex )
				{
					Points points;
					// 첫번째 값은 원점이어서 제외
					std::transform( std::next( std::cbegin( m_lightVertices ) ), std::cend( m_lightVertices ), std::back_inserter( points ), []( auto& v ) { return v.position; } );

					m_lightVertices[0].position = GetCenterPoint( points.begin(), points.end() );
				}

				auto memorySize = static_cast<UINT>( m_lightVertices.size() * sizeof( Vertices::value_type ) );
				CopyToMemory( m_pLightVertexBuffer, m_lightVertices.data(), memorySize );

				if ( FAILED( UpdateBlurMask( pDevice, m_lightVertices ) ) ) {
					assert( FALSE );

					return E_FAIL;
				}

				m_linePointsCaches.clear();
				return S_OK;
			}
		}

		return E_FAIL;
	}


	HRESULT _MutableLightImpl::UpdateLightVertex( LPDIRECT3DDEVICE9 pDevice, const Points& points )
	{
		ASSERT( m_lightVertices.size() == points.size() );

		for ( size_t i{}; i < points.size(); ++i ) {
			m_lightVertices[i].position = points[i];
		}

		auto memorySize = static_cast<UINT>( m_lightVertices.size() * sizeof( Vertices::value_type ) );
		CopyToMemory( m_pLightVertexBuffer, m_lightVertices.data(), memorySize );

		if ( FAILED( UpdateBlurMask( pDevice, m_lightVertices ) ) ) {
			return E_FAIL;
		}

		m_linePointsCaches.clear();

		return S_OK;
	}

	HRESULT _MutableLightImpl::AddLightVertex( LPDIRECT3DDEVICE9 pDevice, size_t index, const D3DXVECTOR3& position )
	{
		if ( m_lightVertices.size() < index ) {
			ASSERT( FALSE );

			return E_FAIL;
		}

		auto points = GetPointsFromVertices( m_lightVertices );
		auto iterator = std::next( std::cbegin( points ), index );
		points.insert( iterator, position );

		if ( FAILED( UpdateLightVertexBuffer( &m_pLightVertexBuffer, m_lightVertices, pDevice, points, m_setting.falloff ) ) ) {
			ASSERT( FALSE );

			return E_FAIL;
		}
		else if ( FAILED( UpdateLightIndexBuffer( &m_pLightIndexBuffer, m_lightIndices, pDevice, points.size() ) ) ) {
			ASSERT( FALSE );

			return E_FAIL;
		}

		ClearEditingStates( points.size() );

		return S_OK;
	}

	HRESULT _MutableLightImpl::RemoveLightVertex( LPDIRECT3DDEVICE9 pDevice, size_t index )
	{
		auto points = GetPointsFromVertices( m_lightVertices );
		auto iterator = std::next( std::cbegin( points ), index );
		ASSERT( iterator != std::cend( points ) );
		points.erase( iterator );
		ASSERT( !points.empty() );

		points[0] = GetCenterPoint( points.begin(), points.end() );

		if ( FAILED( UpdateLightVertexBuffer( &m_pLightVertexBuffer, m_lightVertices, pDevice, points, m_setting.falloff ) ) ) {
			ASSERT( FALSE );

			return E_FAIL;
		}
		else if ( FAILED( UpdateLightIndexBuffer( &m_pLightIndexBuffer, m_lightIndices, pDevice, points.size() ) ) ) {
			ASSERT( FALSE );

			return E_FAIL;
		}
		else if ( FAILED( UpdateBlurMask( pDevice, m_lightVertices ) ) ) {
			ASSERT( FALSE );

			return E_FAIL;
		}

		ClearEditingStates( points.size() );

		m_linePointsCaches.clear();

		return S_OK;
	}

	bool _MutableLightImpl::IsInsidePolygon( const Points& linePoints, const D3DXVECTOR3& point ) const
	{
		//crosses는 점q와 오른쪽 반직선과 다각형과의 교점의 개수
		auto crosses = 0;
		auto y = point.y;

		for ( size_t index = 0; index < linePoints.size(); ++index ) {
			auto nextIndex = ( index + 1 ) % linePoints.size();
			auto& p0 = linePoints[index];
			auto& p1 = linePoints[nextIndex];

			//점 B가 선분 (p[i], p[j])의 y좌표 사이에 있음
			if ( ( p0.y > y ) != ( p1.y > y ) ) {
				//atX는 점 B를 지나는 수평선과 선분 (p[i], p[j])의 교점
				auto at = ( p1.x - p0.x ) * ( y - p0.y ) / ( p1.y - p0.y ) + p0.x;

				//atX가 오른쪽 반직선과의 교점이 맞으면 교점의 개수를 증가시킨다.
				if ( point.x < at ) {
					++crosses;
				}
			}
		}

		return crosses % 2 > 0;
	}

	// 주어진 두 선과의 y 축 간의 교점을 얻는다
	bool _MutableLightImpl::GetCrossPoint( D3DXVECTOR3& out, D3DXVECTOR3 p0, D3DXVECTOR3 p1, const D3DXVECTOR2& mousePosition, D3DDISPLAYMODE const& displayMode ) const
	{
		using _PointPair = std::pair<D3DXVECTOR2, D3DXVECTOR2>;
		using _LinePoints = std::initializer_list<_PointPair>;

		if ( p0.y > p1.y ) {
			std::swap( p0, p1 );
		}

		D3DXVECTOR2 horizonPoint0 = { mousePosition.x,{} };
		D3DXVECTOR2 horizonPoint1 = { mousePosition.x, static_cast<float>( displayMode.Height ) };
		D3DXVECTOR2 verticalPoint0 = { {}, mousePosition.y };
		D3DXVECTOR2 verticalPoint1 = { static_cast<float>( displayMode.Width ), mousePosition.y };

		// 수직선, 수평선을 그어 교점을 알아낸다
		for ( auto pair : _LinePoints{ { horizonPoint0, horizonPoint1 },{ verticalPoint0, verticalPoint1 } } ) {
			auto& q0 = pair.first;
			auto& q1 = pair.second;

			if ( auto under = ( q1.y - q0.y )*( p1.x - p0.x ) - ( q1.x - q0.x )*( p1.y - p0.y ) ) {
				auto dx = p0.x - q0.x;
				auto dy = p0.y - q0.y;
				auto t = ( q1.x - q0.x )* dy - ( q1.y - q0.y )* dx;
				auto s = ( p1.x - p0.x )* dy - ( p1.y - p0.y )* dx;

				if ( t || s ) {
					t /= under;
					s /= under;

					if ( 0 < t && t < 1.f && 0 < s && s < 1.f ) {
						auto cx = p0.x + t * ( p1.x - p0.x );
						auto cy = p0.y + t * ( p1.y - p0.y );

						out = { cx, cy,{} };
						return true;
					}
				}
			}
		}

		out = {};
		return false;
	}

	_MutableLightImpl::Points _MutableLightImpl::GetPointsFromVertices( const Vertices& ) const
	{
		Points points;
		auto addPoint = []( const CUSTOM_VERTEX& vertex ) { return vertex.position; };
		std::transform( std::cbegin( m_lightVertices ), std::cend( m_lightVertices ), std::back_inserter( points ), addPoint );
		ASSERT( points.size() == m_lightVertices.size() );

		return points;
	}

	template<class _InIt>
	D3DXVECTOR3 _MutableLightImpl::GetCenterPoint( _InIt _First, _InIt _Last ) const
	{
		assert( _First != _Last );
		auto getMidPoint = []( const auto& p0, const auto& p1 ) {
			return ( p0 + p1 ) / 2.f;
		};
		auto getNDivedPoint = []( const auto& p0, const auto& p1, const auto i ) {
			return p0 + ( p1 - p0 ) / i;
		};
		auto itr = _First;
		const auto& pos0 = *( itr++ );
		if ( itr == _Last )
			return *_First;
		const auto& pos1 = *( itr++ );
		auto center = getMidPoint( pos0, pos1 );
		decltype( pos0.x ) count = 2;
		for ( ; itr != _Last; ++itr, ++count ) {
			center = getNDivedPoint( center, *itr, count );
		}
		return center;
	}

	HRESULT _MutableLightImpl::Draw( LPDIRECT3DDEVICE9 pDevice )
	{
#ifdef DEBUG_BLUR_MASK
		DWORD oldBlendOp{};
		pDevice->GetRenderState( D3DRS_BLENDOP, &oldBlendOp );
		DWORD oldSrcBlend{};
		pDevice->GetRenderState( D3DRS_SRCBLEND, &oldSrcBlend );
		DWORD oldDestBlend{};
		pDevice->GetRenderState( D3DRS_DESTBLEND, &oldDestBlend );

		pDevice->SetRenderState( D3DRS_BLENDOP, D3DBLENDOP_ADD );
		pDevice->SetRenderState( D3DRS_SRCBLEND, D3DBLEND_ONE );
		pDevice->SetRenderState( D3DRS_DESTBLEND, D3DBLEND_ZERO );
#endif

		auto result = _ImmutableLightImpl::Draw( pDevice );

#ifdef DEBUG_BLUR_MASK
		pDevice->SetRenderState( D3DRS_BLENDOP, oldBlendOp );
		pDevice->SetRenderState( D3DRS_SRCBLEND, oldSrcBlend );
		pDevice->SetRenderState( D3DRS_DESTBLEND, oldDestBlend );
#endif

		return result;
	}

	HRESULT _MutableLightImpl::DrawHelper( LPDIRECT3DDEVICE9 pDevice, D3DDISPLAYMODE const& displayMode, char const* windowTitleName )
	{
		bool mouseHoveringNoWindow = true;

		if ( auto context = ImGui::GetCurrentContext() ) {
			if ( ImGuiWindow* window = ImGui::FindWindowByName( windowTitleName ) ) {
				if ( context->HoveredWindow == window ) {
					mouseHoveringNoWindow = false;
				}
			}
		}

		D3DXMATRIX world{};
		pDevice->GetTransform( D3DTS_WORLD, &world );
		D3DXMATRIX view{};
		pDevice->GetTransform( D3DTS_VIEW, &view );
		D3DXMATRIX projection{};
		pDevice->GetTransform( D3DTS_PROJECTION, &projection );
		D3DVIEWPORT9 viewport{};
		pDevice->GetViewport( &viewport );

		auto projectToScreen = [&world, &view, &projection, &viewport]( const _MutableLightImpl::CUSTOM_VERTEX& vertex ) {
			D3DXVECTOR3 position{};
			D3DXVec3Project( &position, &vertex.position, &viewport, &projection, &view, &world );
			position.z = {};

			return position;
		};
		_MutableLightImpl::Points projectedPoints;
		std::transform( std::begin( m_lightVertices ), std::end( m_lightVertices ), std::back_inserter( projectedPoints ), projectToScreen );

		// 이동 가능한 단추를 그린다. 사실은 부유 창
		for ( size_t i = 0; i < projectedPoints.size(); ++i ) {
			auto index = m_lightIndices[i];
			auto& projectedPoint = projectedPoints[index];
			auto name = std::to_string( i );

			// 단추 그림
			// 부유 가능한 창을 만들고 이동 불가능하게 한다. 창이 선택되면 이동 가능한 상태로 한다. 그렇지 않으면 실수 오차로 인해 조금씩 움직일 수 있다
			{
				auto&& isEditing = m_vertexEditingStates[i];
				ImGui::SetNextWindowPos( { projectedPoint.x, projectedPoint.y }, isEditing ? ImGuiCond_Once : ImGuiCond_Always );
				ImGui::SetNextWindowSize( { 5, 5 }, ImGuiCond_Always );

				auto pointClosed = true;
				auto hasMoreVertexThanTriangle = ( m_lightVertices.size() - 1 ) > 3;
				// 원점 삭제 불가능
				auto noPointDeleted = i && hasMoreVertexThanTriangle ? &pointClosed : nullptr;
				constexpr auto pointButtonStyle = ImGuiWindowFlags_NoDecoration - ImGuiWindowFlags_NoTitleBar;

				if ( ImGui::Begin( name.c_str(), noPointDeleted, pointButtonStyle ) ) {
					if ( mouseHoveringNoWindow ) {
						isEditing = ImGui::IsItemActive();
						auto nextPos = ImGui::GetWindowPos();

						using _Position = std::pair<int, int>;
						_Position p0{ static_cast<int>( projectedPoint.x ), static_cast<int>( projectedPoint.y ) };
						_Position p1{ static_cast<int>( nextPos.x ), static_cast<int>( nextPos.y ) };

						if ( p0 != p1 ) {
							if ( i ) {
								D3DXVECTOR3 out{};
								D3DXVECTOR3 in{ nextPos.x, nextPos.y, 0.f };
								D3DXVec3Unproject( &out, &in, &viewport, &projection, &view, &world );
								out.z = {};

								if ( FAILED( UpdateLightVertex( pDevice, index, out ) ) ) {
									return E_FAIL;
								}
							}
							// 정점 전체 이동
							else {
								auto dx = p1.first - p0.first;
								auto dy = p1.second - p0.second;

								auto movePoints = [&dx, &dy]( const CUSTOM_VERTEX& vertex ) {
									return vertex.position + D3DXVECTOR3{ static_cast<float>( dx ), static_cast<float>( dy ),{} };
								};
								Points points;
								std::transform( std::cbegin( m_lightVertices ), std::cend( m_lightVertices ), std::back_inserter( points ), movePoints );

								if ( FAILED( UpdateLightVertex( pDevice, points ) ) ) {
									return E_FAIL;
								}
							}
						}
					}

					ImGui::Text( "%s", name.c_str() );
					ImGui::End();
				}

				// 정점 삭제
				if ( noPointDeleted && !*noPointDeleted ) {
					if ( FAILED( RemoveLightVertex( pDevice, i ) ) ) {
						ASSERT( FALSE );

						return E_FAIL;
					}

					return S_OK;
				}
			}
		}

		// 선을 그린다
		{
			auto drawList = ImGui::GetBackgroundDrawList();
			auto hasNoCrossPoint = true;
			auto isNoEditing = std::none_of( std::cbegin( m_vertexEditingStates ), std::cend( m_vertexEditingStates ), []( bool v ) { return v == true; } );

			D3DXMATRIX rotationMatrix{};
			D3DXMatrixRotationZ( &rotationMatrix, D3DXToRadian( 90 ) );

			for ( size_t lightIndex = 2; lightIndex < m_lightIndices.size(); ++lightIndex ) {
				auto i0 = m_lightIndices[lightIndex - 1];
				auto i1 = m_lightIndices[lightIndex];
				auto& from = projectedPoints[i0];
				auto& to = projectedPoints[i1];

				drawList->AddLine( { from.x, from.y }, { to.x, to.y }, IM_COL32_WHITE, 2 );

				// 선 주위에 직사각형을 그린다. 이것은 마우스 커서 위치 판정에 쓰인다
				if ( isNoEditing && hasNoCrossPoint && mouseHoveringNoWindow )
				{
					const PointCacheKey cacheKey{ i0, i1 };
					auto iterator = m_linePointsCaches.find( cacheKey );

					if ( std::cend( m_linePointsCaches ) == iterator ) {
						auto direction = to - from;
						D3DXVec3Normalize( &direction, &direction );

						D3DXVECTOR4 rotatedDirection{};
						D3DXVec3Transform( &rotatedDirection, &direction, &rotationMatrix );

						auto offset = 30.f;
						auto directionOffset = direction * offset;
						auto _from = from + directionOffset;
						auto _to = to - directionOffset;

						auto width = 10.f;
						auto xBias = D3DXVECTOR3{ rotatedDirection.x, rotatedDirection.y,{} } * width;
						auto p0 = _from + xBias;
						auto p1 = _from - xBias;
						auto p2 = _to - xBias;
						auto p3 = _to + xBias;

						auto cache = Cache{ { p0, p1, p2, p3 }, std::move( _from ), std::move( _to ) };
						iterator = m_linePointsCaches.emplace_hint( iterator, cacheKey, std::move( cache ) );
					}

					Cache const& cache{ iterator->second };
					auto mousePos = ImGui::GetMousePos();

					if ( IsInsidePolygon( cache.m_points, { mousePos.x, mousePos.y,{} } ) ) {
						hasNoCrossPoint = false;

						D3DXVECTOR3 crossPoint{};
						auto const& _from = cache.m_from;
						auto const& _to = cache.m_to;

						if ( GetCrossPoint( crossPoint, _from, _to, { mousePos.x, mousePos.y }, displayMode ) ) {
#ifdef DEBUG_LINE
							drawList->AddCircle( { crossPoint.x, crossPoint.y }, 50, IM_COL32( 0, 255, 0, 255 ) );
#endif
							constexpr auto controlOffset = -10.f;

							ImGui::SetNextWindowPos( { mousePos.x + controlOffset, mousePos.y + controlOffset } );
							ImGui::Begin( ".", nullptr, ImGuiWindowFlags_NoDecoration );
							auto pushed = ImGui::Button( "o" );
							ImGui::End();

							if ( pushed ) {
								D3DXVec3Unproject( &crossPoint, &crossPoint, &viewport, &projection, &view, &world );
								AddLightVertex( pDevice, lightIndex, crossPoint );
								return S_OK;
							}
						}
					}

#ifdef DEBUG_LINE
					// draw area
					{
						auto debugColor = IM_COL32( 0, 255, 0, 255 );
						auto thickness = 1.f;
						auto& points = cache.m_points;

						for ( size_t pointIndex{}; pointIndex < points.size(); ++pointIndex ) {
							auto& pp0 = points[pointIndex];
							auto nextIndex = ( pointIndex + 1 ) % points.size();
							auto& pp1 = points[nextIndex];

							drawList->AddLine( { pp0.x, pp0.y }, { pp1.x, pp1.y }, debugColor, thickness );
						}
					}
#endif
				}
			}
		}

		return S_OK;
	}

	HRESULT _MutableLightImpl::UpdateLight( LPDIRECT3DDEVICE9 pDevice, const Setting& setting )
	{
		bool blurMaskUpdating{};

		// 텍스처 수정
		if ( m_pLightTexture && ( m_setting.lightColor != setting.lightColor || m_setting.intensity != setting.intensity ) ) {
			SAFE_RELEASE( m_pLightTexture );

			CreateLightTextureByLockRect( pDevice, &m_pLightTexture, setting );

			blurMaskUpdating = true;
		}

		// uv 수정
		if ( m_setting.falloff != setting.falloff ) {
			auto falloff = setting.falloff;

			if ( !m_lightVertices.empty() ) {
				m_lightVertices[0].uv = { falloff, falloff };

				auto updateFalloff = [i = -1, falloff]( CUSTOM_VERTEX& customVertex ) mutable {
					customVertex.uv = ( ++i % 2 ? D3DXVECTOR2{ falloff, 0 } : D3DXVECTOR2{ 0, 0 } );
				};
				std::for_each( std::next( std::begin( m_lightVertices ) ), std::end( m_lightVertices ), updateFalloff );

				auto memorySize = static_cast<UINT>( m_lightVertices.size() * sizeof( Vertices::value_type ) );
				CopyToMemory( m_pLightVertexBuffer, m_lightVertices.data(), memorySize );

				blurMaskUpdating = true;
			}
		}

		if ( blurMaskUpdating ) {
			UpdateBlurMask( pDevice, m_lightVertices );
		}

		return S_OK;
	}

	_MutableLightImpl::Setting _MutableLightImpl::GetDefaultSetting() const
	{
		Setting setting;
		setting.lightColor = D3DCOLOR_XRGB( 125, 125, 125 );
		setting.intensity = 0.5f;
		setting.falloff = 2.f;

		return setting;
	}
}