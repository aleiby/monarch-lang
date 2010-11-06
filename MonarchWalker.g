tree grammar MonarchWalker;

options
{
	tokenVocab=Monarch;
	ASTLabelType=pANTLR3_BASE_TREE;
	language=C;
	output=AST;
}

scope Symbols
{
	pANTLR3_HASH_TABLE table;
}

scope BreakContinue
{
	LLVMBasicBlockRef break_block;
	LLVMBasicBlockRef continue_block;
	pANTLR3_STRING name; // label
}

@includes
{
	#include "Codegen.h"
}

@members
{
	static void ANTLR3_CDECL VariableDelete(void *var){}
	
	LLVMBasicBlockRef getBreakBlock(pMonarchWalker ctx, pANTLR3_STRING name)
	{
		for (int i = (int)SCOPE_SIZE(BreakContinue) - 1; i >= 0; i--)
		{
			SCOPE_TYPE(BreakContinue) scope = (SCOPE_TYPE(BreakContinue))SCOPE_INSTANCE(BreakContinue, i);
			if (!name || scope->name == name)
				return scope->break_block;
		}
		return NULL;
	}
	
	LLVMBasicBlockRef getContinueBlock(pMonarchWalker ctx, pANTLR3_STRING name)
	{
		for (int i = (int)SCOPE_SIZE(BreakContinue) - 1; i >= 0; i--)
		{
			SCOPE_TYPE(BreakContinue) scope = (SCOPE_TYPE(BreakContinue))SCOPE_INSTANCE(BreakContinue, i);
			if (!name || scope->name == name)
				return scope->continue_block;
		}
		return NULL;
	}
	
	LLVMValueRef getSymbol(pMonarchWalker ctx, pANTLR3_STRING name)
	{
		for (int i = (int)SCOPE_SIZE(Symbols) - 1; i >= 0; i--)
		{
			SCOPE_TYPE(Symbols) symbols = (SCOPE_TYPE(Symbols))SCOPE_INSTANCE(Symbols, i);
			void* symbol = symbols->table->get(symbols->table, name->chars);
			if (symbol)
				return (LLVMValueRef)symbol;
		}
		return NULL;
	}
	ANTLR3_BOOLEAN symbolDefined(pMonarchWalker ctx, pANTLR3_STRING name)
	{
		for (int i = (int)SCOPE_SIZE(Symbols) - 1; i >= 0; i--)
		{
			SCOPE_TYPE(Symbols) symbols = (SCOPE_TYPE(Symbols))SCOPE_INSTANCE(Symbols, i);
			if (symbols->table->get(symbols->table, name->chars))
				return ANTLR3_TRUE;
		}
		return ANTLR3_FALSE;
	}
	void defineSymbol(pMonarchWalker ctx, pANTLR3_STRING name)
	{
		if (!symbolDefined(ctx, name))
		{
			LLVMValueRef value = CreateValue((const char *)name->chars);
			SCOPE_TYPE(Symbols) symbols = SCOPE_TOP(Symbols);
			symbols->table->put(symbols->table, name->chars, value, VariableDelete);
		}
	}
	void ANTLR3_CDECL freeTable(SCOPE_TYPE(Symbols) symbols)
	{
		symbols->table->free(symbols->table);
	}
}

program
scope Symbols; // global scope
@init
{
	$Symbols::table = antlr3HashTableNew(11);
	SCOPE_TOP(Symbols)->free = freeTable;

	InitCodegen();
	LLVMBasicBlockRef entry = CreateBlock("entry");
	BeginBlock(entry);
}
@after
{
	TermCodegen();
}
	:	statements
	;

arrayLiteral returns [LLVMValueRef value]
	:	^( ARRAY expression? )
	;

block[const char* name] returns [LLVMValueRef result, LLVMBasicBlockRef ref]
scope Symbols; // specify at higher levels instead?
@init
{
	$Symbols::table = antlr3HashTableNew(11);
	SCOPE_TOP(Symbols)->free = freeTable;
	$ref = CreateBlock(name);
	BeginBlock($ref);
}
	:	^( BLOCK r=statements ) { $result = $r.result; }
	;

breakStatement
	:	^( 'break' label=NameLiteral? )
		{
			JumpTo(getBreakBlock(ctx, $label ? $label.text : NULL));
			// start a new basic block to catch any additional code that might get generated
			// basic blocks cannot have multiple branches (e.g. if we break in an if statement
			// a second branch will be added to endif).
			BeginBlock(CreateBlock("unreachable"));
		}
	;

