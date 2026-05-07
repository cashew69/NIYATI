
GLuint transmittanceLut;

void createLUT(GLuint* texture)
{
    glGenTextures(1, texture);
    glBindTexture(GL_TEXTURE_2D, *transmittanceLUT);

    // Allocate the 16-bit float texture (256 width, 64 height).
    // Passing NULL because we are leaving it blank for the GPU to fill later.
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, 256, 64, 0, GL_RGBA, GL_FLOAT, NULL);

    // Filtering must be linear so the sky blends smoothly
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    // Clamping is CRITICAL. We don't want the bottom of the atmosphere
    // math to wrap around to the top of space!
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

}

void skymodelInit(){
    createLUT(&transmittanceLUT);
}
void skymodelRender();
void skymodelUpdate();
