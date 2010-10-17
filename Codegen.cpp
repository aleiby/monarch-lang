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
		
		LLVMBasicBlockRef entry = LLVMAppendBasicBlock(top_function, "entry");
		LLVMPositionBuilderAtEnd(builder, entry);
	}
}

void TermCodegen()
{
	LLVMBuildRetVoid(builder);
	
	char *error = NULL;
	LLVMVerifyModule(module, LLVMAbortProcessAction, &error);
	LLVMDisposeMessage(error); // Handler == LLVMAbortProcessAction -> No need to check errors
	
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
	//LLVMAddConstantPropagationPass(pass);
	//LLVMAddInstructionCombiningPass(pass);
	//LLVMAddPromoteMemoryToRegisterPass(pass);
	//LLVMAddReassociatePass(pass);
	//LLVMAddGVNPass(pass);
	//LLVMAddCFGSimplificationPass(pass);
	LLVMRunPassManager(pass, module);
	LLVMDumpModule(module);
	
	LLVMRunFunction(engine, top_function, 0, NULL);
	
	LLVMDisposePassManager(pass);
	LLVMDisposeBuilder(builder);
	LLVMDisposeExecutionEngine(engine);
}

LLVMValueRef CreateValue(const char* name)
{
	//!!ARL: Default value?
	return LLVMBuildAlloca(builder, LLVMInt32Type(), name);
}

LLVMValueRef LoadValue(LLVMValueRef v, const char* name)
{
	return LLVMBuildLoad(builder, v, name);
}

LLVMValueRef ConstInt(int value)
{
	return LLVMConstInt(LLVMInt32Type(), value, 0);
}

LLVMValueRef ConstString(const char* value, int length)
{
	return LLVMConstString(value, length, false);
}

void PrintValue(LLVMValueRef v, const char * name)
{
	v = LLVMBuildLoad(builder, v, name);
	LLVMValueRef args[] = { v };
	LLVMBuildCall(builder, printn, args, 1, "");
}

void Assignment(LLVMValueRef lhs, LLVMValueRef rhs)
{
	LLVMBuildStore(builder, rhs, lhs);
}
