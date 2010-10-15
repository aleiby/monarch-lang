/*
 *  Codegen.h
 *  Monarch
 *
 *  Created by Aaron Leiby on 10/14/10.
 *  Copyright 2010 __MyCompanyName__. All rights reserved.
 *
 */

//typedef struct LLVMOpaqueType *LLVMTypeRef;
typedef struct LLVMOpaqueValue *LLVMValueRef;

#ifdef __cplusplus
extern "C" {
#endif

	void InitCodegen();
	void TermCodegen();

	LLVMValueRef CreateValue(const char* name);
	LLVMValueRef LoadValue(LLVMValueRef v, const char* name);
	LLVMValueRef ConstValue(int value);
	void PrintValue(LLVMValueRef v, const char * name);
	void Assignment(LLVMValueRef lhs, LLVMValueRef rhs);

#ifdef __cplusplus
}
#endif
