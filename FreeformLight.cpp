#include "StdAfx.h"
#include <algorithm>
#include <iostream>
#include <string>
#include <tuple>
#include <unordered_set>

#include "utility\zip.h"
#include "FreeformLight.h"

//#define DEBUG_LINE
//#define DEBUG_SURFACE


CFreeformLight::CFreeformLight()
{
	D3DXMatrixRotationZ( &m_rotationMatrix, D3DXToRadian( 90 ) );
}

void CFreeformLight::InvalidateDeviceObjects()
{
	SAFE_RELEASE( m_pLightTexture );
	SAFE_RELEASE( m_pLightIndexBuffer );
	SAFE_RELEASE( m_pLightVertexBuffer );
}

HRESULT CFreeformLight::RestoreDevice( const D3DDISPLAYMODE& displayMode )
{
	m_displayMode = displayMode;
	return S_OK;
}

HRESULT CFreeformLight::AddLight( LPDIRECT3DDEVICE9 pDevice, LONG x, LONG y )
{
	// TODO 메시로 바꾸기
	ASSERT( pDevice );

	if ( m_pLightVertexBuffer ) {
		return E_FAIL;
	}

	m_position = { x, y };

	// 초기화
	if ( !m_pLightTexture ) {
		if ( FAILED( CreateLightTextureByLockRect( pDevice, &m_pLightTexture, m_setting ) ) ) {
			ASSERT( FALSE );
			return E_FAIL;
		}

		ASSERT( m_pLightTexture );
	}

	Points points{ { {},{},{} } };
	// 화면의 절반만 차지하도록 한다
	auto scaledWidth = m_displayMode.Width / 4;
	auto scaledHeight = m_displayMode.Height / 4;

	// 시계 방향으로 면을 살피면서 점을 추가한다
	for ( auto vertices : { m_leftTopSideVertices, m_rightTopSideVertices, m_rightBottomVertices, m_leftBottomVertices } ) {
		points.insert( points.end(), vertices.begin(), vertices.end() );
	}

	auto changeToWorldCoord = [center = D3DXVECTOR3{ static_cast<float>( x ), static_cast<float>( y ),{} }, scaledWidth, scaledHeight]( D3DXVECTOR3& point ) {
		point = D3DXVECTOR3{ point.x * scaledWidth, point.y * scaledHeight,{} } +center;
	};
	std::for_each( std::begin( points ), std::end( points ), changeToWorldCoord );

	if ( FAILED( UpdateLightVertexBuffer( &m_pLightVertexBuffer, m_lightVertices, pDevice, points, m_setting.falloff ) ) ) {
		ASSERT( FALSE );
		return E_FAIL;
	}

	if ( FAILED( UpdateLightIndexBuffer( &m_pLightIndexBuffer, m_lightIndices, pDevice, points.size() ) ) ) {
		ASSERT( FALSE );
		return E_FAIL;
	}

	m_vertexEditingStates.clear();
	m_vertexEditingStates.resize( m_lightVertices.size() );

	return S_OK;
}

HRESULT CFreeformLight::RemoveLight()
{
	if ( !m_pLightVertexBuffer )
	{
		return S_OK;
	}

	SAFE_RELEASE( m_pLightIndexBuffer );
	SAFE_RELEASE( m_pLightVertexBuffer );

	return S_OK;
}

