//===-- SystemZTargetTransformInfo.cpp - SystemZ-specific TTI -------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file implements a TargetTransformInfo analysis pass specific to the
// SystemZ target machine. It uses the target's detailed information to provide
// more precise answers to certain TTI queries, while letting the target
// independent and default TTI implementations handle the rest.
//
//===----------------------------------------------------------------------===//

#include "SystemZTargetTransformInfo.h"
#include "llvm/Analysis/TargetTransformInfo.h"
#include "llvm/CodeGen/BasicTTIImpl.h"
#include "llvm/CodeGen/TargetLowering.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/IR/Intrinsics.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/InstructionCost.h"
#include "llvm/Support/MathExtras.h"

using namespace llvm;

#define DEBUG_TYPE "systemztti"

//===----------------------------------------------------------------------===//
//
// SystemZ cost model.
//
//===----------------------------------------------------------------------===//

static bool isUsedAsMemCpySource(const Value *V, bool &OtherUse) {
  bool UsedAsMemCpySource = false;
  for (const User *U : V->users())
    if (const Instruction *User = dyn_cast<Instruction>(U)) {
      if (isa<BitCastInst>(User) || isa<GetElementPtrInst>(User)) {
        UsedAsMemCpySource |= isUsedAsMemCpySource(User, OtherUse);
        continue;
      }
      if (const MemCpyInst *Memcpy = dyn_cast<MemCpyInst>(User)) {
        if (Memcpy->getOperand(1) == V && !Memcpy->isVolatile()) {
          UsedAsMemCpySource = true;
          continue;
        }
      }
      OtherUse = true;
    }
  return UsedAsMemCpySource;
}

static void countNumMemAccesses(const Value *Ptr, unsigned &NumStores,
                                unsigned &NumLoads, const Function *F) {
  if (!isa<PointerType>(Ptr->getType()))
    return;
  for (const User *U : Ptr->users())
    if (const Instruction *User = dyn_cast<Instruction>(U)) {
      if (User->getParent()->getParent() == F) {
        if (const auto *SI = dyn_cast<StoreInst>(User)) {
          if (SI->getPointerOperand() == Ptr && !SI->isVolatile())
            NumStores++;
        } else if (const auto *LI = dyn_cast<LoadInst>(User)) {
          if (LI->getPointerOperand() == Ptr && !LI->isVolatile())
            NumLoads++;
        } else if (const auto *GEP = dyn_cast<GetElementPtrInst>(User)) {
          if (GEP->getPointerOperand() == Ptr)
            countNumMemAccesses(GEP, NumStores, NumLoads, F);
        }
      }
    }
}

unsigned SystemZTTIImpl::adjustInliningThreshold(const CallBase *CB) const {
  unsigned Bonus = 0;
  const Function *Caller = CB->getParent()->getParent();
  const Function *Callee = CB->getCalledFunction();
  if (!Callee)
    return 0;

  // Increase the threshold if an incoming argument is used only as a memcpy
  // source.
  for (const Argument &Arg : Callee->args()) {
    bool OtherUse = false;
    if (isUsedAsMemCpySource(&Arg, OtherUse) && !OtherUse) {
      Bonus = 1000;
      break;
    }
  }

  // Give bonus for globals used much in both caller and a relatively small
  // callee.
  unsigned InstrCount = 0;
  SmallDenseMap<const Value *, unsigned> Ptr2NumUses;
  for (auto &I : instructions(Callee)) {
    if (++InstrCount == 200) {
      Ptr2NumUses.clear();
      break;
    }
    if (const auto *SI = dyn_cast<StoreInst>(&I)) {
      if (!SI->isVolatile())
        if (auto *GV = dyn_cast<GlobalVariable>(SI->getPointerOperand()))
          Ptr2NumUses[GV]++;
    } else if (const auto *LI = dyn_cast<LoadInst>(&I)) {
      if (!LI->isVolatile())
        if (auto *GV = dyn_cast<GlobalVariable>(LI->getPointerOperand()))
          Ptr2NumUses[GV]++;
    } else if (const auto *GEP = dyn_cast<GetElementPtrInst>(&I)) {
      if (auto *GV = dyn_cast<GlobalVariable>(GEP->getPointerOperand())) {
        unsigned NumStores = 0, NumLoads = 0;
        countNumMemAccesses(GEP, NumStores, NumLoads, Callee);
        Ptr2NumUses[GV] += NumLoads + NumStores;
      }
    }
  }

  for (auto [Ptr, NumCalleeUses] : Ptr2NumUses)
    if (NumCalleeUses > 10) {
      unsigned CallerStores = 0, CallerLoads = 0;
      countNumMemAccesses(Ptr, CallerStores, CallerLoads, Caller);
      if (CallerStores + CallerLoads > 10) {
        Bonus = 1000;
        break;
      }
    }

  // Give bonus when Callee accesses an Alloca of Caller heavily.
  unsigned NumStores = 0;
  unsigned NumLoads = 0;
  for (unsigned OpIdx = 0; OpIdx != Callee->arg_size(); ++OpIdx) {
    Value *CallerArg = CB->getArgOperand(OpIdx);
    Argument *CalleeArg = Callee->getArg(OpIdx);
    if (isa<AllocaInst>(CallerArg))
      countNumMemAccesses(CalleeArg, NumStores, NumLoads, Callee);
  }
  if (NumLoads > 10)
    Bonus += NumLoads * 50;
  if (NumStores > 10)
    Bonus += NumStores * 50;
  Bonus = std::min(Bonus, unsigned(1000));

  LLVM_DEBUG(if (Bonus)
               dbgs() << "++ SZTTI Adding inlining bonus: " << Bonus << "\n";);
  return Bonus;
}

InstructionCost
SystemZTTIImpl::getIntImmCost(const APInt &Imm, Type *Ty,
                              TTI::TargetCostKind CostKind) const {
  assert(Ty->isIntegerTy());

  unsigned BitSize = Ty->getPrimitiveSizeInBits();
  // There is no cost model for constants with a bit size of 0. Return TCC_Free
  // here, so that constant hoisting will ignore this constant.
  if (BitSize == 0)
    return TTI::TCC_Free;
  // No cost model for operations on integers larger than 128 bit implemented yet.
  if ((!ST->hasVector() && BitSize > 64) || BitSize > 128)
    return TTI::TCC_Free;

  if (Imm == 0)
    return TTI::TCC_Free;

  if (Imm.getBitWidth() <= 64) {
    // Constants loaded via lgfi.
    if (isInt<32>(Imm.getSExtValue()))
      return TTI::TCC_Basic;
    // Constants loaded via llilf.
    if (isUInt<32>(Imm.getZExtValue()))
      return TTI::TCC_Basic;
    // Constants loaded via llihf:
    if ((Imm.getZExtValue() & 0xffffffff) == 0)
      return TTI::TCC_Basic;

    return 2 * TTI::TCC_Basic;
  }

  // i128 immediates loads from Constant Pool
  return 2 * TTI::TCC_Basic;
}

