#include "common.hlsli"

[shader("miss")]

void main(inout MyPayload payload) {
    float t = DispatchRaysIndex().y / (float)DispatchRaysDimensions().y;
    payload.color = float4(lerp(float3(0.5, 0.7, 1.0), float3(1.0, 1.0, 1.0), t), 1.0);
}
