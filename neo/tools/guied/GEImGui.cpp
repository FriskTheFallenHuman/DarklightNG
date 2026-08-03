#include "../../idlib/precompiled.h"
#pragma hdrstop

#include "../../renderer/tr_local.h"
#include "../../sys/sys_platform.h"
#include "../../sys/rc/guied_resource.h"

#include "GEApp.h"
#include "GEImGui.h"
#include "../radiant/RadiantImGui.h"

#include "imgui.h"
#include "backends/imgui_impl_opengl2.h"
#include "backends/imgui_impl_win32.h"

#include <GL/gl.h>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler( HWND window, UINT message, WPARAM wParam, LPARAM lParam );

namespace {

static const char *GUI_EDITOR_IMGUI_CLASS = "DarklightGUIEditorImGui";

// Dear ImGui stores the current context globally.  Both editor shells live on
// the same thread, so Win32 calls made while rendering one editor can
// synchronously enter the other editor's window procedure.  Never allow that
// nested call to leak its context back to the caller.
class ScopedImGuiContext {
public:
	explicit ScopedImGuiContext( ImGuiContext *context ) : previous( ImGui::GetCurrentContext() ) {
		ImGui::SetCurrentContext( context );
	}

	~ScopedImGuiContext() {
		ImGui::SetCurrentContext( previous );
	}

private:
	ImGuiContext *previous;
};

static void GEParentDirectory( char *path ) {
	int length = (int)strlen( path );
	while ( length > 3 && ( path[length - 1] == '\\' || path[length - 1] == '/' ) ) {
		path[--length] = '\0';
	}
	char *backslash = strrchr( path, '\\' );
	char *slash = strrchr( path, '/' );
	char *separator = backslash > slash ? backslash : slash;
	if ( separator == NULL ) return;
	if ( separator == path + 2 && path[1] == ':' ) separator[1] = '\0';
	else if ( separator > path ) *separator = '\0';
}

static void GEDefaultGuiDirectory( char *directory, int directorySize ) {
	idStr guiDirectory = fileSystem->RelativePathToOSPath( "guis", "fs_basepath" );
	if ( GetFullPathNameA( guiDirectory.c_str(), directorySize, directory, NULL ) == 0 ) {
		idStr::Copynz( directory, guiDirectory.c_str(), directorySize );
	}
}

static void GEListGuiDirectory( const char *directory, idStrList &directories, idStrList &files ) {
	idStr searchPath = directory;
	searchPath.AppendPath( "*" );
	WIN32_FIND_DATAA findData;
	HANDLE find = FindFirstFileA( searchPath.c_str(), &findData );
	if ( find != INVALID_HANDLE_VALUE ) {
		do {
			if ( !idStr::Cmp( findData.cFileName, "." ) || !idStr::Cmp( findData.cFileName, ".." ) ) continue;
			if ( findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY ) directories.Append( findData.cFileName );
			else if ( idStr::CheckExtension( findData.cFileName, ".gui" ) ) files.Append( findData.cFileName );
		} while ( FindNextFileA( find, &findData ) );
		FindClose( find );
	}
	directories.Sort();
	files.Sort();
}

static void ApplyBlackTitleBar( HWND window ) {
	HMODULE dwmLibrary = LoadLibraryA( "dwmapi.dll" );
	if ( dwmLibrary == NULL ) return;
	typedef HRESULT (WINAPI *setWindowAttribute_t)( HWND, DWORD, LPCVOID, DWORD );
	setWindowAttribute_t setWindowAttribute = (setWindowAttribute_t)GetProcAddress( dwmLibrary, "DwmSetWindowAttribute" );
	if ( setWindowAttribute != NULL ) {
		const DWORD nativeRendering = 2; // DWMNCRP_ENABLED
		const BOOL darkMode = TRUE;
		const COLORREF black = RGB( 0, 0, 0 );
		const COLORREF white = RGB( 235, 235, 235 );
		setWindowAttribute( window, 2, &nativeRendering, sizeof( nativeRendering ) );
		setWindowAttribute( window, 20, &darkMode, sizeof( darkMode ) );
		setWindowAttribute( window, 35, &black, sizeof( black ) );
		setWindowAttribute( window, 36, &white, sizeof( white ) );
	}
	FreeLibrary( dwmLibrary );
}

static void InvalidateImageBindingCache() {
	for ( int unit = 0; unit < MAX_MULTITEXTURE_UNITS; unit++ ) {
		backEnd.glState.tmu[unit].current2DMap = -1;
		backEnd.glState.tmu[unit].current3DMap = -1;
		backEnd.glState.tmu[unit].currentCubeMap = -1;
	}
}

static void RenderImGuiDrawData( ImDrawData *drawData ) {
	GLint previousProgram = 0;
	GLint previousArrayBuffer = 0;
	GLint previousElementArrayBuffer = 0;
	GLint previousActiveTexture = GL_TEXTURE0_ARB;
	GLint previousClientActiveTexture = GL_TEXTURE0_ARB;
	const bool hasProgram = qglUseProgram != NULL;
	const bool hasBuffers = qglBindBufferARB != NULL;
	const bool hasMultitexture = qglActiveTextureARB != NULL && qglClientActiveTextureARB != NULL;
	const GLboolean cubeMapEnabled = qglIsEnabled( GL_TEXTURE_CUBE_MAP_EXT );

	if ( hasProgram ) qglGetIntegerv( 0x8B8D /* GL_CURRENT_PROGRAM */, &previousProgram );
	if ( hasBuffers ) {
		qglGetIntegerv( GL_ARRAY_BUFFER_BINDING_ARB, &previousArrayBuffer );
		qglGetIntegerv( GL_ELEMENT_ARRAY_BUFFER_BINDING_ARB, &previousElementArrayBuffer );
	}
	if ( hasMultitexture ) {
		qglGetIntegerv( GL_ACTIVE_TEXTURE_ARB, &previousActiveTexture );
		qglGetIntegerv( GL_CLIENT_ACTIVE_TEXTURE_ARB, &previousClientActiveTexture );
		qglActiveTextureARB( GL_TEXTURE0_ARB );
		qglClientActiveTextureARB( GL_TEXTURE0_ARB );
	}
	if ( hasProgram ) qglUseProgram( 0 );
	qglDisable( GL_TEXTURE_CUBE_MAP_EXT );
	if ( hasBuffers ) {
		qglBindBufferARB( GL_ARRAY_BUFFER_ARB, 0 );
		qglBindBufferARB( GL_ELEMENT_ARRAY_BUFFER_ARB, 0 );
	}
	ImGui_ImplOpenGL2_RenderDrawData( drawData );
	if ( hasBuffers ) {
		qglBindBufferARB( GL_ARRAY_BUFFER_ARB, previousArrayBuffer );
		qglBindBufferARB( GL_ELEMENT_ARRAY_BUFFER_ARB, previousElementArrayBuffer );
	}
	if ( hasProgram ) qglUseProgram( previousProgram );
	if ( cubeMapEnabled ) qglEnable( GL_TEXTURE_CUBE_MAP_EXT );
	if ( hasMultitexture ) {
		qglClientActiveTextureARB( previousClientActiveTexture );
		qglActiveTextureARB( previousActiveTexture );
	}
}

class GECanvasTexture {
public:
	GECanvasTexture() : texture( 0 ), width( 0 ), height( 0 ), textureWidth( 0 ), textureHeight( 0 ) {}

	void Destroy() {
		if ( texture != 0 ) qglDeleteTextures( 1, &texture );
		texture = 0;
		width = height = textureWidth = textureHeight = 0;
	}

	bool Prepare( int requestedWidth, int requestedHeight ) {
		width = Max( 1, requestedWidth );
		height = Max( 1, requestedHeight );
		const int allocationWidth = NextPowerOfTwo( width );
		const int allocationHeight = NextPowerOfTwo( height );
		if ( texture == 0 ) qglGenTextures( 1, &texture );
		if ( texture == 0 ) return false;
		qglBindTexture( GL_TEXTURE_2D, texture );
		if ( allocationWidth != textureWidth || allocationHeight != textureHeight ) {
			textureWidth = allocationWidth;
			textureHeight = allocationHeight;
			qglTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
			qglTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
			qglTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP );
			qglTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP );
			qglTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA, textureWidth, textureHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL );
		}
		return true;
	}

	void CopyFromFrontBuffer() {
		if ( texture == 0 ) return;
		qglReadBuffer( GL_FRONT );
		qglBindTexture( GL_TEXTURE_2D, texture );
		qglCopyTexSubImage2D( GL_TEXTURE_2D, 0, 0, 0, 0, 0, width, height );
		qglReadBuffer( GL_BACK );
	}

	ImTextureRef Ref() const { return ImTextureRef( (ImTextureID)texture ); }
	ImVec2 UV0() const { return ImVec2( 0.0f, textureHeight ? (float)height / textureHeight : 1.0f ); }
	ImVec2 UV1() const { return ImVec2( textureWidth ? (float)width / textureWidth : 1.0f, 0.0f ); }

private:
	static int NextPowerOfTwo( int value ) {
		int result = 1;
		while ( result < value ) result <<= 1;
		return result;
	}

	GLuint texture;
	int width;
	int height;
	int textureWidth;
	int textureHeight;
};

struct GEPropertyEdit {
	char name[128];
	char value[8192];
};

class GEImGuiHost {
public:
	GEImGuiHost();
	~GEImGuiHost();