InstructionCost SystemZTTIImpl::getIntImmCostInst(unsigned Opcode, unsigned Idx,
                                                  const APInt &Imm, Type *Ty,
                                                  TTI::TargetCostKind CostKind,
                                                  Instruction *Inst) const {
  assert(Ty->isIntegerTy());

  unsigned BitSize = Ty->getPrimitiveSizeInBits();
  // There is no cost model for constants with a bit size of 0. Return TCC_Free
  // here, so that constant hoisting will ignore this constant.
  if (BitSize == 0)
    return TTI::TCC_Free;
  // No cost model for operations on integers larger than 64 bit implemented yet.
  if (BitSize > 64)
    return TTI::TCC_Free;

  switch (Opcode) {
  default:
    return TTI::TCC_Free;
  case Instruction::GetElementPtr:
    // Always hoist the base address of a GetElementPtr. This prevents the
    // creation of new constants for every base constant that gets constant
    // folded with the offset.
    if (Idx == 0)
      return 2 * TTI::TCC_Basic;
    return TTI::TCC_Free;
  case Instruction::Store:
    if (Idx == 0 && Imm.getBitWidth() <= 64) {
      // Any 8-bit immediate store can by implemented via mvi.
      if (BitSize == 8)
        return TTI::TCC_Free;
      // 16-bit immediate values can be stored via mvhhi/mvhi/mvghi.
      if (isInt<16>(Imm.getSExtValue()))
        return TTI::TCC_Free;
    }
    break;
  case Instruction::ICmp:
    if (Idx == 1 && Imm.getBitWidth() <= 64) {
      // Comparisons against signed 32-bit immediates implemented via cgfi.
      if (isInt<32>(Imm.getSExtValue()))
        return TTI::TCC_Free;
      // Comparisons against unsigned 32-bit immediates implemented via clgfi.
      if (isUInt<32>(Imm.getZExtValue()))
        return TTI::TCC_Free;
    }
    break;
  case Instruction::Add:
  case Instruction::Sub:
    if (Idx == 1 && Imm.getBitWidth() <= 64) {
      // We use algfi/slgfi to add/subtract 32-bit unsigned immediates.
      if (isUInt<32>(Imm.getZExtValue()))
        return TTI::TCC_Free;
      // Or their negation, by swapping addition vs. subtraction.
      if (isUInt<32>(-Imm.getSExtValue()))
        return TTI::TCC_Free;
    }
    break;
  case Instruction::Mul:
    if (Idx == 1 && Imm.getBitWidth() <= 64) {
      // We use msgfi to multiply by 32-bit signed immediates.
      if (isInt<32>(Imm.getSExtValue()))
        return TTI::TCC_Free;
    }
    break;
  case Instruction::Or:
  case Instruction::Xor:
    if (Idx == 1 && Imm.getBitWidth() <= 64) {
      // Masks supported by oilf/xilf.
      if (isUInt<32>(Imm.getZExtValue()))
        return TTI::TCC_Free;
      // Masks supported by oihf/xihf.
      if ((Imm.getZExtValue() & 0xffffffff) == 0)
        return TTI::TCC_Free;
    }
    break;
  case Instruction::And:
    if (Idx == 1 && Imm.getBitWidth() <= 64) {
      // Any 32-bit AND operation can by implemented via nilf.
      if (BitSize <= 32)
        return TTI::TCC_Free;
      // 64-bit masks supported by nilf.
      if (isUInt<32>(~Imm.getZExtValue()))
        return TTI::TCC_Free;
      // 64-bit masks supported by nilh.
      if ((Imm.getZExtValue() & 0xffffffff) == 0xffffffff)
        return TTI::TCC_Free;
      // Some 64-bit AND operations can be implemented via risbg.
      const SystemZInstrInfo *TII = ST->getInstrInfo();
      unsigned Start, End;
      if (TII->isRxSBGMask(Imm.getZExtValue(), BitSize, Start, End))
        return TTI::TCC_Free;
    }
    break;
  case Instruction::Shl:
  case Instruction::LShr:
  case Instruction::AShr:
    // Always return TCC_Free for the shift value of a shift instruction.
    if (Idx == 1)
      return TTI::TCC_Free;
    break;
  case Instruction::UDiv:
  case Instruction::SDiv:
  case Instruction::URem:
  case Instruction::SRem:
  case Instruction::Trunc:
  case Instruction::ZExt:
  case Instruction::SExt:
  case Instruction::IntToPtr:
  case Instruction::PtrToInt:
  case Instruction::BitCast:
  case Instruction::PHI:
  case Instruction::Call:
  case Instruction::Select:
  case Instruction::Ret:
  case Instruction::Load:
    break;
  }

  return SystemZTTIImpl::getIntImmCost(Imm, Ty, CostKind);
}

InstructionCost
SystemZTTIImpl::getIntImmCostIntrin(Intrinsic::ID IID, unsigned Idx,
                                    const APInt &Imm, Type *Ty,
                                    TTI::TargetCostKind CostKind) const {
  assert(Ty->isIntegerTy());

  unsigned BitSize = Ty->getPrimitiveSizeInBits();
  // There is no cost model for constants with a bit size of 0. Return TCC_Free
  // here, so that constant hoisting will ignore this constant.
  if (BitSize == 0)
    return TTI::TCC_Free;
  // No cost model for operations on integers larger than 64 bit implemented yet.
  if (BitSize > 64)
    return TTI::TCC_Free;

  switch (IID) {
  default:
    return TTI::TCC_Free;
  case Intrinsic::sadd_with_overflow:
  case Intrinsic::uadd_with_overflow:
  case Intrinsic::ssub_with_overflow:
  case Intrinsic::usub_with_overflow:
    // These get expanded to include a normal addition/subtraction.
    if (Idx == 1 && Imm.getBitWidth() <= 64) {
      if (isUInt<32>(Imm.getZExtValue()))
        return TTI::TCC_Free;
      if (isUInt<32>(-Imm.getSExtValue()))
        return TTI::TCC_Free;
    }
    break;
  case Intrinsic::smul_with_overflow:
  case Intrinsic::umul_with_overflow:
    // These get expanded to include a normal multiplication.
    if (Idx == 1 && Imm.getBitWidth() <= 64) {
      if (isInt<32>(Imm.getSExtValue()))
        return TTI::TCC_Free;
    }
    break;
  case Intrinsic::experimental_stackmap:
    if ((Idx < 2) || (Imm.getBitWidth() <= 64 && isInt<64>(Imm.getSExtValue())))
      return TTI::TCC_Free;
    break;
  case Intrinsic::experimental_patchpoint_void:
  case Intrinsic::experimental_patchpoint:
    if ((Idx < 4) || (Imm.getBitWidth() <= 64 && isInt<64>(Imm.getSExtValue())))
      return TTI::TCC_Free;
    break;
  }
  return SystemZTTIImpl::getIntImmCost(Imm, Ty, CostKind);
}

TargetTransformInfo::PopcntSupportKind
SystemZTTIImpl::getPopcntSupport(unsigned TyWidth) const {
  assert(isPowerOf2_32(TyWidth) && "Type width must be power of 2");
  if (ST->hasPopulationCount() && TyWidth <= 64)
    return TTI::PSK_FastHardware;
  return TTI::PSK_Software;
}

void SystemZTTIImpl::getUnrollingPreferences(
    Loop *L, ScalarEvolution &SE, TTI::UnrollingPreferences &UP,
    OptimizationRemarkEmitter *ORE) const {
  // Find out if L contains a call, what the machine instruction count
  // estimate is, and how many stores there are.
  bool HasCall = false;
  InstructionCost NumStores = 0;
  for (auto &BB : L->blocks())
    for (auto &I : *BB) {
      if (isa<CallInst>(&I) || isa<InvokeInst>(&I)) {
        if (const Function *F = cast<CallBase>(I).getCalledFunction()) {
          if (isLoweredToCall(F))
            HasCall = true;
          if (F->getIntrinsicID() == Intrinsic::memcpy ||
              F->getIntrinsicID() == Intrinsic::memset)
            NumStores++;
        } else { // indirect call.
          HasCall = true;
        }
      }
      if (isa<StoreInst>(&I)) {
        Type *MemAccessTy = I.getOperand(0)->getType();
        NumStores += getMemoryOpCost(Instruction::Store, MemAccessTy, Align(),
                                     0, TTI::TCK_RecipThroughput);
      }
    }

  // The z13 processor will run out of store tags if too many stores
  // are fed into it too quickly. Therefore make sure there are not
  // too many stores in the resulting unrolled loop.
  unsigned const NumStoresVal = NumStores.getValue();
  unsigned const Max = (NumStoresVal ? (12 / NumStoresVal) : UINT_MAX);

  if (HasCall) {
    // Only allow full unrolling if loop has any calls.
    UP.FullUnrollMaxCount = Max;
    UP.MaxCount = 1;
    return;
  }

  UP.MaxCount = Max;
  if (UP.MaxCount <= 1)
    return;

  // Allow partial and runtime trip count unrolling.
  UP.Partial = UP.Runtime = true;

  UP.PartialThreshold = 75;
  UP.DefaultUnrollRuntimeCount = 4;

  // Allow expensive instructions in the pre-header of the loop.
  UP.AllowExpensiveTripCount = true;

  UP.Force = true;
}

void SystemZTTIImpl::getPeelingPreferences(Loop *L, ScalarEvolution &SE,
                                           TTI::PeelingPreferences &PP) const {
  BaseT::getPeelingPreferences(L, SE, PP);
}

