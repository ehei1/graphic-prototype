//-----------------------------------------------------------------------------
// File: Textures.cpp
//
// Desc: Better than just lights and materials, 3D objects look much more
//       convincing when texture-mapped. Textures can be thought of as a sort
//       of wallpaper, that is shrinkwrapped to fit a texture. Textures are
//       typically loaded from image files, and D3DX provides a utility to
//       function to do this for us. Like a vertex buffer, Textures have
//       Lock() and Unlock() functions to access (read or write) the image
//       data. Textures have a width, height, miplevel, and pixel format. The
//       miplevel is for "mipmapped" Textures, an advanced performance-
//       enhancing feature which uses lower resolutions of the texture for
//       objects in the distance where detail is less noticeable. The pixel
//       format determines how the colors are stored in a texel. The most
//       common formats are the 16-bit R5G6B5 format (5 bits of red, 6-bits of
//       green and 5 bits of blue) and the 32-bit A8R8G8B8 format (8 bits each
//       of alpha, red, green, and blue).
//
//       Textures are associated with geometry through texture coordinates.
//       Each vertex has one or more sets of texture coordinates, which are
//       named tu and tv and range from 0.0 to 1.0. Texture coordinates can be
//       supplied by the geometry, or can be automatically generated using
//       Direct3D texture coordinate generation (which is an advanced feature).
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------
#include "StdAfx.h"
#pragma warning( disable : 4996 ) // disable deprecated warning 
#include <strsafe.h>
#pragma warning( default : 4996 )
#include "FreeformLight.h"

//-----------------------------------------------------------------------------
// Global variables
//-----------------------------------------------------------------------------
LPDIRECT3D9             g_pD3D = NULL; // Used to create the D3DDevice
LPDIRECT3DDEVICE9       g_pd3dDevice = NULL; // Our rendering device
LPDIRECT3DVERTEXBUFFER9 g_pVB = NULL; // Buffer to hold vertices
LPDIRECT3DINDEXBUFFER9  g_pIB = NULL;
LPDIRECT3DTEXTURE9      g_pBackgroundTexture = NULL; // Our texture
LPDIRECT3DTEXTURE9		g_pMainScreenTexture = NULL;
LPD3DXMESH				g_pScreenMesh = NULL;
LPDIRECT3DTEXTURE9		g_pScreenTexture = NULL;
std::unique_ptr<CFreeformLight> g_pFreemformLight{ new CFreeformLight };
const D3DDISPLAYMODE	gDisplayMode{ 1024, 768, 0, D3DFMT_A8R8G8B8 };
float					gScale = 100;
D3DXVECTOR2				gTranslation{};

// A structure for our custom vertex type. We added texture coordinates
struct CUSTOM_VERTEX
{
	D3DXVECTOR3 position;
	D3DXVECTOR2 uv;
};

// Our custom FVF, which describes our custom vertex structure
#define D3DFVF_CUSTOM (D3DFVF_XYZ|D3DFVF_TEX1)

//#define DEBUG_SAMPLE

D3DXCOLOR ImVec4ToD3DXCOLOR( const ImVec4& src )
{
	return{ src.x, src.y, src.z, src.w };
}

ImVec4 D3DXCOLORToImVec4( const D3DXCOLOR& src )
{
	return{ src.r, src.g, src.b, src.a };
}

