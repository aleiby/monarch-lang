
#include <iostream>
#include <antlr3treeparser.h>

#include "MonarchLexer.h"
#include "MonarchParser.h"
#include "MonarchWalker.h"

extern "C" 
void printn(ANTLR3_INT32 X)
{
	printf("%d\n", X);
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
