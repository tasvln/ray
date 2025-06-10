struct MyPayload {
    float3 color;
};

[shader("miss")]
void main(inout MyPayload payload)
{
    payload.color = float3(0.2, 0.2, 0.2); // default color
}