//-----------------------------------------------------------------------------
// Name: InitD3D()
// Desc: Initializes Direct3D
//-----------------------------------------------------------------------------
HRESULT InitD3D( HWND hWnd )
{
    // Create the D3D object.
    if( NULL == ( g_pD3D = Direct3DCreate9( D3D_SDK_VERSION ) ) )
        return E_FAIL;

    // Set up the structure used to create the D3DDevice. Since we are now
    // using more complex geometry, we will create a device with a zbuffer.
    D3DPRESENT_PARAMETERS d3dpp;
    ZeroMemory( &d3dpp, sizeof( d3dpp ) );
    d3dpp.Windowed = TRUE;
    d3dpp.SwapEffect = D3DSWAPEFFECT_DISCARD;
    d3dpp.BackBufferFormat = D3DFMT_UNKNOWN;
	d3dpp.PresentationInterval = D3DPRESENT_INTERVAL_ONE;

    // Create the D3DDevice
    if( FAILED( g_pD3D->CreateDevice( D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, hWnd,
		D3DCREATE_HARDWARE_VERTEXPROCESSING,
                                      &d3dpp, &g_pd3dDevice ) ) )
    {
        return E_FAIL;
    }

    // Turn off culling
    g_pd3dDevice->SetRenderState( D3DRS_CULLMODE, D3DCULL_NONE );

    // Turn off D3D lighting
    g_pd3dDevice->SetRenderState( D3DRS_LIGHTING, FALSE );

    // Turn on the zbuffer
	g_pd3dDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, TRUE );
    g_pd3dDevice->SetRenderState( D3DRS_ZENABLE, TRUE);
	g_pd3dDevice->SetRenderState( D3DRS_FILLMODE, D3DFILL_SOLID );

	g_pd3dDevice->SetRenderState( D3DRS_SRCBLEND, D3DBLEND_SRCALPHA );    // set source factor
	g_pd3dDevice->SetRenderState( D3DRS_DESTBLEND, D3DBLEND_DESTALPHA );    // set dest factor
	g_pd3dDevice->SetRenderState( D3DRS_BLENDOP, D3DBLENDOP_ADD );    // set the operation

	g_pFreemformLight->RestoreDevice( gDisplayMode );

	if ( FAILED( g_pd3dDevice->CreateTexture( gDisplayMode.Width, gDisplayMode.Height, 1, D3DUSAGE_RENDERTARGET, gDisplayMode.Format, D3DPOOL_DEFAULT, &g_pMainScreenTexture, NULL ) ) ) {
		return E_FAIL;
	}

    return S_OK;
}

