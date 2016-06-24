/*attribute vec4 qt_Vertex;
attribute vec4 qt_MultiTexCoord0;
uniform mat4 qt_ModelViewProjectionMatrix;
*/
out vec4 texCoord;

void main(void)
{
    gl_Position = ftransform();
    texCoord = gl_MultiTexCoord0;
}
