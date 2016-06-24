/*attribute vec4 qt_Vertex;
attribute vec4 qt_MultiTexCoord0;
uniform mat4 qt_ModelViewProjectionMatrix;
*/
out vec4 myCoord;

void main(void)
{
    gl_Position = ftransform();
    myCoord = gl_MultiTexCoord0;
}