bool SystemZTTIImpl::isLSRCostLess(
    const TargetTransformInfo::LSRCost &C1,
    const TargetTransformInfo::LSRCost &C2) const {
  // SystemZ specific: check instruction count (first), and don't care about
  // ImmCost, since offsets are checked explicitly.
  return std::tie(C1.Insns, C1.NumRegs, C1.AddRecCost,
                  C1.NumIVMuls, C1.NumBaseAdds,
                  C1.ScaleCost, C1.SetupCost) <
    std::tie(C2.Insns, C2.NumRegs, C2.AddRecCost,
             C2.NumIVMuls, C2.NumBaseAdds,
             C2.ScaleCost, C2.SetupCost);
}

bool SystemZTTIImpl::areInlineCompatible(const Function *Caller,
                                         const Function *Callee) const {
  const TargetMachine &TM = getTLI()->getTargetMachine();

  const FeatureBitset &CallerBits =
      TM.getSubtargetImpl(*Caller)->getFeatureBits();
  const FeatureBitset &CalleeBits =
      TM.getSubtargetImpl(*Callee)->getFeatureBits();

  // Support only equal feature bitsets. Restriction should be relaxed in the
  // future to allow inlining when callee's bits are subset of the caller's.
  return CallerBits == CalleeBits;
}

unsigned SystemZTTIImpl::getNumberOfRegisters(unsigned ClassID) const {
  bool Vector = (ClassID == 1);
  if (!Vector)
    // Discount the stack pointer.  Also leave out %r0, since it can't
    // be used in an address.
    return 14;
  if (ST->hasVector())
    return 32;
  return 0;
}

TypeSize
SystemZTTIImpl::getRegisterBitWidth(TargetTransformInfo::RegisterKind K) const {
  switch (K) {
  case TargetTransformInfo::RGK_Scalar:
    return TypeSize::getFixed(64);
  case TargetTransformInfo::RGK_FixedWidthVector:
    return TypeSize::getFixed(ST->hasVector() ? 128 : 0);
  case TargetTransformInfo::RGK_ScalableVector:
    return TypeSize::getScalable(0);
  }

  llvm_unreachable("Unsupported register kind");
}

unsigned SystemZTTIImpl::getMinPrefetchStride(unsigned NumMemAccesses,
                                              unsigned NumStridedMemAccesses,
                                              unsigned NumPrefetches,
                                              bool HasCall) const {
  // Don't prefetch a loop with many far apart accesses.
  if (NumPrefetches > 16)
    return UINT_MAX;

  // Emit prefetch instructions for smaller strides in cases where we think
  // the hardware prefetcher might not be able to keep up.
  if (NumStridedMemAccesses > 32 && !HasCall &&
      (NumMemAccesses - NumStridedMemAccesses) * 32 <= NumStridedMemAccesses)
    return 1;

  return ST->hasMiscellaneousExtensions3() ? 8192 : 2048;
}

bool SystemZTTIImpl::hasDivRemOp(Type *DataType, bool IsSigned) const {
  EVT VT = TLI->getValueType(DL, DataType);
  return (VT.isScalarInteger() && TLI->isTypeLegal(VT));
}

static bool isFreeEltLoad(const Value *Op) {
  if (isa<LoadInst>(Op) && Op->hasOneUse()) {
    const Instruction *UserI = cast<Instruction>(*Op->user_begin());
    return !isa<StoreInst>(UserI); // Prefer MVC
  }
  return false;
}

InstructionCost SystemZTTIImpl::getScalarizationOverhead(
    VectorType *Ty, const APInt &DemandedElts, bool Insert, bool Extract,
    TTI::TargetCostKind CostKind, bool ForPoisonSrc,
    ArrayRef<Value *> VL) const {
  unsigned NumElts = cast<FixedVectorType>(Ty)->getNumElements();
  InstructionCost Cost = 0;

  if (Insert && Ty->isIntOrIntVectorTy(64)) {
    // VLVGP will insert two GPRs with one instruction, while VLE will load
    // an element directly with no extra cost
    assert((VL.empty() || VL.size() == NumElts) &&
           "Type does not match the number of values.");
    InstructionCost CurrVectorCost = 0;
    for (unsigned Idx = 0; Idx < NumElts; ++Idx) {
      if (DemandedElts[Idx] && !(VL.size() && isFreeEltLoad(VL[Idx])))
        ++CurrVectorCost;
      if (Idx % 2 == 1) {
        Cost += std::min(InstructionCost(1), CurrVectorCost);
        CurrVectorCost = 0;
      }
    }
    Insert = false;
  }

  Cost += BaseT::getScalarizationOverhead(Ty, DemandedElts, Insert, Extract,
                                          CostKind, ForPoisonSrc, VL);
  return Cost;
}

// Return the bit size for the scalar type or vector element
// type. getScalarSizeInBits() returns 0 for a pointer type.
static unsigned getScalarSizeInBits(Type *Ty) {
  unsigned Size =
    (Ty->isPtrOrPtrVectorTy() ? 64U : Ty->getScalarSizeInBits());
  assert(Size > 0 && "Element must have non-zero size.");
  return Size;
}

// getNumberOfParts() calls getTypeLegalizationCost() which splits the vector
// type until it is legal. This would e.g. return 4 for <6 x i64>, instead of
// 3.
static unsigned getNumVectorRegs(Type *Ty) {
  auto *VTy = cast<FixedVectorType>(Ty);
  unsigned WideBits = getScalarSizeInBits(Ty) * VTy->getNumElements();
  assert(WideBits > 0 && "Could not compute size of vector");
  return ((WideBits % 128U) ? ((WideBits / 128U) + 1) : (WideBits / 128U));
}

