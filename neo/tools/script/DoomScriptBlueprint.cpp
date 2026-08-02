#include "../../idlib/precompiled.h"
#pragma hdrstop

#include "DoomScriptBlueprint.h"

#include <ctype.h>
#include <map>
#include <sstream>
#include <string>
#include <vector>

static const char *DSBP_BEGIN = "//@doomscript-blueprint begin 6";
static const char *DSBP_BEGIN_PREFIX = "//@doomscript-blueprint begin ";
static const char *DSBP_END = "//@doomscript-blueprint end";

static unsigned int BlueprintHash( const char *text ) {
	unsigned int hash = 2166136261u;
	for ( const unsigned char *current = (const unsigned char *)text; *current; current++ ) {
		hash ^= *current;
		hash *= 16777619u;
	}
	return hash;
}

static idStr HexHash( unsigned int hash ) {
	return va( "%08X", hash );
}

static std::vector<std::string> Split( const std::string &value, char separator ) {
	std::vector<std::string> result;
	std::string item;
	std::istringstream stream( value );
	while ( std::getline( stream, item, separator ) ) {
		result.push_back( item );
	}
	if ( !value.empty() && value[value.size() - 1] == separator ) {
		result.push_back( std::string() );
	}
	return result;
}

static idStr UnescapeCatalog( const std::string &value ) {
	idStr result;
	for ( size_t index = 0; index < value.size(); index++ ) {
		if ( value[index] != '\\' || index + 1 >= value.size() ) {
			result.Append( value[index] );
			continue;
		}
		char escaped = value[++index];
		if ( escaped == 't' ) result.Append( '\t' );
		else if ( escaped == 'r' ) result.Append( '\r' );
		else if ( escaped == 'n' ) result.Append( '\n' );
		else result.Append( escaped );
	}
	return result;
}

static idStr RemoveBlueprintMetadata( const idStr &input, idDict *layout );
static idStr StripLineComments( const idStr &line, bool &blockComment );
static bool LooksLikeFunctionHeader( const idStr &code );
static idStr BlueprintFunctionName( const idStr &header );
static void ParseFunctionParameters( const idStr &header, idList<doomScriptBlueprintPin_t> &parameters );

static idStr BlueprintFunctionReturnType( const idStr &header ) {
	int end = 0;
	while ( end < header.Length() && ( isalnum( (unsigned char)header[end] ) || header[end] == '_' ) ) end++;
	return end > 0 ? header.Left( end ) : idStr( "void" );
}

static idStr BlueprintFunctionCommand( const idStr &name ) {
	int separator = name.Find( "::", true );
	return separator >= 0 ? name.Mid( separator + 2, name.Length() - separator - 2 ) : name;
}

static void AppendDoomScriptFunctions( idList<doomScriptFunctionNode_t> &nodes ) {
	idFileList *files = fileSystem->ListFilesTree( "script", ".script", true );
	if ( files == NULL ) return;
	for ( int fileIndex = 0; fileIndex < files->GetNumFiles(); fileIndex++ ) {
		void *buffer = NULL;
		int length = fileSystem->ReadFile( files->GetFile( fileIndex ), &buffer );
		if ( length < 0 || buffer == NULL ) continue;
		idStr contents( (const char *)buffer, 0, length );
		fileSystem->FreeFile( buffer );
		contents = RemoveBlueprintMetadata( contents, NULL );
		idStr pending;
		bool blockComment = false;
		int cursor = 0;
		int lineNumber = 1;
		int pendingLine = 0;
		while ( cursor <= contents.Length() ) {
			int lineEnd = contents.Find( '\n', cursor );
			if ( lineEnd < 0 ) lineEnd = contents.Length();
			idStr code = StripLineComments( contents.Mid( cursor, lineEnd - cursor ), blockComment );
			if ( pending.Length() == 0 && LooksLikeFunctionHeader( code ) ) {
				pending = code;
				pendingLine = lineNumber;
			} else if ( pending.Length() != 0 && pending.Find( '{' ) < 0 ) {
				pending += " "; pending += code;
			}
			if ( pending.Length() != 0 && pending.Find( '{' ) >= 0 ) {
				idStr functionName = BlueprintFunctionName( pending );
				idStr command = BlueprintFunctionCommand( functionName );
				bool duplicate = false;
				for ( int existing = 0; existing < nodes.Num(); existing++ ) {
					if ( nodes[existing].command == command && nodes[existing].source.Find( files->GetFile( fileIndex ) ) == 0 ) { duplicate = true; break; }
				}
				if ( !duplicate ) {
					doomScriptFunctionNode_t &function = nodes.Alloc();
					function.stableId = "script:" + HexHash( BlueprintHash( ( idStr( files->GetFile( fileIndex ) ) + "|" + functionName ).c_str() ) );
					function.title = functionName;
					function.category = "DoomScript Functions";
					function.command = command;
					function.receiver = functionName.Find( "::" ) >= 0 ? "object" : "global";
					function.emitKind = "functionCall";
					function.returnType = BlueprintFunctionReturnType( pending );
					function.pure = false;
					function.latent = false;
					function.deprecated = false;
					function.owners = functionName.Find( "::" ) >= 0 ? functionName.Left( functionName.Find( "::" ) ) : "script";
					function.callback = functionName;
					function.source = va( "%s:%d", files->GetFile( fileIndex ), pendingLine );
					function.keywords = "script function call";
					function.description = "DoomScript-defined function. Parameters and return value are editable Blueprint pins.";
					ParseFunctionParameters( pending, function.pins );
				}
				pending.Clear();
			} else if ( pending.Length() != 0 && code.Length() && code[code.Length() - 1] == ';' ) {
				pending.Clear();
			}
			if ( lineEnd == contents.Length() ) break;
			cursor = lineEnd + 1;
			lineNumber++;
		}
	}
	fileSystem->FreeFileList( files );
}

bool DoomScriptNodeCatalog::Load() {
	nodes.Clear();
	void *buffer = NULL;
	int length = fileSystem->ReadFile( "editors/doomscript_nodes.def", &buffer );
	if ( length < 0 || buffer == NULL ) {
		return false;
	}

	std::string contents( (const char *)buffer, length );
	fileSystem->FreeFile( buffer );
	checksum = HexHash( BlueprintHash( contents.c_str() ) );
	std::istringstream lines( contents );
	std::string line;
	std::map<std::string, std::vector<std::string> > enumValues;
	while ( std::getline( lines, line ) ) {
		if ( !line.empty() && line[line.size() - 1] == '\r' ) line.erase( line.size() - 1 );
		if ( line.compare( 0, 5, "enum\t" ) == 0 ) {
			std::vector<std::string> fields = Split( line, '\t' );
			if ( fields.size() >= 3 ) {
				enumValues[UnescapeCatalog( fields[1] ).c_str()] = Split( UnescapeCatalog( fields[2] ).c_str(), ',' );
			}
			continue;
		}
		if ( line.compare( 0, 5, "node\t" ) != 0 ) {
			continue;
		}
		std::vector<std::string> fields = Split( line, '\t' );
		if ( fields.size() < 13 ) {
			continue;
		}
		doomScriptFunctionNode_t &node = nodes.Alloc();
		node.stableId = UnescapeCatalog( fields[1] );
		node.title = UnescapeCatalog( fields[2] );
		node.category = UnescapeCatalog( fields[3] );
		node.command = UnescapeCatalog( fields[4] );
		node.receiver = UnescapeCatalog( fields[5] );
		node.emitKind = UnescapeCatalog( fields[6] );
		node.returnType = UnescapeCatalog( fields[7] );
		node.pure = fields[8] == "1";
		node.owners = UnescapeCatalog( fields[9] );
		node.callback = UnescapeCatalog( fields[10] );
		node.source = UnescapeCatalog( fields[11] );
		node.latent = fields.size() > 13 && fields[13] == "1";
		node.deprecated = fields.size() > 14 && fields[14] == "1";
		if ( fields.size() > 15 ) node.keywords = UnescapeCatalog( fields[15] );
		if ( fields.size() > 16 ) node.description = UnescapeCatalog( fields[16] );
		std::vector<std::string> pins = Split( fields[12], ',' );
		for ( size_t pinIndex = 0; pinIndex < pins.size(); pinIndex++ ) {
			size_t colon = pins[pinIndex].find( ':' );
			if ( colon == std::string::npos ) {
				continue;
			}
			doomScriptBlueprintPin_t &pin = node.pins.Alloc();
			std::string type = pins[pinIndex].substr( 0, colon );
			if ( type.size() > 6 && type.compare( 0, 5, "enum(" ) == 0 && type[type.size() - 1] == ')' ) {
				std::string enumType = type.substr( 5, type.size() - 6 );
				pin.type = "float";
				pin.enumType = enumType.c_str();
				std::map<std::string, std::vector<std::string> >::const_iterator values = enumValues.find( enumType );
				if ( values != enumValues.end() ) {
					for ( size_t valueIndex = 0; valueIndex < values->second.size(); valueIndex++ ) {
						if ( !values->second[valueIndex].empty() ) pin.enumValues.Append( values->second[valueIndex].c_str() );
					}
				}
			} else {
				pin.type = type.c_str();
			}
			pin.name = pins[pinIndex].substr( colon + 1 ).c_str();
		}
	}
	AppendDoomScriptFunctions( nodes );
	return nodes.Num() != 0;
}

int DoomScriptNodeCatalog::FindCommand( const char *command ) const {
	for ( int index = 0; index < nodes.Num(); index++ ) {
		if ( nodes[index].command.Icmp( command ) == 0 ) {
			return index;
		}
	}
	return -1;
}

static idStr RemoveBlueprintMetadata( const idStr &input, idDict *layout ) {
	int marker = input.Find( DSBP_BEGIN_PREFIX, true );
	if ( marker < 0 ) {
		return input;
	}
	int end = input.Find( DSBP_END, true, marker );
	if ( end < 0 ) {
		return input;
	}

	if ( layout != NULL ) {
		int cursor = marker;
		while ( cursor < end ) {
			int lineEnd = input.Find( '\n', cursor );
			if ( lineEnd < 0 || lineEnd > end ) lineEnd = end;
			idStr line = input.Mid( cursor, lineEnd - cursor );
			char id[64];
			int x, y;
			if ( sscanf( line.c_str(), "//@node %63s %d %d", id, &x, &y ) == 3 ) {
				layout->Set( id, va( "%d %d", x, y ) );
			}
			cursor = lineEnd + 1;
		}
	}

	idStr result = input.Left( marker );
	result.StripTrailingWhitespace();
	return result;
}

static idStr StripLineComments( const idStr &line, bool &blockComment ) {
	idStr result;
	bool quoted = false;
	for ( int index = 0; index < line.Length(); index++ ) {
		char current = line[index];
		char next = index + 1 < line.Length() ? line[index + 1] : '\0';
		if ( blockComment ) {
			if ( current == '*' && next == '/' ) {
				blockComment = false;
				index++;
			}
			continue;
		}
		if ( !quoted && current == '/' && next == '*' ) {
			blockComment = true;
			index++;
			continue;
		}
		if ( !quoted && current == '/' && next == '/' ) {
			break;
		}
		if ( current == '"' && ( index == 0 || line[index - 1] != '\\' ) ) {
			quoted = !quoted;
		}
		result.Append( current );
	}
	result.StripTrailingWhitespace();
	int first = 0;
	while ( first < result.Length() && isspace( (unsigned char)result[first] ) ) first++;
	if ( first != 0 ) result = result.Mid( first, result.Length() - first );
	return result;
}

static idStr NormalizeBlueprintLine( const idStr &input ) {
	idStr result;
	bool whitespace = false;
	for ( int index = 0; index < input.Length(); index++ ) {
		if ( isspace( (unsigned char)input[index] ) ) {
			whitespace = result.Length() != 0;
			continue;
		}
		if ( whitespace ) result.Append( ' ' );
		whitespace = false;
		result.Append( input[index] );
	}
	return result;
}

static idStr NormalizeBlueprintStatement( const idStr &input ) {
	idStr result = NormalizeBlueprintLine( input );
	while ( result.Length() && result[0] == '}' ) {
		result = result.Mid( 1, result.Length() - 1 );
		while ( result.Length() && isspace( (unsigned char)result[0] ) ) {
			result = result.Mid( 1, result.Length() - 1 );
		}
	}
	while ( result.Length() && result[result.Length() - 1] == '{' ) {
		result = result.Left( result.Length() - 1 );
		result.StripTrailingWhitespace();
	}
	return result;
}

