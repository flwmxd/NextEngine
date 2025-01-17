layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoords;
layout (location = 3) in vec3 aTangent;
layout (location = 4) in vec3 aBitangent;

out vec2 TexCoords;

uniform mat4 view;
uniform mat4 projection;

void main()
{
    TexCoords = vec2(aTexCoords.x, aTexCoords.y);
    gl_Position = vec4(aPos, 1.0);
}