HRESULT CFreeformLight::Draw( LPDIRECT3DDEVICE9 pDevice, LPDIRECT3DSURFACE9 pSurface, float x, float y )
{
	_CRT_UNUSED( pSurface );

	if ( !m_pLightVertexBuffer )
	{
		return S_OK;
	}

	D3DMATRIX curVm{};
	pDevice->GetTransform( D3DTS_VIEW, &curVm );

	// 뷰 행렬을 기본으로 바꾼다. 게임 화면은 확대를 하는 경우가 있기 때문
	{
		D3DXVECTOR3 eye{ x, y, 1 };
		D3DXVECTOR3 at{ x, y, -1 };
		D3DXVECTOR3 up{ 0, -1, 0 };

		D3DXMATRIX view{};
		D3DXMatrixLookAtLH( &view, &eye, &at, &up );

		pDevice->SetTransform( D3DTS_VIEW, &view );
	}

	// 마스크에 조명을 그린다
	if ( SUCCEEDED( pDevice->BeginScene() ) )
	{
		DWORD curBlendOp = {};
		DWORD curSrcBlend = {};
		DWORD curDestBlend = {};
		pDevice->GetRenderState( D3DRS_BLENDOP, &curBlendOp );
		pDevice->GetRenderState( D3DRS_DESTBLEND, &curDestBlend );
		pDevice->GetRenderState( D3DRS_SRCBLEND, &curSrcBlend );

		D3DVERTEXBUFFER_DESC vertexBufferDesc = {};
		m_pLightVertexBuffer->GetDesc( &vertexBufferDesc );

		auto primitiveCount = m_lightIndices.size() - 2;

		DWORD curFVF = {};
		pDevice->GetFVF( &curFVF );
		pDevice->SetFVF( vertexBufferDesc.FVF );
		pDevice->SetTexture( 0, m_pLightTexture );
		pDevice->SetSamplerState( 0, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP );
		pDevice->SetSamplerState( 0, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP );
		// 마스크를 덮어쓴다
		// result = src * srcAlpha + dest * ( 1 - srcAlpha )
		pDevice->SetRenderState( D3DRS_BLENDOP, D3DBLENDOP_ADD );
		pDevice->SetRenderState( D3DRS_SRCBLEND, D3DBLEND_SRCALPHA );
		pDevice->SetRenderState( D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA );
		pDevice->SetStreamSource( 0, m_pLightVertexBuffer, 0, sizeof( Vertices::value_type ) );
		pDevice->SetIndices( m_pLightIndexBuffer );
		pDevice->DrawIndexedPrimitive( D3DPT_TRIANGLEFAN, 0, 0, static_cast<UINT>( m_lightVertices.size() ), 0, static_cast<UINT>( primitiveCount ) );
		pDevice->EndScene();

		pDevice->SetFVF( curFVF );

		pDevice->SetRenderState( D3DRS_BLENDOP, curBlendOp );
		pDevice->SetRenderState( D3DRS_DESTBLEND, curDestBlend );
		pDevice->SetRenderState( D3DRS_SRCBLEND, curSrcBlend );

#ifdef DEBUG_SURFACE
		D3DSURFACE_DESC surfaceDesc{};

		if ( SUCCEEDED( pSurface->GetDesc( &surfaceDesc ) ) ) {
			LPDIRECT3DTEXTURE9 pScreenshotTexture{};

			if ( SUCCEEDED( pDevice->CreateTexture( surfaceDesc.Width, surfaceDesc.Height, 1, surfaceDesc.Usage, surfaceDesc.Format, surfaceDesc.Pool, &pScreenshotTexture, NULL ) ) ) {
				LPDIRECT3DSURFACE9 pScreenshotSurface{};

				if ( SUCCEEDED( pScreenshotTexture->GetSurfaceLevel( 0, &pScreenshotSurface ) ) ) {
					if ( SUCCEEDED( pDevice->StretchRect( pSurface, NULL, pScreenshotSurface, NULL, D3DTEXF_NONE ) ) ) {
						D3DXSaveTextureToFile( TEXT( "D:\\mask.png" ), D3DXIFF_PNG, pScreenshotTexture, NULL );
					}
				}

				SAFE_RELEASE( pScreenshotTexture );
			}
		}
#endif
	}

	// 뷰 행렬 복원
	pDevice->SetTransform( D3DTS_VIEW, &curVm );

	return S_OK;
}