static int CountBrace( const idStr &value, char brace ) {
	int count = 0;
	bool quoted = false;
	for ( int index = 0; index < value.Length(); index++ ) {
		if ( value[index] == '"' && ( index == 0 || value[index - 1] != '\\' ) ) quoted = !quoted;
		else if ( !quoted && value[index] == brace ) count++;
	}
	return count;
}

static bool IsIdentifierCharacter( char value ) {
	return isalnum( (unsigned char)value ) || value == '_' || value == ':';
}

static bool StartsWord( const idStr &value, const char *word ) {
	int length = (int)strlen( word );
	return value.Icmpn( word, length ) == 0 &&
		( value.Length() == length || !IsIdentifierCharacter( value[length] ) );
}

static bool LooksLikeFunctionHeader( const idStr &code ) {
	const char *value = code.c_str();
	if ( StartsWord( code, "if" ) || StartsWord( code, "while" ) || StartsWord( code, "for" ) ||
		 StartsWord( code, "object" ) || StartsWord( code, "namespace" ) || StartsWord( code, "scriptEvent" ) ) {
		return false;
	}
	int index = 0;
	while ( value[index] && IsIdentifierCharacter( value[index] ) ) index++;
	if ( index == 0 || !isspace( (unsigned char)value[index] ) ) return false;
	while ( isspace( (unsigned char)value[index] ) ) index++;
	int nameStart = index;
	while ( value[index] && IsIdentifierCharacter( value[index] ) ) index++;
	if ( index == nameStart ) return false;
	while ( isspace( (unsigned char)value[index] ) ) index++;
	return value[index] == '(';
}

static idStr BlueprintFunctionName( const idStr &header ) {
	const char *value = header.c_str();
	int index = 0;
	while ( value[index] && IsIdentifierCharacter( value[index] ) ) index++;
	while ( isspace( (unsigned char)value[index] ) ) index++;
	int start = index;
	while ( value[index] && IsIdentifierCharacter( value[index] ) ) index++;
	return header.Mid( start, index - start );
}

static const char *BlueprintNodeKind( const idStr &code ) {
	if ( StartsWord( code, "Set" ) ) return "setlocal";
	if ( StartsWord( code, "if" ) || StartsWord( code, "else" ) ) return "branch";
	if ( StartsWord( code, "while" ) || StartsWord( code, "for" ) || StartsWord( code, "do" ) ) return "loop";
	if ( StartsWord( code, "return" ) ) return "return";
	if ( StartsWord( code, "thread" ) ) return "thread";
	if ( StartsWord( code, "break" ) ) return "break";
	if ( StartsWord( code, "continue" ) ) return "continue";
	if ( code.Find( '(' ) >= 0 ) return "call";
	return "statement";
}

struct blueprintExecutionSource_t {
	int node;
	int pin;
};

struct blueprintControlFrame_t {
	int node;
	int bodyDepth;
	bool loop;
	bool inElse;
	std::vector<blueprintExecutionSource_t> trueTails;
	std::vector<blueprintExecutionSource_t> breakTails;
};

static void AppendExecutionSource( std::vector<blueprintExecutionSource_t> &sources, int node, int pin ) {
	if ( node < 0 ) return;
	for ( size_t index = 0; index < sources.size(); index++ ) {
		if ( sources[index].node == node && sources[index].pin == pin ) return;
	}
	blueprintExecutionSource_t source = { node, pin };
	sources.push_back( source );
}

static void AddExecutionLinks( idList<doomScriptGraphLink_t> &links,
	const std::vector<blueprintExecutionSource_t> &sources, int target ) {
	for ( size_t index = 0; index < sources.size(); index++ ) {
		doomScriptGraphLink_t &link = links.Alloc();
		link.from = sources[index].node;
		link.to = target;
		link.fromPin = sources[index].pin;
		link.toPin = 0;
		link.execution = true;
	}
}

static void FinishControlFrame( std::vector<blueprintControlFrame_t> &frames,
	std::vector<blueprintExecutionSource_t> &sources, idList<doomScriptGraphLink_t> &links ) {
	if ( frames.empty() ) return;
	blueprintControlFrame_t frame = frames.back();
	frames.pop_back();
	std::vector<blueprintExecutionSource_t> merged;
	if ( frame.loop ) {
		AddExecutionLinks( links, sources, frame.node );
		AppendExecutionSource( merged, frame.node, 1 );
		for ( size_t index = 0; index < frame.breakTails.size(); index++ ) {
			AppendExecutionSource( merged, frame.breakTails[index].node, frame.breakTails[index].pin );
		}
	} else if ( frame.inElse ) {
		for ( size_t index = 0; index < frame.trueTails.size(); index++ ) {
			AppendExecutionSource( merged, frame.trueTails[index].node, frame.trueTails[index].pin );
		}
		for ( size_t index = 0; index < sources.size(); index++ ) {
			AppendExecutionSource( merged, sources[index].node, sources[index].pin );
		}
	} else {
		for ( size_t index = 0; index < sources.size(); index++ ) {
			AppendExecutionSource( merged, sources[index].node, sources[index].pin );
		}
		AppendExecutionSource( merged, frame.node, 1 );
	}
	sources = merged;
}

static int LeadingClosingBraces( const idStr &code ) {
	int count = 0;
	for ( int index = 0; index < code.Length(); index++ ) {
		if ( isspace( (unsigned char)code[index] ) ) continue;
		if ( code[index] != '}' ) break;
		count++;
	}
	return count;
}

static bool IsSimpleGraphIdentifier( const idStr &value ) {
	if ( value.Length() == 0 || !( isalpha( (unsigned char)value[0] ) || value[0] == '_' ) ) return false;
	for ( int index = 1; index < value.Length(); index++ ) {
		if ( !( isalnum( (unsigned char)value[index] ) || value[index] == '_' ) ) return false;
	}
	return true;
}

static bool ParseAssignmentStatement( const idStr &code, idStr &target, idStr &value ) {
	target.Clear();
	value.Clear();
	if ( code.Length() < 4 || code[code.Length() - 1] != ';' ) return false;
	idStr compact = NormalizeBlueprintLine( code );
	if ( compact.Length() > 3 && compact[compact.Length() - 2] == compact[compact.Length() - 3] &&
		( compact[compact.Length() - 2] == '+' || compact[compact.Length() - 2] == '-' ) ) {
		target = NormalizeBlueprintLine( compact.Left( compact.Length() - 3 ) );
		if ( IsSimpleGraphIdentifier( target ) ) {
			value = target + ( compact[compact.Length() - 2] == '+' ? " + 1" : " - 1" );
			return true;
		}
	}
	int nesting = 0;
	char quote = '\0';
	for ( int index = 0; index < code.Length() - 1; index++ ) {
		char current = code[index];
		if ( quote != '\0' ) {
			if ( current == quote && ( index == 0 || code[index - 1] != '\\' ) ) quote = '\0';
			continue;
		}
		if ( current == '"' || current == '\'' ) { quote = current; continue; }
		if ( current == '(' || current == '[' ) { nesting++; continue; }
		if ( current == ')' || current == ']' ) { nesting--; continue; }
		if ( current != '=' || nesting != 0 ) continue;
		char before = index > 0 ? code[index - 1] : '\0';
		char after = index + 1 < code.Length() ? code[index + 1] : '\0';
		if ( before == '=' || before == '!' || before == '<' || before == '>' || after == '=' ) continue;
		bool compound = before == '+' || before == '-' || before == '*' || before == '/';
		target = NormalizeBlueprintLine( code.Left( compound ? index - 1 : index ) );
		idStr right = NormalizeBlueprintLine( code.Mid( index + 1, code.Length() - index - 2 ) );
		value = compound ? target + " " + before + " ( " + right + " )" : right;
		return IsSimpleGraphIdentifier( target ) && value.Length() != 0;
	}
	return false;
}

static idStr ExtractControlCondition( const idStr &code ) {
	int open = code.Find( '(' );
	int close = code.Last( ')' );
	if ( open < 0 || close <= open ) return "";
	return NormalizeBlueprintLine( code.Mid( open + 1, close - open - 1 ) );
}

static bool ContainsGraphIdentifier( const idStr &expression, const idStr &name ) {
	int offset = 0;
	while ( offset < expression.Length() ) {
		int found = expression.Find( name, true, offset );
		if ( found < 0 ) return false;
		bool leftBoundary = found == 0 || !( isalnum( (unsigned char)expression[found - 1] ) || expression[found - 1] == '_' );
		int end = found + name.Length();
		bool rightBoundary = end == expression.Length() || !( isalnum( (unsigned char)expression[end] ) || expression[end] == '_' );
		if ( leftBoundary && rightBoundary ) return true;
		offset = found + 1;
	}
	return false;
}

static bool IsGraphLiteral( const idStr &expression ) {
	idStr value = NormalizeBlueprintLine( expression );
	if ( value.Length() == 0 ) return false;
	if ( value == "true" || value == "false" || value[0] == '"' || value[0] == '\'' || value[0] == '$' ) return true;
	int index = value[0] == '-' || value[0] == '+' ? 1 : 0;
	if ( index >= value.Length() ) return false;
	for ( ; index < value.Length(); index++ ) {
		if ( !isdigit( (unsigned char)value[index] ) && value[index] != '.' ) return false;
	}
	return true;
}

static idStr FindGraphVariableType( const idList<doomScriptVariable_t> &globals, const idList<doomScriptVariable_t> &sharedVariables,
	const doomScriptGraphFunction_t &function, const idStr &name ) {
	for ( int index = 0; index < function.locals.Num(); index++ ) if ( function.locals[index].name == name ) return function.locals[index].type;
	for ( int index = 0; index < function.parameters.Num(); index++ ) if ( function.parameters[index].name == name ) return function.parameters[index].type;
	for ( int index = 0; index < globals.Num(); index++ ) if ( globals[index].name == name ) return globals[index].type;
	for ( int index = 0; index < sharedVariables.Num(); index++ ) if ( sharedVariables[index].name == name ) return sharedVariables[index].type;
	return "";
}

static bool ParseVariableDeclarations( const idStr &code, idList<doomScriptVariable_t> &variables ) {
	variables.Clear();
	idStr value = NormalizeBlueprintLine( code );
	if ( value.Length() < 3 || value[value.Length() - 1] != ';' ) return false;
	const char *text = value.c_str();
	int index = 0;
	while ( text[index] && IsIdentifierCharacter( text[index] ) ) index++;
	if ( index == 0 || !isspace( (unsigned char)text[index] ) ) return false;
	idStr type = value.Left( index );
	if ( type == "return" || type == "thread" || type == "break" ||
		 type == "continue" || type == "scriptEvent" ) return false;
	while ( isspace( (unsigned char)text[index] ) ) index++;
	int declaratorStart = index;
	int nesting = 0;
	char quote = '\0';
	for ( ; index <= value.Length() - 1; index++ ) {
		char current = value[index];
		if ( quote != '\0' ) {
			if ( current == quote && ( index == 0 || value[index - 1] != '\\' ) ) quote = '\0';
		} else if ( current == '"' || current == '\'' ) quote = current;
		else if ( current == '(' || current == '[' ) nesting++;
		else if ( current == ')' || current == ']' ) nesting--;
		if ( ( current != ',' || quote != '\0' || nesting != 0 ) && current != ';' ) continue;

		idStr declarator = value.Mid( declaratorStart, index - declaratorStart );
		declarator = NormalizeBlueprintLine( declarator );
		const char *declarationText = declarator.c_str();
		int declarationIndex = 0;
		while ( isalnum( (unsigned char)declarationText[declarationIndex] ) || declarationText[declarationIndex] == '_' ) declarationIndex++;
		if ( declarationIndex == 0 ) { variables.Clear(); return false; }
		doomScriptVariable_t &variable = variables.Alloc();
		variable.type = type;
		variable.name = declarator.Left( declarationIndex );
		while ( isspace( (unsigned char)declarationText[declarationIndex] ) ) declarationIndex++;
		if ( declarationText[declarationIndex] == '\0' ) {
			variable.defaultValue.Clear();
		} else {
			if ( declarationText[declarationIndex] != '=' ) { variables.Clear(); return false; }
			declarationIndex++;
			while ( isspace( (unsigned char)declarationText[declarationIndex] ) ) declarationIndex++;
			variable.defaultValue = declarator.Mid( declarationIndex, declarator.Length() - declarationIndex );
			if ( variable.defaultValue.Length() == 0 ) { variables.Clear(); return false; }
		}
		declaratorStart = index + 1;
	}
	return variables.Num() != 0;
}

