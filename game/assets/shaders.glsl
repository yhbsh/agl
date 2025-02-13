#ifdef VERTEX

layout(location = 0) in vec2 aPos;
layout(location = 1) in vec2 aTexCoord;

uniform int angleX;
uniform int angleY;
uniform int angleZ;

out vec2 TexCoord;

void main() {
    TexCoord = aTexCoord;

    float radiansX = radians(float(angleX));
    float radiansY = radians(float(angleY));
    float radiansZ = radians(float(angleZ));

    float cosX = cos(radiansX);
    float sinX = sin(radiansX);
    float cosY = cos(radiansY);
    float sinY = sin(radiansY);
    float cosZ = cos(radiansZ);
    float sinZ = sin(radiansZ);

    mat4 rotationX = mat4(
        +1.00, +0.00, +0.00, +0.00,
        +0.00, +cosX, -sinX, +0.00,
        +0.00, +sinX, +cosX, +0.00,
        +0.00, +0.00, +0.00, +1.00
    );

    mat4 rotationY = mat4(
        +cosY, +0.00, +sinY, +0.0,
        +0.00, +1.00, +0.00, +0.0,
        -sinY, +0.00, +cosY, +0.0,
        +0.00, +0.00, +0.00, +1.0
    );

    mat4 rotationZ = mat4(
        +cosZ, -sinZ, +0.00, +0.00,
        +sinZ, +cosZ, +0.00, +0.00,
        +0.00, +0.00, +1.00, +0.00,
        +0.00, +0.00, +0.00, +1.00
    );

    gl_Position = rotationZ * rotationY * rotationX * vec4(aPos, 0.0, 1.0);
}

#endif // VERTEX


// Fragment Shader
#ifdef FRAGMENT

precision mediump float;

uniform sampler2D Texture;

in vec2 TexCoord;
out vec4 FragColor;

void main() {
    FragColor = texture(Texture, TexCoord);
}

#endif // FRAGMENT