HRESULT CFreeformLight::CreateLightTextureByLockRect( LPDIRECT3DDEVICE9 pDevice, LPDIRECT3DTEXTURE9* pOutTexture, const Setting& setting ) const
{
	ASSERT( !*pOutTexture );

	constexpr auto size = 256;
	ASSERT( ceil( log2( size ) ) == floor( log2( size ) ) );
	LPDIRECT3DTEXTURE9 pTexture = {};

	if ( FAILED( D3DXCreateTexture( pDevice, size, size, 0, D3DUSAGE_DYNAMIC, D3DFMT_A8R8G8B8, D3DPOOL_SYSTEMMEM, &pTexture ) ) ) {
		ASSERT( FALSE );
		return E_FAIL;
	}

	// 그라데이션을 그린다
	{
		D3DLOCKED_RECT lockedRect = {};
		pTexture->LockRect( 0, &lockedRect, NULL, D3DLOCK_READONLY );
		auto* const colors = static_cast<LPDWORD>( lockedRect.pBits );
		auto intensity = setting.intensity * 255.f;
		auto& color = setting.lightColor;
		auto r = static_cast<int>( color.r * intensity );
		auto g = static_cast<int>( color.g * intensity );
		auto b = static_cast<int>( color.b * intensity );

		for ( auto y = 0; y < size; ++y ) {
			auto a = y / static_cast<float>( size ) * 255;

			for ( auto x = 0; x < size; ++x ) {
				auto index = y * size + x;

				colors[index] = D3DCOLOR_ARGB( static_cast<int>( a ), static_cast<int>( r ), static_cast<int>( g ), static_cast<int>( b ) );
			}
		}

		pTexture->UnlockRect( 0 );
	}


	// 메모리에 쓴 것을 다시 읽어들인다. 이러면 렌더링 가능하게 된다
	{
		ID3DXBuffer* buffer = {};
		if ( FAILED( D3DXSaveTextureToFileInMemory( &buffer, D3DXIFF_PNG, pTexture, NULL ) ) ) {
			ASSERT( FALSE );
			return E_FAIL;
		}

		SAFE_RELEASE( pTexture );

		if ( FAILED( D3DXCreateTextureFromFileInMemory( pDevice, buffer->GetBufferPointer(), buffer->GetBufferSize(), &pTexture ) ) ) {
			ASSERT( FALSE );
			return E_FAIL;
		}
	}

#ifdef DEBUG_SURFACE
	D3DXSaveTextureToFile( TEXT( "D:\\lightTex.png" ), D3DXIFF_PNG, pTexture, NULL );
#endif

	*pOutTexture = pTexture;
	return S_OK;
}

HRESULT CFreeformLight::CreateLightTextureByRenderer( LPDIRECT3DDEVICE9 pDevice, LPDIRECT3DTEXTURE9* pOutTexture ) const
{
	ASSERT( !*pOutTexture );

	constexpr auto resolution = 256;
	constexpr auto x = static_cast<float>( resolution );
	constexpr auto y = static_cast<float>( resolution );

	struct CUSTOMVERTEX_TEXTURE {
		D3DXVECTOR4 position; // x, y, z, rhw
		D3DCOLOR color;
	} customVertices[] = {
		{ { 0.f, 0.f, 0.f, 1.f }, 0xffffffff, },
		{ { 0.f,   y, 0.f, 1.f }, 0xff000000, },
		{ { x,   y, 0.f, 1.f }, 0xff000000, },
		{ { x, 0.f, 0.f, 1.f }, 0xffffffff, },
	};
	constexpr auto textureFVF = D3DFVF_XYZRHW | D3DFVF_DIFFUSE;
	LPDIRECT3DVERTEXBUFFER9 pVertexBuffer = {};

	// 버텍스 버퍼 채우기
	{
		if ( FAILED( pDevice->CreateVertexBuffer( sizeof( customVertices ), 0, textureFVF, D3DPOOL_DEFAULT, &pVertexBuffer, NULL ) ) ) {
			ASSERT( FALSE );
			return E_FAIL;
		}

		LPVOID pVertices = {};

		if ( FAILED( pVertexBuffer->Lock( 0, sizeof( customVertices ), &pVertices, 0 ) ) ) {
			ASSERT( FALSE );
			return E_FAIL;
		}

		memcpy( pVertices, customVertices, sizeof( customVertices ) );
		pVertexBuffer->Unlock();
	}

	LPDIRECT3DTEXTURE9 pTexture = {};

	// 그리기
	{
		if ( FAILED( D3DXCreateTexture( pDevice, resolution, resolution, 0, D3DUSAGE_RENDERTARGET, D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, &pTexture ) ) ) {
			ASSERT( FALSE );
			return E_FAIL;
		}

		LPDIRECT3DSURFACE9 currentRenderTarget = {};
		pDevice->GetRenderTarget( 0, &currentRenderTarget );

		LPDIRECT3DSURFACE9 pTextureSurface = {};
		pTexture->GetSurfaceLevel( 0, &pTextureSurface );

		// 렌더링 대상 바꿈
		pDevice->SetRenderTarget( 0, pTextureSurface );
		pDevice->Clear( 0, NULL, D3DCLEAR_TARGET, D3DCOLOR_XRGB( 0, 0, 255 ), 1.0f, 0 );

		if ( SUCCEEDED( pDevice->BeginScene() ) ) {
			pDevice->SetStreamSource( 0, pVertexBuffer, 0, sizeof( *customVertices ) );
			pDevice->SetFVF( textureFVF );
			pDevice->DrawPrimitive( D3DPT_TRIANGLEFAN, 0, 2 );
			pDevice->EndScene();
		}

		// 복구
		pDevice->SetRenderTarget( 0, currentRenderTarget );

		SAFE_RELEASE( pTextureSurface );
		SAFE_RELEASE( currentRenderTarget );

#ifdef DEBUG_SURFACE
		if ( FAILED( D3DXSaveTextureToFile( TEXT( "D:\\lightTex.png" ), D3DXIFF_PNG, pTexture, NULL ) ) ) {
			ASSERT( FALSE );
			return E_FAIL;
		}
#endif
	}

	pVertexBuffer->Release();
	pVertexBuffer = {};

	*pOutTexture = pTexture;
	return S_OK;
}