InstructionCost SystemZTTIImpl::getArithmeticInstrCost(
    unsigned Opcode, Type *Ty, TTI::TargetCostKind CostKind,
    TTI::OperandValueInfo Op1Info, TTI::OperandValueInfo Op2Info,
    ArrayRef<const Value *> Args, const Instruction *CxtI) const {

  // TODO: Handle more cost kinds.
  if (CostKind != TTI::TCK_RecipThroughput)
    return BaseT::getArithmeticInstrCost(Opcode, Ty, CostKind, Op1Info,
                                         Op2Info, Args, CxtI);

  // TODO: return a good value for BB-VECTORIZER that includes the
  // immediate loads, which we do not want to count for the loop
  // vectorizer, since they are hopefully hoisted out of the loop. This
  // would require a new parameter 'InLoop', but not sure if constant
  // args are common enough to motivate this.

  unsigned ScalarBits = Ty->getScalarSizeInBits();

  // There are thre cases of division and remainder: Dividing with a register
  // needs a divide instruction. A divisor which is a power of two constant
  // can be implemented with a sequence of shifts. Any other constant needs a
  // multiply and shifts.
  const unsigned DivInstrCost = 20;
  const unsigned DivMulSeqCost = 10;
  const unsigned SDivPow2Cost = 4;

  bool SignedDivRem =
      Opcode == Instruction::SDiv || Opcode == Instruction::SRem;
  bool UnsignedDivRem =
      Opcode == Instruction::UDiv || Opcode == Instruction::URem;

  // Check for a constant divisor.
  bool DivRemConst = false;
  bool DivRemConstPow2 = false;
  if ((SignedDivRem || UnsignedDivRem) && Args.size() == 2) {
    if (const Constant *C = dyn_cast<Constant>(Args[1])) {
      const ConstantInt *CVal =
          (C->getType()->isVectorTy()
               ? dyn_cast_or_null<const ConstantInt>(C->getSplatValue())
               : dyn_cast<const ConstantInt>(C));
      if (CVal && (CVal->getValue().isPowerOf2() ||
                   CVal->getValue().isNegatedPowerOf2()))
        DivRemConstPow2 = true;
      else
        DivRemConst = true;
    }
  }

  if (!Ty->isVectorTy()) {
    // These FP operations are supported with a dedicated instruction for
    // float, double and fp128 (base implementation assumes float generally
    // costs 2).
    if (Opcode == Instruction::FAdd || Opcode == Instruction::FSub ||
        Opcode == Instruction::FMul || Opcode == Instruction::FDiv)
      return 1;

    // There is no native support for FRem.
    if (Opcode == Instruction::FRem)
      return LIBCALL_COST;

    // Give discount for some combined logical operations if supported.
    if (Args.size() == 2) {
      if (Opcode == Instruction::Xor) {
        for (const Value *A : Args) {
          if (const Instruction *I = dyn_cast<Instruction>(A))
            if (I->hasOneUse() &&
                (I->getOpcode() == Instruction::Or ||
                 I->getOpcode() == Instruction::And ||
                 I->getOpcode() == Instruction::Xor))
              if ((ScalarBits <= 64 && ST->hasMiscellaneousExtensions3()) ||
                  (isInt128InVR(Ty) &&
                   (I->getOpcode() == Instruction::Or || ST->hasVectorEnhancements1())))
                return 0;
        }
      }
      else if (Opcode == Instruction::And || Opcode == Instruction::Or) {
        for (const Value *A : Args) {
          if (const Instruction *I = dyn_cast<Instruction>(A))
            if ((I->hasOneUse() && I->getOpcode() == Instruction::Xor) &&
                ((ScalarBits <= 64 && ST->hasMiscellaneousExtensions3()) ||
                 (isInt128InVR(Ty) &&
                  (Opcode == Instruction::And || ST->hasVectorEnhancements1()))))
              return 0;
        }
      }
    }

    // Or requires one instruction, although it has custom handling for i64.
    if (Opcode == Instruction::Or)
      return 1;

    if (Opcode == Instruction::Xor && ScalarBits == 1) {
      if (ST->hasLoadStoreOnCond2())
        return 5; // 2 * (li 0; loc 1); xor
      return 7; // 2 * ipm sequences ; xor ; shift ; compare
    }

    if (DivRemConstPow2)
      return (SignedDivRem ? SDivPow2Cost : 1);
    if (DivRemConst)
      return DivMulSeqCost;
    if (SignedDivRem || UnsignedDivRem)
      return DivInstrCost;
  }
  else if (ST->hasVector()) {
    auto *VTy = cast<FixedVectorType>(Ty);
    unsigned VF = VTy->getNumElements();
    unsigned NumVectors = getNumVectorRegs(Ty);

    // These vector operations are custom handled, but are still supported
    // with one instruction per vector, regardless of element size.
    if (Opcode == Instruction::Shl || Opcode == Instruction::LShr ||
        Opcode == Instruction::AShr) {
      return NumVectors;
    }

    if (DivRemConstPow2)
      return (NumVectors * (SignedDivRem ? SDivPow2Cost : 1));
    if (DivRemConst) {
      SmallVector<Type *> Tys(Args.size(), Ty);
      return VF * DivMulSeqCost +
             BaseT::getScalarizationOverhead(VTy, Args, Tys, CostKind);
    }
    if (SignedDivRem || UnsignedDivRem) {
      if (ST->hasVectorEnhancements3() && ScalarBits >= 32)
        return NumVectors * DivInstrCost;
      else if (VF > 4)
        // Temporary hack: disable high vectorization factors with integer
        // division/remainder, which will get scalarized and handled with
        // GR128 registers. The mischeduler is not clever enough to avoid
        // spilling yet.
        return 1000;
    }

    // These FP operations are supported with a single vector instruction for
    // double (base implementation assumes float generally costs 2). For
    // FP128, the scalar cost is 1, and there is no overhead since the values
    // are already in scalar registers.
    if (Opcode == Instruction::FAdd || Opcode == Instruction::FSub ||
        Opcode == Instruction::FMul || Opcode == Instruction::FDiv) {
      switch (ScalarBits) {
      case 32: {
        // The vector enhancements facility 1 provides v4f32 instructions.
        if (ST->hasVectorEnhancements1())
          return NumVectors;
        // Return the cost of multiple scalar invocation plus the cost of
        // inserting and extracting the values.
        InstructionCost ScalarCost =
            getArithmeticInstrCost(Opcode, Ty->getScalarType(), CostKind);
        SmallVector<Type *> Tys(Args.size(), Ty);
        InstructionCost Cost =
            (VF * ScalarCost) +
            BaseT::getScalarizationOverhead(VTy, Args, Tys, CostKind);
        // FIXME: VF 2 for these FP operations are currently just as
        // expensive as for VF 4.
        if (VF == 2)
          Cost *= 2;
        return Cost;
      }
      case 64:
      case 128:
        return NumVectors;
      default:
        break;
      }
    }

    // There is no native support for FRem.
    if (Opcode == Instruction::FRem) {
      SmallVector<Type *> Tys(Args.size(), Ty);
      InstructionCost Cost =
          (VF * LIBCALL_COST) +
          BaseT::getScalarizationOverhead(VTy, Args, Tys, CostKind);
      // FIXME: VF 2 for float is currently just as expensive as for VF 4.
      if (VF == 2 && ScalarBits == 32)
        Cost *= 2;
      return Cost;
    }
  }

  // Fallback to the default implementation.
  return BaseT::getArithmeticInstrCost(Opcode, Ty, CostKind, Op1Info, Op2Info,
                                       Args, CxtI);
}

InstructionCost
SystemZTTIImpl::getShuffleCost(TTI::ShuffleKind Kind, VectorType *DstTy,
                               VectorType *SrcTy, ArrayRef<int> Mask,
                               TTI::TargetCostKind CostKind, int Index,
                               VectorType *SubTp, ArrayRef<const Value *> Args,
                               const Instruction *CxtI) const {
  Kind = improveShuffleKindFromMask(Kind, Mask, SrcTy, Index, SubTp);
  if (ST->hasVector()) {
    unsigned NumVectors = getNumVectorRegs(SrcTy);

    // TODO: Since fp32 is expanded, the shuffle cost should always be 0.

    // FP128 values are always in scalar registers, so there is no work
    // involved with a shuffle, except for broadcast. In that case register
    // moves are done with a single instruction per element.
    if (SrcTy->getScalarType()->isFP128Ty())
      return (Kind == TargetTransformInfo::SK_Broadcast ? NumVectors - 1 : 0);

    switch (Kind) {
    case  TargetTransformInfo::SK_ExtractSubvector:
      // ExtractSubvector Index indicates start offset.

      // Extracting a subvector from first index is a noop.
      return (Index == 0 ? 0 : NumVectors);

    case TargetTransformInfo::SK_Broadcast:
      // Loop vectorizer calls here to figure out the extra cost of
      // broadcasting a loaded value to all elements of a vector. Since vlrep
      // loads and replicates with a single instruction, adjust the returned
      // value.
      return NumVectors - 1;

    default:

      // SystemZ supports single instruction permutation / replication.
      return NumVectors;
    }
  }

  return BaseT::getShuffleCost(Kind, DstTy, SrcTy, Mask, CostKind, Index,
                               SubTp);
}

// Return the log2 difference of the element sizes of the two vector types.
static unsigned getElSizeLog2Diff(Type *Ty0, Type *Ty1) {
  unsigned Bits0 = Ty0->getScalarSizeInBits();
  unsigned Bits1 = Ty1->getScalarSizeInBits();

  if (Bits1 >  Bits0)
    return (Log2_32(Bits1) - Log2_32(Bits0));

  return (Log2_32(Bits0) - Log2_32(Bits1));
}

// Return the number of instructions needed to truncate SrcTy to DstTy.
unsigned SystemZTTIImpl::getVectorTruncCost(Type *SrcTy, Type *DstTy) const {
  assert (SrcTy->isVectorTy() && DstTy->isVectorTy());
  assert(SrcTy->getPrimitiveSizeInBits().getFixedValue() >
             DstTy->getPrimitiveSizeInBits().getFixedValue() &&
         "Packing must reduce size of vector type.");
  assert(cast<FixedVectorType>(SrcTy)->getNumElements() ==
             cast<FixedVectorType>(DstTy)->getNumElements() &&
         "Packing should not change number of elements.");

  // TODO: Since fp32 is expanded, the extract cost should always be 0.

  unsigned NumParts = getNumVectorRegs(SrcTy);
  if (NumParts <= 2)
    // Up to 2 vector registers can be truncated efficiently with pack or
    // permute. The latter requires an immediate mask to be loaded, which
    // typically gets hoisted out of a loop.  TODO: return a good value for
    // BB-VECTORIZER that includes the immediate loads, which we do not want
    // to count for the loop vectorizer.
    return 1;

  unsigned Cost = 0;
  unsigned Log2Diff = getElSizeLog2Diff(SrcTy, DstTy);
  unsigned VF = cast<FixedVectorType>(SrcTy)->getNumElements();
  for (unsigned P = 0; P < Log2Diff; ++P) {
    if (NumParts > 1)
      NumParts /= 2;
    Cost += NumParts;
  }

  // Currently, a general mix of permutes and pack instructions is output by
  // isel, which follow the cost computation above except for this case which
  // is one instruction less:
  if (VF == 8 && SrcTy->getScalarSizeInBits() == 64 &&
      DstTy->getScalarSizeInBits() == 8)
    Cost--;

  return Cost;
}