	bool Create();
	void Destroy();
	void Show();
	void Hide();
	void Frame();
	void ShowProperties();
	void ShowScripts();
	void ShowOptions();
	void ShowViewer();
	void ShowAbout();
	void ExecuteCommand( UINT command ) { Command( command ); }
	bool IsVisible() const { return hwnd != NULL && ::IsWindowVisible( hwnd ) != FALSE; }
	HWND Window() const { return hwnd; }
	LRESULT WindowProc( HWND window, UINT message, WPARAM wParam, LPARAM lParam );

private:
	static LRESULT CALLBACK StaticWindowProc( HWND window, UINT message, WPARAM wParam, LPARAM lParam );
	bool InitializeRenderer();
	void ShutdownRenderer();
	void MakeCurrent();
	void RestoreEngineContext();
	void RenderShell();
	void RenderMenuBar();
	void RenderToolbar();
	void RenderDocuments();
	void RenderNavigator( rvGEWorkspace *workspace );
	void RenderWindowNode( rvGEWorkspace *workspace, idWindow *window );
	void RenderCanvas( rvGEWorkspace *workspace, const ImVec2 &size );
	void RenderCanvasContextMenu( rvGEWorkspace *workspace );
	void RenderInspector( rvGEWorkspace *workspace );
	void RenderProperties( rvGEWorkspace *workspace );
	void RenderScripts( rvGEWorkspace *workspace );
	void RenderTransformer( rvGEWorkspace *workspace );
	void RenderOptions();
	void RenderAbout();
	void RenderViewer();
	void BeginOpenFileDialog();
	void RenderOpenFileDialog();
	void BeginSaveFileDialog( rvGEWorkspace *workspace );
	void RenderSaveFileDialog();
	void SyncProperties( rvGEWorkspace *workspace, bool force = false );
	void SyncScripts( rvGEWorkspace *workspace, bool force = false );
	void Command( UINT command );
	void ProcessPendingFileCommand();
	void UpdateTitle( rvGEWorkspace *workspace );
	WPARAM MouseButtons() const;

	HWND hwnd;
	HDC dc;
	HGLRC glrc;
	ImGuiContext *imguiContext;
	backEndState_t editorBackEndState;
	bool rendererReady;
	bool rendering;
	bool canvasCaptured;
	bool canvasFocused;
	bool showNavigator;
	bool showProperties;
	bool showScripts;
	bool showTransformer;
	bool showStatusBar;
	bool showOptions;
	bool showAbout;
	bool showViewer;
	bool requestOpenFileDialog;
	bool requestSaveFileDialog;
	bool saveOverwritePending;
	int requestedInspector;
	UINT pendingFileCommand;
	float uiScale;
	GECanvasTexture canvasTexture;
	idWindow *propertyWindow;
	idList<GEPropertyEdit> propertyEdits;
	idWindow *scriptWindow;
	idList<GEPropertyEdit> scriptEdits;
	idList<GEPropertyEdit> variableEdits;
	rvGEWorkspace *saveFileWorkspace;
	rvGEWorkspace *pendingSaveWorkspace;
	char newPropertyName[128];
	char newPropertyValue[1024];
	char newScriptName[128];
	char newScriptValue[8192];
	char pendingOpenFile[MAX_PATH];
	char pendingSaveFile[MAX_PATH];
	char openFileDirectory[MAX_PATH];
	char openFilePath[MAX_PATH];
	char openFileError[256];
	char saveFileDirectory[MAX_PATH];
	char saveFilePath[MAX_PATH];
	char saveFileError[256];
};

static GEImGuiHost *guiEditorHost = NULL;

GEImGuiHost::GEImGuiHost() :
	hwnd( NULL ), dc( NULL ), glrc( NULL ), imguiContext( NULL ), rendererReady( false ), rendering( false ),
	canvasCaptured( false ), canvasFocused( false ), showNavigator( true ), showProperties( true ),
	showScripts( true ), showTransformer( true ), showStatusBar( true ), showOptions( false ), showAbout( false ),
	showViewer( false ), requestOpenFileDialog( false ), requestSaveFileDialog( false ),
	saveOverwritePending( false ), requestedInspector( 0 ), pendingFileCommand( 0 ), uiScale( 1.0f ),
	propertyWindow( NULL ), scriptWindow( NULL ), saveFileWorkspace( NULL ), pendingSaveWorkspace( NULL ) {
	newPropertyName[0] = '\0';
	newPropertyValue[0] = '\0';
	newScriptName[0] = '\0';
	newScriptValue[0] = '\0';
	pendingOpenFile[0] = '\0';
	pendingSaveFile[0] = '\0';
	openFileDirectory[0] = '\0';
	openFilePath[0] = '\0';
	openFileError[0] = '\0';
	saveFileDirectory[0] = '\0';
	saveFilePath[0] = '\0';
	saveFileError[0] = '\0';
	ZeroMemory( &editorBackEndState, sizeof( editorBackEndState ) );
	editorBackEndState.glState.faceCulling = -1;
	editorBackEndState.glState.forceGlState = true;
	for ( int unit = 0; unit < MAX_MULTITEXTURE_UNITS; unit++ ) {
		editorBackEndState.glState.tmu[unit].current2DMap = -1;
		editorBackEndState.glState.tmu[unit].current3DMap = -1;
		editorBackEndState.glState.tmu[unit].currentCubeMap = -1;
		editorBackEndState.glState.tmu[unit].texEnv = -1;
	}
}

GEImGuiHost::~GEImGuiHost() {
	Destroy();
}

bool GEImGuiHost::Create() {
	ImGui_ImplWin32_EnableDpiAwareness();
	WNDCLASSEXA windowClass;
	ZeroMemory( &windowClass, sizeof( windowClass ) );
	windowClass.cbSize = sizeof( windowClass );
	windowClass.style = CS_OWNDC | CS_DBLCLKS | CS_HREDRAW | CS_VREDRAW;
	windowClass.lpfnWndProc = StaticWindowProc;
	windowClass.hInstance = win32.hInstance;
	windowClass.hCursor = LoadCursor( NULL, IDC_ARROW );
	windowClass.hIcon = LoadIcon( win32.hInstance, MAKEINTRESOURCE( IDI_GUIED ) );
	windowClass.hbrBackground = NULL;
	windowClass.lpszClassName = GUI_EDITOR_IMGUI_CLASS;
	RegisterClassExA( &windowClass );

	HWND owner = RadiantImGuiWindow();
	if ( owner != NULL ) owner = GetAncestor( owner, GA_ROOT );
	if ( owner == NULL && win32.hWnd != NULL && IsWindowVisible( win32.hWnd ) ) owner = win32.hWnd;
	hwnd = CreateWindowExA( 0, GUI_EDITOR_IMGUI_CLASS, "DOOMEdit - GUI Editor",
		WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN, CW_USEDEFAULT, CW_USEDEFAULT, 1280, 820,
		owner, NULL, win32.hInstance, this );
	if ( hwnd == NULL ) return false;
	ApplyBlackTitleBar( hwnd );
	if ( !InitializeRenderer() ) {
		DestroyWindow( hwnd );
		hwnd = NULL;
		return false;
	}
	gApp.GetOptions().GetWindowPlacement( "imguiframe", hwnd );
	return true;
}

void GEImGuiHost::Destroy() {
	if ( hwnd != NULL ) gApp.GetOptions().SetWindowPlacement( "imguiframe", hwnd );
	ShutdownRenderer();
	if ( hwnd != NULL ) {
		HWND oldWindow = hwnd;
		hwnd = NULL;
		DestroyWindow( oldWindow );
	}
}

void GEImGuiHost::Show() {
	if ( hwnd == NULL ) return;
	ShowWindow( hwnd, IsIconic( hwnd ) ? SW_RESTORE : SW_SHOW );
	ApplyBlackTitleBar( hwnd );
	SetForegroundWindow( hwnd );
	SetFocus( hwnd );
	InvalidateRect( hwnd, NULL, FALSE );
}

void GEImGuiHost::Hide() {
	if ( hwnd == NULL ) return;
	gApp.GetOptions().SetWindowPlacement( "imguiframe", hwnd );
	ShowWindow( hwnd, SW_HIDE );
	canvasCaptured = false;
}

void GEImGuiHost::ShowProperties() {
	showProperties = true;
	requestedInspector = 1;
	Show();
}

void GEImGuiHost::ShowScripts() {
	showScripts = true;
	requestedInspector = 2;
	Show();
}

void GEImGuiHost::ShowOptions() {
	showOptions = true;
	Show();
}

void GEImGuiHost::ShowViewer() {
	showViewer = true;
	Show();
}

void GEImGuiHost::ShowAbout() {
	showAbout = true;
	Show();
}

bool GEImGuiHost::InitializeRenderer() {
	dc = GetDC( hwnd );
	if ( dc == NULL ) return false;
	int pixelFormat = ChoosePixelFormat( dc, &win32.pfd );
	if ( pixelFormat <= 0 || !SetPixelFormat( dc, pixelFormat, &win32.pfd ) ) return false;
	glrc = wglCreateContext( dc );
	if ( glrc == NULL ) return false;
	if ( win32.hGLRC != NULL && !wglShareLists( win32.hGLRC, glrc ) ) return false;
	if ( !wglMakeCurrent( dc, glrc ) ) return false;

	IMGUI_CHECKVERSION();
	imguiContext = ImGui::CreateContext();
	ScopedImGuiContext scopedContext( imguiContext );
	ImGuiIO &io = ImGui::GetIO();
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
	io.IniFilename = "guied_imgui.ini";
	uiScale = Max( 1.0f, ImGui_ImplWin32_GetDpiScaleForHwnd( hwnd ) );
	ImFontConfig fontConfig;
	fontConfig.SizePixels = 13.0f * uiScale;
	io.Fonts->AddFontDefault( &fontConfig );
	ImGui::StyleColorsDark();
	ImGuiStyle &style = ImGui::GetStyle();
	ImVec4 *colors = style.Colors;
	colors[ImGuiCol_Text] = ImVec4( 0.88f, 0.89f, 0.90f, 1.0f );
	colors[ImGuiCol_WindowBg] = ImVec4( 0.115f, 0.120f, 0.125f, 1.0f );
	colors[ImGuiCol_ChildBg] = ImVec4( 0.105f, 0.110f, 0.115f, 1.0f );
	colors[ImGuiCol_PopupBg] = ImVec4( 0.135f, 0.140f, 0.145f, 0.98f );
	colors[ImGuiCol_MenuBarBg] = ImVec4( 0.145f, 0.150f, 0.155f, 1.0f );
	colors[ImGuiCol_FrameBg] = ImVec4( 0.18f, 0.19f, 0.20f, 1.0f );
	colors[ImGuiCol_FrameBgHovered] = ImVec4( 0.25f, 0.26f, 0.27f, 1.0f );
	colors[ImGuiCol_FrameBgActive] = ImVec4( 0.31f, 0.32f, 0.33f, 1.0f );
	colors[ImGuiCol_Button] = ImVec4( 0.20f, 0.21f, 0.22f, 1.0f );
	colors[ImGuiCol_ButtonHovered] = ImVec4( 0.29f, 0.30f, 0.31f, 1.0f );
	colors[ImGuiCol_ButtonActive] = ImVec4( 0.37f, 0.38f, 0.39f, 1.0f );
	colors[ImGuiCol_Header] = ImVec4( 0.23f, 0.24f, 0.25f, 1.0f );
	colors[ImGuiCol_HeaderHovered] = ImVec4( 0.31f, 0.32f, 0.33f, 1.0f );
	colors[ImGuiCol_HeaderActive] = ImVec4( 0.39f, 0.40f, 0.41f, 1.0f );
	colors[ImGuiCol_Tab] = ImVec4( 0.17f, 0.18f, 0.19f, 1.0f );
	colors[ImGuiCol_TabHovered] = ImVec4( 0.30f, 0.31f, 0.32f, 1.0f );
	colors[ImGuiCol_TabSelected] = ImVec4( 0.27f, 0.28f, 0.29f, 1.0f );
	colors[ImGuiCol_Border] = ImVec4( 0.30f, 0.31f, 0.32f, 0.75f );
	style.WindowRounding = 3.0f;
	style.ChildRounding = 3.0f;
	style.FrameRounding = 3.0f;
	style.PopupRounding = 3.0f;
	style.ScaleAllSizes( uiScale );
	if ( !ImGui_ImplWin32_InitForOpenGL( hwnd ) || !ImGui_ImplOpenGL2_Init() ) {
		ShutdownRenderer();
		return false;
	}
	rendererReady = true;
	RestoreEngineContext();
	return true;
}