HRESULT CFreeformLight::CreateTexture( LPDIRECT3DDEVICE9 pDevice, LPDIRECT3DTEXTURE9* pOutTexture, UINT width, UINT height )
{
	ASSERT( !*pOutTexture );

	LPDIRECT3DTEXTURE9 pTexture = {};

	if ( FAILED( pDevice->CreateTexture( width, height, 1, D3DUSAGE_RENDERTARGET, D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, &pTexture, NULL ) ) ) {
		ASSERT( FALSE );
		return E_FAIL;
	}

	*pOutTexture = pTexture;

	return S_OK;
}

HRESULT CFreeformLight::CreateMesh( LPDIRECT3DDEVICE9 pDevice, LPD3DXMESH* pOutMesh, UINT width, UINT height )
{
	ASSERT( !*pOutMesh );

	LPD3DXMESH pMesh = {};
	constexpr auto vertexCount = 4;
	constexpr auto faceCount = 2;
	constexpr auto fvf = D3DFVF_XYZ | D3DFVF_TEX1;

	if ( FAILED( D3DXCreateMeshFVF( faceCount, vertexCount, D3DXMESH_MANAGED, fvf, pDevice, &pMesh ) ) ) {
		return E_FAIL;
	}

	// 버텍스 버퍼 채우기
	{
		auto w = width / 2.f;
		auto h = height / 2.f;

		const CUSTOM_VERTEX vertices[] = {
			{ { -w, -h, 0 },{ 0, 0 } },//0
			{ { -w, +h, 0 },{ 0, 1 } },//1
			{ { +w, +h, 0 },{ 1, 1 } },//2
			{ { +w, -h, 0 },{ 1, 0 } },//3
		};
		static_assert( _countof( vertices ) == vertexCount, "invalid size" );

		LPVOID pMeshVertices = {};
		pMesh->LockVertexBuffer( 0, &pMeshVertices );
		memcpy( pMeshVertices, vertices, sizeof( vertices ) );
		pMesh->UnlockIndexBuffer();
	}

	// 인덱스 버퍼 채우기
	{
		const WORD indices[] = {
			0, 1, 2,
			0, 2, 3,
		};
		static_assert( _countof( indices ) == faceCount * 3, "invalid size" );

		LPVOID pMeshIndices = {};
		pMesh->LockIndexBuffer( 0, &pMeshIndices );
		memcpy( pMeshIndices, indices, sizeof( indices ) );
		pMesh->UnlockIndexBuffer();
	}

	*pOutMesh = pMesh;

	return S_OK;
}

