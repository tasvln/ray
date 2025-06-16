struct MyPayload {
    float4 color;
    float hitDistance;
};

[shader("closesthit")]
void main(inout MyPayload payload : SV_RayPayload, in BuiltInTriangleIntersectionAttributes attr)
{
    // Set the color if we hit something
    payload.color = float4(1.0, 0.0, 0.0, 1.0); // Green
    // payload.color = float4(1.0, 0.0, 0.0, 1.0);
}