static void ParseFunctionParameters( const idStr &header, idList<doomScriptBlueprintPin_t> &parameters ) {
	parameters.Clear();
	int open = header.Find( '(' );
	int close = open >= 0 ? header.Find( ')', open + 1 ) : -1;
	if ( open < 0 || close < 0 || close == open + 1 ) return;
	std::vector<std::string> values = Split( header.Mid( open + 1, close - open - 1 ).c_str(), ',' );
	for ( size_t index = 0; index < values.size(); index++ ) {
		idList<doomScriptVariable_t> variables;
		idStr declaration = values[index].c_str();
		declaration += ";";
		if ( ParseVariableDeclarations( declaration, variables ) && variables.Num() == 1 ) {
			doomScriptBlueprintPin_t &pin = parameters.Alloc();
			pin.type = variables[0].type;
			pin.name = variables[0].name;
		}
	}
}

DoomScriptBlueprintDocument::DoomScriptBlueprintDocument() : dirty( false ) {
}

static const char *DefaultPinValue( const idStr &type );
static const char *DefaultPinValue( const doomScriptBlueprintPin_t &pin );

bool DoomScriptBlueprintDocument::Load( const char *virtualPath, const DoomScriptNodeCatalog &catalog ) {
	void *buffer = NULL;
	int length = fileSystem->ReadFile( virtualPath, &buffer );
	if ( length < 0 || buffer == NULL ) return false;
	idStr loaded( (const char *)buffer, 0, length );
	fileSystem->FreeFile( buffer );
	idDict layout;
	source = RemoveBlueprintMetadata( loaded, &layout );
	path = virtualPath;
	catalogChecksum = catalog.Checksum();
	LoadSharedVariables();
	Parse( catalog, layout );
	dirty = false;
	return true;
}

void DoomScriptBlueprintDocument::LoadSharedVariables() {
	sharedVariables.Clear();
	idFileList *files = fileSystem->ListFilesTree( "script", ".script", true );
	if ( files == NULL ) return;
	for ( int fileIndex = 0; fileIndex < files->GetNumFiles(); fileIndex++ ) {
		void *buffer = NULL;
		int length = fileSystem->ReadFile( files->GetFile( fileIndex ), &buffer );
		if ( length < 0 || buffer == NULL ) continue;
		idStr contents( (const char *)buffer, 0, length );
		fileSystem->FreeFile( buffer );
		contents = RemoveBlueprintMetadata( contents, NULL );
		int cursor = 0;
		int braceDepth = 0;
		int objectDepth = -1;
		bool blockComment = false;
		while ( cursor <= contents.Length() ) {
			int lineEnd = contents.Find( '\n', cursor );
			if ( lineEnd < 0 ) lineEnd = contents.Length();
			idStr raw = contents.Mid( cursor, lineEnd - cursor );
			idStr code = StripLineComments( raw, blockComment );
			if ( objectDepth < 0 && StartsWord( code, "object" ) && code.Find( '{' ) >= 0 ) {
				objectDepth = braceDepth + CountBrace( code, '{' ) - CountBrace( code, '}' );
			}
			bool declarationScope = ( objectDepth >= 0 && braceDepth == objectDepth ) || ( objectDepth < 0 && braceDepth == 0 );
			if ( declarationScope ) {
				idList<doomScriptVariable_t> declarations;
				if ( ParseVariableDeclarations( code, declarations ) ) {
					for ( int variableIndex = 0; variableIndex < declarations.Num(); variableIndex++ ) {
						bool duplicate = false;
						for ( int existing = 0; existing < sharedVariables.Num(); existing++ ) {
							if ( sharedVariables[existing].name == declarations[variableIndex].name ) { duplicate = true; break; }
						}
						if ( !duplicate ) {
							declarations[variableIndex].line = 0;
							declarations[variableIndex].startOffset = -1;
							declarations[variableIndex].endOffset = -1;
							sharedVariables.Append( declarations[variableIndex] );
						}
					}
				}
			}
			braceDepth += CountBrace( code, '{' ) - CountBrace( code, '}' );
			if ( objectDepth >= 0 && braceDepth < objectDepth ) objectDepth = -1;
			if ( lineEnd == contents.Length() ) break;
			cursor = lineEnd + 1;
		}
	}
	fileSystem->FreeFileList( files );
}

static bool ParseFunctionCallArguments( const idStr &statement, const idStr &command, idList<idStr> &arguments,
	int *openOffset = NULL, int *closeOffset = NULL ) {
	arguments.Clear();
	idStr needle = command + "(";
	int commandOffset = statement.Find( needle, true );
	if ( commandOffset < 0 ) return false;
	int open = commandOffset + command.Length();
	int start = open + 1;
	int nesting = 1;
	char quote = '\0';
	for ( int index = start; index < statement.Length(); index++ ) {
		char current = statement[index];
		if ( quote != '\0' ) {
			if ( current == quote && ( index == 0 || statement[index - 1] != '\\' ) ) quote = '\0';
			continue;
		}
		if ( current == '"' || current == '\'' ) { quote = current; continue; }
		if ( current == '(' || current == '[' ) { nesting++; continue; }
		if ( current == ')' || current == ']' ) {
			nesting--;
			if ( nesting == 0 ) {
				idStr argument = NormalizeBlueprintLine( statement.Mid( start, index - start ) );
				if ( argument.Length() || arguments.Num() != 0 ) arguments.Append( argument );
				if ( openOffset != NULL ) *openOffset = open;
				if ( closeOffset != NULL ) *closeOffset = index;
				return true;
			}
			continue;
		}
		if ( current == ',' && nesting == 1 ) {
			arguments.Append( NormalizeBlueprintLine( statement.Mid( start, index - start ) ) );
			start = index + 1;
		}
	}
	return false;
}

static void AddFunctionInputGraph( idList<doomScriptGraphNode_t> &nodes, idList<doomScriptGraphLink_t> &links,
	idList<doomScriptGraphFunction_t> &functions, const idList<doomScriptVariable_t> &globals,
	const idList<doomScriptVariable_t> &sharedVariables, int functionIndex, int primaryNode, int inputPin,
	const idStr &dataExpression, int lineNumber, std::map<unsigned int, int> &occurrences, const idDict &layout ) {
	if ( dataExpression.Length() == 0 || inputPin < 0 || inputPin >= nodes[primaryNode].inputs.Num() ) return;
	idList<idStr> referencedNames;
	idList<idStr> referencedTypes;
	idList<bool> referencedParameters;
	for ( int variableIndex = 0; variableIndex < functions[functionIndex].locals.Num(); variableIndex++ ) {
		const doomScriptVariable_t &variable = functions[functionIndex].locals[variableIndex];
		if ( ContainsGraphIdentifier( dataExpression, variable.name ) ) {
			referencedNames.Append( variable.name ); referencedTypes.Append( variable.type ); referencedParameters.Append( false );
		}
	}
	for ( int parameterIndex = 0; parameterIndex < functions[functionIndex].parameters.Num(); parameterIndex++ ) {
		const doomScriptBlueprintPin_t &parameter = functions[functionIndex].parameters[parameterIndex];
		if ( !ContainsGraphIdentifier( dataExpression, parameter.name ) ) continue;
		bool duplicate = false;
		for ( int found = 0; found < referencedNames.Num(); found++ ) if ( referencedNames[found] == parameter.name ) duplicate = true;
		if ( !duplicate ) { referencedNames.Append( parameter.name ); referencedTypes.Append( parameter.type ); referencedParameters.Append( true ); }
	}
	for ( int variableIndex = 0; variableIndex < globals.Num(); variableIndex++ ) {
		const doomScriptVariable_t &variable = globals[variableIndex];
		if ( !ContainsGraphIdentifier( dataExpression, variable.name ) ) continue;
		bool duplicate = false;
		for ( int found = 0; found < referencedNames.Num(); found++ ) if ( referencedNames[found] == variable.name ) duplicate = true;
		if ( !duplicate ) { referencedNames.Append( variable.name ); referencedTypes.Append( variable.type ); referencedParameters.Append( false ); }
	}
	for ( int variableIndex = 0; variableIndex < sharedVariables.Num(); variableIndex++ ) {
		const doomScriptVariable_t &variable = sharedVariables[variableIndex];
		if ( !ContainsGraphIdentifier( dataExpression, variable.name ) ) continue;
		bool duplicate = false;
		for ( int found = 0; found < referencedNames.Num(); found++ ) if ( referencedNames[found] == variable.name ) duplicate = true;
		if ( !duplicate ) { referencedNames.Append( variable.name ); referencedTypes.Append( variable.type ); referencedParameters.Append( false ); }
	}

	idStr normalizedExpression = NormalizeBlueprintLine( dataExpression );
	bool directVariable = referencedNames.Num() == 1 && normalizedExpression == referencedNames[0];
	// Unreal-style unconnected pins keep their literal/constant default inline
	// on the Function Call node; they do not need a separate Value node.
	if ( referencedNames.Num() == 0 ) return;
	int valueNode = -1;
	int valuePin = 0;
	int expressionNode = -1;
	int baseY = nodes[primaryNode].y + 100 + inputPin * 92;
	if ( !directVariable ) {
		idStr dataKind = "expression";
		idStr dataKey = dataKind + "|" + functions[functionIndex].name + "|" + nodes[primaryNode].stableId +
			va( "|input:%d|", inputPin ) + normalizedExpression;
		unsigned int dataHash = BlueprintHash( dataKey.c_str() );
		int dataOccurrence = occurrences[dataHash]++;
		doomScriptGraphNode_t &dataNode = nodes.Alloc();
		dataNode.stableId = va( "%08X-%d", dataHash, dataOccurrence );
		dataNode.title = "Compute " + nodes[primaryNode].inputs[inputPin].name;
		if ( dataNode.title.Length() > 52 ) dataNode.title = dataNode.title.Left( 49 ) + "...";
		dataNode.kind = dataKind;
		dataNode.sourceText.Clear();
		dataNode.variableName.Clear();
		dataNode.expression = normalizedExpression;
		dataNode.valueType = nodes[primaryNode].inputs[inputPin].type;
		dataNode.pure = true;
		dataNode.dataOnly = true;
		dataNode.line = lineNumber;
		dataNode.functionIndex = functionIndex;
		dataNode.startOffset = -1;
		dataNode.endOffset = -1;
		for ( int referenceIndex = 0; referenceIndex < referencedNames.Num(); referenceIndex++ ) {
			doomScriptBlueprintPin_t &input = dataNode.inputs.Alloc();
			input.name = referencedNames[referenceIndex]; input.type = referencedTypes[referenceIndex];
		}
		doomScriptBlueprintPin_t &output = dataNode.outputs.Alloc();
		output.name = "result"; output.type = dataNode.valueType;
		dataNode.x = nodes[primaryNode].x - 205;
		dataNode.y = baseY;
		const char *saved = layout.GetString( dataNode.stableId, "" );
		if ( saved[0] ) sscanf( saved, "%d %d", &dataNode.x, &dataNode.y );
		valueNode = expressionNode = nodes.Num() - 1;
		functions[functionIndex].numNodes++;
	}

	for ( int referenceIndex = 0; referenceIndex < referencedNames.Num(); referenceIndex++ ) {
		if ( referencedParameters[referenceIndex] ) {
			int parameterPin = -1;
			for ( int pinIndex = 0; pinIndex < nodes[functions[functionIndex].firstNode].outputs.Num(); pinIndex++ ) {
				if ( nodes[functions[functionIndex].firstNode].outputs[pinIndex].name == referencedNames[referenceIndex] ) { parameterPin = pinIndex; break; }
			}
			if ( parameterPin >= 0 ) {
				if ( directVariable ) { valueNode = functions[functionIndex].firstNode; valuePin = parameterPin; }
				else {
					doomScriptGraphLink_t &parameterLink = links.Alloc();
					parameterLink.from = functions[functionIndex].firstNode; parameterLink.to = expressionNode;
					parameterLink.fromPin = parameterPin; parameterLink.toPin = referenceIndex; parameterLink.execution = false;
				}
			}
			continue;
		}
		idStr getKey = "getvar|" + functions[functionIndex].name + "|" + nodes[primaryNode].stableId +
			va( "|input:%d|", inputPin ) + referencedNames[referenceIndex];
		unsigned int getHash = BlueprintHash( getKey.c_str() );
		int getOccurrence = occurrences[getHash]++;
		doomScriptGraphNode_t &getNode = nodes.Alloc();
		getNode.stableId = va( "%08X-%d", getHash, getOccurrence );
		getNode.title = "Get " + referencedNames[referenceIndex];
		getNode.kind = "getvar";
		getNode.sourceText.Clear();
		getNode.variableName = referencedNames[referenceIndex];
		getNode.expression = referencedNames[referenceIndex];
		getNode.valueType = referencedTypes[referenceIndex];
		getNode.pure = true;
		getNode.dataOnly = true;
		getNode.line = lineNumber;
		getNode.functionIndex = functionIndex;
		getNode.startOffset = -1;
		getNode.endOffset = -1;
		doomScriptBlueprintPin_t &output = getNode.outputs.Alloc();
		output.name = referencedNames[referenceIndex]; output.type = referencedTypes[referenceIndex];
		getNode.x = nodes[primaryNode].x - ( directVariable ? 205 : 410 );
		getNode.y = baseY + referenceIndex * 78;
		const char *saved = layout.GetString( getNode.stableId, "" );
		if ( saved[0] ) sscanf( saved, "%d %d", &getNode.x, &getNode.y );
		int getNodeIndex = nodes.Num() - 1;
		functions[functionIndex].numNodes++;
		if ( directVariable ) valueNode = getNodeIndex;
		else {
			doomScriptGraphLink_t &dataLink = links.Alloc();
			dataLink.from = getNodeIndex; dataLink.to = expressionNode;
			dataLink.fromPin = 0; dataLink.toPin = referenceIndex; dataLink.execution = false;
		}
	}
	if ( valueNode >= 0 ) {
		doomScriptGraphLink_t &valueLink = links.Alloc();
		valueLink.from = valueNode; valueLink.to = primaryNode;
		valueLink.fromPin = valuePin; valueLink.toPin = inputPin; valueLink.execution = false;
	}
}

