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

static LLVMValueRef printn;
static LLVMValueRef newarray;
static LLVMValueRef getarray;
static LLVMValueRef putarray;

void InitCodegen()
{
	LLVMLinkInJIT();
	LLVMInitializeNativeTarget();
	module = LLVMModuleCreateWithName("monarch");
	builder = LLVMCreateBuilder();
	
	//!!ARL: Should maybe build 'extern' functionality to define these in the script itself.
	
	// stub in print functionality
	{
		LLVMTypeRef args[] = { LLVMInt32Type() };
		LLVMTypeRef type = LLVMFunctionType(LLVMVoidType(), args, 1, 0);
		printn = LLVMAddFunction(module, "printn", type);
		LLVMSetFunctionCallConv(printn, LLVMCCallConv); //!!ARL: Necessary?
		LLVMSetLinkage(printn, LLVMExternalLinkage);
	}
	
	// stub in array functionality
	{
		LLVMAddTypeName(module, "array", LLVMPointerType(LLVMOpaqueType(), 0));
		LLVMTypeRef args[] = { LLVMInt32Type() };
		LLVMTypeRef type = LLVMFunctionType(LLVMGetTypeByName(module, "array"), args, 1, 0);
		newarray = LLVMAddFunction(module, "newarray", type);
		LLVMSetFunctionCallConv(newarray, LLVMCCallConv);
		LLVMSetLinkage(newarray, LLVMExternalLinkage);
	}
	
	{
		LLVMTypeRef args[] = { LLVMGetTypeByName(module, "array"), LLVMInt32Type() };
		LLVMTypeRef type = LLVMFunctionType(LLVMPointerType(LLVMInt8Type(), 0), args, 2, 0);
		getarray = LLVMAddFunction(module, "getarray", type);
		LLVMSetFunctionCallConv(getarray, LLVMCCallConv);
		LLVMSetLinkage(getarray, LLVMExternalLinkage);
	}

	{
		LLVMTypeRef args[] = { LLVMGetTypeByName(module, "array"), LLVMInt32Type(), LLVMPointerType(LLVMInt8Type(), 0) };
		LLVMTypeRef type = LLVMFunctionType(LLVMVoidType(), args, 3, 0);
		putarray = LLVMAddFunction(module, "putarray", type);
		LLVMSetFunctionCallConv(putarray, LLVMCCallConv);
		LLVMSetLinkage(putarray, LLVMExternalLinkage);
	}
}

void TermCodegen(LLVMValueRef function)
{
	//!!ARL: Should probably move this out of tree walker (to main.cpp), and handle errors properly.
	
	fprintf(stdout, "\n=before=\n");
	LLVMDumpModule(module);
	
	char *error = NULL;
	LLVMVerifyModule(module, LLVMAbortProcessAction, &error);
	LLVMDisposeMessage(error); // Handler == LLVMAbortProcessAction -> No need to check errors
	
	//LLVMViewFunctionCFG(function);
	
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
	LLVMAddPromoteMemoryToRegisterPass(pass); //scalarrepl
	LLVMAddReassociatePass(pass);
	LLVMAddGVNPass(pass);
	LLVMAddCFGSimplificationPass(pass);
	LLVMRunPassManager(pass, module);
	
	//LLVMViewFunctionCFG(function);

	fprintf(stdout, "\n=after=\n");
	LLVMDumpModule(module);
	
	fprintf(stdout, "\n=output=\n");
	LLVMRunFunction(engine, function, 0, NULL);
	
	LLVMDisposePassManager(pass);
	LLVMDisposeBuilder(builder);
	LLVMDisposeExecutionEngine(engine);
}

LLVMTypeRef GetType(LLVMValueRef value)
{
	return LLVMTypeOf(value);
}

LLVMValueRef CreateFunction(const char* name)
{
	LLVMTypeRef type = LLVMFunctionType(LLVMVoidType(), NULL, 0, 0);
	LLVMValueRef function = LLVMAddFunction(module, name, type);
	LLVMSetFunctionCallConv(function, LLVMCCallConv); //!!ARL: Necessary?
	LLVMSetLinkage(function, LLVMExternalLinkage);
	return function;
}

LLVMValueRef CallFunction(LLVMValueRef function)
{
	//!!ARL: Need to pass args.
	return LLVMBuildCall(builder, function, NULL, 0, "");
}

void BuildReturn(LLVMValueRef function)
{
	LLVMBuildRetVoid(builder);
}

void ContinueFunction(LLVMValueRef function)
{
	LLVMBasicBlockRef last_block = LLVMGetLastBasicBlock(function);
	LLVMPositionBuilderAtEnd(builder, last_block);
}

LLVMValueRef CreateArray()
{
	LLVMValueRef args[] = { ConstInt(0) };
	return LLVMBuildCall(builder, newarray, args, 1, "");
}

LLVMValueRef GetArray(LLVMValueRef array, LLVMValueRef index)
{
	LLVMValueRef args[] = { array, index };
	LLVMValueRef ptr = LLVMBuildCall(builder, getarray, args, 2, "");
	
	//!!ARL: Need to store type somewhere (per element if we are going to support mixed arrays).
	return LLVMBuildBitCast(builder , ptr, LLVMPointerType(LLVMInt32Type(), 0), "entry");
}

