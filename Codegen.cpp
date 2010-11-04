/*
 *  Codegen.cpp
 *  Monarch
 *
 *  Created by Aaron Leiby on 10/14/10.
 *  Copyright 2010 __MyCompanyName__. All rights reserved.
 *
 */

#include "Codegen.h"

#include <llvm-c/Core.h>
#include <llvm-c/Analysis.h>
#include <llvm-c/ExecutionEngine.h>
#include <llvm-c/Target.h>
#include <llvm-c/Transforms/Scalar.h>

static LLVMModuleRef module;
static LLVMBuilderRef builder;
static LLVMValueRef printn; //!!ARL: Unhardcode
static LLVMValueRef top_function;

void InitCodegen()
{
	LLVMLinkInJIT();
	LLVMInitializeNativeTarget();
	module = LLVMModuleCreateWithName("monarch");
	builder = LLVMCreateBuilder();
	
	// stub in print functionality
	LLVMTypeRef args[] = { LLVMInt32Type() };
	LLVMTypeRef type = LLVMFunctionType(LLVMVoidType(), args, 1, 0);
	printn = LLVMAddFunction(module, "printn", type);
	LLVMSetFunctionCallConv(printn, LLVMCCallConv); //!!ARL: Necessary?
	LLVMSetLinkage(printn, LLVMExternalLinkage);
	
	{
		//!!ARL: Use label to name function (probably have to pull out as separate action)
		LLVMTypeRef type = LLVMFunctionType(LLVMVoidType(), NULL, 0, 0);
		top_function = LLVMAddFunction(module, "", type);
		LLVMSetFunctionCallConv(top_function, LLVMCCallConv); //!!ARL: Necessary?
		LLVMSetLinkage(top_function, LLVMExternalLinkage);
	}
}

void TermCodegen()
{
	LLVMBuildRetVoid(builder);
	
	fprintf(stdout, "\n=before=\n");
	LLVMDumpModule(module);
	
	char *error = NULL;
	LLVMVerifyModule(module, LLVMAbortProcessAction, &error);
	LLVMDisposeMessage(error); // Handler == LLVMAbortProcessAction -> No need to check errors
	
	//LLVMViewFunctionCFG(top_function);
	
	LLVMExecutionEngineRef engine;
	LLVMModuleProviderRef provider = LLVMCreateModuleProviderForExistingModule(module);
	error = NULL;
	if(LLVMCreateJITCompiler(&engine, provider, 2, &error) != 0) {
		fprintf(stderr, "\%s\n", error);
		LLVMDisposeMessage(error);
		abort();
	}
	
	LLVMPassManagerRef pass = LLVMCreatePassManager();
	LLVMAddTargetData(LLVMGetExecutionEngineTargetData(engine), pass);
	LLVMAddConstantPropagationPass(pass);
	LLVMAddInstructionCombiningPass(pass);
	LLVMAddPromoteMemoryToRegisterPass(pass);
	LLVMAddReassociatePass(pass);
	LLVMAddGVNPass(pass);
	LLVMAddCFGSimplificationPass(pass);
	LLVMRunPassManager(pass, module);
	
	//LLVMViewFunctionCFG(top_function);

	fprintf(stdout, "\n=after=\n");
	LLVMDumpModule(module);
	
	fprintf(stdout, "\n=output=\n");
	LLVMRunFunction(engine, top_function, 0, NULL);
	
	LLVMDisposePassManager(pass);
	LLVMDisposeBuilder(builder);
	LLVMDisposeExecutionEngine(engine);
}

LLVMValueRef CreateValue(const char* name)
{
	//!!ARL: Default value?  Also need to infer type.
	return LLVMBuildAlloca(builder, LLVMInt32Type(), name);
}

LLVMValueRef LoadValue(LLVMValueRef v)
{
	return LLVMBuildLoad(builder, v, "");
}

LLVMValueRef ConstInt(int value)
{
	return LLVMConstInt(LLVMInt32Type(), value, 0);
}

LLVMValueRef ConstBool(int value)
{
	return LLVMConstInt(LLVMInt1Type(), value, 0);
}

LLVMValueRef ConstString(const char* value, int length)
{
	return LLVMConstString(value, length, false);
}

LLVMValueRef IncrementValue(LLVMValueRef v)
{
	//!!ARL: Is there a separate increment operation?
	return Assignment(v, AddValues(LoadValue(v), ConstInt(1)));
}

LLVMValueRef DecrementValue(LLVMValueRef v)
{
	return Assignment(v, SubValues(LoadValue(v), ConstInt(1)));
}

LLVMValueRef NegateValue(LLVMValueRef v)
{
	return v; //!!
}

LLVMValueRef InvertValue(LLVMValueRef v)
{
	return v; //!!
}

