#include "common.hlsli"

StructuredBuffer<Material> materials : register(t2); // material buffer

[shader("closesthit")]
void main(inout MyPayload payload, in BuiltInTriangleIntersectionAttributes attr)
{
    uint instanceID = InstanceID();
    Material mat = materials[instanceID];

    switch (mat.type) {
        case MATERIAL_LAMBERT: {
            payload.color = float4(mat.albedo.rgb, 1.0);
            break;
        }
        case MATERIAL_MIRROR: {
            payload.color = float4(1.0, 1.0, 1.0, 1.0);
            // Reflect and recurse
            break;
        }
        case MATERIAL_METAL: {
            float roughness = mat.rmix.x;
            float metallic = mat.rmix.y;
            // Reflect with fuzz and recurse
            break;
        }
        case MATERIAL_DIELECTRIC: {
            float ior = mat.rmix.z;
            // Refract/reflect based on angle
            break;
        }
        case MATERIAL_EMISSIVE: {
            payload.color += mat.emission; // or mat.emission if added
            break;
        }
    }
}

