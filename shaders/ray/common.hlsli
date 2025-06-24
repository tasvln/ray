#ifndef COMMON_HLSLI
#define COMMON_HLSLI

#define MATERIAL_LAMBERT 0 // diffuse
#define MATERIAL_MIRROR  1 // mirror
#define MATERIAL_METAL   2 // PBR shading or metal vs non-metal logic
#define MATERIAL_DIELECTRIC 3 // IOR glass, water
#define MATERIAL_EMISSIVE   4 // Lights

struct MyPayload {
    float4 color;
    float hitDistance;
};

struct Material {
    float4 albedo; // -> base color
    float4 emission; // -> light emitted
    float4 rmix; // x = roughness, y = metallic, z = ior -> index of refraction, w = unused
    uint type;
};

#endif // COMMON_HLSLI