LLVMValueRef DeleteValue(LLVMValueRef v)
{
	return ConstInt(0); //!!
}

LLVMValueRef PrintValue(LLVMValueRef v)
{
	LLVMValueRef args[] = { v };
	return LLVMBuildCall(builder, printn, args, 1, "");
}

LLVMValueRef Assignment(LLVMValueRef lhs, LLVMValueRef rhs)
{
	LLVMBuildStore(builder, rhs, lhs);
	return LoadValue(lhs);
}

LLVMValueRef AddValues(LLVMValueRef lhs, LLVMValueRef rhs)
{
	return LLVMBuildAdd(builder, lhs, rhs, "");
}

LLVMValueRef SubValues(LLVMValueRef lhs, LLVMValueRef rhs)
{
	return LLVMBuildSub(builder, lhs, rhs, "");
}

LLVMValueRef MulValues(LLVMValueRef lhs, LLVMValueRef rhs)
{
	return LLVMBuildMul(builder, lhs, rhs, "");
}

LLVMValueRef DivValues(LLVMValueRef lhs, LLVMValueRef rhs)
{
	return LLVMBuildSDiv(builder, lhs, rhs, "");
}

LLVMValueRef ModValues(LLVMValueRef lhs, LLVMValueRef rhs)
{
	return LLVMBuildSRem(builder, lhs, rhs, "");
}

LLVMValueRef LogicAnd(LLVMValueRef lhs, LLVMValueRef rhs)
{
	return LLVMBuildAnd(builder, lhs, rhs, "");
}

LLVMValueRef LogicOr(LLVMValueRef lhs, LLVMValueRef rhs)
{
	return LLVMBuildOr(builder, lhs, rhs, "");
}

LLVMValueRef CmpEQ(LLVMValueRef lhs, LLVMValueRef rhs)
{
	return LLVMBuildICmp(builder, LLVMIntEQ, lhs, rhs, "");
}

LLVMValueRef CmpNE(LLVMValueRef lhs, LLVMValueRef rhs)
{
	return LLVMBuildICmp(builder, LLVMIntNE, lhs, rhs, "");
}

LLVMValueRef CmpLT(LLVMValueRef lhs, LLVMValueRef rhs)
{
	return LLVMBuildICmp(builder, LLVMIntSLT, lhs, rhs, "");
}

LLVMValueRef CmpGT(LLVMValueRef lhs, LLVMValueRef rhs)
{
	return LLVMBuildICmp(builder, LLVMIntSGT, lhs, rhs, "");
}

LLVMValueRef CmpLE(LLVMValueRef lhs, LLVMValueRef rhs)
{
	return LLVMBuildICmp(builder, LLVMIntSLE, lhs, rhs, "");
}

LLVMValueRef CmpGE(LLVMValueRef lhs, LLVMValueRef rhs)
{
	return LLVMBuildICmp(builder, LLVMIntSGE, lhs, rhs, "");
}

LLVMBasicBlockRef CreateBlock(const char* name)
{
	return LLVMAppendBasicBlock(top_function, name);
}

void BeginBlock(LLVMBasicBlockRef block)
{
	LLVMPositionBuilderAtEnd(builder, block);
}

void LinkTo(LLVMBasicBlockRef block)
{
	LLVMBuildBr(builder, block);
	LLVMPositionBuilderAtEnd(builder, block);
}

LLVMValueRef Branch(LLVMValueRef cond, LLVMValueRef* results, LLVMBasicBlockRef* blocks)
{
	LLVMBasicBlockRef iftrue = blocks[0];
	LLVMBasicBlockRef iffalse = blocks[1];
	LLVMBasicBlockRef endif = blocks[2];
	
	// move endif to the end of the function so far
	LLVMMoveBasicBlockAfter(endif, LLVMGetLastBasicBlock(top_function));
	
	// insert our conditional branch just ahead of the iftrue block
	LLVMBasicBlockRef prev = LLVMGetPreviousBasicBlock(iftrue);
	LLVMPositionBuilderAtEnd(builder, prev);
	
	//!!ARL: Assumes cond is a bool already (need type coersion).
	LLVMBuildCondBr(builder, cond, iftrue, iffalse ? iffalse : endif);
	
	// finish off return var at end of if/then/else statement
	LLVMPositionBuilderAtEnd(builder, endif);
	
	// if we have two results, build a phi node to join them
	if (results[0] && results[1])
	{
		LLVMValueRef result = LLVMBuildPhi(builder, LLVMInt32Type(), "result");  
		LLVMAddIncoming(result, results, blocks, 2);
		return result;
	}
	
	// otherwise, return whichever is non-null (if any).
	return results[0] ? results[0] : results[1];
}

