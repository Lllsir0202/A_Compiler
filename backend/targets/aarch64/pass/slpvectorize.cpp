#include "backend/targets/aarch64/pass/slpvectorize.h"
#include <cstdlib>
#include <string>
#include <unordered_map>
#include <vector>
#include "backend/targets/aarch64/inst/aarch64inst.h"
#include "backend/targets/utils/context/targetcontext.h"
#include "backend/targets/utils/operand/targetliteral.h"
#include "backend/targets/utils/operand/targetreg.h"
#include "backend/targets/utils/operand/targetsymbol.h"
#include "backend/targets/utils/struct/targetblock.h"
#include "backend/targets/utils/struct/targetfunction.h"

namespace backend::targetir::aarch64 {

namespace {
auto Immediate(TargetOperand *operand, int *value) -> bool {
  if (operand == nullptr || operand->GetOpType() != TargetOperandType::Immediate) {
    return false;
  }
  *value = static_cast<TargetIntLiteral *>(operand)->GetValue();
  return true;
}

auto IsScalarI32(TargetOperand *operand) -> bool {
  if (operand == nullptr || (!operand->IsVirtualReg() && !operand->IsPhysicalReg())) {
    return false;
  }
  if (operand->IsVirtualReg()) {
    auto *reg = static_cast<TargetVirtualReg *>(operand);
    return reg->GetRegClass() == TargetRegClass::General && reg->GetRegSize() == TargetRegSize::DWord;
  }
  auto *reg = static_cast<TargetPhysicalReg *>(operand);
  return reg->GetRegClass() == TargetRegClass::General && reg->GetRegSize() == TargetRegSize::DWord;
}

auto IsOpcode(TargetInst *inst, A64Opcode opcode, size_t operands) -> bool {
  auto *a64 = static_cast<AArch64Inst *>(inst);
  return a64->GetOpcode() == opcode && inst->GetOperands().size() == operands;
}

auto EitherOrder(TargetInst *inst, TargetOperand *lhs, TargetOperand *rhs) -> bool {
  const auto &ops = inst->GetOperands();
  return (ops[0] == lhs && ops[1] == rhs) || (ops[0] == rhs && ops[1] == lhs);
}
}  // namespace

auto AArch64SLPVectorize::RunOnFunction(TargetFunction *func) -> bool {
  if (std::getenv("AARCH64_SLP_VECTORIZE_OFF") != nullptr) {
    return false;
  }
  bool changed = false;
  for (auto *block : func->GetBlocks()) {
    bool block_changed = false;
    const auto &insts = block->GetInsts();
    std::unordered_map<TargetOperand *, std::string> global_base;
    for (auto *inst : insts) {
      if (!IsOpcode(inst, A64Opcode::ADD_LO12, 2) || inst->GetResult() == nullptr ||
          inst->GetOperands()[1]->GetOpType() != TargetOperandType::Symbol) {
        continue;
      }
      global_base[inst->GetResult()] = static_cast<TargetSymbol *>(inst->GetOperands()[1])->GetName();
    }
    std::vector<TargetInst *> rewritten;
    rewritten.reserve(insts.size());
    for (size_t i = 0; i < insts.size();) {
      // Four independent lanes of
      //   dst[i] = dst[i] * scalar + src[i]
      // become two q loads, one dup, one MLA, and one q store. Distinct global
      // provenance is required so hoisting all loads before the store preserves
      // the scalar lane order even in the presence of possible pointer aliasing.
      if (i + 20 <= insts.size()) {
        TargetOperand *dst_base = nullptr;
        TargetOperand *src_base = nullptr;
        TargetOperand *scalar = nullptr;
        int first_offset = 0;
        bool match = true;
        for (size_t lane = 0; lane < 4; ++lane) {
          size_t p = i + lane * 5;
          auto *load_dst = insts[p];
          auto *mul = insts[p + 1];
          auto *load_src = insts[p + 2];
          auto *add = insts[p + 3];
          auto *store = insts[p + 4];
          if (!IsOpcode(load_dst, A64Opcode::LDR_OFF, 2) || !IsOpcode(mul, A64Opcode::MUL, 2) ||
              !IsOpcode(load_src, A64Opcode::LDR_OFF, 2) || !IsOpcode(add, A64Opcode::ADD, 2) ||
              !IsOpcode(store, A64Opcode::STR_OFF, 3)) {
            match = false;
            break;
          }
          const auto &dst_ops = load_dst->GetOperands();
          const auto &src_ops = load_src->GetOperands();
          const auto &store_ops = store->GetOperands();
          int dst_offset = 0;
          int src_offset = 0;
          int store_offset = 0;
          if (!Immediate(dst_ops[1], &dst_offset) || !Immediate(src_ops[1], &src_offset) ||
              !Immediate(store_ops[2], &store_offset) || dst_offset != src_offset || dst_offset != store_offset ||
              store_ops[0] != add->GetResult() || store_ops[1] != dst_ops[0] ||
              !EitherOrder(add, mul->GetResult(), load_src->GetResult())) {
            match = false;
            break;
          }
          TargetOperand *lane_scalar = nullptr;
          if (mul->GetOperands()[0] == load_dst->GetResult()) {
            lane_scalar = mul->GetOperands()[1];
          } else if (mul->GetOperands()[1] == load_dst->GetResult()) {
            lane_scalar = mul->GetOperands()[0];
          } else {
            match = false;
            break;
          }
          if (lane == 0) {
            dst_base = dst_ops[0];
            src_base = src_ops[0];
            scalar = lane_scalar;
            first_offset = dst_offset;
          } else if (dst_ops[0] != dst_base || src_ops[0] != src_base || lane_scalar != scalar ||
                     dst_offset != first_offset + static_cast<int>(lane) * 4) {
            match = false;
            break;
          }
        }
        auto dst_global = global_base.find(dst_base);
        auto src_global = global_base.find(src_base);
        bool no_alias = dst_global != global_base.end() && src_global != global_base.end() &&
                        dst_global->second != src_global->second;
        if (match && no_alias && IsScalarI32(scalar) && first_offset >= 0 && first_offset % 16 == 0 &&
            first_offset <= 4095 * 16) {
          auto *dst = ctx_->NewVirtualReg(TargetRegClass::Vector, TargetRegSize::OWord);
          auto *src = ctx_->NewVirtualReg(TargetRegClass::Vector, TargetRegSize::OWord);
          auto *factor = ctx_->NewVirtualReg(TargetRegClass::Vector, TargetRegSize::OWord);
          auto make_load = [&](TargetOperand *result, TargetOperand *base) {
            auto *load = ctx_->CreateInst<AArch64Inst>(A64Opcode::LDR_OFF);
            load->SetResult(result);
            load->AddOperand(base);
            load->AddOperand(ctx_->Imm(first_offset));
            rewritten.push_back(load);
          };
          make_load(dst, dst_base);
          make_load(src, src_base);
          auto *dup = ctx_->CreateInst<AArch64Inst>(A64Opcode::V_DUP);
          dup->SetResult(factor);
          dup->AddOperand(scalar);
          rewritten.push_back(dup);
          auto *mla = ctx_->CreateInst<AArch64Inst>(A64Opcode::V_MLA);
          mla->SetResult(src);
          mla->AddOperand(src);
          mla->AddOperand(dst);
          mla->AddOperand(factor);
          rewritten.push_back(mla);
          auto *store = ctx_->CreateInst<AArch64Inst>(A64Opcode::STR_OFF);
          store->AddOperand(src);
          store->AddOperand(dst_base);
          store->AddOperand(ctx_->Imm(first_offset));
          rewritten.push_back(store);
          i += 20;
          changed = true;
          block_changed = true;
          continue;
        }
      }
      if (i + 4 <= insts.size()) {
        TargetOperand *value = nullptr;
        TargetOperand *base = nullptr;
        int first_offset = 0;
        bool match = true;
        for (size_t lane = 0; lane < 4; ++lane) {
          auto *store = static_cast<AArch64Inst *>(insts[i + lane]);
          const auto &ops = store->GetOperands();
          int offset = 0;
          if (store->GetOpcode() != A64Opcode::STR_OFF || ops.size() != 3 || !Immediate(ops[2], &offset) ||
              !IsScalarI32(ops[0])) {
            match = false;
            break;
          }
          if (lane == 0) {
            value = ops[0];
            base = ops[1];
            first_offset = offset;
          } else if (ops[0] != value || ops[1] != base || offset != first_offset + static_cast<int>(lane) * 4) {
            match = false;
            break;
          }
        }
        // `ldr/str q, [base, #imm]` uses a scaled unsigned immediate. Requiring
        // 16-byte alignment keeps the replacement directly encodable.
        if (match && first_offset >= 0 && first_offset % 16 == 0 && first_offset <= 4095 * 16) {
          auto *vector = ctx_->NewVirtualReg(TargetRegClass::Vector, TargetRegSize::OWord);
          auto *dup = ctx_->CreateInst<AArch64Inst>(A64Opcode::V_DUP);
          dup->SetResult(vector);
          dup->AddOperand(value);
          rewritten.push_back(dup);

          auto *store = ctx_->CreateInst<AArch64Inst>(A64Opcode::STR_OFF);
          store->AddOperand(vector);
          store->AddOperand(base);
          store->AddOperand(ctx_->Imm(first_offset));
          rewritten.push_back(store);
          i += 4;
          changed = true;
          block_changed = true;
          continue;
        }
      }
      rewritten.push_back(insts[i++]);
    }
    if (block_changed) {
      block->SetInsts(std::move(rewritten));
    }
  }
  return changed;
}

}  // namespace backend::targetir::aarch64
