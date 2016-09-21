//===- Z80ISelDAGToDAG.cpp - A DAG pattern matching inst selector for Z80 -===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file defines a DAG pattern matching instruction selector for Z80,
// converting from a legalized dag to a Z80 dag.
//
//===----------------------------------------------------------------------===//

#include "Z80.h"
#include "Z80Subtarget.h"
#include "Z80TargetMachine.h"
#include "llvm/CodeGen/SelectionDAGISel.h"
using namespace llvm;

#define DEBUG_TYPE "z80-isel"

namespace {
  //===--------------------------------------------------------------------===//
  /// ISel - Z80-specific code to select Z80 machine instructions for
  /// SelectionDAG operations.
  ///
  class Z80DAGToDAGISel final : public SelectionDAGISel {
    /// Keep a pointer to the Z80Subtarget around so that we can
    /// make the right decision when generating code for different targets.
    const Z80Subtarget *Subtarget;

    /// If true, selector should try to optimize for code size instead of
    /// performance.
    bool OptForSize;

  public:
    explicit Z80DAGToDAGISel(Z80TargetMachine &TM, CodeGenOpt::Level OptLevel)
      : SelectionDAGISel(TM, OptLevel), OptForSize(false) {}

    const char *getPassName() const override {
      return "Z80 DAG->DAG Instruction Selection";
    }

    bool runOnMachineFunction(MachineFunction &MF) override {
      // Reset the subtarget each time through.
      Subtarget = &MF.getSubtarget<Z80Subtarget>();
      return SelectionDAGISel::runOnMachineFunction(MF);
    }

// Include the pieces autogenerated from the target description.
#include "Z80GenDAGISel.inc"

  private:
    void Select(SDNode *N) override;

    bool SelectMem(SDValue N, SDValue &Mem);
    bool SelectOff(SDValue N, SDValue &Reg, SDValue &Off);

    /// Implement addressing mode selection for inline asm expressions.
    bool SelectInlineAsmMemoryOperand(const SDValue &Op, unsigned ConstraintID,
                                      std::vector<SDValue> &OutOps) override;
  };
}

void Z80DAGToDAGISel::Select(SDNode *Node) {
  SDLoc DL(Node);

  // Dump information about the Node being selected
  DEBUG(dbgs() << "Selecting: "; Node->dump(CurDAG); dbgs() << '\n');

  // If we have a custom node, we already have selected!
  if (Node->isMachineOpcode()) {
    DEBUG(dbgs() << "== "; Node->dump(CurDAG); dbgs() << '\n');
    Node->setNodeId(-1);
    return;
  }

  // Select the default instruction
  SDNode *ResNode = SelectCode(Node);

  DEBUG(dbgs() << "=> ";
        if (ResNode == nullptr || ResNode == Node)
          Node->dump(CurDAG);
        else
          ResNode->dump(CurDAG);
        dbgs() << '\n');
}

bool Z80DAGToDAGISel::SelectMem(SDValue N, SDValue &Mem) {
  switch (N.getOpcode()) {
  default:
    DEBUG(dbgs() << "SelectMem: " << N->getOperationName() << '\n');
    return false;
  case ISD::Constant: {
    uint64_t Val = cast<ConstantSDNode>(N)->getSExtValue();
    Mem = CurDAG->getTargetConstant(Val, SDLoc(N), MVT::i24);
    return true;
  }
  case ISD::GlobalAddress: {
    GlobalAddressSDNode *G = cast<GlobalAddressSDNode>(N);
    Mem = CurDAG->getTargetGlobalAddress(
        G->getGlobal(), SDLoc(N), TLI->getPointerTy(CurDAG->getDataLayout()),
        G->getOffset());
    return true;
  }
  case ISD::ExternalSymbol: {
    ExternalSymbolSDNode *S = cast<ExternalSymbolSDNode>(N);
    Mem = CurDAG->getTargetExternalSymbol(
        S->getSymbol(), TLI->getPointerTy(CurDAG->getDataLayout()));
    return true;
  }
  }
}
bool Z80DAGToDAGISel::SelectOff(SDValue N, SDValue &Reg, SDValue &Off) {
  switch (N.getOpcode()) {
  default: return false;
  case ISD::ADD:
    for (int I = 0; I != 2; ++I) {
      if (SelectMem(N.getOperand(I ^ 1), Off)) {
        Reg = N.getOperand(I);
        DEBUG(dbgs() << "Selected ADD:\n";
              N.dumpr();
              dbgs() << "becomes\n";
              Reg.dumpr();
              Off.dumpr());
        return true;
      }
    }
    return false;
  case ISD::FrameIndex:
    Reg = CurDAG->getTargetFrameIndex(
        cast<FrameIndexSDNode>(N)->getIndex(),
        TLI->getPointerTy(CurDAG->getDataLayout()));
    Off = CurDAG->getTargetConstant(0, SDLoc(N), MVT::i8);
    return true;
  }
}

bool Z80DAGToDAGISel::
SelectInlineAsmMemoryOperand(const SDValue &Op, unsigned ConstraintID,
                             std::vector<SDValue> &OutOps) {
  llvm_unreachable("Unimplemented");
}

/// This pass converts a legalized DAG into Z80-specific DAG,
/// ready for instruction scheduling.
FunctionPass *llvm::createZ80ISelDag(Z80TargetMachine &TM,
                                     CodeGenOpt::Level OptLevel) {
  return new Z80DAGToDAGISel(TM, OptLevel);
}
