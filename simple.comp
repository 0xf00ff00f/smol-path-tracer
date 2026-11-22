#version 450

#define BUFFER_SIZE 50
#define WORKGROUP_SIZE 32

layout(local_size_x = WORKGROUP_SIZE, local_size_y = 1, local_size_z = 1) in;

layout(std430, binding = 0) buffer inputs
{
    float inData[];
};

layout(std430, binding = 1) buffer result
{
    float outData[];
};

void main()
{
    uint id = gl_GlobalInvocationID.x;
    if (id >= BUFFER_SIZE)
        return;
    outData[id] = inData[id] * inData[id];
}