static idStr StripExpressionParentheses( const idStr &input ) {
	idStr value = NormalizeBlueprintLine( input );
	for ( ;; ) {
		if ( value.Length() < 2 || value[0] != '(' || value[value.Length() - 1] != ')' ) return value;
		int nesting = 0;
		char quote = '\0';
		bool enclosesWholeValue = true;
		for ( int index = 0; index < value.Length(); index++ ) {
			char current = value[index];
			if ( quote != '\0' ) {
				if ( current == quote && ( index == 0 || value[index - 1] != '\\' ) ) quote = '\0';
				continue;
			}
			if ( current == '"' || current == '\'' ) { quote = current; continue; }
			if ( current == '(' ) nesting++;
			else if ( current == ')' ) {
				nesting--;
				if ( nesting == 0 && index != value.Length() - 1 ) { enclosesWholeValue = false; break; }
			}
		}
		if ( !enclosesWholeValue || nesting != 0 ) return value;
		value = NormalizeBlueprintLine( value.Mid( 1, value.Length() - 2 ) );
	}
}

static bool AddNestedFunctionConditionGraph( idList<doomScriptGraphNode_t> &nodes, idList<doomScriptGraphLink_t> &links,
	idList<doomScriptGraphFunction_t> &functions, const idList<doomScriptVariable_t> &globals,
	const idList<doomScriptVariable_t> &sharedVariables, const DoomScriptNodeCatalog &catalog, int functionIndex,
	int primaryNode, int inputPin, const idStr &condition, int lineNumber, std::map<unsigned int, int> &occurrences,
	const idDict &layout ) {
	idStr value = StripExpressionParentheses( condition );
	bool negate = false;
	if ( value.Length() && value[0] == '!' && ( value.Length() == 1 || value[1] != '=' ) ) {
		negate = true;
		value = StripExpressionParentheses( value.Mid( 1, value.Length() - 1 ) );
	}
	int open = value.Find( '(' );
	if ( open <= 0 ) return false;
	int commandEnd = open;
	while ( commandEnd > 0 && isspace( (unsigned char)value[commandEnd - 1] ) ) commandEnd--;
	int commandStart = commandEnd;
	while ( commandStart > 0 && IsIdentifierCharacter( value[commandStart - 1] ) ) commandStart--;
	idStr command = value.Mid( commandStart, commandEnd - commandStart );
	idList<idStr> arguments;
	int callOpen = -1, callClose = -1;
	if ( command.Length() == 0 || !ParseFunctionCallArguments( value, command, arguments, &callOpen, &callClose ) ) return false;
	for ( int tail = callClose + 1; tail < value.Length(); tail++ ) if ( !isspace( (unsigned char)value[tail] ) ) return false;

	const doomScriptFunctionNode_t *signature = NULL;
	for ( int catalogIndex = 0; catalogIndex < catalog.Nodes().Num(); catalogIndex++ ) {
		if ( catalog.Nodes()[catalogIndex].command.Icmp( command ) == 0 ) { signature = &catalog.Nodes()[catalogIndex]; break; }
	}
	idStr callKey = "functionexpr|" + functions[functionIndex].name + "|" + nodes[primaryNode].stableId + "|" + command;
	unsigned int callHash = BlueprintHash( callKey.c_str() );
	int callOccurrence = occurrences[callHash]++;
	doomScriptGraphNode_t &callNode = nodes.Alloc();
	callNode.stableId = va( "%08X-%d", callHash, callOccurrence );
	callNode.title = signature != NULL ? signature->title : command;
	callNode.kind = signature != NULL ? "function:" + signature->stableId : "function:script-untyped";
	callNode.sourceText.Clear();
	callNode.variableName = command;
	callNode.expression = value;
	callNode.ownerStableId = nodes[primaryNode].stableId;
	callNode.inputValues = arguments;
	callNode.valueType = signature != NULL ? signature->returnType : "boolean";
	callNode.pure = true;
	callNode.dataOnly = true;
	callNode.line = lineNumber;
	callNode.functionIndex = functionIndex;
	callNode.startOffset = -1;
	callNode.endOffset = -1;
	if ( signature != NULL ) {
		for ( int pin = 0; pin < signature->pins.Num(); pin++ ) {
			callNode.inputs.Append( signature->pins[pin] );
			if ( pin >= callNode.inputValues.Num() ) callNode.inputValues.Append( DefaultPinValue( signature->pins[pin] ) );
		}
	} else {
		for ( int pin = 0; pin < arguments.Num(); pin++ ) {
			doomScriptBlueprintPin_t &input = callNode.inputs.Alloc();
			input.name = va( "argument %d", pin + 1 ); input.type = "float";
		}
	}
	doomScriptBlueprintPin_t &result = callNode.outputs.Alloc();
	result.name = "result"; result.type = callNode.valueType;
	callNode.x = nodes[primaryNode].x - ( negate ? 440 : 225 );
	callNode.y = nodes[primaryNode].y + 105;
	const char *callSaved = layout.GetString( callNode.stableId, "" );
	if ( callSaved[0] ) sscanf( callSaved, "%d %d", &callNode.x, &callNode.y );
	int callNodeIndex = nodes.Num() - 1;
	functions[functionIndex].numNodes++;
	int argumentCount = Min( nodes[callNodeIndex].inputs.Num(), nodes[callNodeIndex].inputValues.Num() );
	for ( int pin = 0; pin < argumentCount; pin++ ) {
		idStr argument = nodes[callNodeIndex].inputValues[pin];
		AddFunctionInputGraph( nodes, links, functions, globals, sharedVariables, functionIndex,
			callNodeIndex, pin, argument, lineNumber, occurrences, layout );
	}

	int outputNode = callNodeIndex;
	if ( negate ) {
		idStr notKey = "operator:not|" + functions[functionIndex].name + "|" + nodes[primaryNode].stableId + "|" + command;
		unsigned int notHash = BlueprintHash( notKey.c_str() );
		int notOccurrence = occurrences[notHash]++;
		doomScriptGraphNode_t &notNode = nodes.Alloc();
		notNode.stableId = va( "%08X-%d", notHash, notOccurrence );
		notNode.title = "Not Boolean";
		notNode.kind = "operator:not";
		notNode.sourceText.Clear();
		notNode.expression = condition;
		notNode.ownerStableId = nodes[primaryNode].stableId;
		notNode.valueType = "boolean";
		notNode.pure = true;
		notNode.dataOnly = true;
		notNode.line = lineNumber;
		notNode.functionIndex = functionIndex;
		notNode.startOffset = -1;
		notNode.endOffset = -1;
		doomScriptBlueprintPin_t &input = notNode.inputs.Alloc(); input.name = "value"; input.type = "boolean";
		notNode.inputValues.Append( value );
		doomScriptBlueprintPin_t &output = notNode.outputs.Alloc(); output.name = "result"; output.type = "boolean";
		notNode.x = nodes[primaryNode].x - 210;
		notNode.y = nodes[primaryNode].y + 105;
		const char *notSaved = layout.GetString( notNode.stableId, "" );
		if ( notSaved[0] ) sscanf( notSaved, "%d %d", &notNode.x, &notNode.y );
		int notNodeIndex = nodes.Num() - 1;
		functions[functionIndex].numNodes++;
		doomScriptGraphLink_t &notInputLink = links.Alloc();
		notInputLink.from = callNodeIndex; notInputLink.to = notNodeIndex;
		notInputLink.fromPin = 0; notInputLink.toPin = 0; notInputLink.execution = false;
		outputNode = notNodeIndex;
	}
	doomScriptGraphLink_t &conditionLink = links.Alloc();
	conditionLink.from = outputNode; conditionLink.to = primaryNode;
	conditionLink.fromPin = 0; conditionLink.toPin = inputPin; conditionLink.execution = false;
	return true;
}

