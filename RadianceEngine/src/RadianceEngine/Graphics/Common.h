#pragma once
#include <type_traits>

#define DEFINE_ENUM_FLAGS_OR(EnumName) \
    inline constexpr EnumName operator|(EnumName a, EnumName b) { \
        using T = std::underlying_type_t<EnumName>; \
        return static_cast<EnumName>(static_cast<T>(a) | static_cast<T>(b)); \
    } \
    inline constexpr EnumName& operator|=(EnumName& a, EnumName b) { \
        using T = std::underlying_type_t<EnumName>; \
        a = static_cast<EnumName>(static_cast<T>(a) | static_cast<T>(b)); \
        return a; \
    }

namespace Rdn
{
    struct Extent2D
    {
        uint32_t Width = 0;
        uint32_t Height = 0;

        bool operator==(const Extent2D&) const = default;
    };

    struct Extent3D
    {
        uint32_t Width = 0;
        uint32_t Height = 0;
        uint32_t Depth = 1;

        constexpr Extent3D() = default;
        constexpr Extent3D(uint32_t w, uint32_t h, uint32_t d = 1) : Width(w), Height(h), Depth(d) {}
        constexpr Extent3D(Extent2D e, uint32_t d = 1) : Width(e.Width), Height(e.Height), Depth(d) {}
        constexpr Extent2D To2D() const { return { Width, Height }; }

        bool operator==(const Extent3D&) const = default;
    };

    enum class PresentMode
    {
        Immediate = 0,
        Mailbox = 1,
        Fifo = 2,
        FifoRelaxed = 3,
    };

    enum class Format
    {
        Undefined = 0,
        R8_Unorm = 9,
        R8_Snorm = 10,
        R8_Uint = 13,
        R8_Sint = 14,
        R8_Srgb = 15,
        R8G8_Unorm = 16,
        R8G8_Snorm = 17,
        R8G8_Uint = 20,
        R8G8_Sint = 21,
        R8G8_Srgb = 22,
        R8G8B8_Unorm = 23,
        R8G8B8_Snorm = 24,
        R8G8B8_Uint = 27,
        R8G8B8_Sint = 28,
        R8G8B8_Srgb = 29,
        B8G8R8_Unorm = 30,
        B8G8R8_Snorm = 31,
        B8G8R8_Uint = 34,
        B8G8R8_Sint = 35,
        B8G8R8_Srgb = 36,
        R8G8B8A8_Unorm = 37,
        R8G8B8A8_Snorm = 38,
        R8G8B8A8_Uint = 41,
        R8G8B8A8_Sint = 42,
        R8G8B8A8_Srgb = 43,
        B8G8R8A8_Unorm = 44,
        B8G8R8A8_Snorm = 45,
        B8G8R8A8_Uint = 48,
        B8G8R8A8_Sint = 49,
        B8G8R8A8_Srgb = 50,
        R16_Unorm = 70,
        R16_Snorm = 71,
        R16_Uint = 74,
        R16_Sint = 75,
        R16_Sfloat = 76,
        R16G16_Unorm = 77,
        R16G16_Snorm = 78,
        R16G16_Uint = 81,
        R16G16_Sint = 82,
        R16G16_Sfloat = 83,
        R16G16B16_Unorm = 84,
        R16G16B16_Snorm = 85,
        R16G16B16_Uint = 88,
        R16G16B16_Sint = 89,
        R16G16B16_Sfloat = 90,
        R16G16B16A16_Unorm = 91,
        R16G16B16A16_Snorm = 92,
        R16G16B16A16_Uint = 95,
        R16G16B16A16_Sint = 96,
        R16G16B16A16_Sfloat = 97,
        R32_Uint = 98,
        R32_Sint = 99,
        R32_Sfloat = 100,
        R32G32_Uint = 101,
        R32G32_Sint = 102,
        R32G32_Sfloat = 103,
        R32G32B32_Uint = 104,
        R32G32B32_Sint = 105,
        R32G32B32_Sfloat = 106,
        R32G32B32A32_Uint = 107,
        R32G32B32A32_Sint = 108,
        R32G32B32A32_Sfloat = 109,
        R64_Uint = 110,
        R64_Sint = 111,
        R64_Sfloat = 112,
        R64G64_Uint = 113,
        R64G64_Sint = 114,
        R64G64_Sfloat = 115,
        R64G64B64_Uint = 116,
        R64G64B64_Sint = 117,
        R64G64B64_Sfloat = 118,
        R64G64B64A64_Uint = 119,
        R64G64B64A64_Sint = 120,
        R64G64B64A64_Sfloat = 121,
        D16_Unorm = 124,
        D32_Sfloat = 126,
        S8_Uint = 127,
        D16_Unorm_S8_Uint = 128,
        D24_Unorm_S8_Uint = 129,
        D32_Sfloat_S8_Uint = 130,
    };