HRESULT CFreeformLight::SetSetting( LPDIRECT3DDEVICE9 pDevice, const Setting& setting )
{
	if ( m_setting != setting ) {
		if ( IsVisible() ) {
			UpdateLight( pDevice, setting );
		}

		m_setting = setting;
	}

	return S_OK;
}

HRESULT CFreeformLight::CreateImgui( LPDIRECT3DDEVICE9 pDevice, LONG xCenter, LONG yCenter, bool isAmbientMode, bool* pIsVisible )
{
	ASSERT( pIsVisible );

	if ( !*pIsVisible ) {
		return S_OK;
	}

	ImGui::Begin( u8"조명", pIsVisible );

	auto pointButtonStyle = ImGuiWindowFlags_NoDecoration - ImGuiWindowFlags_NoTitleBar;
	auto newSetting = GetSetting();

	if ( isAmbientMode ) {
		ImGui::ColorEdit3( u8"주변", reinterpret_cast<float*>( &newSetting.shadowColor ) );
	}
	else {
		ImGui::TextWrapped( u8"주변 색을 바꾸려면 Ambient 플래그를 켜세요" );
	}

	auto freeformLightVisible = IsVisible();

	if ( ImGui::CollapsingHeader( u8"프리폼" ) ) {
		if ( ImGui::IsItemHovered() ) {
			ImGui::BeginTooltip();
			ImGui::TextUnformatted( u8"메시로 만들어진 조명 마스크를 동적으로 편집" );
			ImGui::EndTooltip();
		}

		if ( ImGui::Button( freeformLightVisible ? u8"숨기기" : u8"보이기" ) ) {
			if ( freeformLightVisible ) {
				RemoveLight();
			}
			else {
				AddLight( pDevice, xCenter, yCenter );
			}
		}

		if ( freeformLightVisible ) {
			ImGui::ColorEdit3( u8"빛", reinterpret_cast<float*>( &newSetting.lightColor ) );
			ImGui::SliderFloat( u8"강도", &newSetting.intensity, 0.f, 1.f );
			ImGui::SliderFloat( u8"드리움", &newSetting.falloff, 0, 10 );
			ImGui::Checkbox( u8"도우미", &newSetting.helper );
			ImGui::Checkbox( u8"마스크", &newSetting.maskOnly );
		}
	}

	SetSetting( pDevice, newSetting );

	ImGui::End();

	if ( freeformLightVisible && m_setting.helper ) {
		D3DXMATRIX world{};
		pDevice->GetTransform( D3DTS_WORLD, &world );
		D3DXMATRIX view{};
		pDevice->GetTransform( D3DTS_VIEW, &view );
		D3DXMATRIX projection{};
		pDevice->GetTransform( D3DTS_PROJECTION, &projection );
		D3DVIEWPORT9 viewport{};
		pDevice->GetViewport( &viewport );

		auto projectToScreen = [&world, &view, &projection, &viewport]( const CUSTOM_VERTEX& vertex ) {
			D3DXVECTOR3 position{};
			D3DXVec3Project( &position, &vertex.position, &viewport, &projection, &view, &world );
			position.z = {};

			return position;
		};
		Points projectedPoints;
		std::transform( std::begin( m_lightVertices ), std::end( m_lightVertices ), std::back_inserter( projectedPoints ), projectToScreen );

		auto controlOffset = 20.f;

		// 이동 가능한 단추를 그린다. 사실은 부유 창
		for ( auto i = 0; i < projectedPoints.size(); ++i ) {
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

				if ( ImGui::Begin( name.c_str(), noPointDeleted, pointButtonStyle ) ) {
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

							if ( FAILED( UpdateLightVertex( index, out ) ) ) {
								return E_FAIL;
							}
						}
						// 정점 전체 이동
						else {
							auto dx = p1.first - p0.first;
							auto dy = p1.second - p0.second;

							auto movePoints = [&dx, &dy]( const CUSTOM_VERTEX& vertex ) {
								return vertex.position + D3DXVECTOR3{ static_cast<float>( dx ), static_cast<float>( dy ), {} };
							};
							Points points;
							std::transform( std::cbegin( m_lightVertices ), std::cend( m_lightVertices ), std::back_inserter( points ), movePoints );

							if ( FAILED( UpdateLightVertex( points ) ) ) {
								return E_FAIL;
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

			for ( auto lightIndex = 2; lightIndex < m_lightIndices.size(); ++lightIndex ) {
				auto i0 = m_lightIndices[lightIndex - 1];
				auto i1 = m_lightIndices[lightIndex];
				auto& from = projectedPoints[i0];
				auto& to = projectedPoints[i1];

				drawList->AddLine( { from.x, from.y }, { to.x, to.y }, IM_COL32_WHITE, 2 );

				// 선 주위에 직사각형을 그린다. 이것은 마우스 커서 위치 판정에 쓰인다
				if ( isNoEditing && hasNoCrossPoint )
				{
					const PointCacheKey cacheKey{ i0, i1 };
					auto iterator = m_linePointsCaches.find( cacheKey );

					if ( std::cend( m_linePointsCaches ) == iterator ) {
						auto direction = to - from;
						D3DXVec3Normalize( &direction, &direction );

						D3DXVECTOR4 rotatedDirection{};
						D3DXVec3Transform( &rotatedDirection, &direction, &m_rotationMatrix );

						auto offset = 20.f;
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

					Cache& const cache{ iterator->second };
					auto mousePos = ImGui::GetMousePos();

					if ( IsInsidePolygon( cache.m_points, { mousePos.x, mousePos.y,{} } ) ) {
						hasNoCrossPoint = false;

						D3DXVECTOR3 crossPoint{};
						auto& const _from = cache.m_from;
						auto& const _to = cache.m_to;

						if ( GetCrossPoint( crossPoint, _from, _to, { mousePos.x, mousePos.y } ) ) {
#ifdef DEBUG_LINE
							drawList->AddCircle( { crossPoint.x, crossPoint.y }, 50, IM_COL32( 0, 255, 0, 255 ) );
#endif
							ImGui::SetNextWindowPos( { crossPoint.x - controlOffset, crossPoint.y - controlOffset } );
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

						for ( auto pointIndex = 0; pointIndex < linePoints.size(); ++pointIndex ) {
							auto& pp0 = linePoints[pointIndex];
							auto nextIndex = ( pointIndex + 1 ) % linePoints.size();
							auto& pp1 = linePoints[nextIndex];

							drawList->AddLine( { pp0.x, pp0.y }, { pp1.x, pp1.y }, debugColor, thickness );
						}
					}
#endif
				}
			}
		}
	}

	return S_OK;
}

HRESULT CFreeformLight::UpdateLightVertex( WORD updatingIndex, const D3DXVECTOR3& position )
{
	if ( m_pLightVertexBuffer ) {
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

					m_lightVertices[0].position = GetCenterPoint( points );
				}

				auto memorySize = static_cast<UINT>( m_lightVertices.size() * sizeof( Vertices::value_type ) );
				CopyToMemory( m_pLightVertexBuffer, m_lightVertices.data(), memorySize );

				m_linePointsCaches.clear();
				return S_OK;
			}
		}
	}

	return E_FAIL;
}

HRESULT CFreeformLight::UpdateLightVertex( const Points& points )
{
	if ( m_pLightVertexBuffer ) {
		ASSERT( m_lightVertices.size() == points.size() );

		for ( size_t i{}; i < points.size(); ++i ) {
			m_lightVertices[i].position = points[i];
		}

		auto memorySize = static_cast<UINT>( m_lightVertices.size() * sizeof( Vertices::value_type ) );
		CopyToMemory( m_pLightVertexBuffer, m_lightVertices.data(), memorySize );

		m_linePointsCaches.clear();
		return S_OK;
	}

	return E_FAIL;
}

HRESULT CFreeformLight::UpdateLight( LPDIRECT3DDEVICE9 pDevice, const Setting& setting )
{
	// 텍스처 수정
	if ( m_pLightTexture && ( m_setting.lightColor != setting.lightColor || m_setting.intensity != setting.intensity ) ) {
		SAFE_RELEASE( m_pLightTexture );

		CreateLightTextureByLockRect( pDevice, &m_pLightTexture, setting );
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
		}
	}

	return S_OK;
}

