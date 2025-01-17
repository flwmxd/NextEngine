layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoords;
layout (location = 3) in vec3 aTangent;
layout (location = 4) in vec3 aBitangent;
layout (location = 5) in mat4 model;

layout (location = 0) out vec2 TexCoords;

void main()
{
    gl_Position = vec4(aPos, 1.0);
    TexCoords = vec2(aTexCoords.x, 1.0 - aTexCoords.y);
}