void GEImGuiHost::ShutdownRenderer() {
	if ( rendererReady || imguiContext != NULL ) {
		ImGuiContext *previousContext = ImGui::GetCurrentContext();
		ImGuiContext *destroyedContext = imguiContext;
		MakeCurrent();
		canvasTexture.Destroy();
		ImGui::SetCurrentContext( imguiContext );
		if ( rendererReady ) {
			ImGui_ImplOpenGL2_Shutdown();
			ImGui_ImplWin32_Shutdown();
		}
		if ( imguiContext != NULL ) ImGui::DestroyContext( imguiContext );
		imguiContext = NULL;
		rendererReady = false;
		ImGui::SetCurrentContext( previousContext == destroyedContext ? NULL : previousContext );
	}
	if ( glrc != NULL ) {
		wglMakeCurrent( NULL, NULL );
		wglDeleteContext( glrc );
		glrc = NULL;
	}
	if ( hwnd != NULL && dc != NULL ) ReleaseDC( hwnd, dc );
	dc = NULL;
	RestoreEngineContext();
}

void GEImGuiHost::MakeCurrent() {
	if ( dc != NULL && glrc != NULL ) {
		wglMakeCurrent( dc, glrc );
	}
}

void GEImGuiHost::RestoreEngineContext() {
	if ( win32.hDC != NULL && win32.hGLRC != NULL ) wglMakeCurrent( win32.hDC, win32.hGLRC );
}

void GEImGuiHost::Command( UINT command ) {
	// Keep visible file interaction in the ImGui shell.  The old common dialogs
	// run their own modal Win32 loop, which fights the shared Radiant/GUI-editor
	// pump and repeatedly flashes instead of remaining usable.
	rvGEWorkspace *workspace = gApp.GetActiveWorkspace();
	switch ( command ) {
		case ID_GUIED_FILE_OPEN:
			BeginOpenFileDialog();
			return;
		case ID_GUIED_FILE_SAVE:
			if ( workspace == NULL ) return;
			if ( workspace->IsNew() ) {
				BeginSaveFileDialog( workspace );
			} else {
				pendingSaveWorkspace = workspace;
				idStr::Copynz( pendingSaveFile, workspace->GetFilename(), sizeof( pendingSaveFile ) );
			}
			return;
		case ID_GUIED_FILE_SAVEAS:
			if ( workspace != NULL ) BeginSaveFileDialog( workspace );
			return;
		case ID_GUIED_FILE_NEW:
		case ID_GUIED_FILE_CLOSE:
			pendingFileCommand = command;
			propertyWindow = NULL;
			return;
	}
	gApp.ExecuteCommand( command );
	propertyWindow = NULL;
}

void GEImGuiHost::ProcessPendingFileCommand() {
	if ( pendingOpenFile[0] != '\0' ) {
		char filename[MAX_PATH];
		idStr::Copynz( filename, pendingOpenFile, sizeof( filename ) );
		pendingOpenFile[0] = '\0';
		gApp.OpenFile( filename );
	}
	if ( pendingSaveWorkspace != NULL && pendingSaveFile[0] != '\0' ) {
		rvGEWorkspace *workspace = pendingSaveWorkspace;
		char filename[MAX_PATH];
		idStr::Copynz( filename, pendingSaveFile, sizeof( filename ) );
		pendingSaveWorkspace = NULL;
		pendingSaveFile[0] = '\0';
		bool workspaceStillOpen = false;
		for ( int index = 0; index < gApp.GetWorkspaceCount(); index++ ) {
			if ( gApp.GetWorkspace( index ) == workspace ) {
				workspaceStillOpen = true;
				break;
			}
		}
		if ( workspaceStillOpen ) {
			if ( workspace->SaveFile( filename ) ) {
				gApp.GetOptions().AddRecentFile( workspace->GetFilename() );
			} else {
				common->Warning( "GUI editor could not write '%s'.\n", filename );
			}
		}
	}
	if ( pendingFileCommand != 0 ) {
		const UINT command = pendingFileCommand;
		pendingFileCommand = 0;
		gApp.ExecuteCommand( command );
	}
}

void GEImGuiHost::BeginOpenFileDialog() {
	GEDefaultGuiDirectory( openFileDirectory, sizeof( openFileDirectory ) );
	openFilePath[0] = '\0';
	openFileError[0] = '\0';
	requestOpenFileDialog = true;
}

void GEImGuiHost::RenderOpenFileDialog() {
	if ( requestOpenFileDialog ) {
		ImGui::OpenPopup( "Open GUI" );
		requestOpenFileDialog = false;
	}
	ImGui::SetNextWindowSize( ImVec2( 680.0f * uiScale, 520.0f * uiScale ), ImGuiCond_Appearing );
	if ( !ImGui::BeginPopupModal( "Open GUI", NULL, ImGuiWindowFlags_NoSavedSettings ) ) return;

	if ( ImGui::Button( "Up" ) ) {
		GEParentDirectory( openFileDirectory );
		openFilePath[0] = '\0';
		openFileError[0] = '\0';
	}
	ImGui::SameLine();
	ImGui::SetNextItemWidth( -1.0f );
	ImGui::InputText( "##OpenGUIDirectory", openFileDirectory, sizeof( openFileDirectory ),
		ImGuiInputTextFlags_ReadOnly );

	idStrList directories;
	idStrList files;
	GEListGuiDirectory( openFileDirectory, directories, files );
	bool openSelected = false;
	ImGui::BeginChild( "OpenGUIFiles", ImVec2( 0, 350.0f * uiScale ), ImGuiChildFlags_Borders );
	for ( int index = 0; index < directories.Num(); index++ ) {
		ImGui::PushID( index );
		idStr label = "[";
		label += directories[index];
		label += "]";
		if ( ImGui::Selectable( label.c_str(), false, ImGuiSelectableFlags_AllowDoubleClick ) &&
			ImGui::IsMouseDoubleClicked( ImGuiMouseButton_Left ) ) {
			idStr next = openFileDirectory;
			next.AppendPath( directories[index] );
			idStr::Copynz( openFileDirectory, next.c_str(), sizeof( openFileDirectory ) );
			openFilePath[0] = '\0';
			openFileError[0] = '\0';
			ImGui::PopID();
			break;
		}
		ImGui::PopID();
	}
	for ( int index = 0; index < files.Num(); index++ ) {
		idStr fullPath = openFileDirectory;
		fullPath.AppendPath( files[index] );
		const bool selected = !idStr::Icmp( openFilePath, fullPath.c_str() );
		if ( ImGui::Selectable( files[index].c_str(), selected, ImGuiSelectableFlags_AllowDoubleClick ) ) {
			idStr::Copynz( openFilePath, fullPath.c_str(), sizeof( openFilePath ) );
			openFileError[0] = '\0';
			openSelected = ImGui::IsMouseDoubleClicked( ImGuiMouseButton_Left );
		}
	}
	ImGui::EndChild();

	ImGui::SetNextItemWidth( -1.0f );
	if ( ImGui::InputTextWithHint( "##OpenGUIPath", "Select a .gui file", openFilePath,
		sizeof( openFilePath ), ImGuiInputTextFlags_EnterReturnsTrue ) ) openSelected = true;
	if ( openFileError[0] != '\0' ) {
		ImGui::TextColored( ImVec4( 1.0f, 0.35f, 0.3f, 1.0f ), "%s", openFileError );
	}
	if ( ImGui::Button( "Open" ) ) openSelected = true;
	ImGui::SameLine();
	if ( ImGui::Button( "Cancel" ) ) ImGui::CloseCurrentPopup();

	if ( openSelected ) {
		idStr target = openFilePath;
		const bool absolute = ( target.Length() > 2 && target[1] == ':' ) ||
			( !target.IsEmpty() && ( target[0] == '/' || target[0] == '\\' ) );
		if ( !absolute && !target.IsEmpty() ) {
			idStr relative = target;
			target = openFileDirectory;
			target.AppendPath( relative );
		}
		char fullTarget[MAX_PATH];
		if ( GetFullPathNameA( target.c_str(), sizeof( fullTarget ), fullTarget, NULL ) == 0 ) {
			idStr::Copynz( fullTarget, target.c_str(), sizeof( fullTarget ) );
		}
		DWORD attributes = GetFileAttributesA( fullTarget );
		if ( target.IsEmpty() || attributes == INVALID_FILE_ATTRIBUTES ||
			( attributes & FILE_ATTRIBUTE_DIRECTORY ) || !idStr::CheckExtension( fullTarget, ".gui" ) ) {
			idStr::Copynz( openFileError, "Choose an existing .gui file.", sizeof( openFileError ) );
		} else {
			idStr::Copynz( pendingOpenFile, fullTarget, sizeof( pendingOpenFile ) );
			ImGui::CloseCurrentPopup();
		}
	}
	ImGui::EndPopup();
}