//-----------------------------------------------------------------------------
// Name: InitGeometry()
// Desc: Create the Textures and vertex buffers
//-----------------------------------------------------------------------------
HRESULT InitGeometry()
{
	// 배경판
	{
		// 버텍스
		{
			auto w = static_cast<float>( gDisplayMode.Width );
			auto h = static_cast<float>( gDisplayMode.Height );

			CUSTOM_VERTEX positions[]{
				{ { 0, 0, 0.f }, { 0, 0 } },
				{ { 0, h, 0.f }, { 0, 1 } },
				{ { w, h, 0.f }, { 1, 1 } },
				{ { w, 0, 0.f }, { 1, 0 } },
			};

			if ( FAILED( g_pd3dDevice->CreateVertexBuffer( sizeof( positions ), 0, D3DFVF_CUSTOM, D3DPOOL_DEFAULT, &g_pVB, NULL ) ) )
			{
				return E_FAIL;
			}

			LPVOID pVertices = {};

			if ( FAILED( g_pVB->Lock( 0, sizeof( positions ), &pVertices, 0 ) ) )
				return E_FAIL;

			memcpy( pVertices, positions, sizeof( positions ) );
			g_pVB->Unlock();
		}

		// 인덱스
		{
			WORD indices[]{
				0, 1, 2, 3,
			};

			if ( FAILED( g_pd3dDevice->CreateIndexBuffer( sizeof( indices ),
				D3DUSAGE_WRITEONLY, D3DFMT_INDEX16, D3DPOOL_DEFAULT,
				&g_pIB, NULL ) ) ) {
				return E_FAIL;
			}

			LPVOID pIndices = {};
			g_pIB->Lock( 0, sizeof( indices ), &pIndices, 0 );
			memcpy( pIndices, indices, sizeof( indices ) );
			g_pIB->Unlock();
		}
	}

	g_pFreemformLight->CreateMesh( g_pd3dDevice, &g_pScreenMesh, gDisplayMode.Width, gDisplayMode.Height );
	g_pFreemformLight->CreateTexture( g_pd3dDevice, &g_pScreenTexture, gDisplayMode.Width, gDisplayMode.Height );

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: Cleanup()
// Desc: Releases all previously initialized objects
//-----------------------------------------------------------------------------
VOID Cleanup()
{
	SAFE_RELEASE( g_pScreenTexture );
	SAFE_RELEASE( g_pScreenMesh );

    if( g_pBackgroundTexture != NULL )
        g_pBackgroundTexture->Release();

	if ( g_pMainScreenTexture != NULL )
		g_pMainScreenTexture->Release();

    if( g_pVB != NULL )
        g_pVB->Release();

    if( g_pd3dDevice != NULL )
        g_pd3dDevice->Release();

    if( g_pD3D != NULL )
        g_pD3D->Release();
}



//-----------------------------------------------------------------------------
// Name: SetupMatrices()
// Desc: Sets up the world, view, and projection transform matrices.
//-----------------------------------------------------------------------------
VOID SetupMatrices()
{
    // Set up world matrix
    D3DXMATRIXA16 matWorld;
    D3DXMatrixIdentity( &matWorld );
    D3DXMatrixRotationX( &matWorld, timeGetTime() / 1000.0f );
    g_pd3dDevice->SetTransform( D3DTS_WORLD, &matWorld );

    // Set up our view matrix. A view matrix can be defined given an eye point,
    // a point to lookat, and a direction for which way is up. Here, we set the
    // eye five units back along the z-axis and up three units, look at the
    // origin, and define "up" to be in the y-direction.
    D3DXVECTOR3 vEyePt( 0.0f, 3.0f,-8.0f );
    D3DXVECTOR3 vLookatPt( 0.0f, 0.0f, 0.0f );
    D3DXVECTOR3 vUpVec( 0.0f, 1.0f, 0.0f );
    D3DXMATRIXA16 matView;
    D3DXMatrixLookAtLH( &matView, &vEyePt, &vLookatPt, &vUpVec );
    g_pd3dDevice->SetTransform( D3DTS_VIEW, &matView );

    // For the projection matrix, we set up a perspective transform (which
    // transforms geometry from 3D view space to 2D viewport space, with
    // a perspective divide making objects smaller in the distance). To build
    // a perpsective transform, we need the field of view (1/4 pi is common),
    // the aspect ratio, and the near and far clipping planes (which define at
    // what distances geometry should be no longer be rendered).
    D3DXMATRIXA16 matProj;
    D3DXMatrixPerspectiveFovLH( &matProj, D3DX_PI / 4, 1.0f, 1.0f, 100.0f );
    g_pd3dDevice->SetTransform( D3DTS_PROJECTION, &matProj );
}




//-----------------------------------------------------------------------------
// Name: Render()
// Desc: Draws the scene
//-----------------------------------------------------------------------------
VOID Render()
{	
	auto w = static_cast<float>( gDisplayMode.Width );
	auto h = static_cast<float>( gDisplayMode.Height );
	auto x = w / 2.f;
	auto y = h / 2.f;

	// camera
	{
		D3DXVECTOR3 eye{ x, y, 1 };
		D3DXVECTOR3 at{ x, y, -1 };
		D3DXVECTOR3 up{ 0, -1, 0 };

		D3DXMATRIX projection{};
		D3DXMatrixOrthoLH( &projection, w, h, -1, 1 );
		g_pd3dDevice->SetTransform( D3DTS_PROJECTION, &projection );

		D3DXMATRIX view{};
		D3DXMatrixLookAtLH( &view, &eye, &at, &up );

		D3DXMATRIX scale{};
		D3DXMatrixScaling( &scale, gScale / 100.f, gScale / 100.f, 1 );
		view *= scale;

		D3DXMATRIX translation{};
		D3DXMatrixTranslation( &translation, gTranslation.x, gTranslation.y, 0 );
		view *= translation;

		g_pd3dDevice->SetTransform( D3DTS_VIEW, &view );
	}

	LPDIRECT3DSURFACE9 pCurrentSurface{};
	g_pd3dDevice->GetRenderTarget( 0, &pCurrentSurface );

	// Draw on main screen
	{
		LPDIRECT3DSURFACE9 pMainScreenSurface{};
		g_pMainScreenTexture->GetSurfaceLevel( 0, &pMainScreenSurface );
		g_pd3dDevice->SetRenderTarget( 0, pMainScreenSurface );

		// Clear the backbuffer and the zbuffer
		g_pd3dDevice->Clear( 0, NULL, D3DCLEAR_TARGET, D3DCOLOR_ARGB( 255, 0, 0, 0 ), 0.0f, 0 );

		// Begin the scene
		if ( SUCCEEDED( g_pd3dDevice->BeginScene() ) )
		{
			// Setup the world, view, and projection matrices
			//SetupMatrices();

			DWORD curBlendOp = {};
			DWORD curSrcBlend = {};
			DWORD curDestBlend = {};
			g_pd3dDevice->GetRenderState( D3DRS_BLENDOP, &curBlendOp );
			g_pd3dDevice->GetRenderState( D3DRS_SRCBLEND, &curSrcBlend );
			g_pd3dDevice->GetRenderState( D3DRS_DESTBLEND, &curDestBlend );

			g_pd3dDevice->SetRenderState( D3DRS_BLENDOP, D3DBLENDOP_ADD );
			g_pd3dDevice->SetRenderState( D3DRS_SRCBLEND, D3DBLEND_ONE );
			g_pd3dDevice->SetRenderState( D3DRS_DESTBLEND, D3DBLEND_ZERO );

			DWORD currentFVF = {};
			g_pd3dDevice->GetFVF( &currentFVF );

			// Render the vertex buffer contents
			g_pd3dDevice->SetTexture( 0, g_pBackgroundTexture );
			g_pd3dDevice->SetStreamSource( 0, g_pVB, 0, sizeof( CUSTOM_VERTEX ) );

			g_pd3dDevice->SetIndices( g_pIB );
			g_pd3dDevice->SetFVF( D3DFVF_CUSTOM );
			g_pd3dDevice->DrawIndexedPrimitive( D3DPT_TRIANGLEFAN, 0, 0, 4, 0, 2 );

			// End the scene
			g_pd3dDevice->EndScene();

			// restore previos setting
			g_pd3dDevice->SetFVF( currentFVF );
			g_pd3dDevice->SetRenderState( D3DRS_BLENDOP, curBlendOp );
			g_pd3dDevice->SetRenderState( D3DRS_SRCBLEND, curSrcBlend );
			g_pd3dDevice->SetRenderState( D3DRS_DESTBLEND, curDestBlend );

#ifdef DEBUG_SAMPLE
			D3DXSaveTextureToFile( L"D:\\screenshot.png", D3DXIFF_PNG, g_pMainScreenTexture, NULL );
#endif
		}

		// 프리폼 조명
		if ( g_pFreemformLight->IsVisible() ) 
		{
			LPDIRECT3DSURFACE9 pScreenSurface{};
			g_pScreenTexture->GetSurfaceLevel( 0, &pScreenSurface );

			auto clearColor = g_pFreemformLight->GetSetting().shadowColor;

			LPDIRECT3DSURFACE9 curRT = {};
			g_pd3dDevice->GetRenderTarget( 0, &curRT );
			g_pd3dDevice->SetRenderTarget( 0, pScreenSurface );
			g_pd3dDevice->Clear( 0, 0, D3DCLEAR_TARGET, clearColor, 0.0f, 0 );

			g_pFreemformLight->Draw( g_pd3dDevice, pScreenSurface, x, y );
		}

		SAFE_RELEASE( pMainScreenSurface );
	}

	g_pd3dDevice->SetRenderTarget( 0, pCurrentSurface );
	SAFE_RELEASE( pCurrentSurface );

	g_pd3dDevice->Clear( 0, NULL, D3DCLEAR_TARGET, D3DCOLOR_ARGB( 0, 0, 0, 0 ), 0.0f, 0 );

	// Overwrite to current render target
	if ( SUCCEEDED( g_pd3dDevice->BeginScene() ) )
	{
		DWORD curBlendOp = {};
		DWORD curSrcBlend = {};
		DWORD curDestBlend = {};
		g_pd3dDevice->GetRenderState( D3DRS_BLENDOP, &curBlendOp );
		g_pd3dDevice->GetRenderState( D3DRS_SRCBLEND, &curSrcBlend );
		g_pd3dDevice->GetRenderState( D3DRS_DESTBLEND, &curDestBlend );

		DWORD currentFVF = {};
		g_pd3dDevice->GetFVF( &currentFVF );

		{
			g_pd3dDevice->SetRenderState( D3DRS_BLENDOP, D3DBLENDOP_ADD );
			g_pd3dDevice->SetRenderState( D3DRS_SRCBLEND, D3DBLEND_ONE );
			g_pd3dDevice->SetRenderState( D3DRS_DESTBLEND, D3DBLEND_ZERO );

			g_pd3dDevice->SetTexture( 0, g_pMainScreenTexture );
			g_pd3dDevice->SetStreamSource( 0, g_pVB, 0, sizeof( CUSTOM_VERTEX ) );
			g_pd3dDevice->SetIndices( g_pIB );
			g_pd3dDevice->SetFVF( D3DFVF_CUSTOM );
			g_pd3dDevice->DrawIndexedPrimitive( D3DPT_TRIANGLEFAN, 0, 0, 4, 0, 2 );
		}

		// 마스크 그리기
		if ( g_pFreemformLight->IsVisible() )
		{
			D3DXMATRIX curWm = {};
			g_pd3dDevice->GetTransform( D3DTS_WORLD, &curWm );

			// 이동
			D3DXMATRIX tm{};
			D3DXMatrixTranslation( &tm, x, y, 0 );
			g_pd3dDevice->SetTransform( D3DTS_WORLD, &tm );

			if ( !g_pFreemformLight->GetSetting().maskOnly ) {
				// 마스크를 반전해서 게임 화면이 그려진 렌더타겟의 색깔과 곱한다
				// result = src * 0 + dest * ( 1 - srcAlpha )
				g_pd3dDevice->SetRenderState( D3DRS_BLENDOP, D3DBLENDOP_ADD );
				g_pd3dDevice->SetRenderState( D3DRS_DESTBLEND, D3DBLEND_ZERO );
				g_pd3dDevice->SetRenderState( D3DRS_SRCBLEND, D3DBLEND_DESTCOLOR );
			}

			g_pd3dDevice->SetTexture( 0, g_pScreenTexture );
			g_pScreenMesh->DrawSubset( 0 );

			g_pd3dDevice->SetTransform( D3DTS_WORLD, &curWm );
		}

		g_pd3dDevice->EndScene();
		g_pd3dDevice->SetFVF( currentFVF );
		g_pd3dDevice->SetRenderState( D3DRS_BLENDOP, curBlendOp );
		g_pd3dDevice->SetRenderState( D3DRS_SRCBLEND, curSrcBlend );
		g_pd3dDevice->SetRenderState( D3DRS_DESTBLEND, curDestBlend );
	}
}

// Forward declare message handler from imgui_impl_win32.cpp
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler( HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam );

//-----------------------------------------------------------------------------
// Name: MsgProc()
// Desc: The window's message handler
//-----------------------------------------------------------------------------
LRESULT WINAPI MsgProc( HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam )
{
	if ( ImGui_ImplWin32_WndProcHandler( hWnd, msg, wParam, lParam ) )
		return true;

    switch( msg )
    {
        case WM_DESTROY:
            Cleanup();
            PostQuitMessage( 0 );
            return 0;
		case WM_KEYDOWN:
			switch ( wParam ) {
			case VK_LEFT:
				--gTranslation.x;
				break;
			case VK_RIGHT:
				++gTranslation.x;
				break;
			case VK_UP:
				--gTranslation.y;
				break;
			case VK_DOWN:
				++gTranslation.y;
				break;
			case VK_HOME:
				gTranslation = {};
				gScale = 100;
				break;
			}

			return 0;
    }

    return DefWindowProc( hWnd, msg, wParam, lParam );
}




//-----------------------------------------------------------------------------
// Name: WinMain()
// Desc: The application's entry point
//-----------------------------------------------------------------------------
INT WINAPI wWinMain( HINSTANCE hInst, HINSTANCE, LPWSTR, INT )
{
    UNREFERENCED_PARAMETER( hInst );

    // Register the window class
    WNDCLASSEX wc =
    {
        sizeof( WNDCLASSEX ), CS_CLASSDC, MsgProc, 0L, 0L,
        GetModuleHandle( NULL ), NULL, NULL, NULL, NULL,
        L"D3D Tutorial", NULL
    };
    RegisterClassEx( &wc );

    // Create the application's window
    HWND hWnd = CreateWindow( L"D3D Tutorial", L"Freeform Light",
                              WS_OVERLAPPEDWINDOW, 100, 100, gDisplayMode.Width, gDisplayMode.Height,
                              NULL, NULL, wc.hInstance, NULL );

    // Initialize Direct3D
    if( SUCCEEDED( InitD3D( hWnd ) ) )
    {	
		if ( FAILED( D3DXCreateTextureFromFile( g_pd3dDevice, L"resource\\maplestory-002.jpg", &g_pBackgroundTexture ) ) ) {
			return E_FAIL; 
		}

        // Create the scene geometry
        if( SUCCEEDED( InitGeometry() ) )
        {
            // Show the window
            ShowWindow( hWnd, SW_SHOWDEFAULT );
            UpdateWindow( hWnd );

			IMGUI_CHECKVERSION();
			ImGui::CreateContext();
			ImGuiIO& io = ImGui::GetIO(); (void)io;

			// Setup Dear ImGui style
			ImGui::StyleColorsDark();
			//ImGui::StyleColorsClassic();

			// Setup Platform/Renderer bindings
			ImGui_ImplWin32_Init( hWnd );
			ImGui_ImplDX9_Init( g_pd3dDevice );

			// https://stackoverflow.com/questions/11387564/get-a-font-filepath-from-name-and-style-in-c-windows
			{
				char winDir[MAX_PATH]{};
				GetWindowsDirectoryA( winDir, _countof( winDir ) );

				std::string fontName = "\\fonts\\malgun.ttf";
				fontName.insert( fontName.cbegin(), winDir, winDir + strlen( winDir ) );

				io.Fonts->AddFontFromFileTTF( fontName.c_str(), 15, nullptr, io.Fonts->GetGlyphRangesKorean() );
			}

			auto showWindow = true;

            // Enter the message loop
			MSG msg{};

            while( msg.message != WM_QUIT )
            {
				if ( msg.message == WM_MOUSEWHEEL ) {
					auto delta = GET_WHEEL_DELTA_WPARAM( msg.wParam ) / static_cast<float>( WHEEL_DELTA );
					gScale += delta;

					gScale = max( gScale, 50 );
					gScale = min( gScale, 200 );
				}

                if( PeekMessage( &msg, NULL, 0U, 0U, PM_REMOVE ) )
                {
					TranslateMessage( &msg );
					DispatchMessage( &msg );
					continue;
                }

				// Start the Dear ImGui frame
				ImGui_ImplDX9_NewFrame();
				ImGui_ImplWin32_NewFrame();
				ImGui::NewFrame();

				ImGui::SetNextWindowPos( { gDisplayMode.Width / 2.f, gDisplayMode.Height / 2.f }, ImGuiCond_Once );

				// Create ImGui widget
				auto xCenter = gDisplayMode.Width / 2;
				auto yCenter = gDisplayMode.Height / 2;
				g_pFreemformLight->CreateImgui( g_pd3dDevice, xCenter, yCenter, showWindow );

				//ImGui::ShowDemoWindow();

				// Rendering
				ImGui::EndFrame();

				Render();

				if ( SUCCEEDED( g_pd3dDevice->BeginScene() ) ) {
					ImGui::Render();
					ImGui_ImplDX9_RenderDrawData( ImGui::GetDrawData() );

					g_pd3dDevice->EndScene();
				}

				// Present the backbuffer contents to the display
				g_pd3dDevice->Present( NULL, NULL, NULL, NULL );
            }
        }
    }

	ImGui_ImplDX9_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();

	SAFE_RELEASE( g_pd3dDevice );
	SAFE_RELEASE( g_pD3D );

    UnregisterClass( L"D3D Tutorial", wc.hInstance );
    return 0;
}