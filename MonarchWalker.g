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

@includes
{
	#include "Codegen.h"
}

@members
{
	static void ANTLR3_CDECL VariableDelete(void *var){}

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
	InitCodegen();
	$Symbols::table = antlr3HashTableNew(11);
	SCOPE_TOP(Symbols)->free = freeTable;
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

block
scope Symbols; // specify at higher levels instead?
	:	^( BLOCK statements )
	;

breakStatement
	:	^( 'break' label=NameLiteral? )
	;

caseClause
	:	^( 'case' expression statements )
	;

disruptiveStatement
	:	breakStatement
	|	returnStatement
	|	throwStatement
	//	what about continue?
	;

// do we allow expression to access block scope?
doStatement
	:	^( DO_WHILE expression block )
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
	|	^( '+=' lvalue r=assignment_expression ) { LLVMValueRef tmp = AddValues(LoadValue($lvalue.value), $r.value); $value = Assignment($lvalue.value, tmp); }
	|	^( '-=' lvalue r=assignment_expression ) { LLVMValueRef tmp = SubValues(LoadValue($lvalue.value), $r.value); $value = Assignment($lvalue.value, tmp); }
	|	^( '*=' lvalue r=assignment_expression ) { LLVMValueRef tmp = MulValues(LoadValue($lvalue.value), $r.value); $value = Assignment($lvalue.value, tmp); }
	|	^( '/=' lvalue r=assignment_expression ) { LLVMValueRef tmp = DivValues(LoadValue($lvalue.value), $r.value); $value = Assignment($lvalue.value, tmp); }
	|	conditional_expression { $value = $conditional_expression.value; }
	;

lvalue returns [LLVMValueRef value]
	:	unary_expression { $value = $unary_expression.value; }
	;

conditional_expression returns [LLVMValueRef value]
	:	logical_or_expression { $value = $logical_or_expression.value; }
	|	^( '?' logical_or_expression expression conditional_expression )
	;

logical_or_expression returns [LLVMValueRef value]
	:	logical_and_expression { $value = $logical_and_expression.value; }
	|	^( '||' lhs=logical_or_expression rhs=logical_and_expression )
	;

logical_and_expression returns [LLVMValueRef value]
	:	equality_expression { $value = $equality_expression.value; }
	|	^( '&&' lhs=logical_and_expression rhs=equality_expression )
	;

equality_expression returns [LLVMValueRef value]
	:	relational_expression { $value = $relational_expression.value; }
	|	^( '==' lhs=equality_expression rhs=relational_expression )
	|	^( '!=' lhs=equality_expression rhs=relational_expression )
	;

relational_expression returns [LLVMValueRef value]
	:	additive_expression { $value = $additive_expression.value; }
	|	^( '<'  lhs=relational_expression rhs=additive_expression )
	|	^( '>'  lhs=relational_expression rhs=additive_expression )
	|	^( '<=' lhs=relational_expression rhs=additive_expression )
	|	^( '>=' lhs=relational_expression rhs=additive_expression )
	;

additive_expression returns [LLVMValueRef value]
	:	multiplicative_expression { $value = $multiplicative_expression.value; }
	|	^( '+' lhs=additive_expression rhs=multiplicative_expression ) { $value = AddValues($lhs.value, $rhs.value); }
	|	^( '-' lhs=additive_expression rhs=multiplicative_expression ) { $value = SubValues($lhs.value, $rhs.value); }
	;

multiplicative_expression returns [LLVMValueRef value]
	:	unary_expression { $value = $unary_expression.value; }
	|	^( '*' lhs=multiplicative_expression rhs=unary_expression ) { $value = MulValues($lhs.value, $rhs.value);  }
	|	^( '/' lhs=multiplicative_expression rhs=unary_expression ) { $value = DivValues($lhs.value, $rhs.value);  }
	|	^( '%' lhs=multiplicative_expression rhs=unary_expression ) { $value = ModValues($lhs.value, $rhs.value);  }
	;

unary_expression returns [LLVMValueRef value]
	:	postfix_expression			{ $value = $postfix_expression.value; }
	|	'++' r=unary_expression
	|	'--' r=unary_expression
	|	'+'  r=unary_expression		{ $value = $r.value; }
	|	'-'  r=unary_expression		{ $value = NegateValue($r.value); }
	|	'!'  r=unary_expression		{ $value = InvertValue($r.value); }
	|	'typeof' r=unary_expression
	|	'delete' r=unary_expression	{ $value = DeleteValue($r.value); }
	|	'print' r=unary_expression	{ $value = PrintValue($r.value); }
	;

postfix_expression returns [LLVMValueRef value]
	:	primary_expression { $value = $primary_expression.value; }
	|	^( INDX postfix_expression expression )
	|	^( CALL postfix_expression expression? )
	|	^( '.' postfix_expression NameLiteral )
	|	^( POSTINC postfix_expression )
	|	^( POSTDEC postfix_expression )
	;

primary_expression returns [LLVMValueRef value]
	:	NameLiteral
		{
			defineSymbol(ctx, $NameLiteral.text);
			$value = getSymbol(ctx, $NameLiteral.text);
		}
	|	literal					{ $value = $literal.value; }
	|	^( NESTED expression )	{ $value = $expression.value; }
	;

forStatement
	:	^( 'for'
			(	^( INIT initialization=expressionStatement ) )?
			(	^( COND condition=expression ) )?
			(	^( INCR increment=expressionStatement ) )?
			(	^( VAR var=NameLiteral obj=expression ) )?
		)
		(	block
		|	statement
		)
	;
	
functionLiteral returns [LLVMValueRef value]
	:	^( FUNC args=parameters body=block name=NameLiteral? )
	;

ifStatement
	:	^( COND expression if_block=block else_block=block? )
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
statement
	:	(	expressionStatement
		|	disruptiveStatement
		|	tryStatement
		|	ifStatement
		|	switchStatement
		|	whileStatement
		|	forStatement
		|	doStatement
		)
	;

labeledStatement
	:	^( STAT statement
			(	^( LABEL NameLiteral ) )? )
	;

statements
	:	labeledStatement*
	;

switchStatement
	:	^( SWITCH expression caseClause*
			(	^( DEFAULT statements ) )? )
	;

throwStatement
	:	^( 'throw' expression )
	;

tryStatement
	:	^( TRY try_block=block variable=NameLiteral catch_block=block ) 
	;

whileStatement
	:	^( WHILE expression block )
	;