HRESULT CFreeformLight::CopyToMemory( LPDIRECT3DVERTEXBUFFER9 dest, LPVOID src, UINT size ) const
{
	ASSERT( size );

	LPVOID pVertices{};

	if ( FAILED( dest->Lock( 0, size, &pVertices, 0 ) ) ) {
		ASSERT( FALSE );
		return E_FAIL;
	}

	memcpy( pVertices, src, size );
	dest->Unlock();

	return S_OK;
}

HRESULT CFreeformLight::UpdateLightIndexBuffer( LPDIRECT3DINDEXBUFFER9* pOut, Indices& indices, LPDIRECT3DDEVICE9 pDevice, size_t vertexSize ) const
{
	// 인덱스를 만든다
	// https://en.cppreference.com/w/cpp/algorithm/generate
	auto increaseNumber = [n = 0]() mutable { return n++; };

	indices.resize( vertexSize );
	std::generate( indices.begin(), indices.end(), increaseNumber );

	// 항상 LT 위치에서 끝나야 한다
	indices.push_back( 1 );
	auto indicesSize = sizeof( Indices::value_type ) * indices.size();

	LPDIRECT3DINDEXBUFFER9 pIndexBuffer{};

	// 인덱스 버퍼 갱신
	{
		if ( FAILED( pDevice->CreateIndexBuffer( static_cast<UINT>( indicesSize ), D3DUSAGE_WRITEONLY, D3DFMT_INDEX16, D3DPOOL_DEFAULT, &pIndexBuffer, NULL ) ) ) {
			ASSERT( FALSE );
			return E_FAIL;
		}

		LPVOID pIndices{};

		if ( pIndexBuffer->Lock( 0, static_cast<UINT>(indicesSize), &pIndices, 0 ) ) {
			ASSERT( FALSE );
			return E_FAIL;
		}

		memcpy( pIndices, indices.data(), indicesSize );
		pIndexBuffer->Unlock();
	}


	SAFE_RELEASE( *pOut );
	*pOut = pIndexBuffer;

	return S_OK;
}