// Return the cost of converting a vector bitmask produced by a compare
// (SrcTy), to the type of the select or extend instruction (DstTy).
unsigned SystemZTTIImpl::getVectorBitmaskConversionCost(Type *SrcTy,
                                                        Type *DstTy) const {
  assert (SrcTy->isVectorTy() && DstTy->isVectorTy() &&
          "Should only be called with vector types.");

  unsigned PackCost = 0;
  unsigned SrcScalarBits = SrcTy->getScalarSizeInBits();
  unsigned DstScalarBits = DstTy->getScalarSizeInBits();
  unsigned Log2Diff = getElSizeLog2Diff(SrcTy, DstTy);
  if (SrcScalarBits > DstScalarBits)
    // The bitmask will be truncated.
    PackCost = getVectorTruncCost(SrcTy, DstTy);
  else if (SrcScalarBits < DstScalarBits) {
    unsigned DstNumParts = getNumVectorRegs(DstTy);
    // Each vector select needs its part of the bitmask unpacked.
    PackCost = Log2Diff * DstNumParts;
    // Extra cost for moving part of mask before unpacking.
    PackCost += DstNumParts - 1;
  }

  return PackCost;
}

// Return the type of the compared operands. This is needed to compute the
// cost for a Select / ZExt or SExt instruction.
static Type *getCmpOpsType(const Instruction *I, unsigned VF = 1) {
  Type *OpTy = nullptr;
  if (CmpInst *CI = dyn_cast<CmpInst>(I->getOperand(0)))
    OpTy = CI->getOperand(0)->getType();
  else if (Instruction *LogicI = dyn_cast<Instruction>(I->getOperand(0)))
    if (LogicI->getNumOperands() == 2)
      if (CmpInst *CI0 = dyn_cast<CmpInst>(LogicI->getOperand(0)))
        if (isa<CmpInst>(LogicI->getOperand(1)))
          OpTy = CI0->getOperand(0)->getType();

  if (OpTy != nullptr) {
    if (VF == 1) {
      assert (!OpTy->isVectorTy() && "Expected scalar type");
      return OpTy;
    }
    // Return the potentially vectorized type based on 'I' and 'VF'.  'I' may
    // be either scalar or already vectorized with a same or lesser VF.
    Type *ElTy = OpTy->getScalarType();
    return FixedVectorType::get(ElTy, VF);
  }

  return nullptr;
}

// Get the cost of converting a boolean vector to a vector with same width
// and element size as Dst, plus the cost of zero extending if needed.
unsigned
SystemZTTIImpl::getBoolVecToIntConversionCost(unsigned Opcode, Type *Dst,
                                              const Instruction *I) const {
  auto *DstVTy = cast<FixedVectorType>(Dst);
  unsigned VF = DstVTy->getNumElements();
  unsigned Cost = 0;
  // If we know what the widths of the compared operands, get any cost of
  // converting it to match Dst. Otherwise assume same widths.
  Type *CmpOpTy = ((I != nullptr) ? getCmpOpsType(I, VF) : nullptr);
  if (CmpOpTy != nullptr)
    Cost = getVectorBitmaskConversionCost(CmpOpTy, Dst);
  if (Opcode == Instruction::ZExt || Opcode == Instruction::UIToFP)
    // One 'vn' per dst vector with an immediate mask.
    Cost += getNumVectorRegs(Dst);
  return Cost;
}

InstructionCost SystemZTTIImpl::getCastInstrCost(unsigned Opcode, Type *Dst,
                                                 Type *Src,
                                                 TTI::CastContextHint CCH,
                                                 TTI::TargetCostKind CostKind,
                                                 const Instruction *I) const {
  // FIXME: Can the logic below also be used for these cost kinds?
  if (CostKind == TTI::TCK_CodeSize || CostKind == TTI::TCK_SizeAndLatency) {
    auto BaseCost = BaseT::getCastInstrCost(Opcode, Dst, Src, CCH, CostKind, I);
    return BaseCost == 0 ? BaseCost : 1;
  }

  unsigned DstScalarBits = Dst->getScalarSizeInBits();
  unsigned SrcScalarBits = Src->getScalarSizeInBits();

  if (!Src->isVectorTy()) {
    if (Dst->isVectorTy())
      return BaseT::getCastInstrCost(Opcode, Dst, Src, CCH, CostKind, I);

    if (Opcode == Instruction::SIToFP || Opcode == Instruction::UIToFP) {
      if (Src->isIntegerTy(128))
        return LIBCALL_COST;
      if (SrcScalarBits >= 32 ||
          (I != nullptr && isa<LoadInst>(I->getOperand(0))))
        return 1;
      return SrcScalarBits > 1 ? 2 /*i8/i16 extend*/ : 5 /*branch seq.*/;
    }

    if ((Opcode == Instruction::FPToSI || Opcode == Instruction::FPToUI) &&
        Dst->isIntegerTy(128))
      return LIBCALL_COST;

    if ((Opcode == Instruction::ZExt || Opcode == Instruction::SExt)) {
      if (Src->isIntegerTy(1)) {
        if (DstScalarBits == 128) {
          if (Opcode == Instruction::SExt && ST->hasVectorEnhancements3())
            return 0;/*VCEQQ*/
          return 5 /*branch seq.*/;
        }

        if (ST->hasLoadStoreOnCond2())
          return 2; // li 0; loc 1

        // This should be extension of a compare i1 result, which is done with
        // ipm and a varying sequence of instructions.
        unsigned Cost = 0;
        if (Opcode == Instruction::SExt)
          Cost = (DstScalarBits < 64 ? 3 : 4);
        if (Opcode == Instruction::ZExt)
          Cost = 3;
        Type *CmpOpTy = ((I != nullptr) ? getCmpOpsType(I) : nullptr);
        if (CmpOpTy != nullptr && CmpOpTy->isFloatingPointTy())
          // If operands of an fp-type was compared, this costs +1.
          Cost++;
        return Cost;
      }
      else if (isInt128InVR(Dst)) {
        // Extensions from GPR to i128 (in VR) typically costs two instructions,
        // but a zero-extending load would be just one extra instruction.
        if (Opcode == Instruction::ZExt && I != nullptr)
          if (LoadInst *Ld = dyn_cast<LoadInst>(I->getOperand(0)))
            if (Ld->hasOneUse())
              return 1;
        return 2;
      }
    }

    if (Opcode == Instruction::Trunc && isInt128InVR(Src) && I != nullptr) {
      if (LoadInst *Ld = dyn_cast<LoadInst>(I->getOperand(0)))
        if (Ld->hasOneUse())
          return 0;  // Will be converted to GPR load.
      bool OnlyTruncatingStores = true;
      for (const User *U : I->users())
        if (!isa<StoreInst>(U)) {
          OnlyTruncatingStores = false;
          break;
        }
      if (OnlyTruncatingStores)
        return 0;
      return 2; // Vector element extraction.
    }
  }
  else if (ST->hasVector()) {
    // Vector to scalar cast.
    auto *SrcVecTy = cast<FixedVectorType>(Src);
    auto *DstVecTy = dyn_cast<FixedVectorType>(Dst);
    if (!DstVecTy) {
      // TODO: tune vector-to-scalar cast.
      return BaseT::getCastInstrCost(Opcode, Dst, Src, CCH, CostKind, I);
    }
    unsigned VF = SrcVecTy->getNumElements();
    unsigned NumDstVectors = getNumVectorRegs(Dst);
    unsigned NumSrcVectors = getNumVectorRegs(Src);

    if (Opcode == Instruction::Trunc) {
      if (Src->getScalarSizeInBits() == Dst->getScalarSizeInBits())
        return 0; // Check for NOOP conversions.
      return getVectorTruncCost(Src, Dst);
    }

    if (Opcode == Instruction::ZExt || Opcode == Instruction::SExt) {
      if (SrcScalarBits >= 8) {
        // ZExt will use either a single unpack or a vector permute.
        if (Opcode == Instruction::ZExt)
          return NumDstVectors;

        // SExt will be handled with one unpack per doubling of width.
        unsigned NumUnpacks = getElSizeLog2Diff(Src, Dst);

        // For types that spans multiple vector registers, some additional
        // instructions are used to setup the unpacking.
        unsigned NumSrcVectorOps =
          (NumUnpacks > 1 ? (NumDstVectors - NumSrcVectors)
                          : (NumDstVectors / 2));

        return (NumUnpacks * NumDstVectors) + NumSrcVectorOps;
      }
      else if (SrcScalarBits == 1)
        return getBoolVecToIntConversionCost(Opcode, Dst, I);
    }

    if (Opcode == Instruction::SIToFP || Opcode == Instruction::UIToFP ||
        Opcode == Instruction::FPToSI || Opcode == Instruction::FPToUI) {
      // TODO: Fix base implementation which could simplify things a bit here
      // (seems to miss on differentiating on scalar/vector types).

      // Only 64 bit vector conversions are natively supported before z15.
      if (DstScalarBits == 64 || ST->hasVectorEnhancements2()) {
        if (SrcScalarBits == DstScalarBits)
          return NumDstVectors;

        if (SrcScalarBits == 1)
          return getBoolVecToIntConversionCost(Opcode, Dst, I) + NumDstVectors;
      }

      // Return the cost of multiple scalar invocation plus the cost of
      // inserting and extracting the values. Base implementation does not
      // realize float->int gets scalarized.
      InstructionCost ScalarCost = getCastInstrCost(
          Opcode, Dst->getScalarType(), Src->getScalarType(), CCH, CostKind);
      InstructionCost TotCost = VF * ScalarCost;
      bool NeedsInserts = true, NeedsExtracts = true;
      // FP128 registers do not get inserted or extracted.
      if (DstScalarBits == 128 &&
          (Opcode == Instruction::SIToFP || Opcode == Instruction::UIToFP))
        NeedsInserts = false;
      if (SrcScalarBits == 128 &&
          (Opcode == Instruction::FPToSI || Opcode == Instruction::FPToUI))
        NeedsExtracts = false;

      TotCost += BaseT::getScalarizationOverhead(SrcVecTy, /*Insert*/ false,
                                                 NeedsExtracts, CostKind);
      TotCost += BaseT::getScalarizationOverhead(DstVecTy, NeedsInserts,
                                                 /*Extract*/ false, CostKind);

      // FIXME: VF 2 for float<->i32 is currently just as expensive as for VF 4.
      if (VF == 2 && SrcScalarBits == 32 && DstScalarBits == 32)
        TotCost *= 2;

      return TotCost;
    }

    if (Opcode == Instruction::FPTrunc) {
      if (SrcScalarBits == 128)  // fp128 -> double/float + inserts of elements.
        return VF /*ldxbr/lexbr*/ +
               BaseT::getScalarizationOverhead(DstVecTy, /*Insert*/ true,
                                               /*Extract*/ false, CostKind);
      else // double -> float
        return VF / 2 /*vledb*/ + std::max(1U, VF / 4 /*vperm*/);
    }

    if (Opcode == Instruction::FPExt) {
      if (SrcScalarBits == 32 && DstScalarBits == 64) {
        // float -> double is very rare and currently unoptimized. Instead of
        // using vldeb, which can do two at a time, all conversions are
        // scalarized.
        return VF * 2;
      }
      // -> fp128.  VF * lxdb/lxeb + extraction of elements.
      return VF + BaseT::getScalarizationOverhead(SrcVecTy, /*Insert*/ false,
                                                  /*Extract*/ true, CostKind);
    }
  }

  return BaseT::getCastInstrCost(Opcode, Dst, Src, CCH, CostKind, I);
}

