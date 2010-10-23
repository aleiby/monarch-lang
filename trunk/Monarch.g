// Monarch 1.0 - Aaron Leiby - 2009.04.05

// 'finally' clause for try statements?
// 'else' on for loops?
// do we want/need 'typeof'?
// tuples?  expanding for iteration? (a la python)
// we'll want an 'import' keyword (create a module per file and use LLVM to merge)

grammar Monarch;

options
{
	output=AST;
	language=C;
}

tokens
{
	STAT;
	LABEL;
	CALL;
	INDX;
	PARAMS;
	NESTED;
	
	INIT;
	COND;
	INCR;
	
	VAR;
	FUNC;
	EXPR;
	
	POSTINC;
	POSTDEC;
	PREINC;
	PREDEC;

	ARRAY;
	BLOCK;
	OBJECT;
	
	WHILE;
	DO_WHILE;
	SWITCH;
	DEFAULT;
	TRY;
}

program
	:	statements
	;

arrayLiteral
	:	'[' expression? ']'
	->	^( ARRAY expression? )
	;

// support for free standing blocks?  how to differentiate from object literals?  use anon funcs instead?
block
	:	'{' statements '}'
	->	^( BLOCK statements )
	;

breakStatement
	:	'break'^ label=NameLiteral? ';'!
	;

caseClause
	:	'case'^ expression ':'! statements
	;

disruptiveStatement
	:	breakStatement
	|	returnStatement
	|	throwStatement
	//	what about continue?
	;

doStatement
	:	'do' block 'while' '(' expression ')' ';'
	->	^( DO_WHILE expression block )
	;

expressionStatement
	:	expression
	;

expression
	:	assignment_expression ( ',' assignment_expression )*
	->	^( EXPR assignment_expression+ )
	;

// why is this its own rule?
constant_expression
	:	conditional_expression
	;

assignment_expression
options {backtrack=true;}
	:	lvalue assignment_operator^ assignment_expression
	|	conditional_expression
	;

lvalue
	:	unary_expression
	;

assignment_operator
	:	'='
	|	'+='
	|	'-='
	|	'*='
	|	'/='
	;

conditional_expression
	:	logical_or_expression ( '?'^ expression ':'! conditional_expression )?
	;

logical_or_expression
	:	logical_and_expression ( '||'^ logical_and_expression )*
	;

logical_and_expression
	:	equality_expression ( '&&'^ equality_expression )*
	;

equality_expression
	:	relational_expression ( ( '==' | '!=' )^ relational_expression )*
	;

relational_expression
	:	additive_expression ( ( '<' | '>' | '<=' | '>=' )^ additive_expression )*
	;

additive_expression
	:	multiplicative_expression ( ( '+' | '-' )^ multiplicative_expression )*
	;

multiplicative_expression
	:	unary_expression ( ( '*' | '/' | '%' )^ unary_expression )*
	;

unary_expression
	:	postfix_expression
	|	'++' unary_expression
	|	'--' unary_expression
	|	unary_operator unary_expression
	|	'typeof' unary_expression
	|	'delete' unary_expression
	|	'print' unary_expression
	;

postfix_expression
	:	( primary_expression -> primary_expression )
		(	'[' expression ']'		-> ^( INDX $postfix_expression expression )
		|	'(' expression? ')'		-> ^( CALL $postfix_expression expression? )
		|	'.' NameLiteral			-> ^( '.' $postfix_expression NameLiteral )
		|	'++'					-> ^( POSTINC $postfix_expression )
		|	'--'					-> ^( POSTDEC $postfix_expression )
		)*
	;

unary_operator
	:	'+'
	|	'-'
	|	'!'
	;

primary_expression
	:	NameLiteral
	|	literal
	|	'(' expression ')' -> ^( NESTED expression )
	;

forStatement
	:	'for' '('
	(	initialization=expressionStatement?
	';'	condition=expression?
	';'	increment=expressionStatement?			-> ^( INIT $initialization )? ^( COND $condition )? ^( INCR $increment )?
	|	var=NameLiteral 'in' obj=expression		-> ^( VAR $var $obj )
	)	')'
	(	('{')=> block							-> ^( 'for' $forStatement block )
	|	statement								-> ^( 'for' $forStatement statement )
	)
	;

// should parameters be optional as well?
functionLiteral
	:	'function' name=NameLiteral? args=parameters body=block
	->	^( FUNC $name? $args $body )
	;

// what about elseif?
ifStatement
	:	'if' '(' expression ')' if_block=block ( 'else' else_block=block )?
	->	^( COND expression $if_block $else_block? )
	;

literal
	:	NumberLiteral
	|	StringLiteral
	|	objectLiteral
	|	arrayLiteral
	|	functionLiteral
	;

NameLiteral
	:	Letter ( Letter | Digit | '_' )*
	;

NumberLiteral
	:	Integer Fraction? Exponent?
	;

objectLiteral
	:	'{' vars=namedExpressionList? '}'
	->	^( OBJECT $vars? )
	;

namedExpressionList
	:	namedExpression ( ','! namedExpression )*
	;

namedExpression
	:	( NameLiteral | StringLiteral ) ':'^ constant_expression
	;

parameters
	:	'(' ( NameLiteral ( ',' NameLiteral )* )? ')'
	->	^( PARAMS NameLiteral* )
	;

returnStatement
	:	'return'^ expression? ';'!
	;

// what about empty statements?
statement
	:	(	expressionStatement ';'!
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
	:	( NameLiteral ':' )? statement
	->	^( STAT statement ^( LABEL NameLiteral )? )
	;

statements
	:	labeledStatement*
	;

StringLiteral
	:	DQUOTE ( ~( DQUOTE | BSLASH ) | EscapedCharacter )* DQUOTE
	|	SQUOTE ( ~( SQUOTE | BSLASH ) | EscapedCharacter )* SQUOTE
	;

switchStatement
	:	'switch' '(' expression ')' '{' caseClause* ( 'default' ':' statements )? '}'
	->	^( SWITCH expression caseClause* ^( DEFAULT statements )? )
	;

throwStatement
	:	'throw'^ expression ';'!
	;

tryStatement
	:	'try' try_block=block 'catch' '(' variable=NameLiteral ')' catch_block=block
	->	^( TRY $try_block $variable $catch_block ) 
	;

whileStatement
	:	'while' '(' expression ')' block
	->	^( WHILE expression block )
	;

fragment BSLASH
	:	'\\'
	;

fragment DQUOTE
	:	'"'
	;

fragment SQUOTE
	:	'\''
	;

fragment EscapedCharacter
	:	BSLASH
	(	DQUOTE
	|	SQUOTE
	|	BSLASH
	|	'/'
	|	'b'		// backspace
	|	'f'		// formfeed
	|	'n'		// new line
	|	'r'		// carriage return
	|	't'		// tab
	)
	;

fragment Integer
	:	'0'
	|	'1'..'9' Digit*
	;

fragment Fraction
	:	'.' Digit*
	;

fragment Exponent
	:	( 'e' | 'E' ) ( '+' | '-' )? Digit+
	;

fragment Letter
	:	('a'..'z') | ('A'..'Z')
	;

fragment Digit
	:	'0'..'9'
	;

Whitespace
	:	( ' ' | '\t' | '\n' | '\r' )+ {$channel=HIDDEN;}
	;

Comment
	:	'/*' ( options {greedy=false;} : . )* '*/' {$channel=HIDDEN;}
	;

LineComment
	:	'//' ~( '\n' | '\r' )* '\r'? ( '\n' | EOF ) {$channel=HIDDEN;}
	;

