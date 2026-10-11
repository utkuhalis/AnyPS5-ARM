#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "Recompiler.hpp"
#include "VulkanTestDevice.hpp"
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <iostream>
#include <span>
#include <string>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;
using ShaderRecompiler::ShaderStage;

constexpr std::uint32_t Threads = 256;
constexpr std::uint32_t Iterations = 16;
constexpr std::uint32_t Repeats = 32;
constexpr std::uint32_t LdsDwords = 4;
alignas(256) std::array<std::uint32_t, Threads * 2> Output{};
alignas(256) std::array<std::uint32_t, Threads * 4> Input = [] {
    std::array<std::uint32_t, Threads * 4> rows{};
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        rows[tid * 4u] = 0xffffffffu;
        rows[tid * 4u + 2u] = 1u;
    }
    return rows;
}();

template<std::uint32_t Count, std::size_t Head, std::size_t Body, std::size_t Tail>
constexpr auto Unrolled(const std::array<std::uint32_t, Head>& head, const std::array<std::uint32_t, Body>& body, const std::array<std::uint32_t, Tail>& tail) {
    std::array<std::uint32_t, Head + Body * Count + Tail> code{};
    std::size_t next = 0;
    for (const auto word : head) code[next++] = word;
    for (std::uint32_t iteration = 0; iteration < Count; ++iteration) {
        for (const auto word : body) code[next++] = word;
    }
    for (const auto word : tail) code[next++] = word;
    return code;
}

alignas(256) constexpr auto UniformCode = Unrolled<Iterations>(
    std::to_array<std::uint32_t>({
        0x34100083,
        0x7e020280,
        0x7e0402c1,
        0x7e060280,
        0x7e080281,
        0x7e0c0280,
        0x7e0e0280,
        0xd9340000, 0x00000601,
        0xbf8cc07f,
        0xbf8a0000,
    }),
    std::to_array<std::uint32_t>({
        0xd9000000, 0x00000201,
        0xd8000004, 0x00000401,
        0xbf8cc07f,
    }),
    std::to_array<std::uint32_t>({
        0xbf8cc07f,
        0xbf8a0000,
        0xd9d80000, 0x0a000001,
        0xbf8cc07f,
        0xe0741000, 0x80010a08,
        0xbf810000,
    }));

alignas(256) constexpr auto DivergentCode = Unrolled<Iterations>(
    std::to_array<std::uint32_t>({
        0x34100083,
        0x7e020280,
        0x7e0402c1,
        0x7e060280,
        0x7e080281,
        0x7e0c0280,
        0x7e0e0280,
        0xd9340000, 0x00000601,
        0xbf8cc07f,
        0xbf8a0000,
        0xbe89037e,
    }),
    std::to_array<std::uint32_t>({
        0xbefe03ff, 0x55555555,
        0xd9000000, 0x00000201,
        0xbefe03ff, 0xaaaaaaaa,
        0xd8000004, 0x00000401,
        0xbf8cc07f,
    }),
    std::to_array<std::uint32_t>({
        0xbefe0309,
        0xbf8cc07f,
        0xbf8a0000,
        0xd9d80000, 0x0a000001,
        0xbf8cc07f,
        0xe0741000, 0x80010a08,
        0xbf810000,
    }));

alignas(256) constexpr auto ParityCode = Unrolled<1>(
    std::to_array<std::uint32_t>({
        0x34100083,
        0x7e020280,
        0x7e0402c1,
        0x7e060280,
        0x7e080281,
        0x7e0c0280,
        0x7e0e0280,
        0xd9340000, 0x00000601,
        0xbf8cc07f,
        0xbf8a0000,
        0xbe89037e,
    }),
    std::to_array<std::uint32_t>({
        0xbefe03ff, 0x55555555,
        0xd9000000, 0x00000201,
        0xbf8cc07f,
        0xbefe03ff, 0xaaaaaaaa,
        0xd8000004, 0x00000401,
        0xbf8cc07f,
    }),
    std::to_array<std::uint32_t>({
        0xbefe0309,
        0xbf8cc07f,
        0xbf8a0000,
        0xd9d80000, 0x0a000001,
        0xbf8cc07f,
        0xe0741000, 0x80010a08,
        0xbf810000,
    }));

