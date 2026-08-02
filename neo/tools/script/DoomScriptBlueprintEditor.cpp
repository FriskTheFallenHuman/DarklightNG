#include "../../idlib/precompiled.h"
#pragma hdrstop

#include "../radiant/qe3.h"
#include "../radiant/RadiantImGui.h"
#include "DoomScriptBlueprint.h"
#include "DoomScriptBlueprintEditor.h"

#include "imgui.h"
#include "backends/imgui_impl_opengl2.h"
#include "backends/imgui_impl_win32.h"

#include <GL/gl.h>
#include <float.h>
#include <string.h>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler( HWND window, UINT message, WPARAM wParam, LPARAM lParam );

namespace {

static const char *BLUEPRINT_WINDOW_CLASS = "DarklightDoomScriptBlueprintImGui";

struct BlueprintVariableChoice {
	idStr type;
	idStr name;
};

struct BlueprintPinPayload {
	int node;
	int pin;
};

struct BlueprintConditionClause {
	int join;
	int variable;
	int operation;
	int valueSource;
	idStr value;
};

static idStr TrimConditionValue( const idStr &input ) {
	idStr value = input;
	while ( value.Length() && isspace( (unsigned char)value[0] ) ) value = value.Mid( 1, value.Length() - 1 );
	value.StripTrailingWhitespace();
	return value;
}

static idStr StripConditionParentheses( const idStr &input ) {
	idStr value = TrimConditionValue( input );
	for ( ;; ) {
		if ( value.Length() < 2 || value[0] != '(' || value[value.Length() - 1] != ')' ) return value;
		int depth = 0;
		char quote = '\0';
		bool enclosesWholeValue = true;
		for ( int index = 0; index < value.Length(); index++ ) {
			char current = value[index];
			if ( quote != '\0' ) {
				if ( current == quote && ( index == 0 || value[index - 1] != '\\' ) ) quote = '\0';
				continue;
			}
			if ( current == '"' || current == '\'' ) { quote = current; continue; }
			if ( current == '(' ) depth++;
			else if ( current == ')' ) {
				depth--;
				if ( depth == 0 && index != value.Length() - 1 ) { enclosesWholeValue = false; break; }
			}
		}
		if ( !enclosesWholeValue || depth != 0 ) return value;
		value = TrimConditionValue( value.Mid( 1, value.Length() - 2 ) );
	}
}

static int FindConditionVariable( const idList<BlueprintVariableChoice> &variables, const idStr &name ) {
	idStr value = StripConditionParentheses( name );
	for ( int index = 0; index < variables.Num(); index++ ) if ( variables[index].name == value ) return index;
	return -1;
}

static bool IsNumericConditionType( const idStr &type ) {
	return type == "float" || type == "integer";
}

static bool ConditionTypesCompatible( const idStr &left, const idStr &right ) {
	return left == right || ( ( IsNumericConditionType( left ) || left == "boolean" ) &&
		( IsNumericConditionType( right ) || right == "boolean" ) );
}

static bool FindTopLevelComparison( const idStr &input, int &offset, int &operation, int &length ) {
	static const char *operators[] = { "==", "!=", "<=", ">=", "<", ">" };
	static const int operations[] = { 2, 3, 5, 7, 4, 6 };
	int depth = 0;
	char quote = '\0';
	for ( int index = 0; index < input.Length(); index++ ) {
		char current = input[index];
		if ( quote != '\0' ) {
			if ( current == quote && ( index == 0 || input[index - 1] != '\\' ) ) quote = '\0';
			continue;
		}
		if ( current == '"' || current == '\'' ) { quote = current; continue; }
		if ( current == '(' || current == '[' ) { depth++; continue; }
		if ( current == ')' || current == ']' ) { depth--; continue; }
		if ( depth != 0 ) continue;
		for ( int operatorIndex = 0; operatorIndex < 6; operatorIndex++ ) {
			int operatorLength = (int)strlen( operators[operatorIndex] );
			if ( index + operatorLength <= input.Length() && input.Mid( index, operatorLength ) == operators[operatorIndex] ) {
				offset = index;
				operation = operations[operatorIndex];
				length = operatorLength;
				return true;
			}
		}
	}
	return false;
}

static bool ParseConditionClause( const idStr &input, int join, const idList<BlueprintVariableChoice> &variables,
	BlueprintConditionClause &clause ) {
	idStr value = StripConditionParentheses( input );
	clause.join = join;
	clause.variable = -1;
	clause.operation = 0;
	clause.valueSource = -1;
	clause.value.Clear();
	int comparisonOffset = -1, operation = 0, comparisonLength = 0;
	if ( FindTopLevelComparison( value, comparisonOffset, operation, comparisonLength ) ) {
		idStr left = StripConditionParentheses( value.Left( comparisonOffset ) );
		idStr right = StripConditionParentheses( value.Mid( comparisonOffset + comparisonLength,
			value.Length() - comparisonOffset - comparisonLength ) );
		clause.variable = FindConditionVariable( variables, left );
		if ( clause.variable < 0 || right.Length() == 0 ) return false;
		clause.operation = operation;
		clause.valueSource = FindConditionVariable( variables, right );
		clause.value = right;
		return true;
	}
	bool negate = value.Length() != 0 && value[0] == '!' && ( value.Length() == 1 || value[1] != '=' );
	if ( negate ) value = StripConditionParentheses( value.Mid( 1, value.Length() - 1 ) );
	clause.variable = FindConditionVariable( variables, value );
	if ( clause.variable < 0 ) return false;
	clause.operation = negate ? 1 : 0;
	return true;
}

static bool ParseConditionExpression( const idStr &input, const idList<BlueprintVariableChoice> &variables,
	idList<BlueprintConditionClause> &clauses ) {
	clauses.Clear();
	idStr expression = StripConditionParentheses( input );
	int depth = 0;
	char quote = '\0';
	int start = 0;
	int nextJoin = 0;
	for ( int index = 0; index <= expression.Length(); index++ ) {
		char current = index < expression.Length() ? expression[index] : '\0';
		if ( quote != '\0' ) {
			if ( current == quote && ( index == 0 || expression[index - 1] != '\\' ) ) quote = '\0';
			continue;
		}
		if ( current == '"' || current == '\'' ) { quote = current; continue; }
		if ( current == '(' || current == '[' ) { depth++; continue; }
		if ( current == ')' || current == ']' ) { depth--; continue; }
		bool atEnd = index == expression.Length();
		bool logicalAnd = !atEnd && depth == 0 && current == '&' && index + 1 < expression.Length() && expression[index + 1] == '&';
		bool logicalOr = !atEnd && depth == 0 && current == '|' && index + 1 < expression.Length() && expression[index + 1] == '|';
		if ( !atEnd && !logicalAnd && !logicalOr ) continue;
		BlueprintConditionClause clause;
		if ( !ParseConditionClause( expression.Mid( start, index - start ), nextJoin, variables, clause ) ) {
			clauses.Clear();
			return false;
		}
		clauses.Append( clause );
		if ( !atEnd ) {
			nextJoin = logicalOr ? 1 : 0;
			index++;
			start = index + 1;
		}
	}
	return clauses.Num() != 0;
}

static ImU32 PinColor( const idStr &type ) {
	if ( type == "boolean" ) return IM_COL32( 219, 78, 91, 255 );
	if ( type == "float" || type == "integer" ) return IM_COL32( 91, 196, 118, 255 );
	if ( type == "vector" ) return IM_COL32( 229, 190, 72, 255 );
	if ( type == "string" ) return IM_COL32( 213, 92, 192, 255 );
	if ( type == "entity" ) return IM_COL32( 82, 178, 220, 255 );
	return IM_COL32( 205, 209, 217, 255 );
}

static ImU32 HeaderColor( const doomScriptGraphNode_t &node ) {
	if ( node.kind == "event" ) return IM_COL32( 155, 49, 55, 255 );
	if ( node.kind == "branch" ) return IM_COL32( 158, 100, 31, 255 );
	if ( node.kind == "loop" ) return IM_COL32( 112, 69, 153, 255 );
	if ( node.kind == "setvar" || node.kind == "getvar" ) return IM_COL32( 45, 126, 76, 255 );
	if ( node.kind == "expression" || node.kind == "literal" ) return IM_COL32( 50, 111, 145, 255 );
	if ( node.kind.Find( "operator:" ) == 0 ) return IM_COL32( 110, 62, 135, 255 );
	if ( node.kind.Find( "function:" ) == 0 ) return IM_COL32( 43, 104, 139, 255 );
	return IM_COL32( 72, 83, 103, 255 );
}

static idStr VariableLabel( const doomScriptVariable_t &variable ) {
	idStr label = variable.type + " " + variable.name;
	if ( variable.defaultValue.Length() ) label += " = " + variable.defaultValue;
	return label;
}

static const char *GraphKindLabel( const doomScriptGraphNode_t &node ) {
	if ( node.kind == "event" ) return "Event";
	if ( node.kind == "branch" ) return "Branch";
	if ( node.kind == "loop" ) return "Loop";
	if ( node.kind == "setvar" ) return "Set Variable";
	if ( node.kind == "getvar" ) return "Get Variable";
	if ( node.kind == "literal" ) return "Value";
	if ( node.kind == "expression" ) return node.valueType == "boolean" ? "Condition Builder" : "Operator Graph";
	if ( node.kind.Find( "operator:" ) == 0 ) return "Operator";
	if ( node.kind.Find( "function:" ) == 0 ) return "Function Call";
	if ( node.kind == "return" ) return "Return";
	if ( node.kind == "break" ) return "Break Loop";
	if ( node.kind == "continue" ) return "Continue Loop";
	if ( node.kind == "thread" ) return "Thread";
	return "Flow Node";
}

static int ExecutionOutputCount( const doomScriptGraphNode_t &node ) {
	if ( node.dataOnly || node.kind == "return" ) return 0;
	return node.kind == "branch" || node.kind == "loop" ? 2 : 1;
}

static const char *ExecutionOutputLabel( const doomScriptGraphNode_t &node, int pin ) {
	if ( node.kind == "branch" ) return pin == 0 ? "True" : "False";
	if ( node.kind == "loop" ) return pin == 0 ? "Loop Body" : "Completed";
	return "";
}

static void CopyText( char *destination, int capacity, const idStr &source ) {
	if ( capacity <= 0 ) return;
	idStr::Copynz( destination, source.c_str(), capacity );
}

static const char *PinTypeLabel( const doomScriptBlueprintPin_t &pin ) {
	return pin.enumType.Length() != 0 ? pin.enumType.c_str() : pin.type.c_str();
}

class DoomScriptBlueprintImGuiEditor {
public:
	DoomScriptBlueprintImGuiEditor();
	~DoomScriptBlueprintImGuiEditor();