    enum class PipelineStage
    {
        None = 0ULL,
        TopOfPipe = 0x00000001ULL,
        DrawIndirect = 0x00000002ULL,
        VertexInput = 0x00000004ULL,
        VertexShader = 0x00000008ULL,
        TessellationControlShader = 0x00000010ULL,
        TessellationEvaluationShader = 0x00000020ULL,
        GeometryShader = 0x00000040ULL,
        FragmentShader = 0x00000080ULL,
        EarlyFragmentTests = 0x00000100ULL,
        LateFragmentTests = 0x00000200ULL,
        ColorAttachmentOutput = 0x00000400ULL,
        ComputeShader = 0x00000800ULL,
        AllTransfer = 0x00001000ULL,
        Transfer = 0x00001000ULL,
        BottomOfPipe = 0x00002000ULL,
        Host = 0x00004000ULL,
        AllGraphics = 0x00008000ULL,
        AllCommands = 0x00010000ULL,
        RayTracingShader = 0x00200000ULL,
        Copy = 0x100000000ULL,
        Resolve = 0x200000000ULL,
        Blit = 0x400000000ULL,
        Clear = 0x800000000ULL,
        IndexInput = 0x1000000000ULL,
        VertexAttributeInput = 0x2000000000ULL,
        PreRasterizationShaders = 0x4000000000ULL,
    };
    DEFINE_ENUM_FLAGS_OR(PipelineStage)

    enum class AccessMask
    {
        None = 0ULL,
        IndirectCommandRead = 0x00000001ULL,
        IndexRead = 0x00000002ULL,
        VertexAttributeRead = 0x00000004ULL,
        UniformRead = 0x00000008ULL,
        InputAttachmentRead = 0x00000010ULL,
        ShaderRead = 0x00000020ULL,
        ShaderWrite = 0x00000040ULL,
        ColorAttachmentRead = 0x00000080ULL,
        ColorAttachmentWrite = 0x00000100ULL,
        DepthStencilAttachmentRead = 0x00000200ULL,
        DepthStencilAttachmentWrite = 0x00000400ULL,
        TransferRead = 0x00000800ULL,
        TransferWrite = 0x00001000ULL,
        HostRead = 0x00002000ULL,
        HostWrite = 0x00004000ULL,
        MemoryRead = 0x00008000ULL,
        MemoryWrite = 0x00010000ULL,
        ShaderSampledRead = 0x100000000ULL,
        ShaderStorageRead = 0x200000000ULL,
        ShaderStorageWrite = 0x400000000ULL,
    };
    DEFINE_ENUM_FLAGS_OR(AccessMask)

    enum class ImageAspect
    {
        Color = 0x00000001,
        Depth = 0x00000002,
        Stencil = 0x00000004,
    };
    DEFINE_ENUM_FLAGS_OR(ImageAspect)

    enum class ImageLayout
    {
        Undefined = 0,
        General = 1,
        ColorAttachmentOptimal = 2,
        DepthStencilAttachmentOptimal = 3,
        DepthStencilReadOnlyOptimal = 4,
        ShaderReadOnlyOptimal = 5,
        TransferSrcOptimal = 6,
        TransferDstOptimal = 7,
        Preinitialized = 8,
        DepthReadOnlyStencilAttachmentOptimal = 1000117000,
        DepthAttachmentStencilReadOnlyOptimal = 1000117001,
        DepthAttachmentOptimal = 1000241000,
        DepthReadOnlyOptimal = 1000241001,
        StencilAttachmentOptimal = 1000241002,
        StencilReadOnlyOptimal = 1000241003,
        ReadOnlyOptimal = 1000314000,
        AttachmentOptimal = 1000314001,
        PresentSrc = 1000001002,
    };