void GEImGuiHost::BeginSaveFileDialog( rvGEWorkspace *workspace ) {
	saveFileWorkspace = workspace;
	GEDefaultGuiDirectory( saveFileDirectory, sizeof( saveFileDirectory ) );
	saveFilePath[0] = '\0';
	if ( workspace != NULL ) {
		idStr current = workspace->GetFilename();
		idStr directory;
		current.ExtractFilePath( directory );
		DWORD attributes = GetFileAttributesA( directory.c_str() );
		if ( !directory.IsEmpty() && attributes != INVALID_FILE_ATTRIBUTES &&
			( attributes & FILE_ATTRIBUTE_DIRECTORY ) ) {
			if ( GetFullPathNameA( directory.c_str(), sizeof( saveFileDirectory ), saveFileDirectory, NULL ) == 0 ) {
				idStr::Copynz( saveFileDirectory, directory.c_str(), sizeof( saveFileDirectory ) );
			}
		}
		current.StripPath();
		if ( !current.IsEmpty() ) idStr::Copynz( saveFilePath, current.c_str(), sizeof( saveFilePath ) );
	}
	saveFileError[0] = '\0';
	saveOverwritePending = false;
	requestSaveFileDialog = true;
}

void GEImGuiHost::RenderSaveFileDialog() {
	if ( requestSaveFileDialog ) {
		ImGui::OpenPopup( "Save GUI As" );
		requestSaveFileDialog = false;
	}
	ImGui::SetNextWindowSize( ImVec2( 680.0f * uiScale, 520.0f * uiScale ), ImGuiCond_Appearing );
	if ( !ImGui::BeginPopupModal( "Save GUI As", NULL, ImGuiWindowFlags_NoSavedSettings ) ) return;

	if ( ImGui::Button( "Up" ) ) {
		GEParentDirectory( saveFileDirectory );
		saveFilePath[0] = '\0';
		saveFileError[0] = '\0';
		saveOverwritePending = false;
	}
	ImGui::SameLine();
	ImGui::SetNextItemWidth( -1.0f );
	ImGui::InputText( "##SaveGUIDirectory", saveFileDirectory, sizeof( saveFileDirectory ),
		ImGuiInputTextFlags_ReadOnly );

	idStrList directories;
	idStrList files;
	GEListGuiDirectory( saveFileDirectory, directories, files );
	bool saveSelected = false;
	ImGui::BeginChild( "SaveGUIFiles", ImVec2( 0, 350.0f * uiScale ), ImGuiChildFlags_Borders );
	for ( int index = 0; index < directories.Num(); index++ ) {
		ImGui::PushID( index );
		idStr label = "[";
		label += directories[index];
		label += "]";
		if ( ImGui::Selectable( label.c_str(), false, ImGuiSelectableFlags_AllowDoubleClick ) &&
			ImGui::IsMouseDoubleClicked( ImGuiMouseButton_Left ) ) {
			idStr next = saveFileDirectory;
			next.AppendPath( directories[index] );
			idStr::Copynz( saveFileDirectory, next.c_str(), sizeof( saveFileDirectory ) );
			saveFilePath[0] = '\0';
			saveFileError[0] = '\0';
			saveOverwritePending = false;
			ImGui::PopID();
			break;
		}
		ImGui::PopID();
	}
	for ( int index = 0; index < files.Num(); index++ ) {
		idStr fullPath = saveFileDirectory;
		fullPath.AppendPath( files[index] );
		const bool selected = !idStr::Icmp( saveFilePath, fullPath.c_str() );
		if ( ImGui::Selectable( files[index].c_str(), selected, ImGuiSelectableFlags_AllowDoubleClick ) ) {
			idStr::Copynz( saveFilePath, fullPath.c_str(), sizeof( saveFilePath ) );
			saveFileError[0] = '\0';
			saveOverwritePending = false;
			saveSelected = ImGui::IsMouseDoubleClicked( ImGuiMouseButton_Left );
		}
	}
	ImGui::EndChild();

	ImGui::SetNextItemWidth( -1.0f );
	if ( ImGui::InputTextWithHint( "##SaveGUIPath", "GUI filename", saveFilePath,
		sizeof( saveFilePath ), ImGuiInputTextFlags_EnterReturnsTrue ) ) {
		saveOverwritePending = false;
		saveFileError[0] = '\0';
		saveSelected = true;
	}
	if ( saveFileError[0] != '\0' ) {
		ImGui::TextColored( ImVec4( 1.0f, 0.35f, 0.3f, 1.0f ), "%s", saveFileError );
	}
	if ( ImGui::Button( saveOverwritePending ? "Overwrite" : "Save" ) ) saveSelected = true;
	ImGui::SameLine();
	if ( ImGui::Button( "Cancel" ) ) {
		saveFileWorkspace = NULL;
		ImGui::CloseCurrentPopup();
	}

	if ( saveSelected ) {
		idStr target = saveFilePath;
		if ( target.IsEmpty() ) {
			idStr::Copynz( saveFileError, "Enter a GUI filename.", sizeof( saveFileError ) );
			saveOverwritePending = false;
			ImGui::EndPopup();
			return;
		}
		const bool absolute = ( target.Length() > 2 && target[1] == ':' ) || target[0] == '/' || target[0] == '\\';
		if ( !absolute ) {
			idStr relative = target;
			target = saveFileDirectory;
			target.AppendPath( relative );
		}
		if ( !idStr::CheckExtension( target.c_str(), ".gui" ) ) target.SetFileExtension( "gui" );
		char fullTarget[MAX_PATH];
		if ( GetFullPathNameA( target.c_str(), sizeof( fullTarget ), fullTarget, NULL ) == 0 ) {
			idStr::Copynz( fullTarget, target.c_str(), sizeof( fullTarget ) );
		}
		idStr fullTargetString = fullTarget;
		idStr parent;
		fullTargetString.ExtractFilePath( parent );
		DWORD parentAttributes = GetFileAttributesA( parent.c_str() );
		DWORD targetAttributes = GetFileAttributesA( fullTarget );
		if ( parent.IsEmpty() || parentAttributes == INVALID_FILE_ATTRIBUTES ||
			!( parentAttributes & FILE_ATTRIBUTE_DIRECTORY ) ) {
			idStr::Copynz( saveFileError, "The destination folder does not exist.", sizeof( saveFileError ) );
			saveOverwritePending = false;
		} else if ( targetAttributes != INVALID_FILE_ATTRIBUTES && ( targetAttributes & FILE_ATTRIBUTE_DIRECTORY ) ) {
			idStr::Copynz( saveFileError, "Choose a filename, not a folder.", sizeof( saveFileError ) );
			saveOverwritePending = false;
		} else if ( targetAttributes != INVALID_FILE_ATTRIBUTES && !saveOverwritePending ) {
			idStr::Copynz( saveFilePath, fullTarget, sizeof( saveFilePath ) );
			idStr::Copynz( saveFileError, "That file exists. Click Overwrite to replace it.", sizeof( saveFileError ) );
			saveOverwritePending = true;
		} else if ( saveFileWorkspace != NULL ) {
			pendingSaveWorkspace = saveFileWorkspace;
			idStr::Copynz( pendingSaveFile, fullTarget, sizeof( pendingSaveFile ) );
			saveFileWorkspace = NULL;
			saveOverwritePending = false;
			ImGui::CloseCurrentPopup();
		}
	}
	ImGui::EndPopup();
}

