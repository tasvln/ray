// struct MyPayload {
//     float4 color;
//     float hitDistance;
// };

// RaytracingAccelerationStructure SceneBVH : register(t0);
// RWTexture2D<float4> outputImage : register(u1); // Image output

// [shader("raygeneration")]
// void main()
// {
//     // current pixel
//     uint2 pixel = DispatchRaysIndex().xy;

//     // Ray origin and direction (camera logic comes later)
//     RayDesc ray;
//     ray.Origin = float3(0.0, 0.0, -5.0);
//     ray.Direction = float3(0.0, 0.0, 1.0);
//     ray.TMin = 0.001;
//     ray.TMax = 10000.0;

//     // the result -> usually a struct with the color, hioy distance etc
//     MyPayload payload;

//     // launch ray to the scene
//     TraceRay(SceneBVH,         // Acceleration structure (TLAS)
//              RAY_FLAG_NONE,
//              0xFF,             // instance mask
//              0,                // hit group index
//              1,                // hit group stride
//              0,                // miss shader index
//              ray,
//              payload);         // Output payload

//     outputImage[pixel] = payload.color;
// }

struct MyPayload {
    float4 color;
    float hitDistance;
};

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
    ray.Origin = float3(0.0, 0.0, -2.0);
    ray.Direction = float3(0.0, 0.0, 1.0);
    ray.TMin = 0.001;
    ray.TMax = 1000.0;

    MyPayload payload;
    payload.color = float4(0.0, 0.0, 0.0, 1.0);

    // TraceRay(Scene, RAY_FLAG_NONE, 0xFF, 0, 1, 0, ray, payload);
    TraceRay(Scene, RAY_FLAG_NONE, 0xFF, 2, 3, 1, ray, payload);
    outputImage[launchIndex] = payload.color;
}

