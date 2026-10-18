#version 150

in vec2 uv; 
out vec4 c; 
uniform sampler2D textureImage;

void main()
{

 c = texture(textureImage, uv);
} 
