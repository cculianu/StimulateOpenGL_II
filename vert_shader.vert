out vec4 texCoord;

void main(void)
{
    gl_Position = ftransform();
    texCoord = gl_MultiTexCoord0;
}