	bool Create();
	void Focus();
	LRESULT WindowProc( HWND window, UINT message, WPARAM wParam, LPARAM lParam );

private:
	static LRESULT CALLBACK StaticWindowProc( HWND window, UINT message, WPARAM wParam, LPARAM lParam );
	bool InitializeRenderer();
	void ShutdownRenderer();
	void RenderFrame();
	void RenderEditor();
	void RenderToolbar();
	void RenderScopePanel();
	void RenderPalettePanel();
	void RenderGraph();
	void RenderNodeEditor();
	void RenderStatusBar();
	void FillScripts();
	bool LoadScript( int index );
	bool ConfirmDiscardOrSave();
	void Save();
	void MigrateAll();
	void FrameFunction();
	void BuildVariableChoices( int functionIndex );
	void AddVariableChoice( const char *type, const char *name );
	void OpenNodeEditor( int nodeIndex );
	void ApplyNodeEditor();
	int FindConditionOwner( int nodeIndex ) const;
	void AddConditionClause();
	void SetConditionError( const char *text );
	void AddSelectedVariableSetNode();
	void RestoreEngineContext();
	void SetStatus( const char *text );
	int FindNodeByStableId( const idStr &stableId ) const;
	ImVec2 NodeSize( const doomScriptGraphNode_t &node ) const;
	ImVec2 NodePosition( const ImVec2 &origin, const doomScriptGraphNode_t &node ) const;
	ImVec2 InputPinPosition( const ImVec2 &origin, const doomScriptGraphNode_t &node, int pin, bool execution ) const;
	ImVec2 OutputPinPosition( const ImVec2 &origin, const doomScriptGraphNode_t &node, int pin, bool execution ) const;
	void DrawLink( ImDrawList *draw, const ImVec2 &from, const ImVec2 &to, ImU32 color, float thickness ) const;

	HWND hwnd;
	HDC dc;
	HGLRC glrc;
	ImGuiContext *imguiContext;
	bool rendererReady;
	bool rendering;
	bool deleteOnDestroy;
	DoomScriptNodeCatalog catalog;
	DoomScriptBlueprintDocument document;
	idList<idStr> scriptFiles;
	int selectedScript;
	int selectedFunction;
	int selectedNode;
	int selectedPalette;
	int selectedGlobal;
	int selectedLocal;
	int selectedShared;
	char paletteFilter[128];
	char variableName[128];
	char variableDefault[256];
	int variableType;
	ImVec2 pan;
	float zoom;
	float uiScale;
	bool graphPanning;
	bool frameRequested;
	idStr status;