continueStatement
	:	^( 'continue' label=NameLiteral? )
		{
			JumpTo(getContinueBlock(ctx, $label ? $label.text : NULL));
			// start a new basic block to catch any additional code that might get generated
			// basic blocks cannot have multiple branches (e.g. if we break in an if statement
			// a second branch will be added to endif).
			BeginBlock(CreateBlock("unreachable"));
		}
	;

caseClause
	:	^( 'case' expression statements )
	;

disruptiveStatement
	:	returnStatement
	|	breakStatement
	|	continueStatement
	|	throwStatement
	;

//!!ARL: I'm tempted to leave this out entirely.
doStatement
	:	^( DO_WHILE block["do"] cond=expression ) { DoWhile($cond.value, $block.ref); }
	;

expressionStatement
	:	expression
	;

expression returns [LLVMValueRef value]
	:	^( EXPR first=assignment_expression assignment_expression* ) { $value = $first.value; }
	;

// why is this its own rule?
constant_expression
	:	conditional_expression
	;

assignment_expression returns [LLVMValueRef value]
options {backtrack=true;}
	:	^( '='  lvalue r=assignment_expression ) { $value = Assignment($lvalue.value, $r.value); }
	|	^( '+=' lvalue r=assignment_expression ) { LLVMValueRef tmp = AddValues($lvalue.value, $r.value); $value = Assignment($lvalue.value, tmp); }
	|	^( '-=' lvalue r=assignment_expression ) { LLVMValueRef tmp = SubValues($lvalue.value, $r.value); $value = Assignment($lvalue.value, tmp); }
	|	^( '*=' lvalue r=assignment_expression ) { LLVMValueRef tmp = MulValues($lvalue.value, $r.value); $value = Assignment($lvalue.value, tmp); }
	|	^( '/=' lvalue r=assignment_expression ) { LLVMValueRef tmp = DivValues($lvalue.value, $r.value); $value = Assignment($lvalue.value, tmp); }
	|	conditional_expression { $value = $conditional_expression.value; }
	;

lvalue returns [LLVMValueRef value]
	:	unary_expression[ANTLR3_TRUE] { $value = $unary_expression.value; }
	;

conditional_expression returns [LLVMValueRef value]
@init
{
	LLVMValueRef results[] = { NULL, NULL };
	LLVMBasicBlockRef blocks[] = { NULL, NULL };
}
	:	logical_or_expression { $value = $logical_or_expression.value; }
	|	^( '?' cond=logical_or_expression
			{ BeginBlock(blocks[0] = CreateBlock("iftrue")); } iftrue=expression { results[0]=$iftrue.value; }
			{ BeginBlock(blocks[1] = CreateBlock("iffalse")); } iffalse=conditional_expression { results[1]=$iffalse.value; } )
			{ $value = IfElse($cond.value, results, blocks); }
	;

logical_or_expression returns [LLVMValueRef value]
	:	logical_and_expression { $value = $logical_and_expression.value; }
	|	^( '||' lhs=logical_or_expression rhs=logical_and_expression ){$lhs.value && $rhs.value}? { $value = HASEXCEPTION() ? NULL : LogicOr($lhs.value, $rhs.value); }
	;

logical_and_expression returns [LLVMValueRef value]
	:	equality_expression { $value = $equality_expression.value; }
	|	^( '&&' lhs=logical_and_expression rhs=equality_expression ){$lhs.value && $rhs.value}? { $value = HASEXCEPTION() ? NULL : LogicAnd($lhs.value, $rhs.value); }
	;

equality_expression returns [LLVMValueRef value]
	:	relational_expression { $value = $relational_expression.value; }
	|	^( '==' lhs=equality_expression rhs=relational_expression ){$lhs.value && $rhs.value}? { $value = HASEXCEPTION() ? NULL : CmpEQ($lhs.value, $rhs.value); }
	|	^( '!=' lhs=equality_expression rhs=relational_expression ){$lhs.value && $rhs.value}? { $value = HASEXCEPTION() ? NULL : CmpNE($lhs.value, $rhs.value); }
	;

relational_expression returns [LLVMValueRef value]
	:	additive_expression { $value = $additive_expression.value; }
	|	^( '<'  lhs=relational_expression rhs=additive_expression ){$lhs.value && $rhs.value}? { $value = HASEXCEPTION() ? NULL : CmpLT($lhs.value, $rhs.value); }
	|	^( '>'  lhs=relational_expression rhs=additive_expression ){$lhs.value && $rhs.value}? { $value = HASEXCEPTION() ? NULL : CmpGT($lhs.value, $rhs.value); }
	|	^( '<=' lhs=relational_expression rhs=additive_expression ){$lhs.value && $rhs.value}? { $value = HASEXCEPTION() ? NULL : CmpLE($lhs.value, $rhs.value); }
	|	^( '>=' lhs=relational_expression rhs=additive_expression ){$lhs.value && $rhs.value}? { $value = HASEXCEPTION() ? NULL : CmpGE($lhs.value, $rhs.value); }
	;

