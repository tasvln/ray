struct MyPayload {
    float4 color;
    float hitDistance;
};

[shader("miss")]

void main(inout MyPayload payload)
{
    payload.color = float4(0.0, 0.0, 0.0, 1.0); // Green
}
