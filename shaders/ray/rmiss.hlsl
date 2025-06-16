struct MyPayload {
    float4 color;
    float hitDistance;
};

[shader("miss")]

void main(inout MyPayload payload : SV_RayPayload)
{
    payload.color = float4(0.0, 1.0, 0.0, 1.0); // Red
}
