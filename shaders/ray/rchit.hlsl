#include "common.hlsli"

[shader("closesthit")]
void main(inout MyPayload payload, in BuiltInTriangleIntersectionAttributes attr)
{
    payload.hitDistance = RayTCurrent(); // RayTCurrent() gives distance
    payload.color = float4(payload.hitDistance * 0.1, 0.0, 0.0, 1.0);
}
