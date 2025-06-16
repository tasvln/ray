#include "common.hlsli"

RaytracingAccelerationStructure Scene : register(t0);
RWTexture2D<float4> outputImage : register(u1); // Image output

[shader("raygeneration")]
void main()
{
    uint2 launchIndex = DispatchRaysIndex().xy;
    uint2 launchDims  = DispatchRaysDimensions().xy;

    float2 uv = (float2(launchIndex) + 0.5) / float2(launchDims);
    float3 origin = float3(0.0, 0.0, -2.0);
    float3 target = float3((uv * 2.0 - 1.0), 0.0); // place image plane at z = 0
    float3 direction = normalize(target - origin);

    RayDesc ray;
    ray.Origin = origin;
    ray.Direction = direction;
    ray.TMin = 0.001;
    ray.TMax = 1000.0;

    MyPayload payload;
    payload.color = float4(0.0, 0.0, 0.0, 1.0);

    // TraceRay(Scene, RAY_FLAG_NONE, 0xFF, 0, 1, 0, ray, payload);
    TraceRay(Scene, RAY_FLAG_NONE, 0xFF, 2, 1, 1, ray, payload);
    outputImage[launchIndex] = payload.color;
}