additive_expression returns [LLVMValueRef value]
	:	multiplicative_expression { $value = $multiplicative_expression.value; }
	|	^( '+' lhs=additive_expression rhs=multiplicative_expression ){$lhs.value && $rhs.value}? { $value = HASEXCEPTION() ? NULL : AddValues($lhs.value, $rhs.value); }
	|	^( '-' lhs=additive_expression rhs=multiplicative_expression ){$lhs.value && $rhs.value}? { $value = HASEXCEPTION() ? NULL : SubValues($lhs.value, $rhs.value); }
	;

multiplicative_expression returns [LLVMValueRef value]
	:	unary_expression[ANTLR3_FALSE] { $value = $unary_expression.value; }
	|	^( '*' lhs=multiplicative_expression rhs=unary_expression[ANTLR3_FALSE] ){$lhs.value && $rhs.value}? { $value = HASEXCEPTION() ? NULL : MulValues($lhs.value, $rhs.value);  }
	|	^( '/' lhs=multiplicative_expression rhs=unary_expression[ANTLR3_FALSE] ){$lhs.value && $rhs.value}? { $value = HASEXCEPTION() ? NULL : DivValues($lhs.value, $rhs.value);  }
	|	^( '%' lhs=multiplicative_expression rhs=unary_expression[ANTLR3_FALSE] ){$lhs.value && $rhs.value}? { $value = HASEXCEPTION() ? NULL : ModValues($lhs.value, $rhs.value);  }
	;

unary_expression[ANTLR3_BOOLEAN lvalue] returns [LLVMValueRef value]
	:	postfix_expression[$lvalue]					{ $value = $postfix_expression.value; }
	|	'++' r=unary_expression[ANTLR3_TRUE]		{ $value = IncrementValue($r.value); }
	|	'--' r=unary_expression[ANTLR3_TRUE]		{ $value = DecrementValue($r.value); }
	|	'+'  r=unary_expression[ANTLR3_FALSE]		{ $value = $r.value; }
	|	'-'  r=unary_expression[ANTLR3_FALSE]		{ $value = NegateValue($r.value); }
	|	'!'  r=unary_expression[ANTLR3_FALSE]		{ $value = InvertValue($r.value); }
	|	'typeof' r=unary_expression[ANTLR3_FALSE]
	|	'delete' r=unary_expression[ANTLR3_FALSE]	{ $value = DeleteValue($r.value); }
	|	'print' r=unary_expression[ANTLR3_FALSE]	{ $value = PrintValue($r.value); }
	;

postfix_expression[ANTLR3_BOOLEAN lvalue] returns [LLVMValueRef value]
	:	primary_expression[$lvalue] { $value = $primary_expression.value; }
	|	^( INDX postfix_expression[ANTLR3_FALSE] expression )
	|	^( CALL postfix_expression[ANTLR3_FALSE] expression? )
	|	^( '.' postfix_expression[ANTLR3_FALSE] NameLiteral )
	|	^( POSTINC postfix_expression[ANTLR3_FALSE] )
	|	^( POSTDEC postfix_expression[ANTLR3_FALSE] )
	;

primary_expression[ANTLR3_BOOLEAN lvalue] returns [LLVMValueRef value]
@init { $value = NULL; }
	:	NameLiteral {$lvalue || symbolDefined(ctx, $NameLiteral.text)}?
		{
			if (HASEXCEPTION()) goto ruleprimary_expressionEx; // this should be generated automatically
			if ($lvalue) defineSymbol(ctx, $NameLiteral.text);
			$value = getSymbol(ctx, $NameLiteral.text);
			if (!$lvalue) $value = LoadValue($value);
		}
	|	literal					{ $value = $literal.value; }
	|	^( NESTED expression )	{ $value = $expression.value; }
	;