// Scalar i8 / i16 operations will typically be made after first extending
// the operands to i32.
static unsigned getOperandsExtensionCost(const Instruction *I) {
  unsigned ExtCost = 0;
  for (Value *Op : I->operands())
    // A load of i8 or i16 sign/zero extends to i32.
    if (!isa<LoadInst>(Op) && !isa<ConstantInt>(Op))
      ExtCost++;

  return ExtCost;
}

InstructionCost SystemZTTIImpl::getCmpSelInstrCost(
    unsigned Opcode, Type *ValTy, Type *CondTy, CmpInst::Predicate VecPred,
    TTI::TargetCostKind CostKind, TTI::OperandValueInfo Op1Info,
    TTI::OperandValueInfo Op2Info, const Instruction *I) const {
  if (CostKind != TTI::TCK_RecipThroughput)
    return BaseT::getCmpSelInstrCost(Opcode, ValTy, CondTy, VecPred, CostKind,
                                     Op1Info, Op2Info);

  if (!ValTy->isVectorTy()) {
    switch (Opcode) {
    case Instruction::ICmp: {
      // A loaded value compared with 0 with multiple users becomes Load and
      // Test. The load is then not foldable, so return 0 cost for the ICmp.
      unsigned ScalarBits = ValTy->getScalarSizeInBits();
      if (I != nullptr && (ScalarBits == 32 || ScalarBits == 64))
        if (LoadInst *Ld = dyn_cast<LoadInst>(I->getOperand(0)))
          if (const ConstantInt *C = dyn_cast<ConstantInt>(I->getOperand(1)))
            if (!Ld->hasOneUse() && Ld->getParent() == I->getParent() &&
                C->isZero())
              return 0;

      unsigned Cost = 1;
      if (ValTy->isIntegerTy() && ValTy->getScalarSizeInBits() <= 16)
        Cost += (I != nullptr ? getOperandsExtensionCost(I) : 2);
      return Cost;
    }
    case Instruction::Select:
      if (ValTy->isFloatingPointTy())
        return 4; // No LOC for FP - costs a conditional jump.

      // When selecting based on an i128 comparison, LOC / VSEL is possible
      // if i128 comparisons are directly supported.
      if (I != nullptr)
        if (ICmpInst *CI = dyn_cast<ICmpInst>(I->getOperand(0)))
          if (CI->getOperand(0)->getType()->isIntegerTy(128))
            return ST->hasVectorEnhancements3() ? 1 : 4;

      // Load On Condition / Select Register available, except for i128.
      return !isInt128InVR(ValTy) ? 1 : 4;
    }
  }
  else if (ST->hasVector()) {
    unsigned VF = cast<FixedVectorType>(ValTy)->getNumElements();

    // Called with a compare instruction.
    if (Opcode == Instruction::ICmp || Opcode == Instruction::FCmp) {
      unsigned PredicateExtraCost = 0;
      if (I != nullptr) {
        // Some predicates cost one or two extra instructions.
        switch (cast<CmpInst>(I)->getPredicate()) {
        case CmpInst::Predicate::ICMP_NE:
        case CmpInst::Predicate::ICMP_UGE:
        case CmpInst::Predicate::ICMP_ULE:
        case CmpInst::Predicate::ICMP_SGE:
        case CmpInst::Predicate::ICMP_SLE:
          PredicateExtraCost = 1;
          break;
        case CmpInst::Predicate::FCMP_ONE:
        case CmpInst::Predicate::FCMP_ORD:
        case CmpInst::Predicate::FCMP_UEQ:
        case CmpInst::Predicate::FCMP_UNO:
          PredicateExtraCost = 2;
          break;
        default:
          break;
        }
      }

      // Float is handled with 2*vmr[lh]f + 2*vldeb + vfchdb for each pair of
      // floats.  FIXME: <2 x float> generates same code as <4 x float>.
      unsigned CmpCostPerVector = (ValTy->getScalarType()->isFloatTy() ? 10 : 1);
      unsigned NumVecs_cmp = getNumVectorRegs(ValTy);

      unsigned Cost = (NumVecs_cmp * (CmpCostPerVector + PredicateExtraCost));
      return Cost;
    }
    else { // Called with a select instruction.
      assert (Opcode == Instruction::Select);

      // We can figure out the extra cost of packing / unpacking if the
      // instruction was passed and the compare instruction is found.
      unsigned PackCost = 0;
      Type *CmpOpTy = ((I != nullptr) ? getCmpOpsType(I, VF) : nullptr);
      if (CmpOpTy != nullptr)
        PackCost =
          getVectorBitmaskConversionCost(CmpOpTy, ValTy);

      return getNumVectorRegs(ValTy) /*vsel*/ + PackCost;
    }
  }

  return BaseT::getCmpSelInstrCost(Opcode, ValTy, CondTy, VecPred, CostKind,
                                   Op1Info, Op2Info);
}

