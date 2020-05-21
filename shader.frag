#version 140

uniform float x_rescale;
uniform float x_offset;
uniform float y_rescale;
uniform float y_offset;
uniform int num_iterations;

in vec2 VertPos;
out vec4 LFragment;

void main() {
    float x0 = x_rescale*VertPos.x + x_offset;
    float y0 = y_rescale*VertPos.y + y_offset;
    float x = 0.0;
    float y = 0.0;
    float x2 = 0.0;
    float y2 = 0.0;

    int iter = 0;
    while (iter < num_iterations && x2+y2 <= 4.0) {
      y = 2*x*y + y0;
      x = x2-y2 + x0;
      x2 = x*x;
      y2 = y*y;
      iter++;
    }

    float brightness = iter == num_iterations ? 0.0 : float(iter)/float(num_iterations);

    LFragment = vec4(brightness, brightness, brightness, 1.0);
}