void DoomScriptBlueprintDocument::Parse( const DoomScriptNodeCatalog &catalog, const idDict &layout ) {
	nodes.Clear();
	links.Clear();
	functions.Clear();
	globals.Clear();
	std::map<unsigned int, int> occurrences;
	int braceDepth = 0;
	int functionDepth = -1;
	int functionIndex = -1;
	std::vector<blueprintExecutionSource_t> executionSources;
	std::vector<blueprintControlFrame_t> controlFrames;
	int logicColumn = 0;
	bool blockComment = false;
	idStr pendingHeader;
	bool hasPendingHeader = false;
	int pendingStartOffset = -1;
	int cursor = 0;
	int lineNumber = 1;

	while ( cursor <= source.Length() ) {
		int lineEnd = source.Find( '\n', cursor );
		if ( lineEnd < 0 ) lineEnd = source.Length();
		idStr raw = source.Mid( cursor, lineEnd - cursor );
		idStr code = StripLineComments( raw, blockComment );
		if ( code.Length() != 0 && code[0] != '#' ) {
			if ( functionDepth < 0 ) {
				if ( !hasPendingHeader && LooksLikeFunctionHeader( code ) ) {
					pendingHeader = code;
					hasPendingHeader = true;
					pendingStartOffset = cursor;
				} else if ( hasPendingHeader && pendingHeader.Find( '{' ) < 0 ) {
					pendingHeader += " ";
					pendingHeader += code;
				}
				if ( hasPendingHeader && pendingHeader.Find( '{' ) >= 0 ) {
					doomScriptGraphFunction_t &function = functions.Alloc();
					function.name = BlueprintFunctionName( pendingHeader );
					ParseFunctionParameters( pendingHeader, function.parameters );
					function.firstNode = nodes.Num();
					function.numNodes = 0;
					function.startOffset = pendingStartOffset;
					int openBrace = raw.Find( '{' );
					function.openOffset = openBrace >= 0 ? cursor + openBrace : cursor;
					function.closeOffset = -1;
					functionIndex = functions.Num() - 1;
					idStr key = "entry|" + function.name;
					unsigned int hash = BlueprintHash( key.c_str() );
					int occurrence = occurrences[hash]++;
					doomScriptGraphNode_t &node = nodes.Alloc();
					node.stableId = va( "%08X-%d", hash, occurrence );
					node.title = "Event " + function.name;
					node.kind = "event";
					node.startOffset = pendingStartOffset;
					node.endOffset = function.openOffset;
					node.sourceText = source.Mid( node.startOffset, node.endOffset - node.startOffset );
					node.sourceText.StripTrailingWhitespace();
					node.variableName.Clear();
					node.expression.Clear();
					node.valueType = "void";
					for ( int parameterIndex = 0; parameterIndex < function.parameters.Num(); parameterIndex++ ) {
						node.outputs.Append( function.parameters[parameterIndex] );
					}
					node.pure = false;
					node.dataOnly = false;
					node.line = lineNumber;
					node.functionIndex = functionIndex;
					node.x = 40;
					node.y = 40;
					const char *saved = layout.GetString( node.stableId, "" );
					if ( saved[0] ) sscanf( saved, "%d %d", &node.x, &node.y );
					function.numNodes = 1;
					executionSources.clear();
					controlFrames.clear();
					AppendExecutionSource( executionSources, nodes.Num() - 1, 0 );
					logicColumn = 0;
					functionDepth = braceDepth + CountBrace( pendingHeader, '{' ) - CountBrace( pendingHeader, '}' );
					braceDepth = functionDepth;
					hasPendingHeader = false;
				} else {
					braceDepth += CountBrace( code, '{' ) - CountBrace( code, '}' );
					if ( hasPendingHeader && code.Length() && code[code.Length() - 1] == ';' ) {
						hasPendingHeader = false;
						pendingStartOffset = -1;
					} else if ( !hasPendingHeader ) {
						idList<doomScriptVariable_t> declaredVariables;
						if ( ParseVariableDeclarations( code, declaredVariables ) ) {
							for ( int variableIndex = 0; variableIndex < declaredVariables.Num(); variableIndex++ ) {
								declaredVariables[variableIndex].line = lineNumber;
								declaredVariables[variableIndex].startOffset = cursor;
								declaredVariables[variableIndex].endOffset = lineEnd < source.Length() ? lineEnd + 1 : lineEnd;
								globals.Append( declaredVariables[variableIndex] );
							}
						}
					}
				}
			} else {
				int nextDepth = braceDepth + CountBrace( code, '{' ) - CountBrace( code, '}' );
				idStr normalized = NormalizeBlueprintStatement( code );
				int statementDepth = Max( functionDepth, braceDepth - LeadingClosingBraces( code ) );
				bool startsElse = StartsWord( normalized, "else" );
				if ( startsElse ) {
					int elseBodyDepth = Max( statementDepth + 1, nextDepth );
					while ( !controlFrames.empty() && controlFrames.back().bodyDepth > elseBodyDepth ) {
						FinishControlFrame( controlFrames, executionSources, links );
					}
				}
				if ( startsElse && !controlFrames.empty() && !controlFrames.back().loop && !controlFrames.back().inElse &&
					controlFrames.back().bodyDepth == Max( statementDepth + 1, nextDepth ) ) {
					blueprintControlFrame_t &frame = controlFrames.back();
					frame.trueTails = executionSources;
					frame.inElse = true;
					executionSources.clear();
					AppendExecutionSource( executionSources, frame.node, 1 );
					if ( normalized.Find( "if" ) < 0 ) normalized.Clear();
				}
				if ( !startsElse ) {
					while ( !controlFrames.empty() && controlFrames.back().bodyDepth > statementDepth ) {
						FinishControlFrame( controlFrames, executionSources, links );
					}
				}
				idList<doomScriptVariable_t> declaredVariables;
				bool isLocalDeclaration = ParseVariableDeclarations( normalized, declaredVariables );
				if ( isLocalDeclaration ) {
					for ( int variableIndex = 0; variableIndex < declaredVariables.Num(); variableIndex++ ) {
						declaredVariables[variableIndex].line = lineNumber;
						declaredVariables[variableIndex].startOffset = cursor;
						declaredVariables[variableIndex].endOffset = lineEnd < source.Length() ? lineEnd + 1 : lineEnd;
						functions[functionIndex].locals.Append( declaredVariables[variableIndex] );
					}
					normalized.Clear();
				}
				if ( normalized.Length() && normalized != "{" && normalized != "}" ) {
					idStr kind = BlueprintNodeKind( normalized );
					idStr assignmentTarget;
					idStr assignmentValue;
					bool isAssignment = ParseAssignmentStatement( normalized, assignmentTarget, assignmentValue );
					if ( isAssignment ) kind = "setvar";
					idStr key = kind + "|" + functions[functionIndex].name + "|" + normalized;
					unsigned int hash = BlueprintHash( key.c_str() );
					int occurrence = occurrences[hash]++;
					doomScriptGraphNode_t &node = nodes.Alloc();
					node.stableId = va( "%08X-%d", hash, occurrence );
					node.title = isAssignment ? "Set " + assignmentTarget : normalized;
					if ( node.title.Length() > 68 ) node.title = node.title.Left( 65 ) + "...";
					node.kind = kind;
					node.variableName = assignmentTarget;
					node.expression = isAssignment ? assignmentValue : ExtractControlCondition( normalized );
					node.valueType = isAssignment ? FindGraphVariableType( globals, sharedVariables, functions[functionIndex], assignmentTarget ) : "boolean";
					if ( node.valueType.Length() == 0 ) node.valueType = "float";
					node.startOffset = cursor;
					node.endOffset = lineEnd;
					if ( node.endOffset > node.startOffset && source[node.endOffset - 1] == '\r' ) node.endOffset--;
					node.sourceText = source.Mid( node.startOffset, node.endOffset - node.startOffset );
					node.pure = false;
					node.dataOnly = false;
					node.line = lineNumber;
					node.functionIndex = functionIndex;
					node.x = 330 + logicColumn * 280;
					node.y = 40 + Max( 0, statementDepth - functionDepth ) * 160;
					const char *saved = layout.GetString( node.stableId, "" );
					if ( saved[0] ) sscanf( saved, "%d %d", &node.x, &node.y );
					if ( kind == "setvar" ) {
						doomScriptBlueprintPin_t &valuePin = node.inputs.Alloc();
						valuePin.type = node.valueType;
						valuePin.name = "value";
					} else if ( kind == "branch" && ( !StartsWord( normalized, "else" ) || normalized.Find( "if" ) >= 0 ) ) {
						doomScriptBlueprintPin_t &condition = node.inputs.Alloc();
						condition.type = "boolean";
						condition.name = "condition";
					} else if ( kind == "loop" ) {
						doomScriptBlueprintPin_t &condition = node.inputs.Alloc();
						condition.type = "boolean";
						condition.name = "condition";
					}
					bool canBeFunctionCall = kind == "call" || kind == "statement";
					for ( int catalogIndex = 0; canBeFunctionCall && !isLocalDeclaration && catalogIndex < catalog.Nodes().Num(); catalogIndex++ ) {
						idStr needle = catalog.Nodes()[catalogIndex].command + "(";
						if ( normalized.Find( needle ) >= 0 ) {
							node.kind = "function:" + catalog.Nodes()[catalogIndex].stableId;
							node.title = catalog.Nodes()[catalogIndex].title;
							node.variableName = catalog.Nodes()[catalogIndex].command;
							node.valueType = catalog.Nodes()[catalogIndex].returnType;
							node.pure = catalog.Nodes()[catalogIndex].pure;
							int callOpen = -1, callClose = -1;
							ParseFunctionCallArguments( normalized, catalog.Nodes()[catalogIndex].command, node.inputValues, &callOpen, &callClose );
							int callStart = callOpen - catalog.Nodes()[catalogIndex].command.Length();
							while ( callStart > 0 && ( IsIdentifierCharacter( normalized[callStart - 1] ) || normalized[callStart - 1] == '.' ) ) callStart--;
							node.expression = callOpen >= 0 && callClose > callOpen ? normalized.Mid( callStart, callClose - callStart + 1 ) : normalized;
							for ( int pinIndex = 0; pinIndex < catalog.Nodes()[catalogIndex].pins.Num(); pinIndex++ ) {
								node.inputs.Append( catalog.Nodes()[catalogIndex].pins[pinIndex] );
								if ( pinIndex >= node.inputValues.Num() ) node.inputValues.Append( DefaultPinValue( catalog.Nodes()[catalogIndex].pins[pinIndex] ) );
							}
							if ( catalog.Nodes()[catalogIndex].returnType != "void" ) {
								doomScriptBlueprintPin_t &result = node.outputs.Alloc();
								result.type = catalog.Nodes()[catalogIndex].returnType;
								result.name = "result";
							}
							break;
						}
					}
					if ( node.kind == "call" ) {
						int callOpen = normalized.Find( '(' );
						int commandEnd = callOpen;
						while ( commandEnd > 0 && isspace( (unsigned char)normalized[commandEnd - 1] ) ) commandEnd--;
						int commandStart = commandEnd;
						while ( commandStart > 0 && IsIdentifierCharacter( normalized[commandStart - 1] ) ) commandStart--;
						idStr command = commandEnd > commandStart ? normalized.Mid( commandStart, commandEnd - commandStart ) : "Call";
						int callClose = -1;
						node.inputValues.Clear();
						if ( ParseFunctionCallArguments( normalized, command, node.inputValues, &callOpen, &callClose ) ) {
							node.kind = "function:script-untyped";
							node.title = command;
							node.variableName = command;
							node.valueType = "void";
							node.expression = normalized.Mid( commandStart, callClose - commandStart + 1 );
							for ( int inputIndex = 0; inputIndex < node.inputValues.Num(); inputIndex++ ) {
								doomScriptBlueprintPin_t &input = node.inputs.Alloc();
								input.name = va( "argument %d", inputIndex + 1 );
								input.type = FindGraphVariableType( globals, sharedVariables, functions[functionIndex], node.inputValues[inputIndex] );
								if ( input.type.Length() == 0 ) {
									if ( node.inputValues[inputIndex] == "true" || node.inputValues[inputIndex] == "false" ) input.type = "boolean";
									else if ( node.inputValues[inputIndex].Length() && node.inputValues[inputIndex][0] == '"' ) input.type = "string";
									else input.type = "float";
								}
							}
						}
					}
					int primaryNode = nodes.Num() - 1;
					AddExecutionLinks( links, executionSources, primaryNode );
					executionSources.clear();
					if ( nodes[primaryNode].kind == "branch" ) {
						blueprintControlFrame_t frame;
						frame.node = primaryNode;
						frame.bodyDepth = Max( statementDepth + 1, nextDepth );
						frame.loop = false;
						frame.inElse = false;
						controlFrames.push_back( frame );
						AppendExecutionSource( executionSources, primaryNode, 0 );
					} else if ( nodes[primaryNode].kind == "loop" ) {
						blueprintControlFrame_t frame;
						frame.node = primaryNode;
						frame.bodyDepth = Max( statementDepth + 1, nextDepth );
						frame.loop = true;
						frame.inElse = false;
						controlFrames.push_back( frame );
						AppendExecutionSource( executionSources, primaryNode, 0 );
					} else if ( nodes[primaryNode].kind == "break" || nodes[primaryNode].kind == "continue" ) {
						for ( int frameIndex = (int)controlFrames.size() - 1; frameIndex >= 0; frameIndex-- ) {
							if ( !controlFrames[frameIndex].loop ) continue;
							if ( nodes[primaryNode].kind == "break" ) {
								AppendExecutionSource( controlFrames[frameIndex].breakTails, primaryNode, 0 );
							} else {
								std::vector<blueprintExecutionSource_t> continueSource;
								AppendExecutionSource( continueSource, primaryNode, 0 );
								AddExecutionLinks( links, continueSource, controlFrames[frameIndex].node );
							}
							break;
						}
					} else if ( nodes[primaryNode].kind != "return" ) {
						AppendExecutionSource( executionSources, primaryNode, 0 );
					}
					functions[functionIndex].numNodes++;

					idStr dataExpression = nodes[primaryNode].expression;
					bool structuredFunctionExpression = false;
					if ( dataExpression.Length() != 0 && nodes[primaryNode].inputs.Num() != 0 &&
						( kind == "setvar" || kind == "branch" || kind == "loop" ) ) {
						structuredFunctionExpression = AddNestedFunctionConditionGraph( nodes, links, functions, globals,
							sharedVariables, catalog, functionIndex, primaryNode, 0, dataExpression, lineNumber, occurrences, layout );
					}
					if ( dataExpression.Length() != 0 && nodes[primaryNode].inputs.Num() != 0 &&
						( kind == "setvar" || kind == "branch" || kind == "loop" ) && !structuredFunctionExpression ) {
						idList<idStr> referencedNames;
						idList<idStr> referencedTypes;
						idList<bool> referencedParameters;
						for ( int variableIndex = 0; variableIndex < functions[functionIndex].locals.Num(); variableIndex++ ) {
							const doomScriptVariable_t &variable = functions[functionIndex].locals[variableIndex];
							if ( ContainsGraphIdentifier( dataExpression, variable.name ) ) {
								referencedNames.Append( variable.name );
								referencedTypes.Append( variable.type );
								referencedParameters.Append( false );
							}
						}
						for ( int parameterIndex = 0; parameterIndex < functions[functionIndex].parameters.Num(); parameterIndex++ ) {
							const doomScriptBlueprintPin_t &parameter = functions[functionIndex].parameters[parameterIndex];
							if ( !ContainsGraphIdentifier( dataExpression, parameter.name ) ) continue;
							bool duplicate = false;
							for ( int found = 0; found < referencedNames.Num(); found++ ) if ( referencedNames[found] == parameter.name ) duplicate = true;
							if ( !duplicate ) { referencedNames.Append( parameter.name ); referencedTypes.Append( parameter.type ); referencedParameters.Append( true ); }
						}
						for ( int globalIndex = 0; globalIndex < globals.Num(); globalIndex++ ) {
							const doomScriptVariable_t &variable = globals[globalIndex];
							if ( !ContainsGraphIdentifier( dataExpression, variable.name ) ) continue;
							bool duplicate = false;
							for ( int found = 0; found < referencedNames.Num(); found++ ) if ( referencedNames[found] == variable.name ) duplicate = true;
							if ( !duplicate ) { referencedNames.Append( variable.name ); referencedTypes.Append( variable.type ); referencedParameters.Append( false ); }
						}
						for ( int sharedIndex = 0; sharedIndex < sharedVariables.Num(); sharedIndex++ ) {
							const doomScriptVariable_t &variable = sharedVariables[sharedIndex];
							if ( !ContainsGraphIdentifier( dataExpression, variable.name ) ) continue;
							bool duplicate = false;
							for ( int found = 0; found < referencedNames.Num(); found++ ) if ( referencedNames[found] == variable.name ) duplicate = true;
							if ( !duplicate ) { referencedNames.Append( variable.name ); referencedTypes.Append( variable.type ); referencedParameters.Append( false ); }
						}

						idStr normalizedExpression = NormalizeBlueprintLine( dataExpression );
						bool directVariable = referencedNames.Num() == 1 && normalizedExpression == referencedNames[0];
						int valueNode = -1;
						int valuePin = 0;
						int expressionNode = -1;
						if ( !directVariable ) {
							idStr dataKind = referencedNames.Num() == 0 && IsGraphLiteral( normalizedExpression ) ? "literal" : "expression";
							idStr dataKey = dataKind + "|" + functions[functionIndex].name + "|" + nodes[primaryNode].stableId + "|" + normalizedExpression;
							unsigned int dataHash = BlueprintHash( dataKey.c_str() );
							int dataOccurrence = occurrences[dataHash]++;
							doomScriptGraphNode_t &dataNode = nodes.Alloc();
							dataNode.stableId = va( "%08X-%d", dataHash, dataOccurrence );
							dataNode.title = dataKind == "literal" ? "Value " + normalizedExpression :
								( kind == "setvar" ? "Compute Value" : "Evaluate Condition" );
							if ( dataNode.title.Length() > 52 ) dataNode.title = dataNode.title.Left( 49 ) + "...";
							dataNode.kind = dataKind;
							dataNode.sourceText.Clear();
							dataNode.variableName.Clear();
							dataNode.expression = normalizedExpression;
							dataNode.ownerStableId = nodes[primaryNode].stableId;
							dataNode.valueType = nodes[primaryNode].inputs[0].type;
							dataNode.pure = true;
							dataNode.dataOnly = true;
							dataNode.line = lineNumber;
							dataNode.functionIndex = functionIndex;
							dataNode.startOffset = -1;
							dataNode.endOffset = -1;
							for ( int refIndex = 0; refIndex < referencedNames.Num(); refIndex++ ) {
								doomScriptBlueprintPin_t &input = dataNode.inputs.Alloc();
								input.name = referencedNames[refIndex];
								input.type = referencedTypes[refIndex];
							}
							doomScriptBlueprintPin_t &output = dataNode.outputs.Alloc();
							output.name = "result";
							output.type = dataNode.valueType;
							dataNode.x = nodes[primaryNode].x - 205;
							dataNode.y = nodes[primaryNode].y + 105;
							const char *dataSaved = layout.GetString( dataNode.stableId, "" );
							if ( dataSaved[0] ) sscanf( dataSaved, "%d %d", &dataNode.x, &dataNode.y );
							valueNode = expressionNode = nodes.Num() - 1;
							functions[functionIndex].numNodes++;
						}

						for ( int refIndex = 0; refIndex < referencedNames.Num(); refIndex++ ) {
							if ( referencedParameters[refIndex] ) {
								int parameterPin = -1;
								for ( int pinIndex = 0; pinIndex < nodes[functions[functionIndex].firstNode].outputs.Num(); pinIndex++ ) {
									if ( nodes[functions[functionIndex].firstNode].outputs[pinIndex].name == referencedNames[refIndex] ) { parameterPin = pinIndex; break; }
								}
								if ( parameterPin >= 0 ) {
									if ( directVariable ) { valueNode = functions[functionIndex].firstNode; valuePin = parameterPin; }
									else {
										doomScriptGraphLink_t &parameterLink = links.Alloc();
										parameterLink.from = functions[functionIndex].firstNode;
										parameterLink.to = expressionNode;
										parameterLink.fromPin = parameterPin;
										parameterLink.toPin = refIndex;
										parameterLink.execution = false;
									}
								}
								continue;
							}
							idStr getKey = "getvar|" + functions[functionIndex].name + "|" + nodes[primaryNode].stableId + "|" + referencedNames[refIndex];
							unsigned int getHash = BlueprintHash( getKey.c_str() );
							int getOccurrence = occurrences[getHash]++;
							doomScriptGraphNode_t &getNode = nodes.Alloc();
							getNode.stableId = va( "%08X-%d", getHash, getOccurrence );
							getNode.title = "Get " + referencedNames[refIndex];
							getNode.kind = "getvar";
							getNode.sourceText.Clear();
							getNode.variableName = referencedNames[refIndex];
							getNode.expression = referencedNames[refIndex];
							getNode.valueType = referencedTypes[refIndex];
							getNode.pure = true;
							getNode.dataOnly = true;
							getNode.line = lineNumber;
							getNode.functionIndex = functionIndex;
							getNode.startOffset = -1;
							getNode.endOffset = -1;
							doomScriptBlueprintPin_t &output = getNode.outputs.Alloc();
							output.name = referencedNames[refIndex];
							output.type = referencedTypes[refIndex];
							getNode.x = nodes[primaryNode].x - ( directVariable ? 205 : 410 );
							getNode.y = nodes[primaryNode].y + 105 + refIndex * 84;
							const char *getSaved = layout.GetString( getNode.stableId, "" );
							if ( getSaved[0] ) sscanf( getSaved, "%d %d", &getNode.x, &getNode.y );
							int getNodeIndex = nodes.Num() - 1;
							functions[functionIndex].numNodes++;
							if ( directVariable ) {
								valueNode = getNodeIndex;
							} else {
								doomScriptGraphLink_t &dataLink = links.Alloc();
								dataLink.from = getNodeIndex;
								dataLink.to = expressionNode;
								dataLink.fromPin = 0;
								dataLink.toPin = refIndex;
								dataLink.execution = false;
							}
						}
						if ( valueNode >= 0 ) {
							doomScriptGraphLink_t &valueLink = links.Alloc();
							valueLink.from = valueNode;
							valueLink.to = primaryNode;
							valueLink.fromPin = valuePin;
							valueLink.toPin = 0;
							valueLink.execution = false;
						}
					}
					if ( nodes[primaryNode].kind.Find( "function:" ) == 0 ) {
						int argumentCount = Min( nodes[primaryNode].inputs.Num(), nodes[primaryNode].inputValues.Num() );
						for ( int inputPin = 0; inputPin < argumentCount; inputPin++ ) {
							idStr argumentValue = nodes[primaryNode].inputValues[inputPin];
							AddFunctionInputGraph( nodes, links, functions, globals, sharedVariables, functionIndex,
								primaryNode, inputPin, argumentValue, lineNumber, occurrences, layout );
						}
					}
					logicColumn++;
				}
				braceDepth = nextDepth;
				if ( braceDepth < functionDepth ) {
					while ( !controlFrames.empty() ) {
						FinishControlFrame( controlFrames, executionSources, links );
					}
					int close = raw.Last( '}' );
					functions[functionIndex].closeOffset = close >= 0 ? cursor + close : cursor;
					functionDepth = -1;
					functionIndex = -1;
					executionSources.clear();
					controlFrames.clear();
				}
			}
		}
		if ( lineEnd == source.Length() ) break;
		cursor = lineEnd + 1;
		lineNumber++;
	}
}