void GEImGuiHost::RenderMenuBar() {
	rvGEWorkspace *workspace = gApp.GetActiveWorkspace();
	const bool hasWorkspace = workspace != NULL;
	const int selectionCount = hasWorkspace ? workspace->GetSelectionMgr().Num() : 0;
	if ( ImGui::BeginMenu( "File" ) ) {
		if ( ImGui::MenuItem( "New", "Ctrl+N" ) ) Command( ID_GUIED_FILE_NEW );
		if ( ImGui::MenuItem( "Open...", "Ctrl+O" ) ) Command( ID_GUIED_FILE_OPEN );
		if ( ImGui::MenuItem( "Close", NULL, false, hasWorkspace ) ) Command( ID_GUIED_FILE_CLOSE );
		ImGui::Separator();
		if ( ImGui::MenuItem( "Save", "Ctrl+S", false, hasWorkspace ) ) Command( ID_GUIED_FILE_SAVE );
		if ( ImGui::MenuItem( "Save As...", NULL, false, hasWorkspace ) ) Command( ID_GUIED_FILE_SAVEAS );
		if ( gApp.GetOptions().GetRecentFileCount() > 0 ) {
			ImGui::Separator();
			if ( ImGui::BeginMenu( "Recent Files" ) ) {
				for ( int index = gApp.GetOptions().GetRecentFileCount() - 1; index >= 0; index-- ) {
					const char *filename = gApp.GetOptions().GetRecentFile( index );
					if ( ImGui::MenuItem( filename ) ) {
						idStr::Copynz( pendingOpenFile, filename, sizeof( pendingOpenFile ) );
					}
				}
				ImGui::EndMenu();
			}
		}
		ImGui::Separator();
		if ( ImGui::MenuItem( "Close GUI Editor" ) ) GUIEditorHide();
		ImGui::EndMenu();
	}
	if ( ImGui::BeginMenu( "Edit" ) ) {
		const bool canUndo = hasWorkspace && workspace->GetModifierStack().CanUndo();
		const bool canRedo = hasWorkspace && workspace->GetModifierStack().CanRedo();
		if ( ImGui::MenuItem( "Undo", "Ctrl+Z", false, canUndo ) ) Command( ID_GUIED_EDIT_UNDO );
		if ( ImGui::MenuItem( "Redo", "Ctrl+Y", false, canRedo ) ) Command( ID_GUIED_EDIT_REDO );
		ImGui::Separator();
		if ( ImGui::MenuItem( "Copy", "Ctrl+C", false, selectionCount > 0 ) ) Command( ID_GUIED_EDIT_COPY );
		if ( ImGui::MenuItem( "Paste", "Ctrl+V", false, hasWorkspace && workspace->GetClipboard().Num() > 0 ) ) Command( ID_GUIED_EDIT_PASTE );
		if ( ImGui::MenuItem( "Delete", "Del", false, selectionCount > 0 ) ) Command( ID_GUIED_EDIT_DELETE );
		ImGui::EndMenu();
	}
	if ( ImGui::BeginMenu( "View" ) ) {
		if ( ImGui::MenuItem( "Zoom In", "Ctrl++", false, hasWorkspace ) ) Command( ID_GUIED_VIEW_ZOOMIN );
		if ( ImGui::MenuItem( "Zoom Out", "Ctrl+-", false, hasWorkspace ) ) Command( ID_GUIED_VIEW_ZOOMOUT );
		ImGui::Separator();
		bool grid = gApp.GetOptions().GetGridVisible();
		if ( ImGui::MenuItem( "Show Grid", NULL, grid, hasWorkspace ) ) Command( ID_GUIED_VIEW_SHOWGRID );
		bool snap = gApp.GetOptions().GetGridSnap();
		if ( ImGui::MenuItem( "Snap To Grid", NULL, snap, hasWorkspace ) ) Command( ID_GUIED_VIEW_SNAPTOGRID );
		ImGui::Separator();
		ImGui::MenuItem( "Navigator", NULL, &showNavigator );
		ImGui::MenuItem( "Properties", NULL, &showProperties );
		ImGui::MenuItem( "Scripts", NULL, &showScripts );
		ImGui::MenuItem( "Transformer", NULL, &showTransformer );
		ImGui::MenuItem( "Status Bar", NULL, &showStatusBar );
		ImGui::Separator();
		if ( ImGui::MenuItem( "Options..." ) ) showOptions = true;
		ImGui::EndMenu();
	}
	if ( ImGui::BeginMenu( "Item" ) ) {
		if ( ImGui::BeginMenu( "New", hasWorkspace ) ) {
			if ( ImGui::MenuItem( "windowDef" ) ) Command( ID_GUIED_ITEM_NEWWINDOWDEF );
			if ( ImGui::MenuItem( "editDef" ) ) Command( ID_GUIED_ITEM_NEWEDITDEF );
			if ( ImGui::MenuItem( "htmlDef" ) ) Command( ID_GUIED_ITEM_NEWHTMLDEF );
			if ( ImGui::MenuItem( "choiceDef" ) ) Command( ID_GUIED_ITEM_NEWCHOICEDEF );
			if ( ImGui::MenuItem( "sliderDef" ) ) Command( ID_GUIED_ITEM_NEWSLIDERDEF );
			if ( ImGui::MenuItem( "bindDef" ) ) Command( ID_GUIED_ITEM_NEWBINDDEF );
			if ( ImGui::MenuItem( "listDef" ) ) Command( ID_GUIED_ITEM_NEWLISTDEF );
			if ( ImGui::MenuItem( "renderDef" ) ) Command( ID_GUIED_ITEM_NEWRENDERDEF );
			ImGui::EndMenu();
		}
		if ( ImGui::BeginMenu( "Arrange", selectionCount == 1 ) ) {
			if ( ImGui::MenuItem( "Bring to Front" ) ) Command( ID_GUIED_ITEM_ARRANGEBRINGTOFRONT );
			if ( ImGui::MenuItem( "Bring Forward" ) ) Command( ID_GUIED_ITEM_ARRANGEBRINGFORWARD );
			if ( ImGui::MenuItem( "Send Backward" ) ) Command( ID_GUIED_ITEM_ARRANGESENDBACKWARD );
			if ( ImGui::MenuItem( "Send to Back" ) ) Command( ID_GUIED_ITEM_ARRANGESENDTOBACK );
			ImGui::EndMenu();
		}
		if ( ImGui::BeginMenu( "Align", selectionCount > 1 ) ) {
			if ( ImGui::MenuItem( "Lefts" ) ) Command( ID_GUIED_ITEM_ALIGNLEFTS );
			if ( ImGui::MenuItem( "Centers" ) ) Command( ID_GUIED_ITEM_ALIGNCENTERS );
			if ( ImGui::MenuItem( "Rights" ) ) Command( ID_GUIED_ITEM_ALIGNRIGHTS );
			if ( ImGui::MenuItem( "Tops" ) ) Command( ID_GUIED_ITEM_ALIGNTOPS );
			if ( ImGui::MenuItem( "Middles" ) ) Command( ID_GUIED_ITEM_ALIGNMIDDLES );
			if ( ImGui::MenuItem( "Bottoms" ) ) Command( ID_GUIED_ITEM_ALIGNBOTTOMS );
			ImGui::EndMenu();
		}
		if ( ImGui::BeginMenu( "Make Same Size", selectionCount > 1 ) ) {
			if ( ImGui::MenuItem( "Width" ) ) Command( ID_GUIED_ITEM_MAKESAMESIZEWIDTH );
			if ( ImGui::MenuItem( "Height" ) ) Command( ID_GUIED_ITEM_MAKESAMESIZEHEIGHT );
			if ( ImGui::MenuItem( "Both" ) ) Command( ID_GUIED_ITEM_MAKESAMESIZEBOTH );
			ImGui::EndMenu();
		}
		ImGui::Separator();
		if ( ImGui::MenuItem( "Scripts...", NULL, false, selectionCount == 1 ) ) Command( ID_GUIED_ITEM_SCRIPTS );
		if ( ImGui::MenuItem( "Properties...", NULL, false, selectionCount == 1 ) ) Command( ID_GUIED_ITEM_PROPERTIES );
		ImGui::EndMenu();
	}
	if ( ImGui::BeginMenu( "Tools" ) ) {
		if ( ImGui::MenuItem( "Viewer", NULL, false, hasWorkspace ) ) Command( ID_GUIED_TOOLS_VIEWER );
		if ( ImGui::MenuItem( "Reload Materials" ) ) Command( ID_GUIED_TOOLS_RELOADMATERIALS );
		ImGui::EndMenu();
	}
	if ( ImGui::BeginMenu( "Help" ) ) {
		if ( ImGui::MenuItem( "About" ) ) showAbout = true;
		ImGui::EndMenu();
	}
}

void GEImGuiHost::RenderToolbar() {
	if ( ImGui::Button( "New" ) ) Command( ID_GUIED_FILE_NEW );
	ImGui::SameLine();
	if ( ImGui::Button( "Open" ) ) Command( ID_GUIED_FILE_OPEN );
	ImGui::SameLine();
	ImGui::BeginDisabled( gApp.GetActiveWorkspace() == NULL );
	if ( ImGui::Button( "Save" ) ) Command( ID_GUIED_FILE_SAVE );
	ImGui::SameLine();
	if ( ImGui::Button( "Undo" ) ) Command( ID_GUIED_EDIT_UNDO );
	ImGui::SameLine();
	if ( ImGui::Button( "Redo" ) ) Command( ID_GUIED_EDIT_REDO );
	ImGui::SameLine();
	if ( ImGui::Button( "Zoom +" ) ) Command( ID_GUIED_VIEW_ZOOMIN );
	ImGui::SameLine();
	if ( ImGui::Button( "Zoom -" ) ) Command( ID_GUIED_VIEW_ZOOMOUT );
	ImGui::EndDisabled();
	ImGui::Separator();
}

void GEImGuiHost::RenderDocuments() {
	rvGEWorkspace *closeWorkspace = NULL;
	if ( ImGui::BeginTabBar( "GUIDocuments", ImGuiTabBarFlags_Reorderable | ImGuiTabBarFlags_AutoSelectNewTabs ) ) {
		for ( int index = 0; index < gApp.GetWorkspaceCount(); index++ ) {
			rvGEWorkspace *workspace = gApp.GetWorkspace( index );
			idStr label = workspace->GetFilename();
			label.StripPath();
			if ( workspace->IsModified() ) label.Append( "*" );
			label.Append( va( "###GUIDocument%d", index ) );
			bool open = true;
			if ( ImGui::BeginTabItem( label.c_str(), &open ) ) {
				if ( gApp.GetActiveWorkspace() != workspace ) gApp.SetActiveWorkspace( workspace );
				ImGui::EndTabItem();
			}
			if ( !open ) closeWorkspace = workspace;
		}
		ImGui::EndTabBar();
	}
	if ( closeWorkspace != NULL ) SendMessage( closeWorkspace->GetWindow(), WM_CLOSE, 0, 0 );
}

void GEImGuiHost::RenderWindowNode( rvGEWorkspace *workspace, idWindow *window ) {
	if ( window == NULL ) return;
	rvGEWindowWrapper *wrapper = rvGEWindowWrapper::GetWrapper( window );
	if ( wrapper == NULL || wrapper->IsDeleted() ) return;
	ImGui::PushID( window );
	ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;
	if ( wrapper->GetChildCount() == 0 ) flags |= ImGuiTreeNodeFlags_Leaf;
	if ( workspace->GetSelectionMgr().IsSelected( window ) ) flags |= ImGuiTreeNodeFlags_Selected;
	if ( window == workspace->GetInterface()->GetDesktop() ) ImGui::SetNextItemOpen( true, ImGuiCond_Once );
	const bool open = ImGui::TreeNodeEx( window->GetName(), flags );
	if ( ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen() ) {
		if ( ImGui::GetIO().KeyShift || ImGui::GetIO().KeyCtrl ) workspace->GetSelectionMgr().Add( window, false );
		else workspace->GetSelectionMgr().Set( window );
		gApp.GetNavigator().UpdateSelections();
		gApp.GetProperties().Update();
		gApp.GetTransformer().Update();
		propertyWindow = NULL;
	}
	if ( ImGui::BeginPopupContextItem( "WindowContext" ) ) {
		if ( ImGui::MenuItem( wrapper->IsHidden() ? "Show" : "Hide" ) ) {
			if ( wrapper->IsHidden() ) workspace->UnhideWindow( window );
			else workspace->HideWindow( window );
		}
		if ( ImGui::MenuItem( "Properties..." ) ) Command( ID_GUIED_ITEM_PROPERTIES );
		if ( ImGui::MenuItem( "Scripts..." ) ) Command( ID_GUIED_ITEM_SCRIPTS );
		ImGui::EndPopup();
	}
	if ( open ) {
		for ( int child = 0; child < wrapper->GetChildCount(); child++ ) RenderWindowNode( workspace, wrapper->GetChild( child ) );
		ImGui::TreePop();
	}
	ImGui::PopID();
}

void GEImGuiHost::RenderNavigator( rvGEWorkspace *workspace ) {
	ImGui::BeginChild( "NavigatorPanel", ImVec2( 0, 0 ), ImGuiChildFlags_Borders );
	ImGui::TextUnformatted( "Navigator" );
	ImGui::Separator();
	if ( workspace != NULL ) RenderWindowNode( workspace, workspace->GetInterface()->GetDesktop() );
	else ImGui::TextDisabled( "No GUI document open" );
	ImGui::EndChild();
}