HRESULT CFreeformLight::UpdateLightVertexBuffer( LPDIRECT3DVERTEXBUFFER9* pOut, Vertices& vertices, LPDIRECT3DDEVICE9 pDevice, const Points& points, float falloff )
{
	ASSERT( !points.empty() );

	vertices.clear();

	// 프리폼 조명의 위치를 화면 중앙에 놓는다
	auto updateVertex = [i = -1, falloff]( const D3DXVECTOR3& position ) mutable {
		auto uv = ( ++i % 2 ? D3DXVECTOR2{ falloff, 0 } : D3DXVECTOR2{ 0, 0 } );

		return CUSTOM_VERTEX{ position, uv };
	};
	std::transform( std::cbegin( points ), std::cend( points ), std::back_inserter( vertices ), updateVertex );
	vertices[0].uv = { falloff, falloff };

#ifdef DEBUG_LINE
	auto debugPosition = []( const CUSTOM_VERTEX& vertex ) {
		auto& position = vertex.position;
		TCHAR debugText[MAX_PATH] = {};
		_stprintf_s( debugText, _countof( debugText ), TEXT( "%f,%f,%f\n" ), position.x, position.y, position.z );
		OutputDebugString( debugText );
	};
	std::for_each( vertices.begin(), vertices.end(), debugPosition );
#endif

	auto verticesSize = sizeof( Vertices::value_type ) * vertices.size();
	LPDIRECT3DVERTEXBUFFER9 pVertexBuffer{};

	// 버텍스 버퍼 갱신
	{
		if ( FAILED( pDevice->CreateVertexBuffer( static_cast<UINT>(verticesSize), 0, m_lightVertexFvf, D3DPOOL_DEFAULT, &pVertexBuffer, NULL ) ) ) {
			ASSERT( FALSE );
			return E_FAIL;
		}

		CopyToMemory( pVertexBuffer, vertices.data(), static_cast<UINT>( verticesSize ) );
	}


	SAFE_RELEASE( *pOut );
	*pOut = pVertexBuffer;

	return S_OK;
}