idStr DoomScriptBlueprintDocument::BuildFileText() const {
	idStr output = source;
	output.StripTrailingWhitespace();
	idStr sourceForHash = output + "\r\n";
	output += "\r\n\r\n";
	output += DSBP_BEGIN;
	output += "\r\n//@source ";
	output += HexHash( BlueprintHash( sourceForHash.c_str() ) );
	output += "\r\n//@catalog ";
	output += catalogChecksum;
	output += "\r\n";
	for ( int index = 0; index < nodes.Num(); index++ ) {
		output += va( "//@node %s %d %d\r\n", nodes[index].stableId.c_str(), nodes[index].x, nodes[index].y );
	}
	output += DSBP_END;
	output += "\r\n";
	return output;
}

bool DoomScriptBlueprintDocument::Save() {
	idStr output = BuildFileText();
	if ( fileSystem->WriteFile( path, output.c_str(), output.Length(), "fs_devpath" ) < 0 ) return false;
	dirty = false;
	return true;
}

static const char *DefaultPinValue( const idStr &type ) {
	if ( type == "string" ) return "\"\"";
	if ( type == "vector" ) return "'0 0 0'";
	if ( type == "entity" ) return "$null_entity";
	return "0";
}

static const char *DefaultPinValue( const doomScriptBlueprintPin_t &pin ) {
	return pin.enumValues.Num() != 0 ? pin.enumValues[0].c_str() : DefaultPinValue( pin.type );
}

bool DoomScriptBlueprintDocument::InsertFunctionCall( int functionIndex, const doomScriptFunctionNode_t &functionNode, const DoomScriptNodeCatalog &catalog, idStr &error ) {
	if ( functionIndex < 0 || functionIndex >= functions.Num() || functions[functionIndex].closeOffset < 0 ) {
		error = "Select a function with a complete body first.";
		return false;
	}
	bool objectMethod = functions[functionIndex].name.Find( "::" ) >= 0;
	if ( functionNode.receiver == "object" && !objectMethod ) {
		error = "This event needs an object receiver. Add it inside an object method.";
		return false;
	}

	const char *newline = source.Find( "\r\n" ) >= 0 ? "\r\n" : "\n";
	idStr invocation = newline;
	invocation += "\t";
	if ( functionNode.returnType != "void" ) {
		invocation += functionNode.returnType;
		invocation += va( " blueprintResult%d = ", nodes.Num() );
	}
	if ( functionNode.receiver == "sys" ) invocation += "sys.";
	else if ( functionNode.receiver == "object" ) invocation += "self.";
	invocation += functionNode.command;
	invocation += "( ";
	for ( int index = 0; index < functionNode.pins.Num(); index++ ) {
		if ( index != 0 ) invocation += ", ";
		invocation += DefaultPinValue( functionNode.pins[index] );
	}
	invocation += " );";

	idDict positions;
	for ( int index = 0; index < nodes.Num(); index++ ) positions.Set( nodes[index].stableId, va( "%d %d", nodes[index].x, nodes[index].y ) );
	source = source.Left( functions[functionIndex].closeOffset ) + invocation + newline +
		source.Mid( functions[functionIndex].closeOffset, source.Length() - functions[functionIndex].closeOffset );
	Parse( catalog, positions );
	dirty = true;
	return true;
}

