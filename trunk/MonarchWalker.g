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
			void* symbol = symbols->table->get(symbols->table, name);
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
			if (symbols->table->get(symbols->table, name))
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
			symbols->table->put(symbols->table, name, value, VariableDelete);
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

statements
	:	labeledStatement*
	;

label
	:	^( LABEL NameLiteral )
	;

labeledStatement
	:	^( STAT statement label? )
	;

statement
	:	'print' NameLiteral
		{
			pANTLR3_STRING name = $NameLiteral.text;
			PrintValue(getSymbol(ctx, name), (const char *)name->chars);
		}
	|	assignment_expression
	;

assignment_expression
	:	^( '=' lvalue r=primary_expression ) { Assignment($lvalue.value, $r.value); }
	;

lvalue returns [LLVMValueRef value]
	:	NameLiteral
		{
			defineSymbol(ctx, $NameLiteral.text);
			$value = getSymbol(ctx, $NameLiteral.text);
		}
	;

primary_expression returns [LLVMValueRef value]
	:	NameLiteral
		{
			defineSymbol(ctx, $NameLiteral.text);
			LLVMValueRef v = getSymbol(ctx, $NameLiteral.text);
			$value = LoadValue(v, (const char *)$NameLiteral.text->chars);
		}
	|	literal { $value = $literal.value; }
	;

literal returns [LLVMValueRef value]
	:	NumberLiteral
		{
			pANTLR3_STRING s = $NumberLiteral.text;
			$value = ConstValue(s->toInt32(s));
		}
	;