WPARAM GEImGuiHost::MouseButtons() const {
	WPARAM buttons = 0;
	const ImGuiIO &io = ImGui::GetIO();
	if ( io.MouseDown[0] ) buttons |= MK_LBUTTON;
	if ( io.MouseDown[1] ) buttons |= MK_RBUTTON;
	if ( io.MouseDown[2] ) buttons |= MK_MBUTTON;
	if ( io.KeyShift ) buttons |= MK_SHIFT;
	if ( io.KeyCtrl ) buttons |= MK_CONTROL;
	return buttons;
}

void GEImGuiHost::RenderCanvasContextMenu( rvGEWorkspace *workspace ) {
	if ( ImGui::BeginPopup( "GUICanvasContext" ) ) {
		if ( ImGui::BeginMenu( "New" ) ) {
			if ( ImGui::MenuItem( "windowDef" ) ) Command( ID_GUIED_ITEM_NEWWINDOWDEF );
			if ( ImGui::MenuItem( "editDef" ) ) Command( ID_GUIED_ITEM_NEWEDITDEF );
			if ( ImGui::MenuItem( "choiceDef" ) ) Command( ID_GUIED_ITEM_NEWCHOICEDEF );
			if ( ImGui::MenuItem( "sliderDef" ) ) Command( ID_GUIED_ITEM_NEWSLIDERDEF );
			if ( ImGui::MenuItem( "listDef" ) ) Command( ID_GUIED_ITEM_NEWLISTDEF );
			ImGui::EndMenu();
		}
		ImGui::Separator();
		const int selected = workspace->GetSelectionMgr().Num();
		if ( ImGui::MenuItem( "Properties...", NULL, false, selected == 1 ) ) Command( ID_GUIED_ITEM_PROPERTIES );
		if ( ImGui::MenuItem( "Scripts...", NULL, false, selected == 1 ) ) Command( ID_GUIED_ITEM_SCRIPTS );
		if ( ImGui::MenuItem( "Delete", "Del", false, selected > 0 ) ) Command( ID_GUIED_EDIT_DELETE );
		ImGui::EndPopup();
	}
}

void GEImGuiHost::RenderCanvas( rvGEWorkspace *workspace, const ImVec2 &size ) {
	ImGui::BeginChild( "CanvasPanel", size, ImGuiChildFlags_Borders,
		ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse );
	ImVec2 canvasSize = ImGui::GetContentRegionAvail();
	canvasSize.x = Max( canvasSize.x, 1.0f );
	canvasSize.y = Max( canvasSize.y, 1.0f );
	const ImVec2 minimum = ImGui::GetCursorScreenPos();
	if ( workspace == NULL ) {
		ImGui::Dummy( canvasSize );
		ImGui::SetCursorScreenPos( ImVec2( minimum.x + canvasSize.x * 0.5f - 75.0f, minimum.y + canvasSize.y * 0.5f ) );
		ImGui::TextDisabled( "Create or open a .gui file" );
		ImGui::EndChild();
		return;
	}

	const int width = Max( 1, (int)canvasSize.x );
	const int height = Max( 1, (int)canvasSize.y );
	MakeCurrent();
	if ( canvasTexture.Prepare( width, height ) ) {
		MoveWindow( gApp.GetMDIClient(), 0, 0, width, height, FALSE );
		MoveWindow( workspace->GetWindow(), 0, 0, width, height, FALSE );
		HDC workspaceDC = GetDC( workspace->GetWindow() );
		InvalidateImageBindingCache();
		workspace->Render( workspaceDC, glrc );
		canvasTexture.CopyFromFrontBuffer();
		ReleaseDC( workspace->GetWindow(), workspaceDC );
	}
	MakeCurrent();
	ImGui::SetCurrentContext( imguiContext );
	ImGui::Image( canvasTexture.Ref(), canvasSize, canvasTexture.UV0(), canvasTexture.UV1() );
	const bool hovered = ImGui::IsItemHovered();
	ImGuiIO &io = ImGui::GetIO();
	const int x = (int)( io.MousePos.x - minimum.x );
	const int y = (int)( io.MousePos.y - minimum.y );

	if ( hovered && io.MouseWheel != 0.0f ) workspace->HandleExternalMouseWheel( io.MouseWheel > 0.0f ? 1 : -1 );
	if ( hovered && ImGui::IsMouseClicked( ImGuiMouseButton_Left ) ) {
		canvasFocused = true;
		canvasCaptured = true;
		workspace->HandleExternalMouseButton( 0, true, x, y );
		propertyWindow = NULL;
	}
	if ( hovered && ImGui::IsMouseClicked( ImGuiMouseButton_Middle ) ) {
		canvasFocused = true;
		canvasCaptured = true;
		workspace->HandleExternalMouseButton( 2, true, x, y );
	}
	if ( hovered && ImGui::IsMouseClicked( ImGuiMouseButton_Right ) ) {
		canvasFocused = true;
		idVec2 point( x, y );
		workspace->WindowToWorkspace( point );
		rvGEWindowWrapper *desktop = rvGEWindowWrapper::GetWrapper( workspace->GetInterface()->GetDesktop() );
		idWindow *underCursor = desktop ? desktop->WindowFromPoint( point.x, point.y ) : NULL;
		if ( underCursor != NULL && !workspace->GetSelectionMgr().IsSelected( underCursor ) ) {
			workspace->GetSelectionMgr().Set( underCursor );
			propertyWindow = NULL;
		}
		ImGui::OpenPopup( "GUICanvasContext" );
	}
	if ( canvasCaptured ) workspace->HandleExternalMouseMove( x, y, MouseButtons() );
	if ( canvasCaptured && ImGui::IsMouseReleased( ImGuiMouseButton_Left ) ) workspace->HandleExternalMouseButton( 0, false, x, y );
	if ( canvasCaptured && ImGui::IsMouseReleased( ImGuiMouseButton_Middle ) ) workspace->HandleExternalMouseButton( 2, false, x, y );
	if ( canvasCaptured && !io.MouseDown[0] && !io.MouseDown[2] ) canvasCaptured = false;
	if ( canvasFocused && !io.WantTextInput ) {
		if ( ImGui::IsKeyPressed( ImGuiKey_Delete ) ) Command( ID_GUIED_EDIT_DELETE );
		if ( ImGui::IsKeyPressed( ImGuiKey_LeftArrow ) ) workspace->HandleExternalKey( VK_LEFT );
		if ( ImGui::IsKeyPressed( ImGuiKey_RightArrow ) ) workspace->HandleExternalKey( VK_RIGHT );
		if ( ImGui::IsKeyPressed( ImGuiKey_UpArrow ) ) workspace->HandleExternalKey( VK_UP );
		if ( ImGui::IsKeyPressed( ImGuiKey_DownArrow ) ) workspace->HandleExternalKey( VK_DOWN );
	}
	RenderCanvasContextMenu( workspace );
	ImGui::EndChild();
}

void GEImGuiHost::SyncProperties( rvGEWorkspace *workspace, bool force ) {
	idWindow *selectedWindow = NULL;
	if ( workspace != NULL && workspace->GetSelectionMgr().Num() == 1 ) selectedWindow = workspace->GetSelectionMgr()[0];
	if ( !force && selectedWindow == propertyWindow ) return;
	propertyWindow = selectedWindow;
	propertyEdits.Clear();
	if ( propertyWindow == NULL ) return;
	rvGEWindowWrapper *wrapper = rvGEWindowWrapper::GetWrapper( propertyWindow );
	if ( wrapper == NULL ) return;
	for ( int index = 0; index < wrapper->GetStateDict().GetNumKeyVals(); index++ ) {
		const idKeyValue *keyValue = wrapper->GetStateDict().GetKeyVal( index );
		GEPropertyEdit edit;
		idStr::Copynz( edit.name, keyValue->GetKey(), sizeof( edit.name ) );
		idStr value = keyValue->GetValue();
		value.StripQuotes();
		idStr::Copynz( edit.value, value, sizeof( edit.value ) );
		propertyEdits.Append( edit );
	}
}

void GEImGuiHost::RenderProperties( rvGEWorkspace *workspace ) {
	SyncProperties( workspace );
	ImGui::TextUnformatted( "Properties" );
	ImGui::Separator();
	if ( propertyWindow == NULL ) {
		ImGui::TextDisabled( "Select one window" );
		return;
	}
	bool refresh = false;
	if ( ImGui::BeginTable( "GUIProperties", 2, ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable ) ) {
		ImGui::TableSetupColumn( "Property", ImGuiTableColumnFlags_WidthFixed, 105.0f * uiScale );
		ImGui::TableSetupColumn( "Value", ImGuiTableColumnFlags_WidthStretch );
		for ( int index = 0; index < propertyEdits.Num(); index++ ) {
			ImGui::PushID( index );
			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			ImGui::AlignTextToFramePadding();
			ImGui::TextUnformatted( propertyEdits[index].name );
			ImGui::TableNextColumn();
			ImGui::SetNextItemWidth( -1.0f );
			const bool enter = ImGui::InputText( "##Value", propertyEdits[index].value, sizeof( propertyEdits[index].value ), ImGuiInputTextFlags_EnterReturnsTrue );
			if ( enter || ImGui::IsItemDeactivatedAfterEdit() ) {
				if ( workspace->SetSelectedStateKey( propertyEdits[index].name, propertyEdits[index].value ) ) refresh = true;
			}
			ImGui::PopID();
			if ( refresh ) break;
		}
		ImGui::EndTable();
	}
	if ( refresh ) SyncProperties( workspace, true );
	ImGui::Separator();
	ImGui::SetNextItemWidth( 100.0f * uiScale );
	ImGui::InputTextWithHint( "##NewProperty", "property", newPropertyName, sizeof( newPropertyName ) );
	ImGui::SameLine();
	ImGui::SetNextItemWidth( -55.0f * uiScale );
	ImGui::InputTextWithHint( "##NewValue", "value", newPropertyValue, sizeof( newPropertyValue ) );
	ImGui::SameLine();
	if ( ImGui::Button( "Add" ) && newPropertyName[0] != '\0' ) {
		if ( workspace->SetSelectedStateKey( newPropertyName, newPropertyValue ) ) {
			newPropertyName[0] = newPropertyValue[0] = '\0';
			SyncProperties( workspace, true );
		}
	}
}