forStatement
scope Symbols, BreakContinue;
@init
{
	$Symbols::table = antlr3HashTableNew(11);
	SCOPE_TOP(Symbols)->free = freeTable;
	LLVMBasicBlockRef blocks[] = {
		CreateBlock("for_init"),
		CreateBlock("for_cond"),
		CreateBlock("for_incr"),
		NULL, // for_loop - created below
		CreateBlock("endfor")
	};
	$BreakContinue::continue_block = blocks[2]; // jump to increment on continue
	$BreakContinue::break_block = blocks[4]; // jump to end on break
	$BreakContinue::name = NULL; // pass thru?
}
	:	^( 'for'
			(	{ BeginBlock(blocks[0]); } ^( INIT expressionStatement ) )?
			(	{ BeginBlock(blocks[1]); } ^( COND cond=expression ) )?
			(	{ BeginBlock(blocks[2]); } ^( INCR expressionStatement ) )?
//			(	^( VAR var=NameLiteral obj=expression ) )?
			(	block["for_loop"] { blocks[3]=$block.ref; }
			|	{ blocks[3]=CreateBlock("for_loop"); BeginBlock(blocks[3]); } ^( STAT statement )
			)
		)	{ ForLoop($cond.tree ? $cond.value : NULL, blocks); }
	;
	
functionLiteral returns [LLVMValueRef value]
	:	^( FUNC name=NameLiteral? args=parameters body=block[$name ? (const char *)$name.text->chars : ""] )
	;

ifStatement
@init
{
	LLVMValueRef results[] = { NULL, NULL };
	LLVMBasicBlockRef blocks[] = { NULL, NULL, CreateBlock("endif") };
}
	:	^( COND cond=expression
			(	iftrue=block["iftrue"] { results[0]=$iftrue.result; blocks[0]=$iftrue.ref; JumpTo(blocks[2]); }
			|	{ blocks[0]=CreateBlock("iftrue"); BeginBlock(blocks[0]); } ^( STAT st=statement ) { results[0]=$st.result; JumpTo(blocks[2]); }
			)
			(	iffalse=block["iffalse"] { results[1]=$iffalse.result; blocks[1]=$iffalse.ref; JumpTo(blocks[2]); }
			|	{ blocks[1]=CreateBlock("iffalse"); BeginBlock(blocks[1]); } ^( STAT sf=statement ) { results[1]=$sf.result; JumpTo(blocks[2]); }
			)?
			)	{ IfElse($cond.value, results, blocks); }
	;

literal returns [LLVMValueRef value]
	:	NumberLiteral
		{
			pANTLR3_STRING s = $NumberLiteral.text;
			$value = ConstInt(s->toInt32(s));
		}
	|	StringLiteral
		{
			pANTLR3_STRING s = $StringLiteral.text;
			$value = ConstString((const char *)s->chars, s->len);
		}
	|	'true'			{ $value = ConstBool(1); }	//!!ARL: Add these to the global symbol table instead?
	|	'false'			{ $value = ConstBool(0); }
	|	'null'			{ $value = ConstInt(0); }
	|	objectLiteral	{ $value = $objectLiteral.value; }
	|	arrayLiteral	{ $value = $arrayLiteral.value; }
	|	functionLiteral	{ $value = $functionLiteral.value; }
	;

objectLiteral returns [LLVMValueRef value]
	:	^( OBJECT vars=namedExpression* )
	;

namedExpression
	:	^( ':' NameLiteral constant_expression )
	|	^( ':' StringLiteral constant_expression )
	;

parameters
	:	^( PARAMS NameLiteral* )
	;

returnStatement
	:	^( 'return' expression? )
	;

// what about empty statements?
statement returns [LLVMValueRef result]
	:	(	expressionStatement { $result=NULL; }
		|	disruptiveStatement	{ $result=NULL; }
		|	tryStatement		{ $result=NULL; }
		|	ifStatement			{ $result=NULL; }
		|	switchStatement		{ $result=NULL; }
		|	whileStatement		{ $result=NULL; }
		|	forStatement		{ $result=NULL; }
		|	doStatement			{ $result=NULL; }
		)
	;

// only allow labels on switch, while, do and for?
labeledStatement returns [LLVMValueRef result]
	:	^( STAT r=statement { $result=$r.result; }
			(	^( LABEL NameLiteral ) )? )
	;

statements returns [LLVMValueRef result]
	:	r=labeledStatement* { $result=$r.result; }
	;

switchStatement
	:	^( SWITCH expression caseClause*
			(	^( DEFAULT statements ) )? )
	;

throwStatement
	:	^( 'throw' expression )
	;

tryStatement
	:	^( TRY block["try"] NameLiteral block["catch"] ) 
	;

whileStatement
@init
{
	LLVMBasicBlockRef cond_block = CreateBlock("while_cond");
	BeginBlock(cond_block);
}
	:	^( WHILE cond=expression block["while_loop"] ) { While($cond.value, cond_block, $block.ref); }
	;

