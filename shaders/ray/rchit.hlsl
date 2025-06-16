struct MyPayload {
    float4 color;
    float hitDistance;
};

[shader("closesthit")]
void main(inout MyPayload payload, in BuiltInTriangleIntersectionAttributes attr)
{
    // Set the color if we hit something
    payload.color = float4(1.0, 0.0, 0.0, 1.0); // Red
    // payload.color = float4(1.0, 0.0, 0.0, 1.0);
}