    enum class ImageUsage
    {
        TransferSrc = 0x00000001,
        TransferDst = 0x00000002,
        Sampled = 0x00000004,
        Storage = 0x00000008,
        ColorAttachment = 0x00000010,
        DepthStencilAttachment = 0x00000020,
        TransientAttachment = 0x00000040,
        InputAttachment = 0x00000080,
    };
    DEFINE_ENUM_FLAGS_OR(ImageUsage)

    enum class BufferUsage
    {
        TransferSrc = 0x00000001,
        TransferDst = 0x00000002,
        UniformTexelBuffer = 0x00000004,
        StorageTexelBuffer = 0x00000008,
        UniformBuffer = 0x00000010,
        StorageBuffer = 0x00000020,
        IndexBuffer = 0x00000040,
        VertexBuffer = 0x00000080,
        IndirectBuffer = 0x00000100,
        ShaderDeviceAddress = 0x00020000,
        AccelerationStructureBuildInputReadOnly = 0x00080000,
        AccelerationStructureStorage = 0x00100000,
    };
    DEFINE_ENUM_FLAGS_OR(BufferUsage)

    enum struct MemoryProperty
    {
        DeviceLocal = 0x00000001,
        HostVisible = 0x00000002,
        HostCoherent = 0x00000004,
        HostCached = 0x00000008,
        LazilyAllocated = 0x00000010,
    };
    DEFINE_ENUM_FLAGS_OR(MemoryProperty)

    enum class ShaderStage
    {
        Vertex = 0x00000001,
        Fragment = 0x00000010,
        Compute = 0x00000020,
        RayGen = 0x00000100,
        RayAnyHit = 0x00000200,
        RayClosestHit = 0x00000400,
        RayMiss = 0x00000800,
    };
    DEFINE_ENUM_FLAGS_OR(ShaderStage)

    enum class DescriptorType
    {
        Sampler = 0,
        CombinedImageSampler = 1,
        SampledImage = 2,
        StorageImage = 3,
        UniformTexelBuffer = 4,
        StorageTexelBuffer = 5,
        UniformBuffer = 6,
        StorageBuffer = 7,
        UniformBufferDynamic = 8,
        StorageBufferDynamic = 9,
        InputAttachment = 10,
        InlineUniformBlock = 1000138000,
        AccelerationStructure = 1000150000,
    };

    enum class DescriptorFlag
    {
        None = 0,
        Bindless = 1,
    };

    enum class PrimitiveTopology
    {
        PointList = 0,
        LineList = 1,
        LineStrip = 2,
        TriangleList = 3,
        TriangleStrip = 4,
        TriangleFan = 5,
        LineListWithAdjacency = 6,
        LineStripWithAdjacency = 7,
        TriangleListWithAdjacency = 8,
        TriangleStripWithAdjacency = 9,
        PatchList = 10,
    };

    enum class CullMode
    {
        None = 0,
        Front = 0x00000001,
        Back = 0x00000002,
        FrontAndBack = 0x00000003,
    };

    enum class CompareOperation
    {
        Never = 0,
        Less = 1,
        Equal = 2,
        LessOrEqual = 3,
        Greater = 4,
        NotEqual = 5,
        GreaterOrEqual = 6,
        Always = 7,
    };

    enum class PipelineBindPoint
    {
        Graphics = 0,
        Compute = 1,
        RayTracing = 1000165000,
    };

    enum class AttachmentLoadOp
    {
        Load = 0,
        Clear = 1,
        DontCare = 2,
    };

    enum class AttachmentStoreOp
    {
        Store = 0,
        DontCare = 1,
    };

    enum class Filter
    {
        Nearest = 0,
        Linear = 1
    };

    enum class SamplerAddressMode
    {
        Repeat = 0,
        MirroredRepeat = 1,
        ClampToEdge = 2,
        ClampToBorder = 3,
        MirrorClampToEdge = 4,
    };
}