bool DoomScriptBlueprintDocument::InsertSetVariable( int functionIndex, const char *name, const DoomScriptNodeCatalog &catalog, idStr &error ) {
	if ( functionIndex < 0 || functionIndex >= functions.Num() || functions[functionIndex].closeOffset < 0 ) {
		error = "Select an Event with a complete body first.";
		return false;
	}
	idStr variableName = name == NULL ? "" : name;
	idStr type = FindGraphVariableType( globals, sharedVariables, functions[functionIndex], variableName );
	if ( type.Length() == 0 ) {
		error = "Select a variable visible to this Event.";
		return false;
	}
	idDict positions;
	for ( int index = 0; index < nodes.Num(); index++ ) positions.Set( nodes[index].stableId, va( "%d %d", nodes[index].x, nodes[index].y ) );
	const char *newline = source.Find( "\r\n" ) >= 0 ? "\r\n" : "\n";
	idStr statement = newline;
	statement += "\t" + variableName + " = " + DefaultPinValue( type ) + ";";
	source = source.Left( functions[functionIndex].closeOffset ) + statement + newline +
		source.Mid( functions[functionIndex].closeOffset, source.Length() - functions[functionIndex].closeOffset );
	Parse( catalog, positions );
	dirty = true;
	return true;
}

static bool IsVariableNameValid( const char *name ) {
	if ( name == NULL || !( isalpha( (unsigned char)name[0] ) || name[0] == '_' ) ) return false;
	for ( int index = 1; name[index]; index++ ) {
		if ( !( isalnum( (unsigned char)name[index] ) || name[index] == '_' ) ) return false;
	}
	return idStr::Icmp( name, "if" ) != 0 && idStr::Icmp( name, "while" ) != 0 &&
		idStr::Icmp( name, "return" ) != 0 && idStr::Icmp( name, "thread" ) != 0;
}

bool DoomScriptBlueprintDocument::AddVariable( bool global, int functionIndex, const char *type, const char *name,
	const char *defaultValue, const DoomScriptNodeCatalog &catalog, idStr &error ) {
	if ( !IsVariableNameValid( type ) || !IsVariableNameValid( name ) ) {
		error = "Variable type and name must be valid DoomScript identifiers.";
		return false;
	}
	if ( !global && ( functionIndex < 0 || functionIndex >= functions.Num() ) ) {
		error = "Select an Event before declaring a local variable.";
		return false;
	}
	const idList<doomScriptVariable_t> &variables = global ? globals : functions[functionIndex].locals;
	for ( int index = 0; index < variables.Num(); index++ ) {
		if ( variables[index].name.Icmp( name ) == 0 ) {
			error = "A variable with that name already exists in this scope.";
			return false;
		}
	}
	if ( !global ) {
		for ( int index = 0; index < functions[functionIndex].parameters.Num(); index++ ) {
			if ( functions[functionIndex].parameters[index].name.Icmp( name ) == 0 ) {
				error = "An Event parameter already uses that name.";
				return false;
			}
		}
	}

	idDict positions;
	for ( int index = 0; index < nodes.Num(); index++ ) positions.Set( nodes[index].stableId, va( "%d %d", nodes[index].x, nodes[index].y ) );
	const char *newline = source.Find( "\r\n" ) >= 0 ? "\r\n" : "\n";
	idStr declaration = type;
	declaration += " ";
	declaration += name;
	idStr initial = defaultValue == NULL ? "" : defaultValue;
	initial.StripTrailingWhitespace();
	if ( initial.Length() ) declaration += " = " + initial;
	declaration += ";";

	if ( global ) {
		int offset = functions.Num() ? functions[0].startOffset : source.Length();
		idStr insertion = declaration + newline + newline;
		source = source.Left( offset ) + insertion + source.Mid( offset, source.Length() - offset );
	} else {
		int offset = functions[functionIndex].openOffset + 1;
		idStr insertion = newline;
		insertion += "\t" + declaration;
		insertion += newline;
		source = source.Left( offset ) + insertion + source.Mid( offset, source.Length() - offset );
	}
	Parse( catalog, positions );
	dirty = true;
	return true;
}

bool DoomScriptBlueprintDocument::RemoveVariable( bool global, int functionIndex, int variableIndex,
	const DoomScriptNodeCatalog &catalog, idStr &error ) {
	if ( !global && ( functionIndex < 0 || functionIndex >= functions.Num() ) ) {
		error = "Select an Event first.";
		return false;
	}
	const idList<doomScriptVariable_t> &variables = global ? globals : functions[functionIndex].locals;
	if ( variableIndex < 0 || variableIndex >= variables.Num() ) {
		error = "Select a variable to remove.";
		return false;
	}
	int start = variables[variableIndex].startOffset;
	int end = variables[variableIndex].endOffset;
	if ( start < 0 || end <= start || end > source.Length() ) {
		error = "The declaration source range is invalid.";
		return false;
	}
	idDict positions;
	for ( int index = 0; index < nodes.Num(); index++ ) positions.Set( nodes[index].stableId, va( "%d %d", nodes[index].x, nodes[index].y ) );
	idStr replacement;
	int declarationsOnLine = 0;
	for ( int index = 0; index < variables.Num(); index++ ) {
		if ( variables[index].startOffset == start && variables[index].endOffset == end ) declarationsOnLine++;
	}
	if ( declarationsOnLine > 1 ) {
		idStr original = source.Mid( start, end - start );
		int indentLength = 0;
		while ( indentLength < original.Length() && ( original[indentLength] == ' ' || original[indentLength] == '\t' ) ) indentLength++;
		replacement = original.Left( indentLength ) + variables[variableIndex].type + " ";
		bool first = true;
		for ( int index = 0; index < variables.Num(); index++ ) {
			if ( index == variableIndex || variables[index].startOffset != start || variables[index].endOffset != end ) continue;
			if ( !first ) replacement += ", ";
			replacement += variables[index].name;
			if ( variables[index].defaultValue.Length() ) replacement += " = " + variables[index].defaultValue;
			first = false;
		}
		replacement += ";";
		if ( original.Find( "\r\n" ) >= 0 ) replacement += "\r\n";
		else if ( original.Find( '\n' ) >= 0 ) replacement += "\n";
	}
	source = source.Left( start ) + replacement + source.Mid( end, source.Length() - end );
	Parse( catalog, positions );
	dirty = true;
	return true;
}

bool DoomScriptBlueprintDocument::UpdateNodeSource( int nodeIndex, const char *text, const DoomScriptNodeCatalog &catalog, idStr &error ) {
	if ( nodeIndex < 0 || nodeIndex >= nodes.Num() ) {
		error = "The selected node is no longer available.";
		return false;
	}
	doomScriptGraphNode_t oldNode = nodes[nodeIndex];
	if ( oldNode.startOffset < 0 || oldNode.endOffset <= oldNode.startOffset || oldNode.endOffset > source.Length() ) {
		error = "This node does not have an editable DoomScript source range.";
		return false;
	}
	idStr replacement = text == NULL ? "" : text;
	replacement.StripTrailingWhitespace();
	if ( replacement.Length() == 0 ) {
		error = "Node source cannot be empty.";
		return false;
	}
	if ( replacement.Find( DSBP_BEGIN_PREFIX ) >= 0 || replacement.Find( DSBP_END ) >= 0 ) {
		error = "Blueprint metadata markers are not valid node source.";
		return false;
	}
	if ( oldNode.kind == "event" ) {
		if ( replacement.Find( '{' ) >= 0 || replacement.Find( '}' ) >= 0 ) {
			error = "Edit the Event signature only; its body braces are managed by the graph.";
			return false;
		}
		idStr header = NormalizeBlueprintLine( replacement ) + " {";
		if ( !LooksLikeFunctionHeader( header ) ) {
			error = "The Event signature must use: returnType name( parameters ).";
			return false;
		}
	} else if ( CountBrace( replacement, '{' ) != CountBrace( oldNode.sourceText, '{' ) ||
		CountBrace( replacement, '}' ) != CountBrace( oldNode.sourceText, '}' ) ) {
		error = "Editing a node cannot add or remove body braces. Edit the node expression or statement and keep its existing braces.";
		return false;
	}

	idDict positions;
	for ( int index = 0; index < nodes.Num(); index++ ) positions.Set( nodes[index].stableId, va( "%d %d", nodes[index].x, nodes[index].y ) );
	idList<int> functionX;
	idList<int> functionY;
	if ( oldNode.kind == "event" ) {
		for ( int index = 0; index < nodes.Num(); index++ ) {
			if ( nodes[index].functionIndex != oldNode.functionIndex ) continue;
			functionX.Append( nodes[index].x );
			functionY.Append( nodes[index].y );
		}
	}
	source = source.Left( oldNode.startOffset ) + replacement + source.Mid( oldNode.endOffset, source.Length() - oldNode.endOffset );
	Parse( catalog, positions );
	if ( oldNode.kind == "event" ) {
		int positionIndex = 0;
		for ( int index = 0; index < nodes.Num() && positionIndex < functionX.Num(); index++ ) {
			if ( nodes[index].functionIndex != oldNode.functionIndex ) continue;
			nodes[index].x = functionX[positionIndex];
			nodes[index].y = functionY[positionIndex];
			positionIndex++;
		}
	} else {
		for ( int index = 0; index < nodes.Num(); index++ ) {
			if ( nodes[index].functionIndex == oldNode.functionIndex && nodes[index].startOffset == oldNode.startOffset ) {
				nodes[index].x = oldNode.x;
				nodes[index].y = oldNode.y;
				break;
			}
		}
	}
	dirty = true;
	return true;
}

bool DoomScriptBlueprintDocument::ConnectDataPins( int fromNode, int fromPin, int toNode, int toPin,
	const DoomScriptNodeCatalog &catalog, idStr &error ) {
	if ( fromNode < 0 || fromNode >= nodes.Num() || toNode < 0 || toNode >= nodes.Num() ||
		fromPin < 0 || fromPin >= nodes[fromNode].outputs.Num() || toPin < 0 || toPin >= nodes[toNode].inputs.Num() ) {
		error = "Connect an output pin to a valid input pin.";
		return false;
	}
	if ( nodes[fromNode].functionIndex != nodes[toNode].functionIndex ) {
		error = "Data connections cannot cross Event scopes.";
		return false;
	}
	idStr fromType = nodes[fromNode].outputs[fromPin].type;
	idStr toType = nodes[toNode].inputs[toPin].type;
	bool numericCompatible = ( fromType == "float" || fromType == "boolean" || fromType == "integer" ) &&
		( toType == "float" || toType == "boolean" || toType == "integer" );
	if ( fromType != toType && !numericCompatible ) {
		error = va( "Cannot connect %s to %s.", fromType.c_str(), toType.c_str() );
		return false;
	}

	idStr emittedExpression;
	if ( nodes[fromNode].kind == "getvar" ) emittedExpression = nodes[fromNode].variableName;
	else if ( nodes[fromNode].kind == "event" ) emittedExpression = nodes[fromNode].outputs[fromPin].name;
	else emittedExpression = nodes[fromNode].expression;
	if ( emittedExpression.Length() == 0 ) {
		error = "That output does not emit a reusable value expression yet.";
		return false;
	}

	if ( nodes[toNode].kind.Find( "function:" ) == 0 ) {
		idList<idStr> arguments = nodes[toNode].inputValues;
		if ( toPin >= arguments.Num() ) {
			error = "That Function Call input has no editable argument.";
			return false;
		}
		arguments[toPin] = emittedExpression;
		return UpdateFunctionCall( toNode, arguments, catalog, error );
	}
	if ( nodes[toNode].kind != "branch" && nodes[toNode].kind != "loop" && nodes[toNode].kind != "setvar" ) {
		error = "Data pins connect to Function Call, Branch, Loop, and Set inputs.";
		return false;
	}
	return UpdateVisualNode( toNode, nodes[toNode].variableName.c_str(), emittedExpression.c_str(), catalog, error );
}

