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
typedef struct LLVMOpaqueBasicBlock *LLVMBasicBlockRef;

#ifdef __cplusplus
extern "C" {
#endif

	void InitCodegen();
	void TermCodegen();

	LLVMValueRef CreateValue(const char* name);
	LLVMValueRef LoadValue(LLVMValueRef v);
	LLVMValueRef ConstInt(int value);
	LLVMValueRef ConstBool(int value);
	LLVMValueRef ConstString(const char* value, int length);
	LLVMValueRef IncrementValue(LLVMValueRef v);
	LLVMValueRef DecrementValue(LLVMValueRef v);
	LLVMValueRef NegateValue(LLVMValueRef v);
	LLVMValueRef InvertValue(LLVMValueRef v);
	LLVMValueRef DeleteValue(LLVMValueRef v);
	LLVMValueRef PrintValue(LLVMValueRef v);
	LLVMValueRef Assignment(LLVMValueRef lhs, LLVMValueRef rhs);
	LLVMValueRef AddValues(LLVMValueRef lhs, LLVMValueRef rhs);
	LLVMValueRef SubValues(LLVMValueRef lhs, LLVMValueRef rhs);
	LLVMValueRef MulValues(LLVMValueRef lhs, LLVMValueRef rhs);
	LLVMValueRef DivValues(LLVMValueRef lhs, LLVMValueRef rhs);
	LLVMValueRef ModValues(LLVMValueRef lhs, LLVMValueRef rhs);
	
	LLVMValueRef LogicAnd(LLVMValueRef lhs, LLVMValueRef rhs);
	LLVMValueRef LogicOr(LLVMValueRef lhs, LLVMValueRef rhs);
	LLVMValueRef CmpEQ(LLVMValueRef lhs, LLVMValueRef rhs);
	LLVMValueRef CmpNE(LLVMValueRef lhs, LLVMValueRef rhs);
	LLVMValueRef CmpLT(LLVMValueRef lhs, LLVMValueRef rhs);
	LLVMValueRef CmpGT(LLVMValueRef lhs, LLVMValueRef rhs);
	LLVMValueRef CmpLE(LLVMValueRef lhs, LLVMValueRef rhs);
	LLVMValueRef CmpGE(LLVMValueRef lhs, LLVMValueRef rhs);
	
	LLVMBasicBlockRef CreateBlock(const char* name);
	void BeginBlock(LLVMBasicBlockRef block);
	void LinkTo(LLVMBasicBlockRef block);
	
	LLVMValueRef IfElse(LLVMValueRef cond, LLVMValueRef* results, LLVMBasicBlockRef* blocks);
	void DoWhile(LLVMValueRef cond, LLVMBasicBlockRef block);
	void While(LLVMValueRef cond, LLVMBasicBlockRef cond_block, LLVMBasicBlockRef block);
	void ForLoop(LLVMValueRef cond, LLVMBasicBlockRef* blocks);
	void JumpTo(LLVMBasicBlockRef block);

#ifdef __cplusplus
}
#endif
