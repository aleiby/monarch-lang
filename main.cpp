
#include <iostream>
#include <vector>
#include <antlr3treeparser.h>

#include "MonarchLexer.h"
#include "MonarchParser.h"
#include "MonarchWalker.h"

extern "C" 
int printn(ANTLR3_INT32 X)
{
	return printf("%d\n", X);
}

// if we have an array of ints, we malloc each element
// and store a pointer to it in the array for access
typedef std::vector<void*> dynarray;

extern "C"
dynarray* newarray(ANTLR3_INT32 size)
{
	return new dynarray(size);
}

extern "C" 
void* getarray(dynarray* array, ANTLR3_INT32 index)
{
	return (index < array->size()) ? (*array)[index] : NULL;
}

extern "C" 
void putarray(dynarray* array, ANTLR3_INT32 index, void* ptr)
{
	if (index >= array->size())
		array->resize(index + 1, NULL);
	(*array)[index] = ptr;
}

int ANTLR3_CDECL
main (int argc, char * argv[])
{
	pANTLR3_UINT8 fileName = (pANTLR3_UINT8)"/Users/aleiby/Development/Monarch/test.monarch";
	pANTLR3_INPUT_STREAM input = antlr3AsciiFileStreamNew(fileName);
	if (input == NULL)
	{
		ANTLR3_FPRINTF(stderr, "input failure");
		return 1;
	}
	
	pMonarchLexer lexer = MonarchLexerNew(input);
	if (lexer == NULL)
	{
		ANTLR3_FPRINTF(stderr, "lexer failure");
		return 1;
	}
	
	pANTLR3_COMMON_TOKEN_STREAM tokenStream = antlr3CommonTokenStreamSourceNew(ANTLR3_SIZE_HINT, TOKENSOURCE(lexer));
	if (tokenStream == NULL)
	{
		ANTLR3_FPRINTF(stderr, "tokenStream failure");
		return 1;
	}
		
	pMonarchParser parser = MonarchParserNew(tokenStream);
	if (parser == NULL)
	{
		ANTLR3_FPRINTF(stderr, "parser failure");
		return 1;
	}
	
	MonarchParser_program_return ret = parser->program(parser);
	ANTLR3_UINT32 errorCount = parser->pParser->rec->getNumberOfSyntaxErrors(parser->pParser->rec);
	if (errorCount > 0)
		ANTLR3_FPRINTF(stderr, "The parser returned %d errors!", errorCount);
	else
	{
		pANTLR3_STRING output = ret.tree->toStringTree(ret.tree);
		ANTLR3_FPRINTF(stdout, "Success! AST:%s\n", output->chars);

		//!!ARL: Going to need multiple passes (e.g. so we can collect local vars to alloca in entry).
		// (actually, it looks like we might be able to insert these after the fact - get a builder
		// at the start/begin of the entry's function block)
		pANTLR3_COMMON_TREE_NODE_STREAM nodes = antlr3CommonTreeNodeStreamNewTree(ret.tree, ANTLR3_SIZE_HINT);
		pMonarchWalker walker = MonarchWalkerNew(nodes);
		walker->program(walker);
	}
	
	parser->free(parser);
	tokenStream->free(tokenStream);
	lexer->free(lexer);
	input->close(input);
    return 0;
}
