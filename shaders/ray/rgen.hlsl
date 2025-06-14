// Bind a storage image for output (binding 0, set 0)
RWTexture2D<float4> outputImage : register(u0);

[shader("raygeneration")]
void main()
{
    // Get pixel coordinates from launch ID
    uint2 pixel = DispatchRaysIndex().xy;

    // Write a simple solid color (red) for testing
    outputImage[pixel] = float4(1.0, 0.0, 0.0, 1.0);
}