LLVMValueRef PutArray(LLVMValueRef array, LLVMValueRef index, LLVMTypeRef type)
{
	//!!ARL: Memory leak -- insert check for NULL (and type).
	LLVMValueRef value = LLVMBuildMalloc(builder, type, "");
	
	LLVMValueRef ptr = LLVMBuildBitCast(builder, value, LLVMPointerType(LLVMInt8Type(), 0), "ptr");
	LLVMValueRef args[] = { array, index, ptr };
	LLVMBuildCall(builder, putarray, args, 3, "");
	
	return value;
}

LLVMValueRef CreateValue(const char* name, LLVMTypeRef type)
{
	//!!ARL: Ensure alloca's all happen in the 'entry' block?
	// This will make sure they only execute once (and mem2reg can deal with them).
	return LLVMBuildAlloca(builder, type, name);
	
	// using malloc allows referencing values declared outside of functions
	// it would be nice if we could determine if this is necessary and produce allocas otherwise
	return LLVMBuildMalloc(builder, type, name); //!!ARL: Might have to make these globals to work across functions
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
	return LLVMBuildNeg(builder, v, "");
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

LLVMBasicBlockRef CreateBlock(LLVMValueRef function, const char* name)
{
	return LLVMAppendBasicBlock(function, name);
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

LLVMValueRef IfElse(LLVMValueRef function, LLVMValueRef cond, LLVMValueRef* results, LLVMBasicBlockRef* blocks)
{
	LLVMBasicBlockRef iftrue = blocks[0];
	LLVMBasicBlockRef iffalse = blocks[1];
	LLVMBasicBlockRef endif = blocks[2];
	
	// move endif to the end of the function so far
	LLVMMoveBasicBlockAfter(endif, LLVMGetLastBasicBlock(function));
	
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

void DoWhile(LLVMValueRef function, LLVMValueRef cond, LLVMBasicBlockRef block)
{
	// insert a branch to our block
	LLVMBasicBlockRef prev_block = LLVMGetPreviousBasicBlock(block);
	LLVMPositionBuilderAtEnd(builder, prev_block);
	LLVMBuildBr(builder, block);
	
	// tack on an end block to branch to when the condition fails		
	LLVMBasicBlockRef enddo = LLVMAppendBasicBlock(function, "enddo");
	LLVMPositionBuilderAtEnd(builder, block);
	
	//!!ARL: Assumes cond is a bool already (need type coersion).
	LLVMBuildCondBr(builder, cond, block, enddo);
	LLVMPositionBuilderAtEnd(builder, enddo);
}

void While(LLVMValueRef function, LLVMValueRef cond, LLVMBasicBlockRef cond_block, LLVMBasicBlockRef block)
{
	// insert a branch to the condition block
	LLVMBasicBlockRef prev_block = LLVMGetPreviousBasicBlock(cond_block);
	LLVMPositionBuilderAtEnd(builder, prev_block);
	LLVMBuildBr(builder, cond_block);
	
	// tack on an end block to branch to when the condition fails		
	LLVMBasicBlockRef endwhile = LLVMAppendBasicBlock(function, "endwhile");
	
	// evaluate condition to decide to execute block or exit
	LLVMPositionBuilderAtEnd(builder, cond_block);
	//!!ARL: Assumes cond is a bool already (need type coersion).
	LLVMBuildCondBr(builder, cond, block, endwhile);
	
	// loop back to evaluate condition at end of block
	LLVMPositionBuilderAtEnd(builder, block);
	LLVMBuildBr(builder, cond_block);
	
	LLVMPositionBuilderAtEnd(builder, endwhile);
}

void ForLoop(LLVMValueRef function, LLVMValueRef cond, LLVMBasicBlockRef* blocks)
{
	LLVMBasicBlockRef init_block = blocks[0];
	LLVMBasicBlockRef cond_block = blocks[1];
	LLVMBasicBlockRef incr_block = blocks[2];
	LLVMBasicBlockRef loop_block = blocks[3];
	LLVMBasicBlockRef  end_block = blocks[4];
	
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
		LLVMBuildCondBr(builder, cond, loop_block, end_block);
	}
	else
	{
		// unconditional - continue loop until disrupted
		LLVMBuildBr(builder, loop_block);
	}
	
	// chain last block back to increment block
	LLVMBasicBlockRef last_block = LLVMGetLastBasicBlock(function);
	LLVMPositionBuilderAtEnd(builder, last_block);
	LLVMBuildBr(builder, incr_block);
	
	// chain increment block to condition block
	LLVMPositionBuilderAtEnd(builder, incr_block);
	LLVMBuildBr(builder, cond_block);
	
	// move end block to end of function where it belongs
	LLVMMoveBasicBlockAfter(end_block, last_block);
	LLVMPositionBuilderAtEnd(builder, end_block);
}

void JumpTo(LLVMBasicBlockRef block)
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

