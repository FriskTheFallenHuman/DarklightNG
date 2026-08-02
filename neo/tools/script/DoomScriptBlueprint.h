#ifndef __DOOMSCRIPT_BLUEPRINT_H__
#define __DOOMSCRIPT_BLUEPRINT_H__

struct doomScriptBlueprintPin_t {
	idStr			type;
	idStr			name;
	idStr			enumType;
	idList<idStr>	enumValues;
};

struct doomScriptFunctionNode_t {
	idStr			stableId;
	idStr			title;
	idStr			category;
	idStr			command;
	idStr			receiver;
	idStr			emitKind;
	idStr			returnType;
	bool			pure;
	bool			latent;
	bool			deprecated;
	idStr			owners;
	idStr			callback;
	idStr			source;
	idStr			keywords;
	idStr			description;
	idList<doomScriptBlueprintPin_t> pins;
};

class DoomScriptNodeCatalog {
public:
	bool			Load();
	int			FindCommand( const char *command ) const;
	const idList<doomScriptFunctionNode_t> &Nodes() const { return nodes; }
	const idStr &Checksum() const { return checksum; }

private:
	idList<doomScriptFunctionNode_t> nodes;
	idStr			checksum;
};

struct doomScriptGraphNode_t {
	idStr			stableId;
	idStr			title;
	idStr			kind;
	idStr			sourceText;
	idStr			variableName;
	idStr			expression;
	idStr			ownerStableId;
	idStr			valueType;
	idList<doomScriptBlueprintPin_t> inputs;
	idList<idStr>	inputValues;
	idList<doomScriptBlueprintPin_t> outputs;
	bool			pure;
	bool			dataOnly;
	int				line;
	int				functionIndex;
	int				startOffset;
	int				endOffset;
	int				x;
	int				y;
};

struct doomScriptVariable_t {
	idStr			type;
	idStr			name;
	idStr			defaultValue;
	int				line;
	int				startOffset;
	int				endOffset;
};

struct doomScriptGraphLink_t {
	int				from;
	int				to;
	int				fromPin;
	int				toPin;
	bool			execution;
};

struct doomScriptGraphFunction_t {
	idStr			name;
	idList<doomScriptBlueprintPin_t> parameters;
	idList<doomScriptVariable_t> locals;
	int				firstNode;
	int				numNodes;
	int				startOffset;
	int				openOffset;
	int				closeOffset;
};

class DoomScriptBlueprintDocument {
public:
	DoomScriptBlueprintDocument();

	bool			Load( const char *virtualPath, const DoomScriptNodeCatalog &catalog );
	bool			Save();
	bool			InsertFunctionCall( int functionIndex, const doomScriptFunctionNode_t &functionNode, const DoomScriptNodeCatalog &catalog, idStr &error );
	bool			InsertSetVariable( int functionIndex, const char *name, const DoomScriptNodeCatalog &catalog, idStr &error );
	bool			AddVariable( bool global, int functionIndex, const char *type, const char *name, const char *defaultValue, const DoomScriptNodeCatalog &catalog, idStr &error );
	bool			RemoveVariable( bool global, int functionIndex, int variableIndex, const DoomScriptNodeCatalog &catalog, idStr &error );
	bool			UpdateNodeSource( int nodeIndex, const char *text, const DoomScriptNodeCatalog &catalog, idStr &error );
	bool			ConnectDataPins( int fromNode, int fromPin, int toNode, int toPin, const DoomScriptNodeCatalog &catalog, idStr &error );
	bool			UpdateVisualNode( int nodeIndex, const char *variableName, const char *expression, const DoomScriptNodeCatalog &catalog, idStr &error );
	bool			UpdateConditionExpression( int nodeIndex, const char *expression, const DoomScriptNodeCatalog &catalog, idStr &error );
	bool			UpdateFunctionCall( int nodeIndex, const idList<idStr> &arguments, const DoomScriptNodeCatalog &catalog, idStr &error );
	void			SetNodePosition( int nodeIndex, int x, int y );

	const idStr &Path() const { return path; }
	const idStr &Source() const { return source; }
	const idList<doomScriptGraphNode_t> &Nodes() const { return nodes; }
	const idList<doomScriptGraphLink_t> &Links() const { return links; }
	const idList<doomScriptGraphFunction_t> &Functions() const { return functions; }
	const idList<doomScriptVariable_t> &Globals() const { return globals; }
	const idList<doomScriptVariable_t> &SharedVariables() const { return sharedVariables; }
	bool			IsDirty() const { return dirty; }

	static int	MigrateAllScripts( const DoomScriptNodeCatalog &catalog, int &failed );

private:
	void			Parse( const DoomScriptNodeCatalog &catalog, const idDict &layout );
	void			LoadSharedVariables();
	idStr			BuildFileText() const;

	idStr			path;
	idStr			source;
	idList<doomScriptGraphNode_t> nodes;
	idList<doomScriptGraphLink_t> links;
	idList<doomScriptGraphFunction_t> functions;
	idList<doomScriptVariable_t> globals;
	idList<doomScriptVariable_t> sharedVariables;
	idStr			catalogChecksum;
	bool			dirty;
};

#endif
