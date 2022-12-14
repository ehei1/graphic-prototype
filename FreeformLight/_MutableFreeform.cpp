#include "stdafx.h"
#include <string>
#include "_MutableFreeform.h"


namespace FreeformLight
{
	HRESULT _MutableFreeform::DrawImgui( LPDIRECT3DDEVICE9 pDevice, LONG xCenter, LONG yCenter, bool isAmbientMode, bool* pIsVisible )
	{
		ASSERT( pIsVisible );

		if ( !*pIsVisible ) {
			return S_OK;
		}

		constexpr auto windowTitleName = u8"Light";
		ImGui::Begin( windowTitleName, pIsVisible );

		auto& io = ImGui::GetIO();
		io.WantCaptureMouse = true;

		if ( isAmbientMode ) {
			ImGui::ColorEdit3( u8"Ambient", m_setting.ambient );
		}
		else {
			ImGui::TextWrapped( u8"Check the flag to change ambient" );
		}

		size_t selectedLightIndex{};

		if ( ImGui::CollapsingHeader( u8"Freeform" ) ) {
			if ( ImGui::IsItemHovered() ) {
				ImGui::BeginTooltip();
				ImGui::TextUnformatted( u8"Edit dynamically with mask which made of mesh" );
				ImGui::EndTooltip();
			}

			ImGui::Checkbox( u8"Helper", &m_setting.helper );
			ImGui::Checkbox( u8"Mask", &m_setting.maskVisible );

			if ( ImGui::Button( u8"Add" ) ) {
				AddLight( pDevice, xCenter, yCenter );
			}

			if ( ImGui::BeginTabBar( "Lights" ) )
			{
				for ( size_t i{}; i < m_tabs.size(); ++i ) {
					auto& tab = m_tabs[i];

					if ( ImGui::BeginTabItem( tab.m_name.c_str(), &tab.m_opened, ImGuiTabItemFlags_None ) ) {
						// focused tab
						if ( tab.m_opened ) {
							auto light = m_lightImpls[i];
							auto newLightSetting = light->GetSetting();

							ImGui::ColorEdit3( u8"Color", reinterpret_cast<float*>( &newLightSetting.lightColor ) );
							ImGui::SliderFloat( u8"Intensity", &newLightSetting.intensity, 0.f, 1.f );
							ImGui::SliderFloat( u8"Falloff", &newLightSetting.falloff, 0, 10 );
							light->SetSetting( pDevice, newLightSetting );

							selectedLightIndex = i;
						}
						// unfocused tab
						else {
							RemoveLight( i );
						}

						ImGui::EndTabItem();
					}
					else {
						if ( !tab.m_opened ) {
							RemoveLight( i );
						}
					}
				}

				ImGui::EndTabBar();
			}
		}

		ImGui::End();

		auto freeformLightVisible = !m_lightImpls.empty();

		if ( freeformLightVisible && m_setting.helper ) {
			auto light = m_lightImpls[selectedLightIndex];

			light->DrawHelper( pDevice, m_displayMode, windowTitleName );
		}

		return S_OK;
	}

	HRESULT _MutableFreeform::AddLight( LPDIRECT3DDEVICE9 pDevice, LONG x, LONG y )
	{
		auto points = GetDefaultPoints( m_displayMode, x, y );
		auto lightImpl = new _MutableLightImpl( pDevice, m_pBlurPixelShader, points );

		m_lightImpls.emplace_back( std::move( lightImpl ) );

		// activate new one
		m_tabs.emplace_back( true, std::to_string( m_tabs.size() + 1 ) );

		ASSERT( m_lightImpls.size() == m_tabs.size() );

		return S_OK;
	}

	HRESULT _MutableFreeform::RemoveLight( size_t index )
	{
		if ( m_lightImpls.size() > index ) {
			{
				auto iterator = std::next( std::begin( m_lightImpls ), index );
				m_lightImpls.erase( iterator );
			}

			{
				auto iterator = std::next( std::begin( m_tabs ), index );
				m_tabs.erase( iterator );
			}

			return S_OK;
		}
		else {
			ASSERT( FALSE );

			return E_FAIL;
		}
	}

	_MutableLightImpl::Points _MutableFreeform::GetDefaultPoints( D3DDISPLAYMODE const& displayMode, LONG x, LONG y ) const
	{
		_MutableLightImpl::Points points{ { {},{},{} } };
		auto scaledWidth = displayMode.Width / 4;
		auto scaledHeight = displayMode.Height / 4;

		// vertex
		auto leftTopPoints = { D3DXVECTOR3{ -0.5f, -0.5f, 0.f } };
		auto rightTopPoints = { D3DXVECTOR3{ 0.5f, -0.5f, 0.f } };
		auto rightBottomPoints = { D3DXVECTOR3{ 0.5f, 0.5f, 0.f } };
		auto leftBottomPoints = { D3DXVECTOR3{ -0.5f, 0.5f, 0.f } };

		// add point around face for clock wise
		for ( auto vertices : { leftTopPoints, rightTopPoints, rightBottomPoints, leftBottomPoints } ) {
			points.insert( points.end(), std::cbegin( vertices ), std::cend( vertices ) );
		}

		auto changeToWorldCoord = [center = D3DXVECTOR3{ static_cast<float>( x ), static_cast<float>( y ),{} }, scaledWidth, scaledHeight]( D3DXVECTOR3& point ) {
			point = D3DXVECTOR3{ point.x * scaledWidth, point.y * scaledHeight,{} } +center;
		};
		std::for_each( std::begin( points ), std::end( points ), changeToWorldCoord );

		return points;
	}

	HRESULT _MutableFreeform::RestoreDevice( LPDIRECT3DDEVICE9 pDevice, D3DDISPLAYMODE const& displayMode )
	{
		m_displayMode = displayMode;

		return __super::RestoreDevice( pDevice, displayMode );
	}
}