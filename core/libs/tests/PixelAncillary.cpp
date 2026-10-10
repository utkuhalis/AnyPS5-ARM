#include "IntermediateRepresentation/IrBuilder.hpp"
#include "Optimization/ConstantFolder.hpp"
#include "Optimization/DeadCodeEliminator.hpp"
#include "Optimization/ShaderInfoCollector.hpp"
#include <cstdint>
#include <stdexcept>
#include <string>

using namespace ShaderRecompiler;
static void Require(bool value) { if (!value) throw std::runtime_error("packed pixel ancillary regression"); }
static IrValue& Build(IrProgram& program, IrOpcode opcode, std::uint32_t offset, std::uint32_t count) {
    program.Resources().stage = IrShaderStage::Pixel;
    program.Resources().resourceTrackingComplete = true;
    auto& block = program.CreateBlock();
    program.SetEntryBlock(block);
    program.BlockOrder().push_back(&block);
    IrBuilder builder(program);
    builder.SetInsertionPoint(block);
    auto& ancillary = builder.Emit(IrOpcode::GetBuiltin, IrType::U32, {&builder.Constant(static_cast<std::uint32_t>(StageInputKind::PackedAncillary)), &builder.Constant(0u)});
    auto& user = opcode == IrOpcode::BitwiseOr32 ? builder.Emit(opcode, IrType::U32, {&ancillary, &builder.Constant(offset)})
                                                 : builder.Emit(opcode, IrType::U32, {&ancillary, &builder.Constant(offset), &builder.Constant(count)});
    static_cast<void>(builder.Emit(IrOpcode::ReferenceU32, IrType::Void, {&user}));
    static_cast<void>(builder.Emit(IrOpcode::Return, IrType::Void, {}));
    return user;
}
static void Lower(IrProgram& program) {
    ConstantFolder().Fold(program);
    DeadCodeEliminator().RemoveIdentities(program);
    DeadCodeEliminator().Eliminate(program);
    const ShaderPixelInputInfo pixel {};
    ShaderInfoCollector().Collect(program, ShaderStageInputInfo {nullptr, &pixel, nullptr});
}
static void Extract(IrOpcode opcode, std::uint32_t offset, std::uint32_t count, StageInputKind kind, std::uint32_t fieldOffset) {
    IrProgram program;
    auto& user = Build(program, opcode, offset, count);
    Lower(program);
    const IrValue* field = user.Argument(0)->Resolve();
    Require(field->Opcode() == IrOpcode::GetBuiltin);
    Require(static_cast<StageInputKind>(field->Argument(0)->Resolve()->ImmediateU32()) == kind);
    Require(user.Argument(1)->Resolve()->ImmediateU32() == fieldOffset);
    Require(user.Argument(2)->Resolve()->ImmediateU32() == count);
}
// The packed value reaches the extract through a select (v_cndmask), with the layer field as the other choice.
static IrValue& BuildSelected(IrProgram& program, std::uint32_t offset, std::uint32_t count) {
    program.Resources().stage = IrShaderStage::Pixel;
    program.Resources().resourceTrackingComplete = true;
    auto& block = program.CreateBlock();
    program.SetEntryBlock(block);
    program.BlockOrder().push_back(&block);
    IrBuilder builder(program);
    builder.SetInsertionPoint(block);
    auto& ancillary = builder.Emit(IrOpcode::GetBuiltin, IrType::U32, {&builder.Constant(static_cast<std::uint32_t>(StageInputKind::PackedAncillary)), &builder.Constant(0u)});
    auto& layer = builder.Emit(IrOpcode::GetBuiltin, IrType::U32, {&builder.Constant(static_cast<std::uint32_t>(StageInputKind::Layer)), &builder.Constant(0u)});
    auto& condition = builder.Emit(IrOpcode::IEqual32, IrType::U1, {&layer, &builder.Constant(0u)});
    auto& selected = builder.Emit(IrOpcode::SelectU32, IrType::U32, {&condition, &ancillary, &builder.Constant(0u)});
    auto& user = builder.Emit(IrOpcode::BitFieldUExtract, IrType::U32, {&selected, &builder.Constant(offset), &builder.Constant(count)});
    static_cast<void>(builder.Emit(IrOpcode::ReferenceU32, IrType::Void, {&user}));
    static_cast<void>(builder.Emit(IrOpcode::Return, IrType::Void, {}));
    return user;
}
static bool UsesBuiltin(IrProgram& program, StageInputKind kind) {
    for (const auto& block : program.Blocks()) {
        for (const IrValue* inst : block->Instructions()) {
            if (inst->Opcode() == IrOpcode::GetBuiltin && static_cast<StageInputKind>(inst->Argument(0)->Resolve()->ImmediateU32()) == kind) return true;
        }
    }
    return false;
}
static void Selected() {
    IrProgram program;
    static_cast<void>(BuildSelected(program, 16u, 11u));
    Lower(program);
    // Rebuilt from the layer alone: no packed input and no sample ID (which would force per-sample shading).
    Require(!UsesBuiltin(program, StageInputKind::PackedAncillary) && !UsesBuiltin(program, StageInputKind::SampleId));
}
// A use of other bits rebuilds the packed value from both known fields (the other bits read as zero).
static void Rebuilt(IrProgram& program) {
    Lower(program);
    Require(!UsesBuiltin(program, StageInputKind::PackedAncillary));
    Require(UsesBuiltin(program, StageInputKind::SampleId) && UsesBuiltin(program, StageInputKind::Layer));
}
static void Opaque(IrOpcode opcode, std::uint32_t offset, std::uint32_t count) {
    IrProgram program;
    static_cast<void>(Build(program, opcode, offset, count));
    Rebuilt(program);
}
static void SelectedOpaque(std::uint32_t offset, std::uint32_t count) {
    IrProgram program;
    static_cast<void>(BuildSelected(program, offset, count));
    Rebuilt(program);
}
int main() {
    Extract(IrOpcode::BitFieldUExtract, 8u, 4u, StageInputKind::SampleId, 0u);
    Extract(IrOpcode::BitFieldUExtract, 9u, 2u, StageInputKind::SampleId, 1u);
    Extract(IrOpcode::BitFieldUExtract, 16u, 13u, StageInputKind::Layer, 0u);
    Extract(IrOpcode::BitFieldSExtract, 20u, 9u, StageInputKind::Layer, 4u);
    Opaque(IrOpcode::BitwiseOr32, 1u, 0u);
    Opaque(IrOpcode::BitFieldUExtract, 2u, 4u);
    Opaque(IrOpcode::BitFieldUExtract, 10u, 4u);
    Opaque(IrOpcode::BitFieldUExtract, 13u, 2u);
    Opaque(IrOpcode::BitFieldUExtract, 16u, 14u);
    Selected();
    SelectedOpaque(2u, 4u);
    SelectedOpaque(16u, 14u);
}
