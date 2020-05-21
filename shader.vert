#version 140

in vec2 LVertexPos2D;
out vec2 VertPos;

void main() {
    gl_Position = vec4( LVertexPos2D.x, LVertexPos2D.y, 0, 1 );
    VertPos = gl_Position.xy;
}