	int editNode;
	idStr editStableId;
	idList<BlueprintVariableChoice> editVariables;
	idList<int> editInputSources;
	idList<idStr> editInputValues;
	idList<BlueprintConditionClause> editConditions;
	idStr editConditionError;
	bool editConditionBuilder;
	int editVariable;
	int editOperation;
	int editValueSource;
	char editValue[256];
	bool openEditPopup;
};

static DoomScriptBlueprintImGuiEditor *blueprintEditor = NULL;

DoomScriptBlueprintImGuiEditor::DoomScriptBlueprintImGuiEditor() :
	hwnd( NULL ), dc( NULL ), glrc( NULL ), imguiContext( NULL ), rendererReady( false ), rendering( false ), deleteOnDestroy( false ), selectedScript( -1 ),
	selectedFunction( -1 ), selectedNode( -1 ), selectedPalette( -1 ), selectedGlobal( -1 ), selectedLocal( -1 ),
	selectedShared( -1 ), variableType( 0 ), pan( 30.0f, 30.0f ), zoom( 1.0f ), uiScale( 1.0f ), graphPanning( false ), frameRequested( true ),
	editNode( -1 ), editConditionBuilder( false ), editVariable( 0 ), editOperation( 0 ), editValueSource( -1 ), openEditPopup( false ) {
	paletteFilter[0] = '\0';
	variableName[0] = '\0';
	variableDefault[0] = '\0';
	editValue[0] = '\0';
}

DoomScriptBlueprintImGuiEditor::~DoomScriptBlueprintImGuiEditor() {
	ShutdownRenderer();
}

bool DoomScriptBlueprintImGuiEditor::Create() {
	ImGui_ImplWin32_EnableDpiAwareness();
	HINSTANCE instance = AfxGetInstanceHandle();
	WNDCLASSEXA windowClass;
	ZeroMemory( &windowClass, sizeof( windowClass ) );
	windowClass.cbSize = sizeof( windowClass );
	windowClass.style = CS_OWNDC | CS_DBLCLKS | CS_HREDRAW | CS_VREDRAW;
	windowClass.lpfnWndProc = StaticWindowProc;
	windowClass.hInstance = instance;
	windowClass.hCursor = LoadCursor( NULL, IDC_ARROW );
	windowClass.hIcon = LoadIcon( instance, MAKEINTRESOURCE( IDR_MAINFRAME ) );
	windowClass.hbrBackground = NULL;
	windowClass.lpszClassName = BLUEPRINT_WINDOW_CLASS;
	RegisterClassExA( &windowClass );

	HWND owner = g_pParentWnd != NULL ? g_pParentWnd->GetSafeHwnd() : win32.hWnd;
	hwnd = CreateWindowExA( WS_EX_APPWINDOW, BLUEPRINT_WINDOW_CLASS, "DoomScript Blueprint Editor",
		WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN | WS_CLIPSIBLINGS, CW_USEDEFAULT, CW_USEDEFAULT, 1500, 900,
		owner, NULL, instance, this );
	if ( hwnd == NULL ) return false;
	if ( !InitializeRenderer() ) {
		DestroyWindow( hwnd );
		return false;
	}
	deleteOnDestroy = true;
	ShowWindow( hwnd, SW_SHOW );
	UpdateWindow( hwnd );
	SetTimer( hwnd, 1, 16, NULL );

	if ( !catalog.Load() ) SetStatus( "DoomTypeInfo function catalog is missing. Build DoomTypeInfo first." );
	FillScripts();
	if ( scriptFiles.Num() > 0 ) LoadScript( 0 );
	return true;
}

void DoomScriptBlueprintImGuiEditor::Focus() {
	if ( hwnd == NULL ) return;
	ShowWindow( hwnd, SW_RESTORE );
	SetForegroundWindow( hwnd );
}

bool DoomScriptBlueprintImGuiEditor::InitializeRenderer() {
	dc = GetDC( hwnd );
	if ( dc == NULL ) return false;
	QEW_SetupPixelFormat( dc, false );
	glrc = wglCreateContext( dc );
	if ( glrc == NULL || !wglMakeCurrent( dc, glrc ) ) return false;
	if ( win32.hGLRC != NULL ) wglShareLists( win32.hGLRC, glrc );

	IMGUI_CHECKVERSION();
	ImGuiContext *previousContext = ImGui::GetCurrentContext();
	imguiContext = ImGui::CreateContext();
	ImGui::SetCurrentContext( imguiContext );
	ImGuiIO &io = ImGui::GetIO();
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
	io.IniFilename = NULL;
	uiScale = Max( 1.0f, ImGui_ImplWin32_GetDpiScaleForHwnd( hwnd ) ) * 1.15f;
	ImFontConfig fontConfig;
	fontConfig.SizePixels = 13.0f * uiScale;
	io.Fonts->AddFontDefault( &fontConfig );
	ImGui::StyleColorsDark();
	ImGuiStyle &style = ImGui::GetStyle();
	style.WindowRounding = 3.0f;
	style.ChildRounding = 3.0f;
	style.FrameRounding = 3.0f;
	style.PopupRounding = 3.0f;
	style.ScrollbarRounding = 4.0f;
	style.Colors[ImGuiCol_WindowBg] = ImVec4( 0.075f, 0.082f, 0.095f, 1.0f );
	style.Colors[ImGuiCol_ChildBg] = ImVec4( 0.090f, 0.098f, 0.112f, 1.0f );
	style.Colors[ImGuiCol_Header] = ImVec4( 0.16f, 0.31f, 0.43f, 1.0f );
	style.Colors[ImGuiCol_Button] = ImVec4( 0.16f, 0.31f, 0.43f, 1.0f );
	style.ScaleAllSizes( uiScale );
	pan = ImVec2( 30.0f * uiScale, 30.0f * uiScale );
	if ( !ImGui_ImplWin32_InitForOpenGL( hwnd ) || !ImGui_ImplOpenGL2_Init() ) {
		ImGui::SetCurrentContext( previousContext );
		return false;
	}
	rendererReady = true;
	ImGui::SetCurrentContext( previousContext );
	RestoreEngineContext();
	return true;
}

void DoomScriptBlueprintImGuiEditor::ShutdownRenderer() {
	if ( hwnd != NULL ) KillTimer( hwnd, 1 );
	if ( rendererReady || imguiContext != NULL ) {
		ImGuiContext *previousContext = ImGui::GetCurrentContext() == imguiContext ? NULL : ImGui::GetCurrentContext();
		ImGui::SetCurrentContext( imguiContext );
		wglMakeCurrent( dc, glrc );
		if ( rendererReady ) {
			ImGui_ImplOpenGL2_Shutdown();
			ImGui_ImplWin32_Shutdown();
		}
		if ( imguiContext != NULL ) {
			ImGui::DestroyContext( imguiContext );
			imguiContext = NULL;
		}
		ImGui::SetCurrentContext( previousContext );
		rendererReady = false;
	}
	if ( glrc != NULL ) {
		wglMakeCurrent( NULL, NULL );
		wglDeleteContext( glrc );
		glrc = NULL;
	}
	if ( hwnd != NULL && dc != NULL ) {
		ReleaseDC( hwnd, dc );
		dc = NULL;
	}
	RestoreEngineContext();
}

void DoomScriptBlueprintImGuiEditor::RestoreEngineContext() {
	if ( win32.hDC != NULL && win32.hGLRC != NULL ) wglMakeCurrent( win32.hDC, win32.hGLRC );
}

void DoomScriptBlueprintImGuiEditor::RenderFrame() {
	if ( !rendererReady || rendering || IsIconic( hwnd ) ) return;
	ImGuiContext *previousContext = ImGui::GetCurrentContext();
	ImGui::SetCurrentContext( imguiContext );
	rendering = true;
	if ( !wglMakeCurrent( dc, glrc ) ) {
		rendering = false;
		ImGui::SetCurrentContext( previousContext );
		return;
	}
	RECT client;
	GetClientRect( hwnd, &client );
	if ( client.right <= 0 || client.bottom <= 0 ) {
		RestoreEngineContext();
		rendering = false;
		ImGui::SetCurrentContext( previousContext );
		return;
	}
	ImGui_ImplOpenGL2_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();
	RenderEditor();
	ImGui::Render();
	glViewport( 0, 0, client.right, client.bottom );
	glClearColor( 0.055f, 0.060f, 0.070f, 1.0f );
	glClear( GL_COLOR_BUFFER_BIT );
	ImGui_ImplOpenGL2_RenderDrawData( ImGui::GetDrawData() );
	SwapBuffers( dc );
	RestoreEngineContext();
	rendering = false;
	ImGui::SetCurrentContext( previousContext );
}

void DoomScriptBlueprintImGuiEditor::RenderEditor() {
	ImGuiIO &io = ImGui::GetIO();
	ImGui::SetNextWindowPos( ImVec2( 0, 0 ) );
	ImGui::SetNextWindowSize( io.DisplaySize );
	ImGui::PushStyleVar( ImGuiStyleVar_WindowPadding, ImVec2( 8.0f * uiScale, 8.0f * uiScale ) );
	ImGui::Begin( "DoomScript Blueprint Root", NULL, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
		ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings );
	RenderToolbar();
	ImGui::Separator();
	float statusHeight = ImGui::GetFrameHeightWithSpacing() + 4.0f * uiScale;
	float panelHeight = ImGui::GetContentRegionAvail().y - statusHeight;
	ImGui::BeginChild( "Scopes", ImVec2( 292.0f * uiScale, panelHeight ), true );
	RenderScopePanel();
	ImGui::EndChild();
	ImGui::SameLine();
	float rightWidth = 310.0f * uiScale;
	float graphWidth = ImGui::GetContentRegionAvail().x - rightWidth - ImGui::GetStyle().ItemSpacing.x;
	ImGui::BeginChild( "GraphPane", ImVec2( graphWidth, panelHeight ), true,
		ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse );
	RenderGraph();
	ImGui::EndChild();
	ImGui::SameLine();
	ImGui::BeginChild( "Palette", ImVec2( 0, panelHeight ), true );
	RenderPalettePanel();
	ImGui::EndChild();
	RenderStatusBar();
	RenderNodeEditor();
	ImGui::End();
	ImGui::PopStyleVar();
}

void DoomScriptBlueprintImGuiEditor::RenderToolbar() {
	if ( ImGui::Button( "Save DoomScript" ) ) Save();
	ImGui::SameLine();
	if ( ImGui::Button( "Regenerate all metadata" ) ) MigrateAll();
	ImGui::SameLine();
	if ( ImGui::Button( "Frame Event" ) ) FrameFunction();
	ImGui::SameLine();
	ImGui::TextDisabled( "Drag nodes with left mouse  |  Pan with right mouse  |  Zoom with wheel  |  Double-click to edit" );
	if ( document.IsDirty() ) {
		ImGui::SameLine();
		ImGui::TextColored( ImVec4( 1.0f, 0.72f, 0.22f, 1.0f ), "UNSAVED" );
	}
}

void DoomScriptBlueprintImGuiEditor::RenderScopePanel() {
	ImGui::TextUnformatted( "DoomScript files" );
	ImGui::BeginChild( "ScriptList", ImVec2( 0, 150.0f * uiScale ), true );
	for ( int index = 0; index < scriptFiles.Num(); index++ ) {
		if ( ImGui::Selectable( scriptFiles[index].c_str(), selectedScript == index ) && index != selectedScript ) {
			if ( ConfirmDiscardOrSave() ) LoadScript( index );
		}
	}
	ImGui::EndChild();

	ImGui::TextUnformatted( "Events (one graph per function)" );
	ImGui::BeginChild( "EventList", ImVec2( 0, 130.0f * uiScale ), true );
	for ( int index = 0; index < document.Functions().Num(); index++ ) {
		idStr label = "Event " + document.Functions()[index].name;
		if ( ImGui::Selectable( label.c_str(), selectedFunction == index ) ) {
			selectedFunction = index;
			selectedLocal = -1;
			selectedNode = -1;
			FrameFunction();
		}
	}
	ImGui::EndChild();

	if ( ImGui::CollapsingHeader( "File / object variables", ImGuiTreeNodeFlags_DefaultOpen ) ) {
		ImGui::BeginChild( "GlobalList", ImVec2( 0, 95.0f * uiScale ), true );
		for ( int index = 0; index < document.Globals().Num(); index++ ) {
			idStr label = VariableLabel( document.Globals()[index] );
			if ( ImGui::Selectable( label.c_str(), selectedGlobal == index ) ) {
				selectedGlobal = index; selectedLocal = selectedShared = -1;
			}
		}
		ImGui::EndChild();
	}
	if ( ImGui::CollapsingHeader( "Event-local variables", ImGuiTreeNodeFlags_DefaultOpen ) ) {
		ImGui::BeginChild( "LocalList", ImVec2( 0, 95.0f * uiScale ), true );
		if ( selectedFunction >= 0 && selectedFunction < document.Functions().Num() ) {
			const idList<doomScriptVariable_t> &locals = document.Functions()[selectedFunction].locals;
			for ( int index = 0; index < locals.Num(); index++ ) {
				idStr label = VariableLabel( locals[index] );
				if ( ImGui::Selectable( label.c_str(), selectedLocal == index ) ) {
					selectedLocal = index; selectedGlobal = selectedShared = -1;
				}
			}
		}
		ImGui::EndChild();
	}
	if ( ImGui::CollapsingHeader( "Inherited / shared object fields" ) ) {
		ImGui::BeginChild( "SharedList", ImVec2( 0, 125.0f * uiScale ), true );
		for ( int index = 0; index < document.SharedVariables().Num(); index++ ) {
			idStr label = VariableLabel( document.SharedVariables()[index] );
			if ( ImGui::Selectable( label.c_str(), selectedShared == index ) ) {
				selectedShared = index; selectedGlobal = selectedLocal = -1;
			}
		}
		ImGui::EndChild();
	}

	if ( ImGui::Button( "Add Set node for selected variable", ImVec2( -FLT_MIN, 0 ) ) ) AddSelectedVariableSetNode();
	ImGui::SeparatorText( "Declare variable" );
	static const char *types[] = { "boolean", "float", "integer", "string", "vector", "entity" };
	ImGui::SetNextItemWidth( 92.0f * uiScale );
	ImGui::Combo( "##VariableType", &variableType, types, sizeof( types ) / sizeof( types[0] ) );
	ImGui::SameLine();
	ImGui::SetNextItemWidth( -FLT_MIN );
	ImGui::InputTextWithHint( "##VariableName", "name", variableName, sizeof( variableName ) );
	ImGui::InputTextWithHint( "##VariableDefault", "optional initial value", variableDefault, sizeof( variableDefault ) );
	if ( ImGui::Button( "Add file/object", ImVec2( 132.0f * uiScale, 0 ) ) ) {
		idStr error;
		if ( document.AddVariable( true, -1, types[variableType], variableName, variableDefault, catalog, error ) ) {
			variableName[0] = variableDefault[0] = '\0'; SetStatus( "Declared file/object variable." );
		} else SetStatus( error.c_str() );
	}
	ImGui::SameLine();
	if ( ImGui::Button( "Add event-local", ImVec2( -FLT_MIN, 0 ) ) ) {
		idStr error;
		if ( document.AddVariable( false, selectedFunction, types[variableType], variableName, variableDefault, catalog, error ) ) {
			variableName[0] = variableDefault[0] = '\0'; SetStatus( "Declared Event-local variable." );
		} else SetStatus( error.c_str() );
	}
	if ( ImGui::Button( "Remove selected declaration", ImVec2( -FLT_MIN, 0 ) ) ) {
		idStr error;
		bool global = selectedGlobal >= 0;
		int index = global ? selectedGlobal : selectedLocal;
		if ( selectedShared >= 0 ) SetStatus( "Inherited fields must be removed from the object that declares them." );
		else if ( document.RemoveVariable( global, selectedFunction, index, catalog, error ) ) {
			selectedGlobal = selectedLocal = -1; SetStatus( "Removed variable declaration." );
		} else SetStatus( error.c_str() );
	}
}

void DoomScriptBlueprintImGuiEditor::RenderPalettePanel() {
	ImGui::TextUnformatted( "Callable functions" );
	ImGui::InputTextWithHint( "##PaletteFilter", "filter by name, category, or keyword", paletteFilter, sizeof( paletteFilter ) );
	float detailHeight = 275.0f * uiScale;
	ImGui::BeginChild( "PaletteList", ImVec2( 0, ImGui::GetContentRegionAvail().y - detailHeight ), true );
	for ( int index = 0; index < catalog.Nodes().Num(); index++ ) {
		const doomScriptFunctionNode_t &node = catalog.Nodes()[index];
		idStr searchable = node.category + " " + node.title + " " + node.command + " " + node.keywords;
		if ( paletteFilter[0] != '\0' && searchable.Find( paletteFilter, false ) < 0 ) continue;
		idStr label = node.category + " | " + node.title;
		if ( ImGui::Selectable( label.c_str(), selectedPalette == index ) ) selectedPalette = index;
	}
	ImGui::EndChild();
	if ( selectedPalette >= 0 && selectedPalette < catalog.Nodes().Num() ) {
		const doomScriptFunctionNode_t &node = catalog.Nodes()[selectedPalette];
		ImGui::TextColored( ImVec4( 0.42f, 0.78f, 1.0f, 1.0f ), "%s", node.title.c_str() );
		ImGui::TextWrapped( "%s", node.description.c_str() );
		ImGui::TextDisabled( "%s.%s  -> %s", node.receiver.c_str(), node.command.c_str(), node.returnType.c_str() );
		for ( int pin = 0; pin < node.pins.Num(); pin++ ) ImGui::BulletText( "%s %s", PinTypeLabel( node.pins[pin] ), node.pins[pin].name.c_str() );
		if ( ImGui::Button( "Add Function Call", ImVec2( -FLT_MIN, 0 ) ) ) {
			idStr error;
			if ( document.InsertFunctionCall( selectedFunction, node, catalog, error ) ) SetStatus( "Added editable Function Call. Save to emit DoomScript." );
			else SetStatus( error.c_str() );
		}
	} else {
		ImGui::TextWrapped( "Select a function to see its typed parameters and return value." );
	}
	ImGui::SeparatorText( "Condition builder" );
	if ( FindConditionOwner( selectedNode ) >= 0 ) {
		if ( ImGui::Button( "Edit / add selected conditions", ImVec2( -FLT_MIN, 0 ) ) ) OpenNodeEditor( selectedNode );
	} else {
		ImGui::TextDisabled( "Select a Branch, Loop, or Evaluate Condition node." );
	}
}

ImVec2 DoomScriptBlueprintImGuiEditor::NodeSize( const doomScriptGraphNode_t &node ) const {
	int rows = Max( node.inputs.Num(), node.outputs.Num() );
	float width = node.dataOnly ? 220.0f : 280.0f;
	float canvasScale = uiScale * zoom;
	return ImVec2( width * canvasScale, ( 64.0f + rows * 22.0f ) * canvasScale );
}

ImVec2 DoomScriptBlueprintImGuiEditor::NodePosition( const ImVec2 &origin, const doomScriptGraphNode_t &node ) const {
	float canvasScale = uiScale * zoom;
	return ImVec2( origin.x + pan.x + node.x * canvasScale, origin.y + pan.y + node.y * canvasScale );
}

ImVec2 DoomScriptBlueprintImGuiEditor::InputPinPosition( const ImVec2 &origin, const doomScriptGraphNode_t &node, int pin, bool execution ) const {
	ImVec2 position = NodePosition( origin, node );
	return ImVec2( position.x, position.y + ( execution ? 43.0f : 61.0f + pin * 22.0f ) * uiScale * zoom );
}

ImVec2 DoomScriptBlueprintImGuiEditor::OutputPinPosition( const ImVec2 &origin, const doomScriptGraphNode_t &node, int pin, bool execution ) const {
	ImVec2 position = NodePosition( origin, node );
	ImVec2 size = NodeSize( node );
	float offset = execution ? 43.0f + Max( 0, pin ) * 22.0f : 61.0f + pin * 22.0f;
	return ImVec2( position.x + size.x, position.y + offset * uiScale * zoom );
}

void DoomScriptBlueprintImGuiEditor::DrawLink( ImDrawList *draw, const ImVec2 &from, const ImVec2 &to, ImU32 color, float thickness ) const {
	float tangent = Max( 55.0f * uiScale, fabsf( to.x - from.x ) * 0.45f );
	draw->AddBezierCubic( from, ImVec2( from.x + tangent, from.y ), ImVec2( to.x - tangent, to.y ), to, color, thickness * uiScale * zoom );
}

void DoomScriptBlueprintImGuiEditor::RenderGraph() {
	ImVec2 origin = ImGui::GetCursorScreenPos();
	ImVec2 available = ImGui::GetContentRegionAvail();
	if ( available.x < 50 || available.y < 50 ) return;
	// The background covers the full graph, but nodes and pins are submitted
	// afterward and must win hit testing wherever they overlap it.
	ImGui::SetNextItemAllowOverlap();
	ImGui::InvisibleButton( "GraphCanvas", available, ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight );
	bool canvasHovered = ImGui::IsItemHovered();
	bool graphHovered = ImGui::IsWindowHovered( ImGuiHoveredFlags_ChildWindows );
	ImDrawList *draw = ImGui::GetWindowDrawList();
	draw->PushClipRect( origin, ImVec2( origin.x + available.x, origin.y + available.y ), true );

	float canvasScale = uiScale * zoom;
	float grid = 48.0f * canvasScale;
	if ( grid >= 12.0f * uiScale ) {
		float offsetX = fmodf( pan.x, grid );
		float offsetY = fmodf( pan.y, grid );
		for ( float x = offsetX; x < available.x; x += grid ) draw->AddLine( ImVec2( origin.x + x, origin.y ), ImVec2( origin.x + x, origin.y + available.y ), IM_COL32( 55, 61, 71, 120 ) );
		for ( float y = offsetY; y < available.y; y += grid ) draw->AddLine( ImVec2( origin.x, origin.y + y ), ImVec2( origin.x + available.x, origin.y + y ), IM_COL32( 55, 61, 71, 120 ) );
	}

	ImVec2 mousePosition = ImGui::GetIO().MousePos;
	bool mouseInsideGraph = mousePosition.x >= origin.x && mousePosition.y >= origin.y &&
		mousePosition.x < origin.x + available.x && mousePosition.y < origin.y + available.y;
	if ( mouseInsideGraph && ImGui::IsMouseClicked( ImGuiMouseButton_Right ) ) graphPanning = true;
	if ( !ImGui::IsMouseDown( ImGuiMouseButton_Right ) ) graphPanning = false;
	if ( graphPanning ) pan = ImVec2( pan.x + ImGui::GetIO().MouseDelta.x, pan.y + ImGui::GetIO().MouseDelta.y );
	if ( graphHovered && ImGui::GetIO().MouseWheel != 0.0f ) {
		float oldZoom = zoom;
		zoom = Min( 1.65f, Max( 0.45f, zoom + ImGui::GetIO().MouseWheel * 0.10f ) );
		ImVec2 mouse = ImGui::GetIO().MousePos;
		pan.x = mouse.x - origin.x - ( mouse.x - origin.x - pan.x ) * zoom / oldZoom;
		pan.y = mouse.y - origin.y - ( mouse.y - origin.y - pan.y ) * zoom / oldZoom;
	}

	const idList<doomScriptGraphNode_t> &nodes = document.Nodes();
	const idList<doomScriptGraphLink_t> &links = document.Links();
	for ( int index = 0; index < links.Num(); index++ ) {
		const doomScriptGraphLink_t &link = links[index];
		if ( link.from < 0 || link.to < 0 || link.from >= nodes.Num() || link.to >= nodes.Num() ) continue;
		if ( nodes[link.from].functionIndex != selectedFunction || nodes[link.to].functionIndex != selectedFunction ) continue;
		ImVec2 from = OutputPinPosition( origin, nodes[link.from], link.fromPin, link.execution );
		ImVec2 to = InputPinPosition( origin, nodes[link.to], link.toPin, link.execution );
		ImU32 color = link.execution ? IM_COL32( 195, 202, 215, 220 ) : PinColor( nodes[link.from].outputs[link.fromPin].type );
		DrawLink( draw, from, to, color, link.execution ? 2.1f : 3.0f );
	}

	for ( int index = 0; index < nodes.Num(); index++ ) {
		const doomScriptGraphNode_t &node = nodes[index];
		if ( node.functionIndex != selectedFunction ) continue;
		ImVec2 position = NodePosition( origin, node );
		ImVec2 size = NodeSize( node );
		ImVec2 maximum( position.x + size.x, position.y + size.y );
		bool selected = selectedNode == index;
		draw->AddRectFilled( position, maximum, IM_COL32( 38, 41, 47, 252 ), 5.0f * canvasScale );
		draw->AddRectFilled( position, ImVec2( maximum.x, position.y + 31.0f * canvasScale ), HeaderColor( node ), 5.0f * canvasScale, ImDrawFlags_RoundCornersTop );
		draw->AddRect( position, maximum, selected ? IM_COL32( 35, 157, 255, 255 ) : IM_COL32( 101, 109, 122, 255 ), 5.0f * canvasScale, 0, ( selected ? 2.3f : 1.0f ) * uiScale );
		draw->AddText( ImVec2( position.x + 10.0f * canvasScale, position.y + 7.0f * canvasScale ), IM_COL32_WHITE, node.title.c_str() );
		idStr subtitle = GraphKindLabel( node );
		if ( node.line > 0 ) subtitle += va( "  | line %d", node.line );
		draw->AddText( ImVec2( position.x + 10.0f * canvasScale, position.y + 36.0f * canvasScale ), IM_COL32( 190, 196, 206, 255 ), subtitle.c_str() );

		ImGui::SetCursorScreenPos( position );
		ImGui::PushID( index );
		// Pins are smaller controls placed on top of the node body.
		ImGui::SetNextItemAllowOverlap();
		ImGui::InvisibleButton( "Node", size, ImGuiButtonFlags_MouseButtonLeft );
		if ( ImGui::IsItemClicked() ) selectedNode = index;
		if ( ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked( ImGuiMouseButton_Left ) ) OpenNodeEditor( index );
		if ( ImGui::IsItemActive() && ImGui::IsMouseDragging( ImGuiMouseButton_Left, 1.0f ) ) {
			int x = node.x + (int)( ImGui::GetIO().MouseDelta.x / canvasScale );
			int y = node.y + (int)( ImGui::GetIO().MouseDelta.y / canvasScale );
			document.SetNodePosition( index, x, y );
		}

		if ( !node.dataOnly && node.kind != "event" ) {
			ImVec2 executeInput = InputPinPosition( origin, node, 0, true );
			draw->AddCircleFilled( executeInput, 5.0f * canvasScale, IM_COL32( 205, 211, 222, 255 ) );
		}
		int executionOutputs = ExecutionOutputCount( node );
		for ( int executePin = 0; executePin < executionOutputs; executePin++ ) {
			ImVec2 executeOutput = OutputPinPosition( origin, node, executePin, true );
			draw->AddCircleFilled( executeOutput, 5.0f * canvasScale, IM_COL32( 205, 211, 222, 255 ) );
			const char *label = ExecutionOutputLabel( node, executePin );
			if ( label[0] != '\0' ) {
				ImVec2 labelSize = ImGui::CalcTextSize( label );
				draw->AddText( ImVec2( executeOutput.x - labelSize.x - 9.0f * canvasScale,
					executeOutput.y - 7.0f * canvasScale ), IM_COL32( 220, 224, 232, 255 ), label );
			}
		}

		for ( int pin = 0; pin < node.inputs.Num(); pin++ ) {
			ImVec2 pinPosition = InputPinPosition( origin, node, pin, false );
			draw->AddCircleFilled( pinPosition, 5.0f * canvasScale, PinColor( node.inputs[pin].type ) );
			idStr inputLabel = node.inputs[pin].name;
			if ( node.kind.Find( "function:" ) == 0 && pin < node.inputValues.Num() ) inputLabel += " = " + node.inputValues[pin];
			if ( inputLabel.Length() > 34 ) inputLabel = inputLabel.Left( 31 ) + "...";
			draw->AddText( ImVec2( pinPosition.x + 9.0f * canvasScale, pinPosition.y - 7.0f * canvasScale ), IM_COL32( 220, 224, 232, 255 ), inputLabel.c_str() );
			ImGui::SetCursorScreenPos( ImVec2( pinPosition.x - 8.0f * canvasScale, pinPosition.y - 8.0f * canvasScale ) );
			ImGui::PushID( pin );
			ImGui::InvisibleButton( "Input", ImVec2( 16.0f * canvasScale, 16.0f * canvasScale ) );
			if ( ImGui::BeginDragDropTarget() ) {
				const ImGuiPayload *payload = ImGui::AcceptDragDropPayload( "DSBP_DATA_PIN" );
				if ( payload != NULL && payload->DataSize == sizeof( BlueprintPinPayload ) ) {
					const BlueprintPinPayload *source = (const BlueprintPinPayload *)payload->Data;
					idStr error;
					if ( document.ConnectDataPins( source->node, source->pin, index, pin, catalog, error ) ) SetStatus( "Connected typed variable/value pin and updated DoomScript." );
					else SetStatus( error.c_str() );
				}
				ImGui::EndDragDropTarget();
			}
			ImGui::PopID();
		}
		for ( int pin = 0; pin < node.outputs.Num(); pin++ ) {
			ImVec2 pinPosition = OutputPinPosition( origin, node, pin, false );
			draw->AddCircleFilled( pinPosition, 5.0f * canvasScale, PinColor( node.outputs[pin].type ) );
			ImVec2 textSize = ImGui::CalcTextSize( node.outputs[pin].name.c_str() );
			draw->AddText( ImVec2( pinPosition.x - textSize.x - 9.0f * canvasScale, pinPosition.y - 7.0f * canvasScale ), IM_COL32( 220, 224, 232, 255 ), node.outputs[pin].name.c_str() );
			ImGui::SetCursorScreenPos( ImVec2( pinPosition.x - 8.0f * canvasScale, pinPosition.y - 8.0f * canvasScale ) );
			ImGui::PushID( 1000 + pin );
			ImGui::InvisibleButton( "Output", ImVec2( 16.0f * canvasScale, 16.0f * canvasScale ) );
			if ( ImGui::BeginDragDropSource( ImGuiDragDropFlags_SourceNoPreviewTooltip ) ) {
				BlueprintPinPayload payload = { index, pin };
				ImGui::SetDragDropPayload( "DSBP_DATA_PIN", &payload, sizeof( payload ) );
				ImGui::Text( "%s %s", node.outputs[pin].type.c_str(), node.outputs[pin].name.c_str() );
				ImGui::EndDragDropSource();
			}
			ImGui::PopID();
		}
		ImGui::PopID();
	}
	if ( canvasHovered && ImGui::IsMouseDoubleClicked( ImGuiMouseButton_Left ) ) selectedNode = -1;
	draw->PopClipRect();
}

void DoomScriptBlueprintImGuiEditor::BuildVariableChoices( int functionIndex ) {
	editVariables.Clear();
	if ( functionIndex >= 0 && functionIndex < document.Functions().Num() ) {
		const doomScriptGraphFunction_t &function = document.Functions()[functionIndex];
		for ( int index = 0; index < function.parameters.Num(); index++ ) AddVariableChoice( function.parameters[index].type, function.parameters[index].name );
		for ( int index = 0; index < function.locals.Num(); index++ ) AddVariableChoice( function.locals[index].type, function.locals[index].name );
	}
	for ( int index = 0; index < document.Globals().Num(); index++ ) AddVariableChoice( document.Globals()[index].type, document.Globals()[index].name );
	for ( int index = 0; index < document.SharedVariables().Num(); index++ ) AddVariableChoice( document.SharedVariables()[index].type, document.SharedVariables()[index].name );
}

void DoomScriptBlueprintImGuiEditor::AddVariableChoice( const char *type, const char *name ) {
	for ( int index = 0; index < editVariables.Num(); index++ ) if ( editVariables[index].name.Icmp( name ) == 0 ) return;
	BlueprintVariableChoice &choice = editVariables.Alloc();
	choice.type = type;
	choice.name = name;
}

int DoomScriptBlueprintImGuiEditor::FindConditionOwner( int nodeIndex ) const {
	if ( nodeIndex < 0 || nodeIndex >= document.Nodes().Num() ) return -1;
	const doomScriptGraphNode_t &node = document.Nodes()[nodeIndex];
	if ( node.kind == "branch" || node.kind == "loop" ) return nodeIndex;
	if ( node.kind != "expression" && node.kind != "literal" ) return -1;
	for ( int index = 0; node.ownerStableId.Length() != 0 && index < document.Nodes().Num(); index++ ) {
		const doomScriptGraphNode_t &owner = document.Nodes()[index];
		if ( owner.stableId == node.ownerStableId && ( owner.kind == "branch" || owner.kind == "loop" ) ) return index;
	}
	return -1;
}

void DoomScriptBlueprintImGuiEditor::AddConditionClause() {
	BlueprintConditionClause &clause = editConditions.Alloc();
	clause.join = 0;
	clause.variable = editVariables.Num() != 0 ? 0 : -1;
	clause.operation = clause.variable >= 0 && editVariables[clause.variable].type == "boolean" ? 0 : 2;
	clause.valueSource = -1;
	clause.value = clause.operation >= 2 ? "0" : "true";
}

void DoomScriptBlueprintImGuiEditor::SetConditionError( const char *text ) {
	editConditionError = text == NULL ? "" : text;
	SetStatus( editConditionError.c_str() );
}

void DoomScriptBlueprintImGuiEditor::OpenNodeEditor( int nodeIndex ) {
	if ( nodeIndex < 0 || nodeIndex >= document.Nodes().Num() ) return;
	const doomScriptGraphNode_t &node = document.Nodes()[nodeIndex];
	editNode = nodeIndex;
	editStableId = node.stableId;
	BuildVariableChoices( node.functionIndex );
	editInputSources.Clear();
	editInputValues.Clear();
	for ( int input = 0; input < node.inputs.Num(); input++ ) {
		idStr value = input < node.inputValues.Num() ? node.inputValues[input] : "";
		int source = -1;
		for ( int variableIndex = 0; variableIndex < editVariables.Num(); variableIndex++ ) {
			if ( value == editVariables[variableIndex].name ) { source = variableIndex; break; }
		}
		editInputSources.Append( source );
		editInputValues.Append( value );
	}
	editVariable = 0;
	editOperation = 0;
	editValueSource = -1;
	editConditions.Clear();
	editConditionError.Clear();
	editConditionBuilder = false;
	CopyText( editValue, sizeof( editValue ), node.expression );
	int conditionOwner = FindConditionOwner( nodeIndex );
	if ( conditionOwner >= 0 ) {
		editConditionBuilder = true;
		if ( !ParseConditionExpression( document.Nodes()[conditionOwner].expression, editVariables, editConditions ) ) {
			editConditionError = "This imported condition contains grouping or operands that could not be decomposed into variable comparisons. Add condition rows below to replace it visually.";
		}
	}
	for ( int index = 0; index < editVariables.Num(); index++ ) {
		if ( ( node.kind == "setvar" && node.variableName == editVariables[index].name ) ||
			( node.kind != "setvar" && ( node.expression == editVariables[index].name || node.expression == "!" + editVariables[index].name ) ) ) {
			editVariable = index;
			break;
		}
	}
	if ( !editConditionBuilder && ( node.kind == "branch" || node.kind == "loop" ) ) {
		idStr variable = editVariables.Num() ? editVariables[editVariable].name : "";
		if ( node.expression == variable ) editOperation = 0;
		else if ( node.expression == "!" + variable ) editOperation = 1;
		else {
			const char *operators[] = { "==", "!=", "<=", ">=", "<", ">" };
			const int operations[] = { 2, 3, 5, 7, 4, 6 };
			for ( int operatorIndex = 0; operatorIndex < 6; operatorIndex++ ) {
				int found = node.expression.Find( operators[operatorIndex] );
				if ( found < 0 ) continue;
				idStr left = node.expression.Left( found ); left.StripTrailingWhitespace();
				for ( int variableIndex = 0; variableIndex < editVariables.Num(); variableIndex++ ) if ( left == editVariables[variableIndex].name ) editVariable = variableIndex;
				idStr right = node.expression.Mid( found + (int)strlen( operators[operatorIndex] ), node.expression.Length() - found - (int)strlen( operators[operatorIndex] ) );
				while ( right.Length() && isspace( (unsigned char)right[0] ) ) right = right.Mid( 1, right.Length() - 1 );
				CopyText( editValue, sizeof( editValue ), right );
				editOperation = operations[operatorIndex];
				for ( int variableIndex = 0; variableIndex < editVariables.Num(); variableIndex++ ) if ( right == editVariables[variableIndex].name ) editValueSource = variableIndex;
				break;
			}
		}
	} else if ( node.kind == "setvar" ) {
		for ( int index = 0; index < editVariables.Num(); index++ ) if ( node.expression == editVariables[index].name ) editValueSource = index;
	}
	openEditPopup = true;
}

void DoomScriptBlueprintImGuiEditor::RenderNodeEditor() {
	if ( openEditPopup ) {
		ImGui::OpenPopup( "Edit Blueprint Node" );
		openEditPopup = false;
	}
	// AlwaysAutoResize feeds wrapped content dimensions back into the popup on
	// every frame, which makes a newly opened dialog visibly shrink. Give each
	// editor a stable initial viewport and let normal window scrolling handle
	// nodes with more properties than fit vertically.
	ImVec2 dialogSize = editConditionBuilder ? ImVec2( 820.0f * uiScale, 600.0f * uiScale ) :
		ImVec2( 610.0f * uiScale, 520.0f * uiScale );
	ImGui::SetNextWindowSize( dialogSize, ImGuiCond_Appearing );
	if ( !ImGui::BeginPopupModal( "Edit Blueprint Node", NULL, ImGuiWindowFlags_None ) ) return;
	int nodeIndex = FindNodeByStableId( editStableId );
	if ( nodeIndex < 0 ) {
		ImGui::TextUnformatted( "This node changed while the editor was open." );
		if ( ImGui::Button( "Close" ) ) ImGui::CloseCurrentPopup();
		ImGui::EndPopup();
		return;
	}
	const doomScriptGraphNode_t &node = document.Nodes()[nodeIndex];
	ImGui::TextColored( ImVec4( 0.44f, 0.80f, 1.0f, 1.0f ), "%s", node.title.c_str() );
	ImGui::TextDisabled( "%s | line %d", GraphKindLabel( node ), node.line );
	ImGui::Separator();
	bool editable = editConditionBuilder || node.kind.Find( "function:" ) == 0 ||
		( node.kind == "setvar" && editVariables.Num() > 0 );
	if ( editConditionBuilder ) {
		ImGui::TextWrapped( "Each row is a typed condition. Choose scoped variables and comparisons; the editor generates the complete DoomScript condition and its data-pin graph." );
		if ( editConditionError.Length() != 0 ) {
			ImGui::TextColored( ImVec4( 1.0f, 0.67f, 0.28f, 1.0f ), "%s", editConditionError.c_str() );
		}
		static const char *joins[] = { "AND", "OR" };
		static const char *operations[] = { "Is True", "Is False", "==", "!=", "<", "<=", ">", ">=" };
		int removeClause = -1;
		ImGui::BeginChild( "ConditionRows", ImVec2( 0, 360.0f * uiScale ), true );
		for ( int clauseIndex = 0; clauseIndex < editConditions.Num(); clauseIndex++ ) {
			BlueprintConditionClause &clause = editConditions[clauseIndex];
			ImGui::PushID( 7000 + clauseIndex );
			if ( clauseIndex > 0 ) {
				ImGui::SetNextItemWidth( 105.0f * uiScale );
				ImGui::Combo( "##Join", &clause.join, joins, 2 );
			}
			ImGui::SeparatorText( va( "Condition %d", clauseIndex + 1 ) );
			const char *variablePreview = clause.variable >= 0 && clause.variable < editVariables.Num()
				? editVariables[clause.variable].name.c_str() : "Choose variable";
			ImGui::SetNextItemWidth( 250.0f * uiScale );
			if ( ImGui::BeginCombo( "##ConditionVariable", variablePreview ) ) {
				for ( int variableIndex = 0; variableIndex < editVariables.Num(); variableIndex++ ) {
					idStr label = editVariables[variableIndex].name + " : " + editVariables[variableIndex].type;
					if ( ImGui::Selectable( label.c_str(), clause.variable == variableIndex ) ) {
						clause.variable = variableIndex;
						if ( clause.valueSource >= 0 && !ConditionTypesCompatible( editVariables[variableIndex].type,
							editVariables[clause.valueSource].type ) ) clause.valueSource = -1;
					}
				}
				ImGui::EndCombo();
			}
			ImGui::SameLine();
			ImGui::SetNextItemWidth( 135.0f * uiScale );
			ImGui::Combo( "##ConditionOperation", &clause.operation, operations, 8 );
			if ( clause.operation >= 2 ) {
				ImGui::SameLine();
				const char *rightPreview = clause.valueSource >= 0 && clause.valueSource < editVariables.Num()
					? editVariables[clause.valueSource].name.c_str() : "Constant value";
				ImGui::SetNextItemWidth( 230.0f * uiScale );
				if ( ImGui::BeginCombo( "##ConditionValueSource", rightPreview ) ) {
					if ( ImGui::Selectable( "Constant value", clause.valueSource < 0 ) ) clause.valueSource = -1;
					for ( int variableIndex = 0; variableIndex < editVariables.Num(); variableIndex++ ) {
						if ( clause.variable >= 0 && clause.variable < editVariables.Num() &&
							!ConditionTypesCompatible( editVariables[clause.variable].type, editVariables[variableIndex].type ) ) continue;
						idStr label = editVariables[variableIndex].name + " : " + editVariables[variableIndex].type;
						if ( ImGui::Selectable( label.c_str(), clause.valueSource == variableIndex ) ) clause.valueSource = variableIndex;
					}
					ImGui::EndCombo();
				}
				if ( clause.valueSource < 0 ) {
					if ( clause.variable >= 0 && clause.variable < editVariables.Num() && editVariables[clause.variable].type == "boolean" ) {
						int booleanValue = clause.value.Icmp( "false" ) == 0 || clause.value == "0" ? 0 : 1;
						ImGui::SetNextItemWidth( 150.0f * uiScale );
						if ( ImGui::Combo( "Constant##Boolean", &booleanValue, "false\0true\0" ) ) clause.value = booleanValue ? "true" : "false";
					} else {
						char constantValue[256];
						CopyText( constantValue, sizeof( constantValue ), clause.value );
						ImGui::SetNextItemWidth( 300.0f * uiScale );
						if ( ImGui::InputTextWithHint( "Constant value", "number, text, or entity value", constantValue, sizeof( constantValue ) ) ) clause.value = constantValue;
					}
				}
			}
			ImGui::SameLine();
			if ( ImGui::SmallButton( "Remove" ) ) removeClause = clauseIndex;
			ImGui::PopID();
		}
		if ( editConditions.Num() == 0 ) ImGui::TextDisabled( "No conditions. Add a row to build this Branch/Loop condition." );
		ImGui::EndChild();
		if ( removeClause >= 0 ) editConditions.RemoveIndex( removeClause );
		if ( ImGui::Button( "+ Add condition", ImVec2( 170.0f * uiScale, 0 ) ) ) {
			AddConditionClause();
			editConditionError.Clear();
		}
	} else if ( node.kind == "setvar" ) {
		ImGui::TextWrapped( "Choose the variable to set, then connect another variable or provide a typed literal value." );
		const char *preview = editVariables.Num() ? editVariables[editVariable].name.c_str() : "No variables in scope";
		if ( ImGui::BeginCombo( "Set variable", preview ) ) {
			for ( int index = 0; index < editVariables.Num(); index++ ) {
				idStr label = editVariables[index].name + " : " + editVariables[index].type;
				if ( ImGui::Selectable( label.c_str(), editVariable == index ) ) editVariable = index;
			}
			ImGui::EndCombo();
		}
		const char *valuePreview = editValueSource >= 0 && editValueSource < editVariables.Num() ? editVariables[editValueSource].name.c_str() : "Typed value";
		if ( ImGui::BeginCombo( "Value source", valuePreview ) ) {
			if ( ImGui::Selectable( "Typed value", editValueSource < 0 ) ) editValueSource = -1;
			for ( int index = 0; index < editVariables.Num(); index++ ) {
				idStr label = editVariables[index].name + " : " + editVariables[index].type;
				if ( ImGui::Selectable( label.c_str(), editValueSource == index ) ) editValueSource = index;
			}
			ImGui::EndCombo();
		}
		if ( editValueSource < 0 ) ImGui::InputTextWithHint( "Typed value", "value only (not a statement)", editValue, sizeof( editValue ) );
	} else if ( node.kind.Find( "function:" ) == 0 ) {
		ImGui::TextWrapped( "Every parameter is an editable Blueprint pin. Choose a variable connection or set the pin's unconnected default/constant value." );
		for ( int input = 0; input < node.inputs.Num(); input++ ) {
			ImGui::PushID( 4000 + input );
			ImGui::SeparatorText( node.inputs[input].name.c_str() );
			ImGui::TextDisabled( "%s", PinTypeLabel( node.inputs[input] ) );
			ImGui::SameLine( 105.0f * uiScale );
			int source = input < editInputSources.Num() ? editInputSources[input] : -1;
			const char *preview = source >= 0 && source < editVariables.Num() ? editVariables[source].name.c_str() : "Pin default / constant";
			ImGui::SetNextItemWidth( -FLT_MIN );
			if ( ImGui::BeginCombo( "##InputSource", preview ) ) {
				if ( ImGui::Selectable( "Pin default / constant", source < 0 ) && input < editInputSources.Num() ) editInputSources[input] = -1;
				for ( int variableIndex = 0; variableIndex < editVariables.Num(); variableIndex++ ) {
					bool numeric = ( node.inputs[input].type == "float" || node.inputs[input].type == "integer" || node.inputs[input].type == "boolean" ) &&
						( editVariables[variableIndex].type == "float" || editVariables[variableIndex].type == "integer" || editVariables[variableIndex].type == "boolean" );
					if ( node.inputs[input].type != editVariables[variableIndex].type && !numeric ) continue;
					idStr label = editVariables[variableIndex].name + " : " + editVariables[variableIndex].type;
					if ( ImGui::Selectable( label.c_str(), source == variableIndex ) && input < editInputSources.Num() ) editInputSources[input] = variableIndex;
				}
				ImGui::EndCombo();
			}
			if ( input < editInputSources.Num() && editInputSources[input] < 0 ) {
				if ( node.inputs[input].enumValues.Num() != 0 ) {
					const char *enumPreview = input < editInputValues.Num() && editInputValues[input].Length() != 0
						? editInputValues[input].c_str()
						: node.inputs[input].enumValues[0].c_str();
					ImGui::SetNextItemWidth( -FLT_MIN );
					if ( ImGui::BeginCombo( "##InputDefaultEnum", enumPreview ) ) {
						for ( int valueIndex = 0; valueIndex < node.inputs[input].enumValues.Num(); valueIndex++ ) {
							const idStr &enumValue = node.inputs[input].enumValues[valueIndex];
							bool selected = input < editInputValues.Num() && editInputValues[input] == enumValue;
							if ( ImGui::Selectable( enumValue.c_str(), selected ) && input < editInputValues.Num() ) editInputValues[input] = enumValue;
							if ( selected ) ImGui::SetItemDefaultFocus();
						}
						ImGui::EndCombo();
					}
				} else {
					char inputValue[512];
					CopyText( inputValue, sizeof( inputValue ), input < editInputValues.Num() ? editInputValues[input] : idStr() );
					ImGui::SetNextItemWidth( -FLT_MIN );
					if ( ImGui::InputTextWithHint( "##InputDefault", "unconnected pin value", inputValue, sizeof( inputValue ) ) && input < editInputValues.Num() ) editInputValues[input] = inputValue;
				}
			}
			ImGui::PopID();
		}
		if ( node.outputs.Num() ) {
			ImGui::SeparatorText( "Return values" );
			for ( int output = 0; output < node.outputs.Num(); output++ ) ImGui::BulletText( "%s %s", PinTypeLabel( node.outputs[output] ), node.outputs[output].name.c_str() );
		}
	} else {
		ImGui::TextWrapped( "This flow node has no editable data properties. Its behavior is controlled by execution and typed data-pin connections." );
		if ( node.inputs.Num() ) {
			ImGui::SeparatorText( "Inputs" );
			for ( int index = 0; index < node.inputs.Num(); index++ ) ImGui::BulletText( "%s %s", PinTypeLabel( node.inputs[index] ), node.inputs[index].name.c_str() );
		}
		if ( node.outputs.Num() ) {
			ImGui::SeparatorText( "Outputs" );
			for ( int index = 0; index < node.outputs.Num(); index++ ) ImGui::BulletText( "%s %s", PinTypeLabel( node.outputs[index] ), node.outputs[index].name.c_str() );
		}
	}
	ImGui::Separator();
	if ( editable ) {
		if ( ImGui::Button( "Apply", ImVec2( 100.0f * uiScale, 0 ) ) ) ApplyNodeEditor();
		ImGui::SameLine();
	}
	if ( ImGui::Button( "Cancel", ImVec2( 100.0f * uiScale, 0 ) ) ) ImGui::CloseCurrentPopup();
	ImGui::EndPopup();
}

void DoomScriptBlueprintImGuiEditor::ApplyNodeEditor() {
	int nodeIndex = FindNodeByStableId( editStableId );
	if ( nodeIndex < 0 ) return;
	const doomScriptGraphNode_t &node = document.Nodes()[nodeIndex];
	if ( editConditionBuilder ) {
		if ( editConditions.Num() == 0 ) { SetConditionError( "Add at least one condition row." ); return; }
		static const char *operators[] = { "==", "!=", "<", "<=", ">", ">=" };
		idStr expression;
		for ( int clauseIndex = 0; clauseIndex < editConditions.Num(); clauseIndex++ ) {
			const BlueprintConditionClause &clause = editConditions[clauseIndex];
			if ( clause.variable < 0 || clause.variable >= editVariables.Num() ) {
				SetConditionError( va( "Choose a variable for condition %d.", clauseIndex + 1 ) );
				return;
			}
			const BlueprintVariableChoice &left = editVariables[clause.variable];
			if ( clause.operation < 0 || clause.operation > 7 ) {
				SetConditionError( va( "Choose a comparison for condition %d.", clauseIndex + 1 ) );
				return;
			}
			if ( ( clause.operation == 0 || clause.operation == 1 ) && left.type != "boolean" ) {
				SetConditionError( va( "%s needs an explicit comparison because it is %s, not boolean.", left.name.c_str(), left.type.c_str() ) );
				return;
			}
			idStr clauseExpression;
			if ( clause.operation == 0 ) clauseExpression = left.name;
			else if ( clause.operation == 1 ) clauseExpression = "!" + left.name;
			else {
				idStr right;
				idStr rightType = left.type;
				if ( clause.valueSource >= 0 && clause.valueSource < editVariables.Num() ) {
					right = editVariables[clause.valueSource].name;
					rightType = editVariables[clause.valueSource].type;
					if ( !ConditionTypesCompatible( left.type, rightType ) ) {
						SetConditionError( va( "Condition %d cannot compare %s to %s.", clauseIndex + 1, left.type.c_str(), rightType.c_str() ) );
						return;
					}
				} else {
					right = TrimConditionValue( clause.value );
					if ( right.Length() == 0 ) {
						SetConditionError( va( "Choose a variable or constant for condition %d.", clauseIndex + 1 ) );
						return;
					}
					if ( left.type == "string" && right[0] != '"' ) right = "\"" + right + "\"";
				}
				if ( clause.operation >= 4 && ( !IsNumericConditionType( left.type ) || !IsNumericConditionType( rightType ) ) ) {
					SetConditionError( va( "Condition %d uses an ordered comparison that requires numeric values.", clauseIndex + 1 ) );
					return;
				}
				clauseExpression = left.name + " ";
				clauseExpression += operators[clause.operation - 2];
				clauseExpression += " ";
				clauseExpression += right;
			}
			if ( clauseIndex != 0 ) expression += clause.join == 1 ? " || " : " && ";
			expression += "( ";
			expression += clauseExpression;
			expression += " )";
		}
		idStr error;
		if ( document.UpdateConditionExpression( nodeIndex, expression.c_str(), catalog, error ) ) {
			SetStatus( va( "Updated %d typed condition%s and regenerated DoomScript.", editConditions.Num(), editConditions.Num() == 1 ? "" : "s" ) );
			ImGui::CloseCurrentPopup();
		} else SetConditionError( error.c_str() );
		return;
	}
	if ( node.kind.Find( "function:" ) == 0 ) {
		idList<idStr> arguments;
		for ( int input = 0; input < node.inputs.Num(); input++ ) {
			int source = input < editInputSources.Num() ? editInputSources[input] : -1;
			arguments.Append( source >= 0 && source < editVariables.Num() ? editVariables[source].name : editInputValues[input] );
		}
		idStr error;
		if ( document.UpdateFunctionCall( nodeIndex, arguments, catalog, error ) ) {
			SetStatus( "Updated Function Call pin defaults/connections and regenerated DoomScript." );
			ImGui::CloseCurrentPopup();
		} else SetStatus( error.c_str() );
		return;
	}
	if ( editVariable < 0 || editVariable >= editVariables.Num() ) return;
	idStr expression;
	if ( node.kind == "branch" || node.kind == "loop" ) {
		expression = editOperation == 1 ? "!" + editVariables[editVariable].name : editVariables[editVariable].name;
		if ( editOperation >= 2 ) {
			static const char *operators[] = { "==", "!=", "<", "<=", ">", ">=" };
			idStr right = editValueSource >= 0 && editValueSource < editVariables.Num() ? editVariables[editValueSource].name : editValue;
			right.StripTrailingWhitespace();
			if ( right.Length() == 0 ) { SetStatus( "Choose a comparison variable or typed value." ); return; }
			expression += " "; expression += operators[editOperation - 2]; expression += " "; expression += right;
		}
	} else if ( node.kind == "setvar" ) {
		expression = editValueSource >= 0 && editValueSource < editVariables.Num() ? editVariables[editValueSource].name : editValue;
		expression.StripTrailingWhitespace();
	}
	idStr target = node.kind == "setvar" ? editVariables[editVariable].name : node.variableName;
	idStr error;
	if ( document.UpdateVisualNode( nodeIndex, target.c_str(), expression.c_str(), catalog, error ) ) {
		SetStatus( "Updated typed node properties and regenerated its variable/value connections." );
		ImGui::CloseCurrentPopup();
	} else SetStatus( error.c_str() );
}

void DoomScriptBlueprintImGuiEditor::RenderStatusBar() {
	ImGui::Separator();
	ImGui::TextUnformatted( status.c_str() );
}

void DoomScriptBlueprintImGuiEditor::FillScripts() {
	scriptFiles.Clear();
	idFileList *files = fileSystem->ListFilesTree( "script", ".script", true );
	if ( files == NULL ) return;
	for ( int index = 0; index < files->GetNumFiles(); index++ ) scriptFiles.Append( files->GetFile( index ) );
	fileSystem->FreeFileList( files );
}

bool DoomScriptBlueprintImGuiEditor::LoadScript( int index ) {
	if ( index < 0 || index >= scriptFiles.Num() || !document.Load( scriptFiles[index], catalog ) ) {
		SetStatus( "Could not load the selected DoomScript file." );
		return false;
	}
	selectedScript = index;
	selectedFunction = document.Functions().Num() ? 0 : -1;
	selectedNode = selectedGlobal = selectedLocal = selectedShared = -1;
	FrameFunction();
	int loops = 0, gets = 0, sets = 0;
	for ( int node = 0; node < document.Nodes().Num(); node++ ) {
		if ( document.Nodes()[node].kind == "loop" ) loops++;
		else if ( document.Nodes()[node].kind == "getvar" ) gets++;
		else if ( document.Nodes()[node].kind == "setvar" ) sets++;
	}
	SetStatus( va( "%s: %d Events, %d loops, %d Get nodes, %d Set nodes", document.Path().c_str(), document.Functions().Num(), loops, gets, sets ) );
	return true;
}

bool DoomScriptBlueprintImGuiEditor::ConfirmDiscardOrSave() {
	if ( !document.IsDirty() ) return true;
	int answer = MessageBoxA( hwnd, "Save Blueprint changes before continuing?", "DoomScript Blueprint", MB_YESNOCANCEL | MB_ICONQUESTION );
	if ( answer == IDCANCEL ) return false;
	if ( answer == IDYES && !document.Save() ) {
		MessageBoxA( hwnd, "Could not save the DoomScript file.", "DoomScript Blueprint", MB_OK | MB_ICONERROR );
		return false;
	}
	return true;
}

void DoomScriptBlueprintImGuiEditor::Save() {
	if ( document.Path().Length() == 0 ) return;
	SetStatus( document.Save() ? "Saved DoomScript and Blueprint layout metadata." : "Could not save the DoomScript file." );
}

void DoomScriptBlueprintImGuiEditor::MigrateAll() {
	if ( !ConfirmDiscardOrSave() ) return;
	int failed = 0;
	int saved = DoomScriptBlueprintDocument::MigrateAllScripts( catalog, failed );
	SetStatus( va( "Regenerated Blueprint metadata for %d scripts; %d failed.", saved, failed ) );
	if ( selectedScript >= 0 ) LoadScript( selectedScript );
}

void DoomScriptBlueprintImGuiEditor::FrameFunction() {
	bool found = false;
	int minimumX = 0, minimumY = 0;
	for ( int index = 0; index < document.Nodes().Num(); index++ ) {
		const doomScriptGraphNode_t &node = document.Nodes()[index];
		if ( node.functionIndex != selectedFunction ) continue;
		if ( !found ) { minimumX = node.x; minimumY = node.y; found = true; }
		else { minimumX = Min( minimumX, node.x ); minimumY = Min( minimumY, node.y ); }
	}
	zoom = 1.0f;
	pan = ImVec2( ( 30.0f - minimumX ) * uiScale, ( 30.0f - minimumY ) * uiScale );
}

void DoomScriptBlueprintImGuiEditor::AddSelectedVariableSetNode() {
	if ( selectedFunction < 0 ) { SetStatus( "Select an Event first." ); return; }
	idStr name;
	if ( selectedLocal >= 0 && selectedLocal < document.Functions()[selectedFunction].locals.Num() ) name = document.Functions()[selectedFunction].locals[selectedLocal].name;
	else if ( selectedGlobal >= 0 && selectedGlobal < document.Globals().Num() ) name = document.Globals()[selectedGlobal].name;
	else if ( selectedShared >= 0 && selectedShared < document.SharedVariables().Num() ) name = document.SharedVariables()[selectedShared].name;
	if ( name.Length() == 0 ) { SetStatus( "Select an Event-local, file/object, or inherited variable first." ); return; }
	idStr error;
	if ( document.InsertSetVariable( selectedFunction, name, catalog, error ) ) SetStatus( va( "Added Set %s. Double-click it or connect a typed output pin.", name.c_str() ) );
	else SetStatus( error.c_str() );
}

void DoomScriptBlueprintImGuiEditor::SetStatus( const char *text ) {
	status = text == NULL ? "" : text;
}

int DoomScriptBlueprintImGuiEditor::FindNodeByStableId( const idStr &stableId ) const {
	for ( int index = 0; index < document.Nodes().Num(); index++ ) if ( document.Nodes()[index].stableId == stableId ) return index;
	return -1;
}

LRESULT DoomScriptBlueprintImGuiEditor::WindowProc( HWND window, UINT message, WPARAM wParam, LPARAM lParam ) {
	if ( rendererReady && imguiContext != NULL ) {
		ImGuiContext *previousContext = ImGui::GetCurrentContext();
		ImGui::SetCurrentContext( imguiContext );
		bool handled = ImGui_ImplWin32_WndProcHandler( window, message, wParam, lParam );
		ImGui::SetCurrentContext( previousContext );
		if ( handled ) return TRUE;
	}
	switch ( message ) {
		case WM_ERASEBKGND: return 1;
		case WM_PAINT: {
			PAINTSTRUCT paint;
			BeginPaint( window, &paint );
			EndPaint( window, &paint );
			RenderFrame();
			return 0;
		}
		case WM_TIMER:
			RenderFrame();
			return 0;
		case WM_SIZE:
			if ( wParam != SIZE_MINIMIZED ) InvalidateRect( window, NULL, FALSE );
			return 0;
		case WM_DPICHANGED: {
			const RECT *suggested = (const RECT *)lParam;
			SetWindowPos( window, NULL, suggested->left, suggested->top,
				suggested->right - suggested->left, suggested->bottom - suggested->top,
				SWP_NOACTIVATE | SWP_NOZORDER );
			return 0;
		}
		case WM_CLOSE:
			if ( ConfirmDiscardOrSave() ) DestroyWindow( window );
			return 0;
		case WM_NCDESTROY:
			ShutdownRenderer();
			SetWindowLongPtr( window, GWLP_USERDATA, 0 );
			hwnd = NULL;
			if ( deleteOnDestroy ) {
				blueprintEditor = NULL;
				delete this;
			}
			return 0;
	}
	return DefWindowProc( window, message, wParam, lParam );
}

LRESULT CALLBACK DoomScriptBlueprintImGuiEditor::StaticWindowProc( HWND window, UINT message, WPARAM wParam, LPARAM lParam ) {
	DoomScriptBlueprintImGuiEditor *editor = (DoomScriptBlueprintImGuiEditor *)GetWindowLongPtr( window, GWLP_USERDATA );
	if ( message == WM_NCCREATE ) {
		CREATESTRUCT *create = (CREATESTRUCT *)lParam;
		editor = (DoomScriptBlueprintImGuiEditor *)create->lpCreateParams;
		SetWindowLongPtr( window, GWLP_USERDATA, (LONG_PTR)editor );
		editor->hwnd = window;
	}
	return editor != NULL ? editor->WindowProc( window, message, wParam, lParam ) : DefWindowProc( window, message, wParam, lParam );
}

} // namespace

void ShowDoomScriptBlueprintEditor() {
	if ( blueprintEditor != NULL ) {
		blueprintEditor->Focus();
		return;
	}
	blueprintEditor = new DoomScriptBlueprintImGuiEditor();
	if ( !blueprintEditor->Create() ) {
		delete blueprintEditor;
		blueprintEditor = NULL;
		MessageBoxA( g_pParentWnd != NULL ? g_pParentWnd->GetSafeHwnd() : NULL,
			"Could not create the Dear ImGui Blueprint editor window.", "DoomScript Blueprint", MB_OK | MB_ICONERROR );
	}
}