void DoWhile(LLVMValueRef cond, LLVMBasicBlockRef block)
{
	// insert a branch to our block
	LLVMBasicBlockRef prev_block = LLVMGetPreviousBasicBlock(block);
	LLVMPositionBuilderAtEnd(builder, prev_block);
	LLVMBuildBr(builder, block);
	
	// tack on an end block to branch to when the condition fails		
	LLVMBasicBlockRef enddo = LLVMAppendBasicBlock(top_function, "enddo");
	LLVMPositionBuilderAtEnd(builder, block);
	
	//!!ARL: Assumes cond is a bool already (need type coersion).
	LLVMBuildCondBr(builder, cond, block, enddo);
	LLVMPositionBuilderAtEnd(builder, enddo);
}

void While(LLVMValueRef cond, LLVMBasicBlockRef cond_block, LLVMBasicBlockRef block)
{
	// insert a branch to the condition block
	LLVMBasicBlockRef prev_block = LLVMGetPreviousBasicBlock(cond_block);
	LLVMPositionBuilderAtEnd(builder, prev_block);
	LLVMBuildBr(builder, cond_block);
	
	// tack on an end block to branch to when the condition fails		
	LLVMBasicBlockRef endwhile = LLVMAppendBasicBlock(top_function, "endwhile");
	
	// evaluate condition to decide to execute block or exit
	LLVMPositionBuilderAtEnd(builder, cond_block);
	//!!ARL: Assumes cond is a bool already (need type coersion).
	LLVMBuildCondBr(builder, cond, block, endwhile);
	
	// loop back to evaluate condition at end of block
	LLVMPositionBuilderAtEnd(builder, block);
	LLVMBuildBr(builder, cond_block);
	
	LLVMPositionBuilderAtEnd(builder, endwhile);
}

void ForLoop(LLVMValueRef cond, LLVMBasicBlockRef* blocks, LLVMBasicBlockRef end)
{
	LLVMBasicBlockRef init_block = blocks[0];
	LLVMBasicBlockRef cond_block = blocks[1];
	LLVMBasicBlockRef incr_block = blocks[2];
	LLVMBasicBlockRef loop_block = blocks[3];
	
	// branch to our init block
	LLVMBasicBlockRef prev_block = LLVMGetPreviousBasicBlock(init_block);
	LLVMPositionBuilderAtEnd(builder, prev_block);
	LLVMBuildBr(builder, init_block);
	
	// chain init block to cond block
	LLVMPositionBuilderAtEnd(builder, init_block);
	LLVMBuildBr(builder, cond_block);
	
	// evaluate condition to decide to execute loop block or exit
	LLVMPositionBuilderAtEnd(builder, cond_block);
	if (cond)
	{
		//!!ARL: Assumes cond is a bool already (need type coersion).
		LLVMBuildCondBr(builder, cond, loop_block, end);
	}
	else
	{
		// unconditional - continue loop until disrupted
		LLVMBuildBr(builder, loop_block);
	}
	
	// chain last block back to increment block
	LLVMBasicBlockRef last = LLVMGetLastBasicBlock(top_function);
	LLVMPositionBuilderAtEnd(builder, last);
	LLVMBuildBr(builder, incr_block);
	
	// chain increment block to condition block
	LLVMPositionBuilderAtEnd(builder, incr_block);
	LLVMBuildBr(builder, cond_block);
	
	// move end block to end of function where it belongs
	LLVMMoveBasicBlockAfter(end, last);
	LLVMPositionBuilderAtEnd(builder, end);
}

void Break(LLVMBasicBlockRef block)
{
	LLVMBuildBr(builder, block);
}

// Labels - maybe blocks should create a 'begin' and 'end' llvm-block
// - store in symbol table as "begin" and "end"
// - if labeled, also store as label.begin and label.end?
// - we push a new symbol hash on the stack for each block, so store begin and end on that instead
// - what about nested blocks?  (no such thing?  always explicitly branched)
// - check if symbol is already defined in scope first?
// - disallow defining symbols for any labels in scope?
// ** maybe add a label at end when encountering a break?  What about existing end blocks? (return vars?)
// - continue in for loop needs to jump to increment, but its block will mask that label
// - maybe pass blocks to jump to as params (block -> statements -> labeledStatement -> statement -> disruptiveStatement) 
// - need to walk stack to support break w/ label (can't pass as param)
// - disruptiveStatements only make sense in the context of for/while/do (break in switch), return in function
// - might want to support break & continue in freestanding blocks (to jump to top or bottom)
// ** if a disruptiveStatement (break, return) isn't at the end of a block, it means there is dead code.
// - a block has to end in exactly one branch "if (...) break;" winds up with a branch for the break, and a second for the endif.

