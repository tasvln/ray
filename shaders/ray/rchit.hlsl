struct MyPayload {
    float3 color;
};

struct Attributes {
    float2 barycentrics;
};

[shader("closesthit")]
void main(inout MyPayload payload, in Attributes attr)
{
    payload.color = float3(1.0, 0.0, 0.0); // red hit
}
