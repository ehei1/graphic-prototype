#include "StdAfx.h"

#include <chrono>
#include <ctime>
#include <future>
#include <list>
#include <string>
#include <thread>

#pragma warning( disable : 4996 ) // disable deprecated warning 
#include <strsafe.h>
#pragma warning( default : 4996 )

#ifdef _DEBUG
#include "Debug\ps_gaussianblur.h"
#else
#include "Release\ps_gaussianblur.h"
#endif
#include "FreeformLight\light.h"
#include "ImageFilter/IImageFilter.h"
#include "ImageFilter/DirectXTex/DirectXTex.h"

//-----------------------------------------------------------------------------
// Global variables
//-----------------------------------------------------------------------------
LPDIRECT3D9             g_pD3D = NULL; // Used to create the D3DDevice
LPDIRECT3DDEVICE9       g_pDevice = NULL; // Our rendering device
LPDIRECT3DVERTEXBUFFER9 g_pVB = NULL; // Buffer to hold vertices
LPDIRECT3DINDEXBUFFER9  g_pIB = NULL;
LPDIRECT3DTEXTURE9		g_pMainScreenTexture = NULL;
LPD3DXMESH				g_pScreenMesh = NULL;
LPDIRECT3DTEXTURE9		g_pScreenTexture = NULL;
std::unique_ptr<CMutableFreeformLight> g_pFreemformLight = NULL;
D3DDISPLAYMODE			gDisplayMode{ 1024, 768, 0, D3DFMT_A8R8G8B8 };
float					gScale = 100;
D3DXVECTOR2				gTranslation{};
LPDIRECT3DPIXELSHADER9  g_pBlurPixelShader{};

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

struct ExampleAppLog
{
	ImGuiTextBuffer     Buf;
	ImGuiTextFilter     Filter;
	ImVector<int>       LineOffsets;        // Index to lines offset. We maintain this with AddLog() calls, allowing us to have a random access on lines
	bool                AutoScroll;     // Keep scrolling if already at the bottom

	ExampleAppLog()
	{
		AutoScroll = true;
		Clear();
	}

	void    Clear()
	{
		Buf.clear();
		LineOffsets.clear();
		LineOffsets.push_back(0);
	}

	void    AddLog(const char* fmt, ...) IM_FMTARGS(2)
	{
		int old_size = Buf.size();
		va_list args;
		va_start(args, fmt);
		Buf.appendfv(fmt, args);
		va_end(args);
		for (int new_size = Buf.size(); old_size < new_size; old_size++)
			if (Buf[old_size] == '\n')
				LineOffsets.push_back(old_size + 1);
	}

	void    Draw(const char* title, bool* p_open = NULL)
	{
		if (!ImGui::Begin(title, p_open))
		{
			ImGui::End();
			return;
		}

		// Options menu
		if (ImGui::BeginPopup("Options"))
		{
			ImGui::Checkbox("Auto-scroll", &AutoScroll);
			ImGui::EndPopup();
		}

		// Main window
		if (ImGui::Button("Options"))
			ImGui::OpenPopup("Options");
		ImGui::SameLine();
		bool clear = ImGui::Button("Clear");
		ImGui::SameLine();
		bool copy = ImGui::Button("Copy");
		ImGui::SameLine();
		Filter.Draw("Filter", -100.0f);

		ImGui::Separator();
		ImGui::BeginChild("scrolling", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);

		if (clear)
			Clear();
		if (copy)
			ImGui::LogToClipboard();

		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
		const char* buf = Buf.begin();
		const char* buf_end = Buf.end();
		if (Filter.IsActive())
		{
			// In this example we don't use the clipper when Filter is enabled.
			// This is because we don't have a random access on the result on our filter.
			// A real application processing logs with ten of thousands of entries may want to store the result of search/filter.
			// especially if the filtering function is not trivial (e.g. reg-exp).
			for (int line_no = 0; line_no < LineOffsets.Size; line_no++)
			{
				const char* line_start = buf + LineOffsets[line_no];
				const char* line_end = (line_no + 1 < LineOffsets.Size) ? (buf + LineOffsets[line_no + 1] - 1) : buf_end;
				if (Filter.PassFilter(line_start, line_end))
					ImGui::TextUnformatted(line_start, line_end);
			}
		}
		else
		{
			// The simplest and easy way to display the entire buffer:
			//   ImGui::TextUnformatted(buf_begin, buf_end);
			// And it'll just work. TextUnformatted() has specialization for large blob of text and will fast-forward to skip non-visible lines.
			// Here we instead demonstrate using the clipper to only process lines that are within the visible area.
			// If you have tens of thousands of items and their processing cost is non-negligible, coarse clipping them on your side is recommended.
			// Using ImGuiListClipper requires A) random access into your data, and B) items all being the  same height,
			// both of which we can handle since we an array pointing to the beginning of each line of text.
			// When using the filter (in the block of code above) we don't have random access into the data to display anymore, which is why we don't use the clipper.
			// Storing or skimming through the search result would make it possible (and would be recommended if you want to search through tens of thousands of entries)
			ImGuiListClipper clipper;
			clipper.Begin(LineOffsets.Size);
			while (clipper.Step())
			{
				for (int line_no = clipper.DisplayStart; line_no < clipper.DisplayEnd; line_no++)
				{
					const char* line_start = buf + LineOffsets[line_no];
					const char* line_end = (line_no + 1 < LineOffsets.Size) ? (buf + LineOffsets[line_no + 1] - 1) : buf_end;
					ImGui::TextUnformatted(line_start, line_end);
				}
			}
			clipper.End();
		}
		ImGui::PopStyleVar();

		if (AutoScroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
			ImGui::SetScrollHereY(1.0f);

		ImGui::EndChild();
		ImGui::End();
	}
};

struct Layer
{
	Layer(LPDIRECT3DTEXTURE9 pTexture) : m_pTexture{ pTexture }
	{}