InstructionCost SystemZTTIImpl::getVectorInstrCost(unsigned Opcode, Type *Val,
                                                   TTI::TargetCostKind CostKind,
                                                   unsigned Index,
                                                   const Value *Op0,
                                                   const Value *Op1) const {
  if (Opcode == Instruction::InsertElement) {
    // Vector Element Load.
    if (Op1 != nullptr && isFreeEltLoad(Op1))
      return 0;

    // vlvgp will insert two grs into a vector register, so count half the
    // number of instructions as an estimate when we don't have the full
    // picture (as in getScalarizationOverhead()).
    if (Val->isIntOrIntVectorTy(64))
      return ((Index % 2 == 0) ? 1 : 0);
  }

  if (Opcode == Instruction::ExtractElement) {
    int Cost = ((getScalarSizeInBits(Val) == 1) ? 2 /*+test-under-mask*/ : 1);

    // Give a slight penalty for moving out of vector pipeline to FXU unit.
    if (Index == 0 && Val->isIntOrIntVectorTy())
      Cost += 1;

    return Cost;
  }

  return BaseT::getVectorInstrCost(Opcode, Val, CostKind, Index, Op0, Op1);
}

// Check if a load may be folded as a memory operand in its user.
bool SystemZTTIImpl::isFoldableLoad(const LoadInst *Ld,
                                    const Instruction *&FoldedValue) const {
  if (!Ld->hasOneUse())
    return false;
  FoldedValue = Ld;
  const Instruction *UserI = cast<Instruction>(*Ld->user_begin());
  unsigned LoadedBits = getScalarSizeInBits(Ld->getType());
  unsigned TruncBits = 0;
  unsigned SExtBits = 0;
  unsigned ZExtBits = 0;
  if (UserI->hasOneUse()) {
    unsigned UserBits = UserI->getType()->getScalarSizeInBits();
    if (isa<TruncInst>(UserI))
      TruncBits = UserBits;
    else if (isa<SExtInst>(UserI))
      SExtBits = UserBits;
    else if (isa<ZExtInst>(UserI))
      ZExtBits = UserBits;
  }
  if (TruncBits || SExtBits || ZExtBits) {
    FoldedValue = UserI;
    UserI = cast<Instruction>(*UserI->user_begin());
    // Load (single use) -> trunc/extend (single use) -> UserI
  }
  if ((UserI->getOpcode() == Instruction::Sub ||
       UserI->getOpcode() == Instruction::SDiv ||
       UserI->getOpcode() == Instruction::UDiv) &&
      UserI->getOperand(1) != FoldedValue)
    return false; // Not commutative, only RHS foldable.
  // LoadOrTruncBits holds the number of effectively loaded bits, but 0 if an
  // extension was made of the load.
  unsigned LoadOrTruncBits =
      ((SExtBits || ZExtBits) ? 0 : (TruncBits ? TruncBits : LoadedBits));
  switch (UserI->getOpcode()) {
  case Instruction::Add: // SE: 16->32, 16/32->64, z14:16->64. ZE: 32->64
  case Instruction::Sub:
  case Instruction::ICmp:
    if (LoadedBits == 32 && ZExtBits == 64)
      return true;
    [[fallthrough]];
  case Instruction::Mul: // SE: 16->32, 32->64, z14:16->64
    if (UserI->getOpcode() != Instruction::ICmp) {
      if (LoadedBits == 16 &&
          (SExtBits == 32 ||
           (SExtBits == 64 && ST->hasMiscellaneousExtensions2())))
        return true;
      if (LoadOrTruncBits == 16)
        return true;
    }
    [[fallthrough]];
  case Instruction::SDiv:// SE: 32->64
    if (LoadedBits == 32 && SExtBits == 64)
      return true;
    [[fallthrough]];
  case Instruction::UDiv:
  case Instruction::And:
  case Instruction::Or:
  case Instruction::Xor:
    // This also makes sense for float operations, but disabled for now due
    // to regressions.
    // case Instruction::FCmp:
    // case Instruction::FAdd:
    // case Instruction::FSub:
    // case Instruction::FMul:
    // case Instruction::FDiv:

    // All possible extensions of memory checked above.

    // Comparison between memory and immediate.
    if (UserI->getOpcode() == Instruction::ICmp)
      if (ConstantInt *CI = dyn_cast<ConstantInt>(UserI->getOperand(1)))
        if (CI->getValue().isIntN(16))
          return true;
    return (LoadOrTruncBits == 32 || LoadOrTruncBits == 64);
    break;
  }
  return false;
}

static bool isBswapIntrinsicCall(const Value *V) {
  if (const Instruction *I = dyn_cast<Instruction>(V))
    if (auto *CI = dyn_cast<CallInst>(I))
      if (auto *F = CI->getCalledFunction())
        if (F->getIntrinsicID() == Intrinsic::bswap)
          return true;
  return false;
}

InstructionCost SystemZTTIImpl::getMemoryOpCost(unsigned Opcode, Type *Src,
                                                Align Alignment,
                                                unsigned AddressSpace,
                                                TTI::TargetCostKind CostKind,
                                                TTI::OperandValueInfo OpInfo,
                                                const Instruction *I) const {
  assert(!Src->isVoidTy() && "Invalid type");

  // TODO: Handle other cost kinds.
  if (CostKind != TTI::TCK_RecipThroughput)
    return 1;

  if (!Src->isVectorTy() && Opcode == Instruction::Load && I != nullptr) {
    // Store the load or its truncated or extended value in FoldedValue.
    const Instruction *FoldedValue = nullptr;
    if (isFoldableLoad(cast<LoadInst>(I), FoldedValue)) {
      const Instruction *UserI = cast<Instruction>(*FoldedValue->user_begin());
      assert (UserI->getNumOperands() == 2 && "Expected a binop.");

      // UserI can't fold two loads, so in that case return 0 cost only
      // half of the time.
      for (unsigned i = 0; i < 2; ++i) {
        if (UserI->getOperand(i) == FoldedValue)
          continue;

        if (Instruction *OtherOp = dyn_cast<Instruction>(UserI->getOperand(i))){
          LoadInst *OtherLoad = dyn_cast<LoadInst>(OtherOp);
          if (!OtherLoad &&
              (isa<TruncInst>(OtherOp) || isa<SExtInst>(OtherOp) ||
               isa<ZExtInst>(OtherOp)))
            OtherLoad = dyn_cast<LoadInst>(OtherOp->getOperand(0));
          if (OtherLoad && isFoldableLoad(OtherLoad, FoldedValue/*dummy*/))
            return i == 0; // Both operands foldable.
        }
      }

      return 0; // Only I is foldable in user.
    }
  }

  // Type legalization (via getNumberOfParts) can't handle structs
  if (TLI->getValueType(DL, Src, true) == MVT::Other)
    return BaseT::getMemoryOpCost(Opcode, Src, Alignment, AddressSpace,
                                  CostKind);

  // FP128 is a legal type but kept in a register pair on older CPUs.
  if (Src->isFP128Ty() && !ST->hasVectorEnhancements1())
    return 2;

  unsigned NumOps =
    (Src->isVectorTy() ? getNumVectorRegs(Src) : getNumberOfParts(Src));

  // Store/Load reversed saves one instruction.
  if (((!Src->isVectorTy() && NumOps == 1) || ST->hasVectorEnhancements2()) &&
      I != nullptr) {
    if (Opcode == Instruction::Load && I->hasOneUse()) {
      const Instruction *LdUser = cast<Instruction>(*I->user_begin());
      // In case of load -> bswap -> store, return normal cost for the load.
      if (isBswapIntrinsicCall(LdUser) &&
          (!LdUser->hasOneUse() || !isa<StoreInst>(*LdUser->user_begin())))
        return 0;
    }
    else if (const StoreInst *SI = dyn_cast<StoreInst>(I)) {
      const Value *StoredVal = SI->getValueOperand();
      if (StoredVal->hasOneUse() && isBswapIntrinsicCall(StoredVal))
        return 0;
    }
  }

  return  NumOps;
}