void GEImGuiHost::SyncScripts( rvGEWorkspace *workspace, bool force ) {
	idWindow *selectedWindow = NULL;
	if ( workspace != NULL && workspace->GetSelectionMgr().Num() == 1 ) selectedWindow = workspace->GetSelectionMgr()[0];
	if ( !force && selectedWindow == scriptWindow ) return;
	scriptWindow = selectedWindow;
	scriptEdits.Clear();
	variableEdits.Clear();
	if ( scriptWindow == NULL ) return;
	rvGEWindowWrapper *wrapper = rvGEWindowWrapper::GetWrapper( scriptWindow );
	if ( wrapper == NULL ) return;
	for ( int index = 0; index < wrapper->GetVariableDict().GetNumKeyVals(); index++ ) {
		const idKeyValue *keyValue = wrapper->GetVariableDict().GetKeyVal( index );
		GEPropertyEdit edit;
		idStr::Copynz( edit.name, keyValue->GetKey(), sizeof( edit.name ) );
		idStr::Copynz( edit.value, keyValue->GetValue(), sizeof( edit.value ) );
		variableEdits.Append( edit );
	}
	for ( int index = 0; index < wrapper->GetScriptDict().GetNumKeyVals(); index++ ) {
		const idKeyValue *keyValue = wrapper->GetScriptDict().GetKeyVal( index );
		GEPropertyEdit edit;
		idStr::Copynz( edit.name, keyValue->GetKey(), sizeof( edit.name ) );
		idStr::Copynz( edit.value, keyValue->GetValue(), sizeof( edit.value ) );
		scriptEdits.Append( edit );
	}
}

void GEImGuiHost::RenderScripts( rvGEWorkspace *workspace ) {
	SyncScripts( workspace );
	ImGui::TextUnformatted( "Scripts and Variables" );
	ImGui::Separator();
	if ( scriptWindow == NULL ) {
		ImGui::TextDisabled( "Select one window" );
		return;
	}
	bool refresh = false;
	if ( variableEdits.Num() > 0 && ImGui::CollapsingHeader( "Variables", ImGuiTreeNodeFlags_DefaultOpen ) ) {
		for ( int index = 0; index < variableEdits.Num(); index++ ) {
			ImGui::PushID( index );
			ImGui::TextUnformatted( variableEdits[index].name );
			ImGui::SetNextItemWidth( -1.0f );
			const bool enter = ImGui::InputText( "##Variable", variableEdits[index].value, sizeof( variableEdits[index].value ), ImGuiInputTextFlags_EnterReturnsTrue );
			if ( enter || ImGui::IsItemDeactivatedAfterEdit() ) {
				if ( workspace->SetSelectedScriptKey( variableEdits[index].name, variableEdits[index].value, true ) ) refresh = true;
			}
			ImGui::PopID();
			if ( refresh ) break;
		}
	}
	if ( !refresh ) {
		for ( int index = 0; index < scriptEdits.Num(); index++ ) {
			ImGui::PushID( index + 10000 );
			if ( ImGui::CollapsingHeader( scriptEdits[index].name, ImGuiTreeNodeFlags_DefaultOpen ) ) {
				const float height = Max( 90.0f * uiScale, ImGui::GetTextLineHeight() * 7.0f );
				if ( ImGui::InputTextMultiline( "##ScriptBody", scriptEdits[index].value, sizeof( scriptEdits[index].value ), ImVec2( -1.0f, height ) ) ) {
				}
				if ( ImGui::IsItemDeactivatedAfterEdit() ) {
					if ( workspace->SetSelectedScriptKey( scriptEdits[index].name, scriptEdits[index].value, false ) ) refresh = true;
				}
			}
			ImGui::PopID();
			if ( refresh ) break;
		}
	}
	if ( refresh ) SyncScripts( workspace, true );
	ImGui::SeparatorText( "Add Script" );
	ImGui::InputTextWithHint( "##NewScriptName", "onAction / onTime / scriptDef", newScriptName, sizeof( newScriptName ) );
	ImGui::InputTextMultiline( "##NewScriptBody", newScriptValue, sizeof( newScriptValue ), ImVec2( -1.0f, 90.0f * uiScale ) );
	if ( ImGui::Button( "Add script" ) && newScriptName[0] != '\0' ) {
		if ( workspace->SetSelectedScriptKey( newScriptName, newScriptValue, false ) ) {
			newScriptName[0] = newScriptValue[0] = '\0';
			SyncScripts( workspace, true );
		}
	}
}

void GEImGuiHost::RenderTransformer( rvGEWorkspace *workspace ) {
	ImGui::TextUnformatted( "Transformer" );
	ImGui::Separator();
	if ( workspace == NULL || workspace->GetSelectionMgr().Num() == 0 ) {
		ImGui::TextDisabled( "No selection" );
		return;
	}
	idRectangle rect = workspace->GetSelectionMgr().GetRect();
	workspace->WindowToWorkspace( rect );
	idWindow *relative = workspace->GetSelectionMgr().GetBottomMost();
	relative = relative ? relative->GetParent() : NULL;
	if ( relative != NULL ) {
		const idRectangle &parentRect = rvGEWindowWrapper::GetWrapper( relative )->GetScreenRect();
		rect.x -= parentRect.x;
		rect.y -= parentRect.y;
	}
	int values[4] = { (int)rect.x, (int)rect.y, (int)rect.w, (int)rect.h };
	const int oldValues[4] = { values[0], values[1], values[2], values[3] };
	ImGui::SetNextItemWidth( -1.0f );
	if ( ImGui::InputInt4( "X Y W H", values ) ) {
		if ( values[0] != oldValues[0] || values[1] != oldValues[1] ) {
			workspace->AddModifierMove( "Transform Move", values[0] - oldValues[0], values[1] - oldValues[1], false );
		}
		if ( values[2] != oldValues[2] || values[3] != oldValues[3] ) {
			workspace->AddModifierSize( "Transform Size", 0, 0, values[2] - oldValues[2], values[3] - oldValues[3], false );
		}
		propertyWindow = NULL;
	}
}

void GEImGuiHost::RenderInspector( rvGEWorkspace *workspace ) {
	ImGui::BeginChild( "InspectorPanel", ImVec2( 0, 0 ), ImGuiChildFlags_Borders );
	const int visibleTabs = ( showProperties ? 1 : 0 ) + ( showScripts ? 1 : 0 ) + ( showTransformer ? 1 : 0 );
	if ( visibleTabs > 1 ) {
		if ( ImGui::BeginTabBar( "GUIInspectorTabs" ) ) {
			if ( showProperties && ImGui::BeginTabItem( "Properties", NULL, requestedInspector == 1 ? ImGuiTabItemFlags_SetSelected : 0 ) ) { RenderProperties( workspace ); ImGui::EndTabItem(); }
			if ( showScripts && ImGui::BeginTabItem( "Scripts", NULL, requestedInspector == 2 ? ImGuiTabItemFlags_SetSelected : 0 ) ) { RenderScripts( workspace ); ImGui::EndTabItem(); }
			if ( showTransformer && ImGui::BeginTabItem( "Transformer", NULL, requestedInspector == 3 ? ImGuiTabItemFlags_SetSelected : 0 ) ) { RenderTransformer( workspace ); ImGui::EndTabItem(); }
			ImGui::EndTabBar();
		}
	} else if ( showProperties ) RenderProperties( workspace );
	else if ( showScripts ) RenderScripts( workspace );
	else if ( showTransformer ) RenderTransformer( workspace );
	requestedInspector = 0;
	ImGui::EndChild();
}

void GEImGuiHost::RenderOptions() {
	if ( !showOptions ) return;
	ImGui::SetNextWindowSize( ImVec2( 420.0f * uiScale, 240.0f * uiScale ), ImGuiCond_FirstUseEver );
	if ( ImGui::Begin( "GUI Editor Options", &showOptions ) ) {
		bool gridVisible = gApp.GetOptions().GetGridVisible();
		if ( ImGui::Checkbox( "Show grid", &gridVisible ) ) gApp.GetOptions().SetGridVisible( gridVisible );
		bool gridSnap = gApp.GetOptions().GetGridSnap();
		if ( ImGui::Checkbox( "Snap to grid", &gridSnap ) ) gApp.GetOptions().SetGridSnap( gridSnap );
		int gridWidth = gApp.GetOptions().GetGridWidth();
		int gridHeight = gApp.GetOptions().GetGridHeight();
		if ( ImGui::InputInt( "Grid width", &gridWidth ) ) gApp.GetOptions().SetGridWidth( Max( 1, gridWidth ) );
		if ( ImGui::InputInt( "Grid height", &gridHeight ) ) gApp.GetOptions().SetGridHeight( Max( 1, gridHeight ) );
		bool ignoreDesktop = gApp.GetOptions().GetIgnoreDesktopSelect();
		if ( ImGui::Checkbox( "Ignore desktop when selecting", &ignoreDesktop ) ) gApp.GetOptions().SetIgnoreDesktopSelect( ignoreDesktop );
	}
	ImGui::End();
}

void GEImGuiHost::RenderAbout() {
	if ( showAbout ) ImGui::OpenPopup( "About GUI Editor" );
	showAbout = false;
	if ( ImGui::BeginPopupModal( "About GUI Editor", NULL, ImGuiWindowFlags_AlwaysAutoResize ) ) {
		ImGui::TextUnformatted( "DOOMEdit GUI Editor" );
		ImGui::TextDisabled( "Dear ImGui shell with the original idTech GUI renderer and editing backend." );
		ImGui::Spacing();
		if ( ImGui::Button( "Close", ImVec2( 100.0f * uiScale, 0 ) ) ) ImGui::CloseCurrentPopup();
		ImGui::EndPopup();
	}
}