	LPDIRECT3DTEXTURE9  m_pTexture{};
};
std::list< Layer > g_layers;


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
    if( FAILED( g_pD3D->CreateDevice( D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, hWnd, D3DCREATE_MULTITHREADED | D3DCREATE_HARDWARE_VERTEXPROCESSING | D3DCREATE_FPU_PRESERVE, &d3dpp, &g_pDevice ) ) )
    {
        return E_FAIL;
    }

    // Turn off culling
    g_pDevice->SetRenderState( D3DRS_CULLMODE, D3DCULL_NONE );

    // Turn off D3D lighting
    g_pDevice->SetRenderState( D3DRS_LIGHTING, FALSE );

    // Turn on the zbuffer
	g_pDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, TRUE );
    g_pDevice->SetRenderState( D3DRS_ZENABLE, TRUE);
	g_pDevice->SetRenderState( D3DRS_FILLMODE, D3DFILL_SOLID );

	g_pDevice->SetRenderState( D3DRS_SRCBLEND, D3DBLEND_SRCALPHA );    // set source factor
	g_pDevice->SetRenderState( D3DRS_DESTBLEND, D3DBLEND_DESTALPHA );    // set dest factor
	g_pDevice->SetRenderState( D3DRS_BLENDOP, D3DBLENDOP_ADD );    // set the operation

	if ( !g_pBlurPixelShader )
	{
		auto function = const_cast<BYTE*>( g_ps21_gaussianblur );

		if ( FAILED( g_pDevice->CreatePixelShader( reinterpret_cast<LPDWORD>( function ), &g_pBlurPixelShader ) ) ) {
			SAFE_RELEASE( g_pBlurPixelShader );
			return E_FAIL;
		}
	}

	g_pFreemformLight.reset( new CMutableFreeformLight{ g_pBlurPixelShader, gDisplayMode } );

	if ( FAILED( g_pDevice->CreateTexture( gDisplayMode.Width, gDisplayMode.Height, 1, D3DUSAGE_RENDERTARGET, gDisplayMode.Format, D3DPOOL_DEFAULT, &g_pMainScreenTexture, NULL ) ) ) {
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
	{
		{
			auto w = static_cast<float>( gDisplayMode.Width );
			auto h = static_cast<float>( gDisplayMode.Height );

			CUSTOM_VERTEX positions[]{
				{ { 0, 0, 0.f }, { 0, 0 } },
				{ { 0, h, 0.f }, { 0, 1 } },
				{ { w, h, 0.f }, { 1, 1 } },
				{ { w, 0, 0.f }, { 1, 0 } },
			};

			if ( FAILED( g_pDevice->CreateVertexBuffer( sizeof( positions ), 0, D3DFVF_CUSTOM, D3DPOOL_DEFAULT, &g_pVB, NULL ) ) )
			{
				return E_FAIL;
			}

			LPVOID pVertices = {};

			if ( FAILED( g_pVB->Lock( 0, sizeof( positions ), &pVertices, 0 ) ) )
				return E_FAIL;

			memcpy( pVertices, positions, sizeof( positions ) );
			g_pVB->Unlock();
		}

		{
			WORD indices[]{
				0, 1, 2, 3,
			};

			if ( FAILED( g_pDevice->CreateIndexBuffer( sizeof( indices ),
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

	FreeformLight::_ImmutableLightImpl::CreateMesh( g_pDevice, &g_pScreenMesh, gDisplayMode.Width, gDisplayMode.Height );
	FreeformLight::_ImmutableLightImpl::CreateTexture( g_pDevice, &g_pScreenTexture, gDisplayMode.Width, gDisplayMode.Height );

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

	for (auto& layer : g_layers) {
		SAFE_RELEASE(layer.m_pTexture);
	}

	if ( g_pMainScreenTexture != NULL )
		g_pMainScreenTexture->Release();

    if( g_pVB != NULL )
        g_pVB->Release();

	g_pFreemformLight.reset();

    if( g_pDevice != NULL )
        g_pDevice->Release();

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
    g_pDevice->SetTransform( D3DTS_WORLD, &matWorld );

    // Set up our view matrix. A view matrix can be defined given an eye point,
    // a point to lookat, and a direction for which way is up. Here, we set the
    // eye five units back along the z-axis and up three units, look at the
    // origin, and define "up" to be in the y-direction.
    D3DXVECTOR3 vEyePt( 0.0f, 3.0f,-8.0f );
    D3DXVECTOR3 vLookatPt( 0.0f, 0.0f, 0.0f );
    D3DXVECTOR3 vUpVec( 0.0f, 1.0f, 0.0f );
    D3DXMATRIXA16 matView;
    D3DXMatrixLookAtLH( &matView, &vEyePt, &vLookatPt, &vUpVec );
    g_pDevice->SetTransform( D3DTS_VIEW, &matView );

    // For the projection matrix, we set up a perspective transform (which
    // transforms geometry from 3D view space to 2D viewport space, with
    // a perspective divide making objects smaller in the distance). To build
    // a perpsective transform, we need the field of view (1/4 pi is common),
    // the aspect ratio, and the near and far clipping planes (which define at
    // what distances geometry should be no longer be rendered).
    D3DXMATRIXA16 matProj;
    D3DXMatrixPerspectiveFovLH( &matProj, D3DX_PI / 4, 1.0f, 1.0f, 100.0f );
    g_pDevice->SetTransform( D3DTS_PROJECTION, &matProj );
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
		g_pDevice->SetTransform( D3DTS_PROJECTION, &projection );

		D3DXMATRIX view{};
		D3DXMatrixLookAtLH( &view, &eye, &at, &up );

		D3DXMATRIX scale{};
		D3DXMatrixScaling( &scale, gScale / 100.f, gScale / 100.f, 1 );
		view *= scale;

		D3DXMATRIX translation{};
		D3DXMatrixTranslation( &translation, gTranslation.x, gTranslation.y, 0 );
		view *= translation;

		g_pDevice->SetTransform( D3DTS_VIEW, &view );
	}

	LPDIRECT3DSURFACE9 pCurrentSurface{};
	g_pDevice->GetRenderTarget( 0, &pCurrentSurface );

	// Draw on main screen
	{
		LPDIRECT3DSURFACE9 pMainScreenSurface{};
		g_pMainScreenTexture->GetSurfaceLevel( 0, &pMainScreenSurface );
		g_pDevice->SetRenderTarget( 0, pMainScreenSurface );

		// Clear the backbuffer and the zbuffer
		g_pDevice->Clear( 0, NULL, D3DCLEAR_TARGET, D3DCOLOR_ARGB( 255, 0, 0, 0 ), 0.0f, 0 );

		// Begin the scene
		if ( SUCCEEDED( g_pDevice->BeginScene() ) )
		{
			DWORD currentFVF = {};
			g_pDevice->GetFVF( &currentFVF );
			g_pDevice->SetStreamSource( 0, g_pVB, 0, sizeof( CUSTOM_VERTEX ) );
			g_pDevice->SetIndices( g_pIB );
			g_pDevice->SetFVF( D3DFVF_CUSTOM );

			DWORD curBlendOp = {};
			DWORD curSrcBlend = {};
			DWORD curDestBlend = {};
			g_pDevice->GetRenderState( D3DRS_BLENDOP, &curBlendOp );
			g_pDevice->GetRenderState( D3DRS_SRCBLEND, &curSrcBlend );
			g_pDevice->GetRenderState( D3DRS_DESTBLEND, &curDestBlend );

			g_pDevice->SetRenderState( D3DRS_BLENDOP, D3DBLENDOP_ADD );
			g_pDevice->SetRenderState( D3DRS_SRCBLEND, D3DBLEND_ONE );
			g_pDevice->SetRenderState( D3DRS_DESTBLEND, D3DBLEND_ZERO );

			for (auto& layer : g_layers) {
				g_pDevice->SetTexture(0, layer.m_pTexture);
				g_pDevice->DrawIndexedPrimitive(D3DPT_TRIANGLEFAN, 0, 0, 4, 0, 2);
			}

			g_pDevice->SetRenderState( D3DRS_BLENDOP, curBlendOp );
			g_pDevice->SetRenderState( D3DRS_SRCBLEND, curSrcBlend );
			g_pDevice->SetRenderState( D3DRS_DESTBLEND, curDestBlend );

			// End the scene
			g_pDevice->EndScene();

			// restore previos setting
			g_pDevice->SetFVF( currentFVF );
		}

		{
			LPDIRECT3DSURFACE9 pScreenSurface{};
			g_pScreenTexture->GetSurfaceLevel( 0, &pScreenSurface );

			auto clearColor = g_pFreemformLight->GetAmbientColor();

			LPDIRECT3DSURFACE9 curRT = {};
			g_pDevice->GetRenderTarget( 0, &curRT );
			g_pDevice->SetRenderTarget( 0, pScreenSurface );
			g_pDevice->Clear( 0, 0, D3DCLEAR_TARGET, clearColor, 1.0f, 0 );

			g_pFreemformLight->Draw( g_pDevice, x + gTranslation.x, y + gTranslation.y );

			SAFE_RELEASE( pScreenSurface );
			SAFE_RELEASE( curRT );
		}
	}

	g_pDevice->SetRenderTarget( 0, pCurrentSurface );
	SAFE_RELEASE( pCurrentSurface );

	g_pDevice->Clear( 0, NULL, D3DCLEAR_TARGET, D3DCOLOR_ARGB( 0, 0, 0, 0 ), 0.0f, 0 );

	// Overwrite to current render target
	if ( SUCCEEDED( g_pDevice->BeginScene() ) )
	{
		DWORD curBlendOp = {};
		DWORD curSrcBlend = {};
		DWORD curDestBlend = {};
		g_pDevice->GetRenderState( D3DRS_BLENDOP, &curBlendOp );
		g_pDevice->GetRenderState( D3DRS_SRCBLEND, &curSrcBlend );
		g_pDevice->GetRenderState( D3DRS_DESTBLEND, &curDestBlend );

		DWORD currentFVF = {};
		g_pDevice->GetFVF( &currentFVF );

		{
			g_pDevice->SetRenderState( D3DRS_BLENDOP, D3DBLENDOP_ADD );
			g_pDevice->SetRenderState( D3DRS_SRCBLEND, D3DBLEND_ONE );
			g_pDevice->SetRenderState( D3DRS_DESTBLEND, D3DBLEND_ZERO );

			g_pDevice->SetTexture( 0, g_pMainScreenTexture );
			g_pDevice->SetStreamSource( 0, g_pVB, 0, sizeof( CUSTOM_VERTEX ) );
			g_pDevice->SetIndices( g_pIB );
			g_pDevice->SetFVF( D3DFVF_CUSTOM );
			g_pDevice->DrawIndexedPrimitive( D3DPT_TRIANGLEFAN, 0, 0, 4, 0, 2 );
		}

		{
			D3DXMATRIX curWm = {};
			g_pDevice->GetTransform( D3DTS_WORLD, &curWm );

			D3DXMATRIX tm{};
			D3DXMatrixTranslation( &tm, x, y, 0 );
			g_pDevice->SetTransform( D3DTS_WORLD, &tm );

			if ( g_pFreemformLight->IsMaskInvisible() ) {
				g_pDevice->SetRenderState( D3DRS_BLENDOP, D3DBLENDOP_ADD );
				g_pDevice->SetRenderState( D3DRS_DESTBLEND, D3DBLEND_ZERO );
				g_pDevice->SetRenderState( D3DRS_SRCBLEND, D3DBLEND_DESTCOLOR );
			}

			g_pDevice->SetTexture( 0, g_pScreenTexture );
			g_pScreenMesh->DrawSubset( 0 );

			g_pDevice->SetTransform( D3DTS_WORLD, &curWm );
		}

		g_pDevice->EndScene();
		g_pDevice->SetFVF( currentFVF );
		g_pDevice->SetRenderState( D3DRS_BLENDOP, curBlendOp );
		g_pDevice->SetRenderState( D3DRS_SRCBLEND, curSrcBlend );
		g_pDevice->SetRenderState( D3DRS_DESTBLEND, curDestBlend );
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


void FilterCallback(LPDIRECT3DTEXTURE9 pFilteredTexture)
{
	assert(!g_layers.empty());
	auto layer_iter = std::begin(g_layers);
	auto& layer = *layer_iter;

	SAFE_RELEASE(layer.m_pTexture);
	layer.m_pTexture = pFilteredTexture;
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
        TEXT( "D3D Tutorial" ), NULL
    };
    RegisterClassEx( &wc );

    // Create the application's window
    HWND hWnd = CreateWindow( TEXT( "D3D Tutorial" ), TEXT( "Waifu2x on async" ),
                              WS_OVERLAPPEDWINDOW, 100, 100, gDisplayMode.Width, gDisplayMode.Height,
                              NULL, NULL, wc.hInstance, NULL );

    // Initialize Direct3D
    if( SUCCEEDED( InitD3D( hWnd ) ) )
    {
		LPDIRECT3DTEXTURE9 pTexture{};
		D3DXIMAGE_INFO info{};

		if (FAILED(D3DXCreateTextureFromFileEx(g_pDevice, TEXT("resource\\miku32.dds"), D3DX_DEFAULT_NONPOW2, D3DX_DEFAULT_NONPOW2, 1, 0, D3DFMT_FROM_FILE, D3DPOOL_MANAGED, D3DX_FILTER_NONE, D3DX_FILTER_NONE, NULL, &info, NULL, &pTexture))) {
			return E_FAIL;
		}

		D3DSURFACE_DESC surfaceDesc{};
		
		g_layers.emplace_back(pTexture);

		{
			pTexture->GetLevelDesc(0, &surfaceDesc);
			gDisplayMode.Width = surfaceDesc.Width;
			gDisplayMode.Height = surfaceDesc.Height;

			RECT windowRect{};
			GetWindowRect(hWnd, &windowRect);
			windowRect.right = windowRect.left + surfaceDesc.Width;
			windowRect.bottom = windowRect.top + surfaceDesc.Height;
			AdjustWindowRect(&windowRect, WS_OVERLAPPEDWINDOW, NULL);
		}

		std::shared_ptr<Flat::IToken> token_ptr;
		auto imageFilter = Flat::ImageFilterFactory::createInstance();

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
			ImGui_ImplDX9_Init( g_pDevice );

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
			ExampleAppLog logWindow;

			auto write_log = [&logWindow](std::string const& log) {
				auto now = std::chrono::system_clock::now();
				auto time = std::chrono::system_clock::to_time_t(now);
				tm local_time{};
				localtime_s(&local_time, &time);

				logWindow.AddLog("[%02d:%02d:%02d] %s\n", local_time.tm_hour, local_time.tm_min, local_time.tm_sec, log.c_str());
			};
			imageFilter->bind_log_callback(write_log);

			if (!token_ptr) {
				auto layer_iter = std::begin(g_layers);
				auto& layer = *layer_iter;

				auto callback = std::bind(FilterCallback, std::placeholders::_1);
				token_ptr = imageFilter->filter_async(layer.m_pTexture, 3, 2.f, callback);
			}

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

				//ImGui::ShowDemoWindow();

				ImGui::SetNextWindowPos( { gDisplayMode.Width / 2.f, gDisplayMode.Height / 2.f }, ImGuiCond_Once );

				// Create ImGui widget
				auto xCenter = gDisplayMode.Width / 2;
				auto yCenter = gDisplayMode.Height / 2;
				g_pFreemformLight->DrawImgui( g_pDevice, xCenter, yCenter, true, &showWindow );

				logWindow.Draw("Waifu2x Log");

				// Rendering
				ImGui::EndFrame();

				Render();

				imageFilter->update(g_pDevice);

				if ( SUCCEEDED( g_pDevice->BeginScene() ) ) {
					ImGui::Render();
					ImGui_ImplDX9_RenderDrawData( ImGui::GetDrawData() );

					g_pDevice->EndScene();
				}

				// Present the backbuffer contents to the display
				g_pDevice->Present( NULL, NULL, NULL, NULL );
            }
        }

		token_ptr.reset();
		imageFilter.reset();
    }

	ImGui_ImplDX9_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();

	SAFE_RELEASE( g_pDevice );
	SAFE_RELEASE( g_pD3D );

    UnregisterClass( TEXT( "D3D Tutorial" ), wc.hInstance );
    return 0;
}