alignas(256) constexpr auto LoadedParityCode = Unrolled<1>(
    std::to_array<std::uint32_t>({
        0x34100083,
        0x340a0084,
        0x7e020280,
        0xe0381000, 0x80000c05,
        0x7e0c0280,
        0x7e0e0280,
        0xd9340000, 0x00000601,
        0xbf8c0070,
        0xbf8a0000,
        0xbe89037e,
    }),
    std::to_array<std::uint32_t>({
        0xbefe03ff, 0x55555555,
        0xd9000000, 0x00000c01,
        0xbf8cc07f,
        0xbefe03ff, 0xaaaaaaaa,
        0xd8000004, 0x00000e01,
        0xbf8cc07f,
    }),
    std::to_array<std::uint32_t>({
        0xbefe0309,
        0xbf8cc07f,
        0xbf8a0000,
        0xd9d80000, 0x0a000001,
        0xbf8cc07f,
        0xe0741000, 0x80010a08,
        0xbf810000,
    }));

std::array<std::uint32_t, 4> BufferDescriptor(const void* data, std::uint32_t bytes) {
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu), bytes, 0x31016facu};
}

std::string Hex(std::uint64_t value) {
    char text[24];
    std::snprintf(text, sizeof(text), "0x%016llx", static_cast<unsigned long long>(value));
    return text;
}

void Check(const char* name, std::uint64_t operations, std::uint32_t run) {
    const std::uint64_t expected = operations * 0xffffffffull + (operations << 32u);
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        const std::uint64_t actual = Output[tid * 2u] | (static_cast<std::uint64_t>(Output[tid * 2u + 1u]) << 32u);
        Require(actual == expected, std::string("lds mixed width atomics ") + name + " run " + std::to_string(run) + ": lane " + std::to_string(tid) + " read " + Hex(actual) + ", expected " + Hex(expected) + " after " + std::to_string(operations) + " ds_add_u64 and " + std::to_string(operations) + " ds_add_u32 on one slot");
    }
}

void Run(AgcDriver::VulkanDevice& device, std::span<const std::uint32_t> code, const char* name, std::uint64_t operations, std::uint32_t repeats) {
    std::vector<std::uint32_t> userData(8, 0u);
    const auto input = BufferDescriptor(Input.data(), static_cast<std::uint32_t>(Input.size() * 4u));
    const auto output = BufferDescriptor(Output.data(), static_cast<std::uint32_t>(Output.size() * 4u));
    std::copy(input.begin(), input.end(), userData.begin());
    std::copy(output.begin(), output.end(), userData.begin() + 4);
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(code.data()), std::as_bytes(code)}}};
    const ShaderRecompiler::ShaderComputeStageInfo compute{{Threads, 1, 1}, LdsDwords, {false, false, false}, false, 1};
    ShaderRecompiler::RecompileRequest request{
        {ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(code.data()), code, 0, {}},
        {32, 0, userData, compute, std::nullopt, std::nullopt, memory},
        device.Target(),
        {0, 0, 0, 128}
    };
    request.useCache = false;
    const auto result = ShaderRecompiler::Recompile(request);
    for (std::uint32_t run = 0; run < repeats; ++run) {
        Output.fill(0xdeadbeefu);
        device.Dispatch(result, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(code.data()));
        device.WaitIdle();
        Check(name, operations, run);
    }
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        Run(*device, UniformCode, "uniform", static_cast<std::uint64_t>(Threads) * Iterations, 1);
        Run(*device, DivergentCode, "divergent", static_cast<std::uint64_t>(Threads) / 2u * Iterations, 1);
        Run(*device, ParityCode, "split lanes", Threads / 2u, Repeats);
        Run(*device, LoadedParityCode, "split lanes with per-lane operands", Threads / 2u, Repeats);
        std::puts("lds mixed width atomics tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