void GEImGuiHost::RenderViewer() {
	if ( !showViewer ) return;
	ImGui::SetNextWindowSize( ImVec2( 700.0f * uiScale, 560.0f * uiScale ), ImGuiCond_FirstUseEver );
	if ( ImGui::Begin( "GUI Preview", &showViewer ) ) {
		if ( gApp.GetActiveWorkspace() == NULL ) {
			ImGui::TextDisabled( "No GUI document open" );
		} else {
			ImVec2 available = ImGui::GetContentRegionAvail();
			const float aspect = (float)SCREEN_WIDTH / (float)SCREEN_HEIGHT;
			ImVec2 preview = available;
			if ( preview.x / Max( 1.0f, preview.y ) > aspect ) preview.x = preview.y * aspect;
			else preview.y = preview.x / aspect;
			ImGui::SetCursorPosX( ImGui::GetCursorPosX() + Max( 0.0f, ( available.x - preview.x ) * 0.5f ) );
			ImGui::Image( canvasTexture.Ref(), preview, canvasTexture.UV0(), canvasTexture.UV1() );
		}
	}
	ImGui::End();
}

void GEImGuiHost::UpdateTitle( rvGEWorkspace *workspace ) {
	idStr title = "DOOMEdit - GUI Editor";
	if ( workspace != NULL ) {
		title.Append( " - " );
		title.Append( workspace->GetFilename() );
		if ( workspace->IsModified() ) title.Append( "*" );
	}
	char current[1024];
	GetWindowTextA( hwnd, current, sizeof( current ) );
	if ( idStr::Cmp( current, title.c_str() ) ) SetWindowTextA( hwnd, title.c_str() );
}

void GEImGuiHost::RenderShell() {
	ImGuiIO &io = ImGui::GetIO();
	ImGui::SetNextWindowPos( ImVec2( 0, 0 ) );
	ImGui::SetNextWindowSize( io.DisplaySize );
	ImGui::Begin( "GUI Editor Root", NULL,
		ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
		ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_MenuBar |
		ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse );
	if ( ImGui::BeginMenuBar() ) {
		RenderMenuBar();
		ImGui::EndMenuBar();
	}
	RenderToolbar();
	RenderDocuments();
	rvGEWorkspace *workspace = gApp.GetActiveWorkspace();
	UpdateTitle( workspace );
	ImVec2 available = ImGui::GetContentRegionAvail();
	const float statusHeight = showStatusBar ? ImGui::GetFrameHeightWithSpacing() : 0.0f;
	available.y = Max( 1.0f, available.y - statusHeight );
	const bool showInspector = showProperties || showScripts || showTransformer;
	const int columns = 1 + ( showNavigator ? 1 : 0 ) + ( showInspector ? 1 : 0 );
	if ( ImGui::BeginTable( "GUIEditorWorkspace", columns, ImGuiTableFlags_Resizable | ImGuiTableFlags_NoSavedSettings, available ) ) {
		if ( showNavigator ) ImGui::TableSetupColumn( "Navigator", ImGuiTableColumnFlags_WidthFixed, 230.0f * uiScale );
		ImGui::TableSetupColumn( "Canvas", ImGuiTableColumnFlags_WidthStretch );
		if ( showInspector ) ImGui::TableSetupColumn( "Inspector", ImGuiTableColumnFlags_WidthFixed, 330.0f * uiScale );
		if ( showNavigator ) { ImGui::TableNextColumn(); RenderNavigator( workspace ); }
		ImGui::TableNextColumn(); RenderCanvas( workspace, ImVec2( 0, 0 ) );
		if ( showInspector ) { ImGui::TableNextColumn(); RenderInspector( workspace ); }
		ImGui::EndTable();
	}
	if ( showStatusBar ) {
		ImGui::Separator();
		if ( workspace != NULL ) {
			ImGui::Text( "%s    Zoom: %d%%    Selection: %d%s", workspace->GetFilename(),
				(int)( workspace->GetZoomScale() * 100.0f ), workspace->GetSelectionMgr().Num(),
				workspace->IsModified() ? "    Modified" : "" );
		} else ImGui::TextDisabled( "Ready" );
	}
	ImGui::End();
	RenderOpenFileDialog();
	RenderSaveFileDialog();
	RenderOptions();
	RenderAbout();
	RenderViewer();
}

void GEImGuiHost::Frame() {
	if ( !rendererReady || rendering || !IsVisible() || IsIconic( hwnd ) ) return;
	ScopedImGuiContext scopedContext( imguiContext );
	rendering = true;
	ProcessPendingFileCommand();

	// The GUI canvas invokes the full Doom renderer.  Its state cache is global,
	// even though this host now owns a separate shared HGLRC, so maintain a cache
	// image for each context and restore Radiant's renderer globals afterward.
	backEndState_t engineBackEndState = backEnd;
	performanceCounters_t enginePerformanceCounters = tr.pc;
	const int engineVideoWidth = glConfig.vidWidth;
	const int engineVideoHeight = glConfig.vidHeight;
	renderCrop_t engineRenderCrops[MAX_RENDER_CROPS];
	memcpy( engineRenderCrops, tr.renderCrops, sizeof( engineRenderCrops ) );
	const int engineRenderCrop = tr.currentRenderCrop;
	const bool engineGuiPillarbox = tr.guiPillarbox;
	backEnd = editorBackEndState;
	InvalidateImageBindingCache();

	MakeCurrent();
	ImGui_ImplOpenGL2_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();
	RenderShell();
	MakeCurrent();
	ImGui::SetCurrentContext( imguiContext );
	ImGui::Render();
	RECT client;
	GetClientRect( hwnd, &client );
	qglViewport( 0, 0, client.right, client.bottom );
	qglScissor( 0, 0, client.right, client.bottom );
	qglClearColor( 0.105f, 0.110f, 0.115f, 1.0f );
	qglClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );
	RenderImGuiDrawData( ImGui::GetDrawData() );
	SwapBuffers( dc );

	editorBackEndState = backEnd;
	backEnd = engineBackEndState;
	tr.pc = enginePerformanceCounters;
	glConfig.vidWidth = engineVideoWidth;
	glConfig.vidHeight = engineVideoHeight;
	memcpy( tr.renderCrops, engineRenderCrops, sizeof( engineRenderCrops ) );
	tr.currentRenderCrop = engineRenderCrop;
	tr.guiPillarbox = engineGuiPillarbox;
	RestoreEngineContext();
	rendering = false;
}

LRESULT GEImGuiHost::WindowProc( HWND window, UINT message, WPARAM wParam, LPARAM lParam ) {
	ScopedImGuiContext scopedContext( imguiContext );
	bool imguiHandled = false;
	if ( imguiContext != NULL ) {
		imguiHandled = rendererReady && ImGui_ImplWin32_WndProcHandler( window, message, wParam, lParam );
	}
	if ( message == WM_LBUTTONDOWN || message == WM_RBUTTONDOWN || message == WM_MBUTTONDOWN ) {
		if ( GetFocus() != window ) SetFocus( window );
	}
	if ( imguiHandled ) return TRUE;
	switch ( message ) {
		case WM_ERASEBKGND: return 1;
		case WM_PAINT: {
			PAINTSTRUCT paint;
			BeginPaint( window, &paint );
			EndPaint( window, &paint );
			Frame();
			return 0;
		}
		case WM_SIZE:
			if ( wParam != SIZE_MINIMIZED ) InvalidateRect( window, NULL, FALSE );
			return 0;
		case WM_CLOSE:
			GUIEditorHide();
			return 0;
		case WM_ACTIVATE:
			if ( LOWORD( wParam ) != WA_INACTIVE ) ApplyBlackTitleBar( window );
			break;
		case WM_NCDESTROY:
			SetWindowLongPtr( window, GWLP_USERDATA, 0 );
			if ( hwnd == window ) hwnd = NULL;
			return 0;
	}
	return DefWindowProc( window, message, wParam, lParam );
}

LRESULT CALLBACK GEImGuiHost::StaticWindowProc( HWND window, UINT message, WPARAM wParam, LPARAM lParam ) {
	GEImGuiHost *host = (GEImGuiHost *)GetWindowLongPtr( window, GWLP_USERDATA );
	if ( message == WM_NCCREATE ) {
		CREATESTRUCT *create = (CREATESTRUCT *)lParam;
		host = (GEImGuiHost *)create->lpCreateParams;
		SetWindowLongPtr( window, GWLP_USERDATA, (LONG_PTR)host );
		host->hwnd = window;
	}
	return host != NULL ? host->WindowProc( window, message, wParam, lParam ) : DefWindowProc( window, message, wParam, lParam );
}

} // namespace

bool GEImGuiCreate() {
	if ( guiEditorHost != NULL ) return true;
	guiEditorHost = new GEImGuiHost();
	if ( !guiEditorHost->Create() ) {
		delete guiEditorHost;
		guiEditorHost = NULL;
		return false;
	}
	return true;
}

void GEImGuiDestroy() {
	delete guiEditorHost;
	guiEditorHost = NULL;
}

void GEImGuiShow() {
	if ( guiEditorHost != NULL ) guiEditorHost->Show();
}

void GEImGuiHide() {
	if ( guiEditorHost != NULL ) guiEditorHost->Hide();
}

void GEImGuiToggle() {
	if ( guiEditorHost == NULL ) {
		GUIEditorInit();
	} else if ( guiEditorHost->IsVisible() ) {
		GUIEditorHide();
	} else {
		com_editors |= EDITOR_GUI;
		guiEditorHost->Show();
	}
}

void GEImGuiFrame() {
	if ( guiEditorHost != NULL ) guiEditorHost->Frame();
}

void GEImGuiExecuteCommand( UINT command ) {
	if ( guiEditorHost != NULL ) guiEditorHost->ExecuteCommand( command );
}

bool GEImGuiIsVisible() {
	return guiEditorHost != NULL && guiEditorHost->IsVisible();
}

HWND GEImGuiWindow() {
	return guiEditorHost != NULL ? guiEditorHost->Window() : NULL;
}

bool GEImGuiOwnsWindow( HWND window ) {
	HWND host = GEImGuiWindow();
	return host != NULL && window != NULL && ( window == host || GetAncestor( window, GA_ROOT ) == host );
}

void GEImGuiShowProperties() {
	if ( guiEditorHost != NULL ) guiEditorHost->ShowProperties();
}

void GEImGuiShowScripts() {
	if ( guiEditorHost != NULL ) guiEditorHost->ShowScripts();
}

void GEImGuiShowOptions() {
	if ( guiEditorHost != NULL ) guiEditorHost->ShowOptions();
}

void GEImGuiShowViewer() {
	if ( guiEditorHost != NULL ) guiEditorHost->ShowViewer();
}

void GEImGuiShowAbout() {
	if ( guiEditorHost != NULL ) guiEditorHost->ShowAbout();
}
