#include "backend/targets/aarch64/pass/slpvectorize.h"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <string>
#include <unordered_map>
#include <vector>
#include "backend/targets/aarch64/inst/aarch64inst.h"
#include "backend/targets/aarch64/regs/aarch64reg.h"
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

auto IsScalarF32(TargetOperand *operand) -> bool {
  if (operand == nullptr || (!operand->IsVirtualReg() && !operand->IsPhysicalReg())) {
    return false;
  }
  if (operand->IsVirtualReg()) {
    auto *reg = static_cast<TargetVirtualReg *>(operand);
    return reg->GetRegClass() == TargetRegClass::Float && reg->GetRegSize() == TargetRegSize::DWord;
  }
  auto *reg = static_cast<TargetPhysicalReg *>(operand);
  return reg->GetRegClass() == TargetRegClass::Float && reg->GetRegSize() == TargetRegSize::DWord;
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
  size_t float_windows = 0;
  size_t float_structural_matches = 0;
  size_t float_safe_matches = 0;
  size_t float_affine_matches = 0;
  size_t copy_structural_matches = 0;
  size_t copy_safe_matches = 0;
  // Global bases are materialized in the entry block and then consumed by
  // arbitrary later blocks.  Collect their provenance once per function so a
  // vector pack can prove that moving its four loads before its four stores
  // cannot change the scalar memory order.
  std::unordered_map<TargetOperand *, std::string> global_base;
  std::unordered_map<TargetOperand *, std::string> direct_global_base;
  std::unordered_map<TargetOperand *, TargetInst *> definitions;
  std::unordered_map<TargetOperand *, std::vector<std::pair<TargetInst *, TargetBlock *>>> all_definitions;
  for (auto *block : func->GetBlocks()) {
    for (auto *inst : block->GetInsts()) {
      if (inst->GetResult() != nullptr) {
        all_definitions[inst->GetResult()].emplace_back(inst, block);
        auto [definition, inserted] = definitions.emplace(inst->GetResult(), inst);
        if (!inserted) definition->second = nullptr;
        if (IsOpcode(inst, A64Opcode::MOV, 1) && inst->GetResult()->IsVirtualReg() &&
            inst->GetOperands()[0]->IsPhysicalReg()) {
          auto *result = static_cast<TargetVirtualReg *>(inst->GetResult());
          auto *incoming = static_cast<TargetPhysicalReg *>(inst->GetOperands()[0]);
          if (result->GetRegClass() == TargetRegClass::General && result->GetRegSize() == TargetRegSize::QWord &&
              incoming->GetRegClass() == TargetRegClass::General &&
              incoming->GetID() <= static_cast<uint8_t>(GPR::X7)) {
            // SysY pointer parameters arrive in x0-x7.  All parameter pointers
            // share one may-alias class, but are disjoint from globals and the
            // current function's stack frame.
            global_base[inst->GetResult()] = "\1param";
          }
        }
      }
      if (!IsOpcode(inst, A64Opcode::ADD_LO12, 2) || inst->GetResult() == nullptr ||
          inst->GetOperands()[1]->GetOpType() != TargetOperandType::Symbol) {
        continue;
      }
      auto symbol = static_cast<TargetSymbol *>(inst->GetOperands()[1])->GetName();
      global_base[inst->GetResult()] = symbol;
      direct_global_base[inst->GetResult()] = symbol;
    }
  }
  struct PointerRecurrence {
    TargetOperand *initial;
    int64_t step;
    TargetBlock *latch;
  };
  std::unordered_map<TargetOperand *, PointerRecurrence> pointer_recurrences;
  for (const auto &[value, defs] : all_definitions) {
    if (defs.size() != 2) continue;
    TargetOperand *initial = nullptr;
    int64_t step = 0;
    TargetBlock *latch = nullptr;
    bool invalid = false;
    for (const auto &[definition, block] : defs) {
      if (!IsOpcode(definition, A64Opcode::MOV, 1)) {
        invalid = true;
        break;
      }
      auto *incoming = definition->GetOperands()[0];
      auto update = definitions.find(incoming);
      int immediate = 0;
      if (update != definitions.end() && update->second != nullptr && IsOpcode(update->second, A64Opcode::ADD_I, 2) &&
          update->second->GetOperands()[0] == value && Immediate(update->second->GetOperands()[1], &immediate)) {
        if (latch != nullptr) {
          invalid = true;
          break;
        }
        step = immediate;
        latch = block;
      } else {
        if (initial != nullptr) {
          invalid = true;
          break;
        }
        initial = incoming;
      }
    }
    if (!invalid && initial != nullptr && latch != nullptr && step != 0) {
      pointer_recurrences[value] = {initial, step, latch};
    }
  }
  auto designated_base = [](TargetInst *inst) -> TargetOperand * {
    auto opcode = static_cast<AArch64Inst *>(inst)->GetOpcode();
    const auto &operands = inst->GetOperands();
    if ((opcode == A64Opcode::MOV || opcode == A64Opcode::ADD_I || opcode == A64Opcode::ADD_I_LSL12 ||
         opcode == A64Opcode::SUB_I || opcode == A64Opcode::SUB_I_LSL12 || opcode == A64Opcode::ADD_SHIFT_LSL ||
         opcode == A64Opcode::ADD_SHIFT_LSR || opcode == A64Opcode::ADD_SHIFT_ASR ||
         opcode == A64Opcode::ADD_EXT_SXTW || opcode == A64Opcode::SUB_EXT_SXTW) &&
        !operands.empty()) {
      return operands[0];
    }
    if (opcode == A64Opcode::MADD && operands.size() == 3) return operands[2];
    return nullptr;
  };
  auto provenance_source = [&](TargetOperand *value, TargetOperand *cycle, auto &&source_ref,
                               int depth) -> TargetOperand * {
    if (value == nullptr || value == cycle || depth > 24 || direct_global_base.count(value) != 0 ||
        value->IsPhysicalReg()) {
      return value;
    }
    auto definition = definitions.find(value);
    if (definition == definitions.end() || definition->second == nullptr) return value;
    auto *base = designated_base(definition->second);
    return base == nullptr ? value : source_ref(base, cycle, source_ref, depth + 1);
  };
  // Folded addresses and pointer phis no longer use the original object base
  // directly.  Propagate provenance only when every non-self incoming edge
  // resolves to the same object.  This handles pointer-IV backedges while an
  // unresolved or conflicting incoming edge keeps the result unknown.
  bool provenance_changed = true;
  while (provenance_changed) {
    provenance_changed = false;
    for (const auto &[result, defs] : all_definitions) {
      if (global_base.count(result) != 0) continue;
      std::string object;
      bool saw_object = false;
      bool conflict = false;
      for (const auto &[definition, block] : defs) {
        static_cast<void>(block);
        auto *base = designated_base(definition);
        if (base == nullptr) {
          conflict = true;
          break;
        }
        auto *source = provenance_source(base, result, provenance_source, 0);
        if (source == result) continue;
        std::string incoming;
        auto known = global_base.find(source);
        if (known != global_base.end()) {
          incoming = known->second;
        } else if (source != nullptr && source->IsPhysicalReg()) {
          auto *reg = static_cast<TargetPhysicalReg *>(source);
          if (reg->GetRegClass() == TargetRegClass::General && reg->GetID() == static_cast<uint8_t>(GPR::Sp)) {
            incoming = "\1stack";
          }
        }
        if (incoming.empty() || (saw_object && incoming != object)) {
          conflict = true;
          break;
        }
        object = incoming;
        saw_object = true;
      }
      if (!conflict && saw_object) {
        global_base[result] = object;
        provenance_changed = true;
      }
    }
  }
  struct AffineAddress {
    bool valid{true};
    std::string symbol;
    std::unordered_map<const void *, int64_t> terms;
    int64_t constant{0};
  };
  auto combine = [](AffineAddress lhs, const AffineAddress &rhs, int64_t scale) {
    if (!lhs.valid || !rhs.valid || (!lhs.symbol.empty() && !rhs.symbol.empty())) {
      lhs.valid = false;
      return lhs;
    }
    if (lhs.symbol.empty()) lhs.symbol = rhs.symbol;
    __int128 constant = static_cast<__int128>(lhs.constant) + static_cast<__int128>(rhs.constant) * scale;
    if (constant < std::numeric_limits<int64_t>::min() || constant > std::numeric_limits<int64_t>::max()) {
      lhs.valid = false;
      return lhs;
    }
    lhs.constant = static_cast<int64_t>(constant);
    for (const auto &[term, coefficient] : rhs.terms) {
      __int128 value = static_cast<__int128>(lhs.terms[term]) + static_cast<__int128>(coefficient) * scale;
      if (value < std::numeric_limits<int64_t>::min() || value > std::numeric_limits<int64_t>::max()) {
        lhs.valid = false;
        return lhs;
      }
      lhs.terms[term] = static_cast<int64_t>(value);
      if (lhs.terms[term] == 0) lhs.terms.erase(term);
    }
    return lhs;
  };
  auto address_of = [&](TargetOperand *value, auto &&address_of_ref, int depth) -> AffineAddress {
    if (value == nullptr || depth > 24) return {false, {}, {}, 0};
    int immediate = 0;
    if (Immediate(value, &immediate)) return {true, {}, {}, immediate};
    if (value->IsPhysicalReg()) {
      auto *reg = static_cast<TargetPhysicalReg *>(value);
      if (reg->GetRegClass() == TargetRegClass::General && reg->GetID() == static_cast<uint8_t>(GPR::Sp)) {
        return {true, "\1stack", {}, 0};
      }
    }
    auto global = direct_global_base.find(value);
    if (global != direct_global_base.end()) return {true, global->second, {}, 0};
    auto recurrence = pointer_recurrences.find(value);
    if (recurrence != pointer_recurrences.end()) {
      auto expression = address_of_ref(recurrence->second.initial, address_of_ref, depth + 1);
      expression.terms[recurrence->second.latch] += recurrence->second.step;
      return expression;
    }
    auto definition = definitions.find(value);
    if (definition == definitions.end() || definition->second == nullptr) return {true, {}, {{value, 1}}, 0};
    auto *inst = definition->second;
    auto opcode = static_cast<AArch64Inst *>(inst)->GetOpcode();
    const auto &operands = inst->GetOperands();
    if ((opcode == A64Opcode::MOV || opcode == A64Opcode::SXTW) && !operands.empty()) {
      return address_of_ref(operands[0], address_of_ref, depth + 1);
    }
    if ((opcode == A64Opcode::ADD_I || opcode == A64Opcode::ADD_I_LSL12 || opcode == A64Opcode::SUB_I ||
         opcode == A64Opcode::SUB_I_LSL12) &&
        operands.size() >= 2 && Immediate(operands[1], &immediate)) {
      int64_t scale = (opcode == A64Opcode::ADD_I_LSL12 || opcode == A64Opcode::SUB_I_LSL12) ? 4096 : 1;
      if (opcode == A64Opcode::SUB_I || opcode == A64Opcode::SUB_I_LSL12) scale = -scale;
      return combine(address_of_ref(operands[0], address_of_ref, depth + 1), AffineAddress{true, {}, {}, immediate},
                     scale);
    }
    if ((opcode == A64Opcode::ADD_SHIFT_LSL || opcode == A64Opcode::SUB_SHIFT_LSL ||
         opcode == A64Opcode::ADD_EXT_SXTW || opcode == A64Opcode::SUB_EXT_SXTW) &&
        operands.size() >= 3 && Immediate(operands[2], &immediate) && immediate >= 0 && immediate < 63) {
      int64_t scale = int64_t{1} << immediate;
      if (opcode == A64Opcode::SUB_SHIFT_LSL || opcode == A64Opcode::SUB_EXT_SXTW) scale = -scale;
      return combine(address_of_ref(operands[0], address_of_ref, depth + 1),
                     address_of_ref(operands[1], address_of_ref, depth + 1), scale);
    }
    if ((opcode == A64Opcode::ADD || opcode == A64Opcode::SUB) && operands.size() == 2) {
      return combine(address_of_ref(operands[0], address_of_ref, depth + 1),
                     address_of_ref(operands[1], address_of_ref, depth + 1), opcode == A64Opcode::ADD ? 1 : -1);
    }
    if (opcode == A64Opcode::MADD && operands.size() == 3) {
      int lhs_constant = 0;
      int rhs_constant = 0;
      if (Immediate(operands[0], &lhs_constant)) {
        return combine(address_of_ref(operands[2], address_of_ref, depth + 1),
                       address_of_ref(operands[1], address_of_ref, depth + 1), lhs_constant);
      }
      if (Immediate(operands[1], &rhs_constant)) {
        return combine(address_of_ref(operands[2], address_of_ref, depth + 1),
                       address_of_ref(operands[0], address_of_ref, depth + 1), rhs_constant);
      }
    }
    return {true, {}, {{value, 1}}, 0};
  };
  auto proven_distinct_objects = [&](TargetOperand *first, TargetOperand *second) {
    auto lhs = global_base.find(first);
    auto rhs = global_base.find(second);
    if (lhs == global_base.end() || rhs == global_base.end() || lhs->second == rhs->second) return false;
    if (lhs->second == "\1stack" || rhs->second == "\1stack") return true;
    // A formal pointer may designate any global supplied by its caller.  Two
    // different named globals, however, are distinct language objects.
    return lhs->second != "\1param" && rhs->second != "\1param";
  };
  auto safe_stream = [&](TargetOperand *store_base, int store_offset, TargetOperand *load_base, int load_offset) {
    if (store_base == load_base) {
      return store_offset == load_offset;
    }
    if (proven_distinct_objects(store_base, load_base)) return true;
    auto store_address = address_of(store_base, address_of, 0);
    auto load_address = address_of(load_base, address_of, 0);
    if (!store_address.valid || !load_address.valid || store_address.symbol != load_address.symbol ||
        store_address.terms != load_address.terms) {
      if (store_address.valid && load_address.valid &&
          ((store_address.symbol == "\1stack") != (load_address.symbol == "\1stack"))) {
        return true;
      }
      return false;
    }
    __int128 distance =
        static_cast<__int128>(store_address.constant) + store_offset - load_address.constant - load_offset;
    return distance <= -16 || distance >= 16;
  };
  for (auto *block : func->GetBlocks()) {
    bool block_changed = false;
    const auto &insts = block->GetInsts();
    std::vector<TargetInst *> rewritten;
    rewritten.reserve(insts.size());
    for (size_t i = 0; i < insts.size();) {
      // The machine pipeline reaches a fixpoint. Once its late tree combine
      // has formed scalar MADDs, recognize the same SAXPY lanes directly so a
      // later iteration still reaches the vector form.
      if (i + 16 <= insts.size()) {
        TargetOperand *dst_base = nullptr;
        TargetOperand *src_base = nullptr;
        TargetOperand *scalar = nullptr;
        int first_offset = 0;
        bool match = true;
        for (size_t lane = 0; lane < 4; ++lane) {
          size_t p = i + lane * 4;
          auto *load_dst = insts[p];
          auto *load_src = insts[p + 1];
          auto *madd = insts[p + 2];
          auto *store = insts[p + 3];
          if (!IsOpcode(load_dst, A64Opcode::LDR_OFF, 2) || !IsOpcode(load_src, A64Opcode::LDR_OFF, 2) ||
              !IsOpcode(madd, A64Opcode::MADD, 3) || !IsOpcode(store, A64Opcode::STR_OFF, 3)) {
            match = false;
            break;
          }
          const auto &dst_ops = load_dst->GetOperands();
          const auto &src_ops = load_src->GetOperands();
          const auto &madd_ops = madd->GetOperands();
          const auto &store_ops = store->GetOperands();
          int dst_offset = 0;
          int src_offset = 0;
          int store_offset = 0;
          if (!Immediate(dst_ops[1], &dst_offset) || !Immediate(src_ops[1], &src_offset) ||
              !Immediate(store_ops[2], &store_offset) || store_ops[0] != madd->GetResult() ||
              madd_ops[0] != load_dst->GetResult() || madd_ops[2] != load_src->GetResult() ||
              store_ops[1] != dst_ops[0] || dst_offset != src_offset || dst_offset != store_offset) {
            match = false;
            break;
          }
          if (lane == 0) {
            dst_base = dst_ops[0];
            src_base = src_ops[0];
            scalar = madd_ops[1];
            first_offset = dst_offset;
          } else if (dst_ops[0] != dst_base || src_ops[0] != src_base || madd_ops[1] != scalar ||
                     dst_offset != first_offset + static_cast<int>(lane) * 4) {
            match = false;
            break;
          }
        }
        bool no_alias = proven_distinct_objects(dst_base, src_base);
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
          i += 16;
          changed = true;
          block_changed = true;
          continue;
        }
      }
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
        bool no_alias = proven_distinct_objects(dst_base, src_base);
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
      // Pack four lanes of
      //   dst[i] = dst[i] / divisor + constant
      // without changing either IEEE-754 rounding point.  Float literals are
      // selected as MOVZ+MOVK+FMOV_FROM_GPR; keep one materialization and
      // broadcast it instead of rebuilding it in every lane.
      if (std::getenv("AARCH64_SLP_FLOAT_AFFINE_OFF") == nullptr && i + 28 <= insts.size()) {
        TargetOperand *base = nullptr;
        TargetOperand *divisor = nullptr;
        int first_offset = 0;
        int movz_value = 0;
        int movk_value = 0;
        int movk_shift = 0;
        TargetInst *kept_movz = nullptr;
        TargetInst *kept_movk = nullptr;
        TargetInst *kept_fmov = nullptr;
        bool match = true;
        for (size_t lane = 0; lane < 4; ++lane) {
          size_t p = i + lane * 7;
          auto *load = insts[p];
          auto *divide = insts[p + 1];
          auto *movz = insts[p + 2];
          auto *movk = insts[p + 3];
          auto *fmov = insts[p + 4];
          auto *add = insts[p + 5];
          auto *store = insts[p + 6];
          if (!IsOpcode(load, A64Opcode::LDR_OFF, 2) || !IsOpcode(divide, A64Opcode::FDIV, 2) ||
              !IsOpcode(movz, A64Opcode::MOVZ, 1) || !IsOpcode(movk, A64Opcode::MOVK, 3) ||
              !IsOpcode(fmov, A64Opcode::FMOV_FROM_GPR, 1) || !IsOpcode(add, A64Opcode::FADD, 2) ||
              !IsOpcode(store, A64Opcode::STR_OFF, 3) || !IsScalarF32(load->GetResult()) ||
              divide->GetOperands()[0] != load->GetResult() || !IsScalarF32(divide->GetOperands()[1]) ||
              movk->GetResult() != movz->GetResult() || movk->GetOperands()[0] != movz->GetResult() ||
              fmov->GetOperands()[0] != movz->GetResult() || !IsScalarF32(fmov->GetResult()) ||
              !EitherOrder(add, divide->GetResult(), fmov->GetResult()) ||
              store->GetOperands()[0] != add->GetResult() || store->GetOperands()[1] != load->GetOperands()[0]) {
            match = false;
            break;
          }
          int offset = 0;
          int store_offset = 0;
          int lane_movz = 0;
          int lane_movk = 0;
          int lane_shift = 0;
          if (!Immediate(load->GetOperands()[1], &offset) || !Immediate(store->GetOperands()[2], &store_offset) ||
              !Immediate(movz->GetOperands()[0], &lane_movz) || !Immediate(movk->GetOperands()[1], &lane_movk) ||
              !Immediate(movk->GetOperands()[2], &lane_shift) || offset != store_offset) {
            match = false;
            break;
          }
          if (lane == 0) {
            base = load->GetOperands()[0];
            divisor = divide->GetOperands()[1];
            first_offset = offset;
            movz_value = lane_movz;
            movk_value = lane_movk;
            movk_shift = lane_shift;
            kept_movz = movz;
            kept_movk = movk;
            kept_fmov = fmov;
          } else if (load->GetOperands()[0] != base || divide->GetOperands()[1] != divisor ||
                     offset != first_offset + static_cast<int>(lane) * 4 || lane_movz != movz_value ||
                     lane_movk != movk_value || lane_shift != movk_shift) {
            match = false;
            break;
          }
        }
        if (match && first_offset >= 0 && first_offset % 16 == 0 && first_offset <= 4095 * 16) {
          ++float_affine_matches;
          auto *values = ctx_->NewVirtualReg(TargetRegClass::Vector, TargetRegSize::OWord);
          auto *vector_divisor = ctx_->NewVirtualReg(TargetRegClass::Vector, TargetRegSize::OWord);
          auto *quotient = ctx_->NewVirtualReg(TargetRegClass::Vector, TargetRegSize::OWord);
          auto *vector_addend = ctx_->NewVirtualReg(TargetRegClass::Vector, TargetRegSize::OWord);
          auto *result = ctx_->NewVirtualReg(TargetRegClass::Vector, TargetRegSize::OWord);
          auto *load = ctx_->CreateInst<AArch64Inst>(A64Opcode::LDR_OFF);
          load->SetResult(values);
          load->AddOperand(base);
          load->AddOperand(ctx_->Imm(first_offset));
          rewritten.push_back(load);
          auto *dup_divisor = ctx_->CreateInst<AArch64Inst>(A64Opcode::V_DUP_F);
          dup_divisor->SetResult(vector_divisor);
          dup_divisor->AddOperand(divisor);
          rewritten.push_back(dup_divisor);
          auto *divide = ctx_->CreateInst<AArch64Inst>(A64Opcode::V_FDIV);
          divide->SetResult(quotient);
          divide->AddOperand(values);
          divide->AddOperand(vector_divisor);
          rewritten.push_back(divide);
          rewritten.push_back(kept_movz);
          rewritten.push_back(kept_movk);
          rewritten.push_back(kept_fmov);
          auto *dup_addend = ctx_->CreateInst<AArch64Inst>(A64Opcode::V_DUP_F);
          dup_addend->SetResult(vector_addend);
          dup_addend->AddOperand(kept_fmov->GetResult());
          rewritten.push_back(dup_addend);
          auto *add = ctx_->CreateInst<AArch64Inst>(A64Opcode::V_FADD);
          add->SetResult(result);
          add->AddOperand(quotient);
          add->AddOperand(vector_addend);
          rewritten.push_back(add);
          auto *store = ctx_->CreateInst<AArch64Inst>(A64Opcode::STR_OFF);
          store->AddOperand(result);
          store->AddOperand(base);
          store->AddOperand(ctx_->Imm(first_offset));
          rewritten.push_back(store);
          i += 28;
          changed = true;
          block_changed = true;
          continue;
        }
      }
      // Preserve scalar IEEE-754 evaluation order while packing
      //   dst[i] = dst[i] - factor * src[i].
      // A vector FMUL followed by FSUB has the same two rounding points as the
      // source.  Deliberately do not contract this to FMLS.
      if (std::getenv("AARCH64_SLP_FLOAT_SAXPY_OFF") == nullptr && i + 20 <= insts.size()) {
        ++float_windows;
        TargetOperand *dst_base = nullptr;
        TargetOperand *src_base = nullptr;
        TargetOperand *factor = nullptr;
        int dst_first = 0;
        int src_first = 0;
        bool match = true;
        for (size_t lane = 0; lane < 4; ++lane) {
          size_t p = i + lane * 5;
          auto *load_dst = insts[p];
          auto *load_src = insts[p + 1];
          auto *multiply = insts[p + 2];
          auto *subtract = insts[p + 3];
          auto *store = insts[p + 4];
          if (!IsOpcode(load_dst, A64Opcode::LDR_OFF, 2) || !IsOpcode(load_src, A64Opcode::LDR_OFF, 2) ||
              !IsOpcode(multiply, A64Opcode::FMUL, 2) || !IsOpcode(subtract, A64Opcode::FSUB, 2) ||
              !IsOpcode(store, A64Opcode::STR_OFF, 3) || !IsScalarF32(load_dst->GetResult()) ||
              !IsScalarF32(load_src->GetResult()) || subtract->GetOperands()[0] != load_dst->GetResult() ||
              subtract->GetOperands()[1] != multiply->GetResult() || store->GetOperands()[0] != subtract->GetResult()) {
            match = false;
            break;
          }
          TargetOperand *lane_factor = nullptr;
          if (multiply->GetOperands()[0] == load_src->GetResult()) {
            lane_factor = multiply->GetOperands()[1];
          } else if (multiply->GetOperands()[1] == load_src->GetResult()) {
            lane_factor = multiply->GetOperands()[0];
          } else {
            match = false;
            break;
          }
          int dst_offset = 0;
          int src_offset = 0;
          if (!Immediate(load_dst->GetOperands()[1], &dst_offset) ||
              !Immediate(load_src->GetOperands()[1], &src_offset) || !Immediate(store->GetOperands()[2], &dst_offset) ||
              !IsScalarF32(lane_factor) || store->GetOperands()[1] != load_dst->GetOperands()[0]) {
            match = false;
            break;
          }
          int load_dst_offset = 0;
          if (!Immediate(load_dst->GetOperands()[1], &load_dst_offset) || load_dst_offset != dst_offset) {
            match = false;
            break;
          }
          if (lane == 0) {
            dst_base = load_dst->GetOperands()[0];
            src_base = load_src->GetOperands()[0];
            factor = lane_factor;
            dst_first = dst_offset;
            src_first = src_offset;
          } else if (load_dst->GetOperands()[0] != dst_base || load_src->GetOperands()[0] != src_base ||
                     lane_factor != factor || dst_offset != dst_first + static_cast<int>(lane) * 4 ||
                     src_offset != src_first + static_cast<int>(lane) * 4) {
            match = false;
            break;
          }
        }
        const bool encodable = dst_first >= 0 && src_first >= 0 && dst_first % 16 == 0 && src_first % 16 == 0 &&
                               dst_first <= 4095 * 16 && src_first <= 4095 * 16;
        if (match) ++float_structural_matches;
        if (match && encodable && safe_stream(dst_base, dst_first, src_base, src_first)) {
          ++float_safe_matches;
          auto *dst = ctx_->NewVirtualReg(TargetRegClass::Vector, TargetRegSize::OWord);
          auto *src = ctx_->NewVirtualReg(TargetRegClass::Vector, TargetRegSize::OWord);
          auto *broadcast = ctx_->NewVirtualReg(TargetRegClass::Vector, TargetRegSize::OWord);
          auto *product = ctx_->NewVirtualReg(TargetRegClass::Vector, TargetRegSize::OWord);
          auto *result = ctx_->NewVirtualReg(TargetRegClass::Vector, TargetRegSize::OWord);
          auto make_load = [&](TargetOperand *vector, TargetOperand *base, int offset) {
            auto *load = ctx_->CreateInst<AArch64Inst>(A64Opcode::LDR_OFF);
            load->SetResult(vector);
            load->AddOperand(base);
            load->AddOperand(ctx_->Imm(offset));
            rewritten.push_back(load);
          };
          make_load(dst, dst_base, dst_first);
          make_load(src, src_base, src_first);
          auto *dup = ctx_->CreateInst<AArch64Inst>(A64Opcode::V_DUP_F);
          dup->SetResult(broadcast);
          dup->AddOperand(factor);
          rewritten.push_back(dup);
          auto *multiply = ctx_->CreateInst<AArch64Inst>(A64Opcode::V_FMUL);
          multiply->SetResult(product);
          multiply->AddOperand(src);
          multiply->AddOperand(broadcast);
          rewritten.push_back(multiply);
          auto *subtract = ctx_->CreateInst<AArch64Inst>(A64Opcode::V_FSUB);
          subtract->SetResult(result);
          subtract->AddOperand(dst);
          subtract->AddOperand(product);
          rewritten.push_back(subtract);
          auto *store = ctx_->CreateInst<AArch64Inst>(A64Opcode::STR_OFF);
          store->AddOperand(result);
          store->AddOperand(dst_base);
          store->AddOperand(ctx_->Imm(dst_first));
          rewritten.push_back(store);
          i += 20;
          changed = true;
          block_changed = true;
          continue;
        }
      }
      // Four independent lanes of either
      //   dst[i] = src[i] + scalar
      // or
      //   dst[i] = src[i] * scalar
      // become one q load, dup, vector arithmetic and one q store.  The
      // scalar operand must be identical in all lanes, which proves that it
      // is loop-invariant over the unrolled group; an IV-derived scalar is
      // intentionally left scalar until a proper vector-IV form exists.
      if (std::getenv("AARCH64_SLP_SCALAR_OFF") == nullptr && i + 12 <= insts.size()) {
        TargetOperand *src_base = nullptr;
        TargetOperand *dst_base = nullptr;
        TargetOperand *scalar = nullptr;
        A64Opcode scalar_opcode = A64Opcode::ADD;
        int src_first = 0;
        int dst_first = 0;
        bool match = true;
        for (size_t lane = 0; lane < 4; ++lane) {
          size_t p = i + lane * 3;
          auto *load = insts[p];
          auto *binary = insts[p + 1];
          auto *store = insts[p + 2];
          if (!IsOpcode(load, A64Opcode::LDR_OFF, 2) || !IsOpcode(store, A64Opcode::STR_OFF, 3) ||
              !IsScalarI32(load->GetResult())) {
            match = false;
            break;
          }
          auto opcode = static_cast<AArch64Inst *>(binary)->GetOpcode();
          if ((opcode != A64Opcode::ADD && opcode != A64Opcode::MUL) || !IsOpcode(binary, opcode, 2) ||
              store->GetOperands()[0] != binary->GetResult()) {
            match = false;
            break;
          }
          TargetOperand *lane_scalar = nullptr;
          if (binary->GetOperands()[0] == load->GetResult()) {
            lane_scalar = binary->GetOperands()[1];
          } else if (binary->GetOperands()[1] == load->GetResult()) {
            lane_scalar = binary->GetOperands()[0];
          } else {
            match = false;
            break;
          }
          int src_offset = 0;
          int dst_offset = 0;
          if (!Immediate(load->GetOperands()[1], &src_offset) || !Immediate(store->GetOperands()[2], &dst_offset) ||
              !IsScalarI32(lane_scalar)) {
            match = false;
            break;
          }
          if (lane == 0) {
            src_base = load->GetOperands()[0];
            dst_base = store->GetOperands()[1];
            scalar = lane_scalar;
            scalar_opcode = opcode;
            src_first = src_offset;
            dst_first = dst_offset;
          } else if (load->GetOperands()[0] != src_base || store->GetOperands()[1] != dst_base ||
                     lane_scalar != scalar || opcode != scalar_opcode ||
                     src_offset != src_first + static_cast<int>(lane) * 4 ||
                     dst_offset != dst_first + static_cast<int>(lane) * 4) {
            match = false;
            break;
          }
        }
        const bool encodable = src_first >= 0 && dst_first >= 0 && src_first % 16 == 0 && dst_first % 16 == 0 &&
                               src_first <= 4095 * 16 && dst_first <= 4095 * 16;
        if (match && encodable && safe_stream(dst_base, dst_first, src_base, src_first)) {
          auto *src = ctx_->NewVirtualReg(TargetRegClass::Vector, TargetRegSize::OWord);
          auto *factor = ctx_->NewVirtualReg(TargetRegClass::Vector, TargetRegSize::OWord);
          auto *result = ctx_->NewVirtualReg(TargetRegClass::Vector, TargetRegSize::OWord);
          auto *load = ctx_->CreateInst<AArch64Inst>(A64Opcode::LDR_OFF);
          load->SetResult(src);
          load->AddOperand(src_base);
          load->AddOperand(ctx_->Imm(src_first));
          rewritten.push_back(load);
          auto *dup = ctx_->CreateInst<AArch64Inst>(A64Opcode::V_DUP);
          dup->SetResult(factor);
          dup->AddOperand(scalar);
          rewritten.push_back(dup);
          auto *vector_binary =
              ctx_->CreateInst<AArch64Inst>(scalar_opcode == A64Opcode::ADD ? A64Opcode::V_ADD : A64Opcode::V_MUL);
          vector_binary->SetResult(result);
          vector_binary->AddOperand(src);
          vector_binary->AddOperand(factor);
          rewritten.push_back(vector_binary);
          auto *store = ctx_->CreateInst<AArch64Inst>(A64Opcode::STR_OFF);
          store->AddOperand(result);
          store->AddOperand(dst_base);
          store->AddOperand(ctx_->Imm(dst_first));
          rewritten.push_back(store);
          i += 12;
          changed = true;
          block_changed = true;
          continue;
        }
      }
      // Four independent lanes of either
      //   dst[i] = lhs[i] + rhs[i]
      // or
      //   dst[i] = lhs[i] * rhs[i]
      // become two q loads, one vector arithmetic operation and one q store.
      // This is a data-flow pattern, not an array- or loop-specific rule. The
      // store may be in-place with either input only when it names the exact
      // same contiguous stream; otherwise distinct global provenance proves
      // that hoisting the loads preserves scalar semantics.
      if (i + 16 <= insts.size()) {
        TargetOperand *lhs_base = nullptr;
        TargetOperand *rhs_base = nullptr;
        TargetOperand *dst_base = nullptr;
        A64Opcode scalar_opcode = A64Opcode::ADD;
        int lhs_first = 0;
        int rhs_first = 0;
        int dst_first = 0;
        bool match = true;
        for (size_t lane = 0; lane < 4; ++lane) {
          size_t p = i + lane * 4;
          auto *load_lhs = insts[p];
          auto *load_rhs = insts[p + 1];
          auto *binary = insts[p + 2];
          auto *store = insts[p + 3];
          if (!IsOpcode(load_lhs, A64Opcode::LDR_OFF, 2) || !IsOpcode(load_rhs, A64Opcode::LDR_OFF, 2) ||
              !IsOpcode(store, A64Opcode::STR_OFF, 3)) {
            match = false;
            break;
          }
          auto opcode = static_cast<AArch64Inst *>(binary)->GetOpcode();
          if ((opcode != A64Opcode::ADD && opcode != A64Opcode::MUL) || !IsOpcode(binary, opcode, 2) ||
              !IsScalarI32(load_lhs->GetResult()) || !IsScalarI32(load_rhs->GetResult()) ||
              !EitherOrder(binary, load_lhs->GetResult(), load_rhs->GetResult()) ||
              store->GetOperands()[0] != binary->GetResult()) {
            match = false;
            break;
          }
          const auto &lhs_ops = load_lhs->GetOperands();
          const auto &rhs_ops = load_rhs->GetOperands();
          const auto &dst_ops = store->GetOperands();
          int lhs_offset = 0;
          int rhs_offset = 0;
          int dst_offset = 0;
          if (!Immediate(lhs_ops[1], &lhs_offset) || !Immediate(rhs_ops[1], &rhs_offset) ||
              !Immediate(dst_ops[2], &dst_offset)) {
            match = false;
            break;
          }
          if (lane == 0) {
            lhs_base = lhs_ops[0];
            rhs_base = rhs_ops[0];
            dst_base = dst_ops[1];
            scalar_opcode = opcode;
            lhs_first = lhs_offset;
            rhs_first = rhs_offset;
            dst_first = dst_offset;
          } else if (lhs_ops[0] != lhs_base || rhs_ops[0] != rhs_base || dst_ops[1] != dst_base ||
                     opcode != scalar_opcode || lhs_offset != lhs_first + static_cast<int>(lane) * 4 ||
                     rhs_offset != rhs_first + static_cast<int>(lane) * 4 ||
                     dst_offset != dst_first + static_cast<int>(lane) * 4) {
            match = false;
            break;
          }
        }
        const bool encodable = lhs_first >= 0 && rhs_first >= 0 && dst_first >= 0 && lhs_first % 16 == 0 &&
                               rhs_first % 16 == 0 && dst_first % 16 == 0 && lhs_first <= 4095 * 16 &&
                               rhs_first <= 4095 * 16 && dst_first <= 4095 * 16;
        if (match && encodable && safe_stream(dst_base, dst_first, lhs_base, lhs_first) &&
            safe_stream(dst_base, dst_first, rhs_base, rhs_first)) {
          auto *lhs = ctx_->NewVirtualReg(TargetRegClass::Vector, TargetRegSize::OWord);
          auto *rhs = ctx_->NewVirtualReg(TargetRegClass::Vector, TargetRegSize::OWord);
          auto *result = ctx_->NewVirtualReg(TargetRegClass::Vector, TargetRegSize::OWord);
          auto make_load = [&](TargetOperand *vector, TargetOperand *base, int offset) {
            auto *load = ctx_->CreateInst<AArch64Inst>(A64Opcode::LDR_OFF);
            load->SetResult(vector);
            load->AddOperand(base);
            load->AddOperand(ctx_->Imm(offset));
            rewritten.push_back(load);
          };
          make_load(lhs, lhs_base, lhs_first);
          make_load(rhs, rhs_base, rhs_first);
          auto *vector_binary =
              ctx_->CreateInst<AArch64Inst>(scalar_opcode == A64Opcode::ADD ? A64Opcode::V_ADD : A64Opcode::V_MUL);
          vector_binary->SetResult(result);
          vector_binary->AddOperand(lhs);
          vector_binary->AddOperand(rhs);
          rewritten.push_back(vector_binary);
          auto *store = ctx_->CreateInst<AArch64Inst>(A64Opcode::STR_OFF);
          store->AddOperand(result);
          store->AddOperand(dst_base);
          store->AddOperand(ctx_->Imm(dst_first));
          rewritten.push_back(store);
          i += 16;
          changed = true;
          block_changed = true;
          continue;
        }
      }
      // Four adjacent scalar load/store pairs are a bitwise 16-byte copy for
      // both SysY int and float arrays.  Hoisting all four loads before the
      // store is legal only for the exact same stream or proven disjoint
      // address ranges (including stack-vs-nonstack objects).
      if (std::getenv("AARCH64_SLP_COPY_OFF") == nullptr && i + 8 <= insts.size()) {
        TargetOperand *src_base = nullptr;
        TargetOperand *dst_base = nullptr;
        int src_first = 0;
        int dst_first = 0;
        bool match = true;
        for (size_t lane = 0; lane < 4; ++lane) {
          auto *load = insts[i + lane * 2];
          auto *store = insts[i + lane * 2 + 1];
          if (!IsOpcode(load, A64Opcode::LDR_OFF, 2) || !IsOpcode(store, A64Opcode::STR_OFF, 3) ||
              store->GetOperands()[0] != load->GetResult()) {
            match = false;
            break;
          }
          int src_offset = 0;
          int dst_offset = 0;
          if (!Immediate(load->GetOperands()[1], &src_offset) || !Immediate(store->GetOperands()[2], &dst_offset)) {
            match = false;
            break;
          }
          if (lane == 0) {
            src_base = load->GetOperands()[0];
            dst_base = store->GetOperands()[1];
            src_first = src_offset;
            dst_first = dst_offset;
          } else if (load->GetOperands()[0] != src_base || store->GetOperands()[1] != dst_base ||
                     src_offset != src_first + static_cast<int>(lane) * 4 ||
                     dst_offset != dst_first + static_cast<int>(lane) * 4) {
            match = false;
            break;
          }
        }
        const bool encodable = src_first >= 0 && dst_first >= 0 && src_first % 16 == 0 && dst_first % 16 == 0 &&
                               src_first <= 4095 * 16 && dst_first <= 4095 * 16;
        if (match) ++copy_structural_matches;
        if (match && encodable && safe_stream(dst_base, dst_first, src_base, src_first)) {
          ++copy_safe_matches;
          auto *vector = ctx_->NewVirtualReg(TargetRegClass::Vector, TargetRegSize::OWord);
          auto *load = ctx_->CreateInst<AArch64Inst>(A64Opcode::LDR_OFF);
          load->SetResult(vector);
          load->AddOperand(src_base);
          load->AddOperand(ctx_->Imm(src_first));
          rewritten.push_back(load);
          auto *store = ctx_->CreateInst<AArch64Inst>(A64Opcode::STR_OFF);
          store->AddOperand(vector);
          store->AddOperand(dst_base);
          store->AddOperand(ctx_->Imm(dst_first));
          rewritten.push_back(store);
          i += 8;
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
  if (std::getenv("AARCH64_SLP_FLOAT_STATS") != nullptr) {
    std::fprintf(stderr, "AArch64SLPFloat: windows=%zu structural=%zu safe=%zu affine=%zu\n", float_windows,
                 float_structural_matches, float_safe_matches, float_affine_matches);
  }
  if (std::getenv("AARCH64_SLP_COPY_STATS") != nullptr) {
    std::fprintf(stderr, "AArch64SLPCopy: structural=%zu safe=%zu\n", copy_structural_matches, copy_safe_matches);
  }
  return changed;
}

}  // namespace backend::targetir::aarch64