D3DXVECTOR3 CFreeformLight::GetCenterPoint( const Points& points ) const
{
	auto getMidPoint = []( const D3DXVECTOR3& p0, const D3DXVECTOR3& p1 ) -> D3DXVECTOR3 {
		return ( p0 + p1 ) / 2.f;
	};
	auto getNDivedPoint = []( const D3DXVECTOR3& p0, const D3DXVECTOR3& p1, int i ) -> D3DXVECTOR3 {
		return p0 + ( p1 - p0 ) / static_cast<float>( i );
	};
	auto center = getMidPoint( points[0], points[1] );

	for ( auto i = 2; i < points.size(); ++i ) {
		center = getNDivedPoint( center, points[i], i );
	}

	return center;
}

// https://bowbowbow.tistory.com/24
BOOL CFreeformLight::IsInsidePolygon( const Points& linePoints, const D3DXVECTOR3& point ) const
{
	//crosses는 점q와 오른쪽 반직선과 다각형과의 교점의 개수
	auto crosses = 0;
	auto y = point.y;

	for ( auto index = 0; index < linePoints.size(); ++index ) {
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

// http://www.gisdeveloper.co.kr/?p=89
BOOL CFreeformLight::GetCrossPoint( D3DXVECTOR3& out, D3DXVECTOR3 p0, D3DXVECTOR3 p1, const D3DXVECTOR2& mousePosition ) const
{
	using _PointPair = std::pair<D3DXVECTOR2, D3DXVECTOR2>;
	using _LinePoints = std::initializer_list<_PointPair>;

	if ( p0.y > p1.y ) {
		std::swap( p0, p1 );
	}
	
	D3DXVECTOR2 horizonPoint0 = { mousePosition.x, {} };
	D3DXVECTOR2 horizonPoint1 = { mousePosition.x, static_cast<float>( m_displayMode.Height ) };
	D3DXVECTOR2 verticalPoint0 = { {}, mousePosition.y };
	D3DXVECTOR2 verticalPoint1 = { static_cast<float>( m_displayMode.Width ), mousePosition.y };

	// 수직선, 수평선을 그어 교점을 알아낸다
	for ( auto pair : _LinePoints{ { horizonPoint0, horizonPoint1 }, { verticalPoint0, verticalPoint1 } } ) {
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

					out = { cx, cy, {} };
					return TRUE;
				}
			}
		}
	}

	out = {};
	return FALSE;
}

HRESULT CFreeformLight::AddLightVertex( LPDIRECT3DDEVICE9 pDevice, size_t index, const D3DXVECTOR3& position )
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

	ClearEditingStates( m_lightVertices.size() );
	m_linePointsCaches.clear();
	return S_OK;
}

HRESULT CFreeformLight::RemoveLightVertex( LPDIRECT3DDEVICE9 pDevice, size_t index )
{
	auto points = GetPointsFromVertices( m_lightVertices );
	auto iterator = std::next( std::cbegin( points ), index );
	ASSERT( iterator != std::cend( points ) );
	points.erase( iterator );
	ASSERT( !points.empty() );

	points[0] = GetCenterPoint( points );

	if ( FAILED( UpdateLightVertexBuffer( &m_pLightVertexBuffer, m_lightVertices, pDevice, points, m_setting.falloff ) ) ) {
		ASSERT( FALSE );

		return E_FAIL;
	}
	else if ( FAILED( UpdateLightIndexBuffer( &m_pLightIndexBuffer, m_lightIndices, pDevice, points.size() ) ) ) {
		ASSERT( FALSE );

		return E_FAIL;
	}

	ClearEditingStates( m_lightVertices.size() );
	m_linePointsCaches.clear();
	return S_OK;
}

CFreeformLight::Points CFreeformLight::GetPointsFromVertices( const Vertices& ) const
{
	Points points;
	auto addPoint = []( const CUSTOM_VERTEX& vertex ) { return vertex.position; };
	std::transform( std::cbegin( m_lightVertices ), std::cend( m_lightVertices ), std::back_inserter( points ), addPoint );
	ASSERT( points.size() == m_lightVertices.size() );

	return points;
}

void CFreeformLight::ClearEditingStates( size_t vertexCount )
{
	m_vertexEditingStates.clear();
	m_vertexEditingStates.resize( vertexCount );
}