bool DoomScriptBlueprintDocument::UpdateFunctionCall( int nodeIndex, const idList<idStr> &arguments,
	const DoomScriptNodeCatalog &catalog, idStr &error ) {
	if ( nodeIndex < 0 || nodeIndex >= nodes.Num() || nodes[nodeIndex].kind.Find( "function:" ) != 0 ) {
		error = "The selected node is not an editable Function Call.";
		return false;
	}
	if ( arguments.Num() != nodes[nodeIndex].inputs.Num() ) {
		error = "Function argument count does not match its DoomTypeInfo signature.";
		return false;
	}
	for ( int index = 0; index < arguments.Num(); index++ ) {
		idStr value = NormalizeBlueprintLine( arguments[index] );
		if ( value.Length() == 0 || value.Find( ';' ) >= 0 || value.Find( '{' ) >= 0 || value.Find( '}' ) >= 0 ) {
			error = va( "Choose a variable or typed default value for input '%s'.", nodes[nodeIndex].inputs[index].name.c_str() );
			return false;
		}
	}
	if ( nodes[nodeIndex].sourceText.Length() == 0 && nodes[nodeIndex].ownerStableId.Length() != 0 ) {
		int ownerIndex = -1;
		for ( int index = 0; index < nodes.Num(); index++ ) {
			if ( nodes[index].stableId == nodes[nodeIndex].ownerStableId ) { ownerIndex = index; break; }
		}
		if ( ownerIndex < 0 ) {
			error = "The expression node's owning statement is no longer available.";
			return false;
		}
		int callOpen = -1, callClose = -1;
		idList<idStr> previousArguments;
		if ( !ParseFunctionCallArguments( nodes[nodeIndex].expression, nodes[nodeIndex].variableName,
			previousArguments, &callOpen, &callClose ) || callOpen < 0 || callClose <= callOpen ) {
			error = "The nested Function Call expression could not be parsed.";
			return false;
		}
		idStr replacementCall = nodes[nodeIndex].expression.Left( callOpen + 1 );
		if ( arguments.Num() ) replacementCall += " ";
		for ( int index = 0; index < arguments.Num(); index++ ) {
			if ( index != 0 ) replacementCall += ", ";
			replacementCall += NormalizeBlueprintLine( arguments[index] );
		}
		if ( arguments.Num() ) replacementCall += " ";
		replacementCall += nodes[nodeIndex].expression.Mid( callClose, nodes[nodeIndex].expression.Length() - callClose );
		idStr ownerExpression = nodes[ownerIndex].expression;
		int nestedOffset = ownerExpression.Find( nodes[nodeIndex].expression, true );
		if ( nestedOffset < 0 ) {
			error = "The nested Function Call no longer matches its owning expression.";
			return false;
		}
		ownerExpression = ownerExpression.Left( nestedOffset ) + replacementCall +
			ownerExpression.Mid( nestedOffset + nodes[nodeIndex].expression.Length(),
				ownerExpression.Length() - nestedOffset - nodes[nodeIndex].expression.Length() );
		return UpdateVisualNode( ownerIndex, nodes[ownerIndex].variableName.c_str(), ownerExpression.c_str(), catalog, error );
	}
	int open = -1, close = -1;
	idList<idStr> previousArguments;
	if ( !ParseFunctionCallArguments( nodes[nodeIndex].sourceText, nodes[nodeIndex].variableName, previousArguments, &open, &close ) || open < 0 || close <= open ) {
		error = "The Function Call source range could not be matched to its DoomTypeInfo command.";
		return false;
	}
	idStr replacement = nodes[nodeIndex].sourceText.Left( open + 1 );
	if ( arguments.Num() ) replacement += " ";
	for ( int index = 0; index < arguments.Num(); index++ ) {
		if ( index != 0 ) replacement += ", ";
		replacement += NormalizeBlueprintLine( arguments[index] );
	}
	if ( arguments.Num() ) replacement += " ";
	replacement += nodes[nodeIndex].sourceText.Mid( close, nodes[nodeIndex].sourceText.Length() - close );
	return UpdateNodeSource( nodeIndex, replacement.c_str(), catalog, error );
}

bool DoomScriptBlueprintDocument::UpdateVisualNode( int nodeIndex, const char *variableName, const char *expression,
	const DoomScriptNodeCatalog &catalog, idStr &error ) {
	if ( nodeIndex < 0 || nodeIndex >= nodes.Num() ) {
		error = "The selected visual node is no longer available.";
		return false;
	}
	idStr emittedExpression = NormalizeBlueprintLine( expression == NULL ? "" : expression );
	if ( emittedExpression.Length() == 0 || emittedExpression.Find( ';' ) >= 0 ||
		emittedExpression.Find( '{' ) >= 0 || emittedExpression.Find( '}' ) >= 0 ) {
		error = "Choose a variable or enter a single typed value expression.";
		return false;
	}
	idStr replacement = nodes[nodeIndex].sourceText;
	if ( nodes[nodeIndex].kind == "branch" || nodes[nodeIndex].kind == "loop" ) {
		int open = replacement.Find( '(' );
		int close = replacement.Last( ')' );
		if ( open < 0 || close <= open ) {
			error = "This control-flow node has no editable condition.";
			return false;
		}
		replacement = replacement.Left( open + 1 ) + " " + emittedExpression + " " +
			replacement.Mid( close, replacement.Length() - close );
	} else if ( nodes[nodeIndex].kind == "setvar" ) {
		idStr target = variableName == NULL ? "" : variableName;
		if ( !IsSimpleGraphIdentifier( target ) ) {
			error = "Choose a valid variable for the Set node.";
			return false;
		}
		idStr targetType = FindGraphVariableType( globals, sharedVariables, functions[nodes[nodeIndex].functionIndex], target );
		if ( targetType.Length() == 0 ) {
			error = "The selected Set variable is not visible in this Event.";
			return false;
		}
		if ( IsSimpleGraphIdentifier( emittedExpression ) ) {
			idStr sourceType = FindGraphVariableType( globals, sharedVariables, functions[nodes[nodeIndex].functionIndex], emittedExpression );
			bool numericCompatible = ( sourceType == "float" || sourceType == "boolean" || sourceType == "integer" ) &&
				( targetType == "float" || targetType == "boolean" || targetType == "integer" );
			if ( sourceType.Length() != 0 && sourceType != targetType && !numericCompatible ) {
				error = va( "Cannot assign %s to %s.", sourceType.c_str(), targetType.c_str() );
				return false;
			}
		}
		int assignment = replacement.Find( '=' );
		int semicolon = replacement.Last( ';' );
		if ( assignment < 0 || semicolon <= assignment ) {
			error = "This Set node has no editable value assignment.";
			return false;
		}
		int first = 0;
		while ( first < replacement.Length() && isspace( (unsigned char)replacement[first] ) ) first++;
		replacement = replacement.Left( first ) + target + " = " + emittedExpression +
			replacement.Mid( semicolon, replacement.Length() - semicolon );
	} else {
		error = "This node is edited through its typed data pins.";
		return false;
	}
	return UpdateNodeSource( nodeIndex, replacement.c_str(), catalog, error );
}

bool DoomScriptBlueprintDocument::UpdateConditionExpression( int nodeIndex, const char *expression,
	const DoomScriptNodeCatalog &catalog, idStr &error ) {
	if ( nodeIndex < 0 || nodeIndex >= nodes.Num() ) {
		error = "The selected condition node is no longer available.";
		return false;
	}
	int ownerIndex = nodeIndex;
	if ( nodes[ownerIndex].kind != "branch" && nodes[ownerIndex].kind != "loop" ) {
		ownerIndex = -1;
		const idStr ownerId = nodes[nodeIndex].ownerStableId;
		for ( int index = 0; ownerId.Length() != 0 && index < nodes.Num(); index++ ) {
			if ( nodes[index].stableId == ownerId && ( nodes[index].kind == "branch" || nodes[index].kind == "loop" ) ) {
				ownerIndex = index;
				break;
			}
		}
	}
	if ( ownerIndex < 0 ) {
		error = "This condition is not attached to a Branch or Loop.";
		return false;
	}

	int conditionIndex = -1;
	for ( int linkIndex = 0; linkIndex < links.Num(); linkIndex++ ) {
		const doomScriptGraphLink_t &link = links[linkIndex];
		if ( link.execution || link.to != ownerIndex || link.from < 0 || link.from >= nodes.Num() ) continue;
		if ( nodes[link.from].kind == "expression" || nodes[link.from].kind == "literal" ) {
			conditionIndex = link.from;
			break;
		}
	}
	int conditionX = conditionIndex >= 0 ? nodes[conditionIndex].x : 0;
	int conditionY = conditionIndex >= 0 ? nodes[conditionIndex].y : 0;
	bool preserveConditionPosition = conditionIndex >= 0;
	idDict inputPositions;
	if ( conditionIndex >= 0 ) {
		for ( int linkIndex = 0; linkIndex < links.Num(); linkIndex++ ) {
			const doomScriptGraphLink_t &link = links[linkIndex];
			if ( link.execution || link.to != conditionIndex || link.from < 0 || link.from >= nodes.Num() ) continue;
			const doomScriptGraphNode_t &inputNode = nodes[link.from];
			if ( inputNode.kind == "getvar" ) inputPositions.Set( inputNode.variableName, va( "%d %d", inputNode.x, inputNode.y ) );
		}
	}
	int ownerStart = nodes[ownerIndex].startOffset;
	int ownerFunction = nodes[ownerIndex].functionIndex;
	if ( !UpdateVisualNode( ownerIndex, nodes[ownerIndex].variableName.c_str(), expression, catalog, error ) ) return false;

	int newOwner = -1;
	for ( int index = 0; index < nodes.Num(); index++ ) {
		if ( nodes[index].functionIndex == ownerFunction && nodes[index].startOffset == ownerStart &&
			( nodes[index].kind == "branch" || nodes[index].kind == "loop" ) ) {
			newOwner = index;
			break;
		}
	}
	if ( newOwner < 0 ) return true;
	int newCondition = -1;
	for ( int linkIndex = 0; linkIndex < links.Num(); linkIndex++ ) {
		const doomScriptGraphLink_t &link = links[linkIndex];
		if ( link.execution || link.to != newOwner || link.from < 0 || link.from >= nodes.Num() ) continue;
		if ( nodes[link.from].kind == "expression" || nodes[link.from].kind == "literal" ) {
			newCondition = link.from;
			break;
		}
	}
	if ( newCondition >= 0 && preserveConditionPosition ) {
		nodes[newCondition].x = conditionX;
		nodes[newCondition].y = conditionY;
		for ( int linkIndex = 0; linkIndex < links.Num(); linkIndex++ ) {
			const doomScriptGraphLink_t &link = links[linkIndex];
			if ( link.execution || link.to != newCondition || link.from < 0 || link.from >= nodes.Num() ) continue;
			doomScriptGraphNode_t &inputNode = nodes[link.from];
			if ( inputNode.kind != "getvar" ) continue;
			const char *saved = inputPositions.GetString( inputNode.variableName, "" );
			if ( saved[0] ) sscanf( saved, "%d %d", &inputNode.x, &inputNode.y );
		}
	}
	return true;
}

void DoomScriptBlueprintDocument::SetNodePosition( int nodeIndex, int x, int y ) {
	if ( nodeIndex < 0 || nodeIndex >= nodes.Num() ) return;
	nodes[nodeIndex].x = x;
	nodes[nodeIndex].y = y;
	dirty = true;
}

int DoomScriptBlueprintDocument::MigrateAllScripts( const DoomScriptNodeCatalog &catalog, int &failed ) {
	failed = 0;
	int saved = 0;
	idFileList *files = fileSystem->ListFilesTree( "script", ".script", true );
	if ( files == NULL ) return 0;
	for ( int index = 0; index < files->GetNumFiles(); index++ ) {
		DoomScriptBlueprintDocument document;
		if ( !document.Load( files->GetFile( index ), catalog ) || !document.Save() ) failed++;
		else saved++;
	}
	fileSystem->FreeFileList( files );
	return saved;
}