// The generic implementation of getInterleavedMemoryOpCost() is based on
// adding costs of the memory operations plus all the extracts and inserts
// needed for using / defining the vector operands. The SystemZ version does
// roughly the same but bases the computations on vector permutations
// instead.
InstructionCost SystemZTTIImpl::getInterleavedMemoryOpCost(
    unsigned Opcode, Type *VecTy, unsigned Factor, ArrayRef<unsigned> Indices,
    Align Alignment, unsigned AddressSpace, TTI::TargetCostKind CostKind,
    bool UseMaskForCond, bool UseMaskForGaps) const {
  if (UseMaskForCond || UseMaskForGaps)
    return BaseT::getInterleavedMemoryOpCost(Opcode, VecTy, Factor, Indices,
                                             Alignment, AddressSpace, CostKind,
                                             UseMaskForCond, UseMaskForGaps);
  assert(isa<VectorType>(VecTy) &&
         "Expect a vector type for interleaved memory op");

  unsigned NumElts = cast<FixedVectorType>(VecTy)->getNumElements();
  assert(Factor > 1 && NumElts % Factor == 0 && "Invalid interleave factor");
  unsigned VF = NumElts / Factor;
  unsigned NumEltsPerVecReg = (128U / getScalarSizeInBits(VecTy));
  unsigned NumVectorMemOps = getNumVectorRegs(VecTy);
  unsigned NumPermutes = 0;

  if (Opcode == Instruction::Load) {
    // Loading interleave groups may have gaps, which may mean fewer
    // loads. Find out how many vectors will be loaded in total, and in how
    // many of them each value will be in.
    BitVector UsedInsts(NumVectorMemOps, false);
    std::vector<BitVector> ValueVecs(Factor, BitVector(NumVectorMemOps, false));
    for (unsigned Index : Indices)
      for (unsigned Elt = 0; Elt < VF; ++Elt) {
        unsigned Vec = (Index + Elt * Factor) / NumEltsPerVecReg;
        UsedInsts.set(Vec);
        ValueVecs[Index].set(Vec);
      }
    NumVectorMemOps = UsedInsts.count();

    for (unsigned Index : Indices) {
      // Estimate that each loaded source vector containing this Index
      // requires one operation, except that vperm can handle two input
      // registers first time for each dst vector.
      unsigned NumSrcVecs = ValueVecs[Index].count();
      unsigned NumDstVecs = divideCeil(VF * getScalarSizeInBits(VecTy), 128U);
      assert (NumSrcVecs >= NumDstVecs && "Expected at least as many sources");
      NumPermutes += std::max(1U, NumSrcVecs - NumDstVecs);
    }
  } else {
    // Estimate the permutes for each stored vector as the smaller of the
    // number of elements and the number of source vectors. Subtract one per
    // dst vector for vperm (S.A.).
    unsigned NumSrcVecs = std::min(NumEltsPerVecReg, Factor);
    unsigned NumDstVecs = NumVectorMemOps;
    NumPermutes += (NumDstVecs * NumSrcVecs) - NumDstVecs;
  }

  // Cost of load/store operations and the permutations needed.
  return NumVectorMemOps + NumPermutes;
}

InstructionCost getIntAddReductionCost(unsigned NumVec, unsigned ScalarBits) {
  InstructionCost Cost = 0;
  // Binary Tree of N/2 + N/4 + ... operations yields N - 1 operations total.
  Cost += NumVec - 1;
  // For integer adds, VSUM creates shorter reductions on the final vector.
  Cost += (ScalarBits < 32) ? 3 : 2;
  return Cost;
}

InstructionCost getFastReductionCost(unsigned NumVec, unsigned NumElems,
                                     unsigned ScalarBits) {
  unsigned NumEltsPerVecReg = (SystemZ::VectorBits / ScalarBits);
  InstructionCost Cost = 0;
  // Binary Tree of N/2 + N/4 + ... operations yields N - 1 operations total.
  Cost += NumVec - 1;
  // For each shuffle / arithmetic layer, we need 2 instructions, and we need
  // log2(Elements in Last Vector) layers.
  Cost += 2 * Log2_32_Ceil(std::min(NumElems, NumEltsPerVecReg));
  return Cost;
}

inline bool customCostReductions(unsigned Opcode) {
  return Opcode == Instruction::FAdd || Opcode == Instruction::FMul ||
         Opcode == Instruction::Add || Opcode == Instruction::Mul;
}

InstructionCost
SystemZTTIImpl::getArithmeticReductionCost(unsigned Opcode, VectorType *Ty,
                                           std::optional<FastMathFlags> FMF,
                                           TTI::TargetCostKind CostKind) const {
  unsigned ScalarBits = Ty->getScalarSizeInBits();
  // The following is only for subtargets with vector math, non-ordered
  // reductions, and reasonable scalar sizes for int and fp add/mul.
  if (customCostReductions(Opcode) && ST->hasVector() &&
      !TTI::requiresOrderedReduction(FMF) &&
      ScalarBits <= SystemZ::VectorBits) {
    unsigned NumVectors = getNumVectorRegs(Ty);
    unsigned NumElems = ((FixedVectorType *)Ty)->getNumElements();
    // Integer Add is using custom code gen, that needs to be accounted for.
    if (Opcode == Instruction::Add)
      return getIntAddReductionCost(NumVectors, ScalarBits);
    // The base cost is the same across all other arithmetic instructions
    InstructionCost Cost =
        getFastReductionCost(NumVectors, NumElems, ScalarBits);
    // But we need to account for the final op involving the scalar operand.
    if ((Opcode == Instruction::FAdd) || (Opcode == Instruction::FMul))
      Cost += 1;
    return Cost;
  }
  // otherwise, fall back to the standard implementation
  return BaseT::getArithmeticReductionCost(Opcode, Ty, FMF, CostKind);
}

InstructionCost
SystemZTTIImpl::getMinMaxReductionCost(Intrinsic::ID IID, VectorType *Ty,
                                       FastMathFlags FMF,
                                       TTI::TargetCostKind CostKind) const {
  // Return custom costs only on subtargets with vector enhancements.
  if (ST->hasVectorEnhancements1()) {
    unsigned NumVectors = getNumVectorRegs(Ty);
    unsigned NumElems = ((FixedVectorType *)Ty)->getNumElements();
    unsigned ScalarBits = Ty->getScalarSizeInBits();
    InstructionCost Cost = 0;
    // Binary Tree of N/2 + N/4 + ... operations yields N - 1 operations total.
    Cost += NumVectors - 1;
    // For the final vector, we need shuffle + min/max operations, and
    // we need #Elements - 1 of them.
    Cost += 2 * (std::min(NumElems, SystemZ::VectorBits / ScalarBits) - 1);
    return Cost;
  }
  // For other targets, fall back to the standard implementation
  return BaseT::getMinMaxReductionCost(IID, Ty, FMF, CostKind);
}

static int
getVectorIntrinsicInstrCost(Intrinsic::ID ID, Type *RetTy,
                            const SmallVectorImpl<Type *> &ParamTys) {
  if (RetTy->isVectorTy() && ID == Intrinsic::bswap)
    return getNumVectorRegs(RetTy); // VPERM

  return -1;
}

InstructionCost
SystemZTTIImpl::getIntrinsicInstrCost(const IntrinsicCostAttributes &ICA,
                                      TTI::TargetCostKind CostKind) const {
  InstructionCost Cost = getVectorIntrinsicInstrCost(
      ICA.getID(), ICA.getReturnType(), ICA.getArgTypes());
  if (Cost != -1)
    return Cost;
  return BaseT::getIntrinsicInstrCost(ICA, CostKind);
}

bool SystemZTTIImpl::shouldExpandReduction(const IntrinsicInst *II) const {
  // Always expand on Subtargets without vector instructions.
  if (!ST->hasVector())
    return true;

  // Whether or not to expand is a per-intrinsic decision.
  switch (II->getIntrinsicID()) {
  default:
    return true;
  // Do not expand vector.reduce.add...
  case Intrinsic::vector_reduce_add:
    auto *VType = cast<FixedVectorType>(II->getOperand(0)->getType());
    // ...unless the scalar size is i64 or larger,
    // or the operand vector is not full, since the
    // performance benefit is dubious in those cases.
    return VType->getScalarSizeInBits() >= 64 ||
           VType->getPrimitiveSizeInBits() < SystemZ::VectorBits;
  }
}
