

void freeThyShader(Shader* shader) {
    if (shader) {
        if (shader->id) glDeleteShader(shader->id);
        free(shader);
    }
}

void freeThyShaderProgram(ShaderProgram* program) {
    if (program) {
        if (program->id) glDeleteProgram(program->id);
        for (int i = 0; i < program->shaderCount; i++) {
            freeThyShader(program->shaders[i]);
            program->shaders[i] = NULL;
        }
        program->shaderCount = 0;
        free(program);
    }
}

// function to read shader source from a file
char* readShaderFile(const char* filename)
{
    FILE* file = fopen(filename, "rb"); // Binary mode to avoid newline issues
    if (!file)
    {
        fprintf(gpFile, "Failed to open shader file: %s (Error: %s)\n", filename, strerror(errno));
        return NULL;
    }

    // Get file size
    if (fseek(file, 0, SEEK_END) != 0)
    {
        fprintf(gpFile, "Failed to seek to end of shader file: %s (Error: %s)\n", filename, strerror(errno));
        fclose(file);
        return NULL;
    }
    long fileSize = ftell(file);
    if (fileSize < 0)
    {
        fprintf(gpFile, "Failed to get size of shader file: %s (Error: %s)\n", filename, strerror(errno));
        fclose(file);
        return NULL;
    }
    if (fileSize == 0)
    {
        fprintf(gpFile, "Shader file is empty: %s\n", filename);
        fclose(file);
        return NULL;
    }
    rewind(file); // Reset to start of file

    // Allocate memory for shader source (including null terminator)
    char* shaderSource = (char*)malloc(fileSize + 1);
    if (!shaderSource)
    {
        fprintf(gpFile, "Memory allocation failed for shader file: %s (Size: %ld bytes)\n", filename, fileSize);
        fclose(file);
        return NULL;
    }

    // Read file contents
    size_t bytesRead = fread(shaderSource, 1, fileSize, file);
    shaderSource[bytesRead] = '\0'; // Null-terminate the string

    if (bytesRead != fileSize)
    {
        fprintf(gpFile, "Failed to read entire shader file: %s (Expected: %ld bytes, Read: %zu bytes)\n", 
                filename, fileSize, bytesRead);
        // Log first few characters (if any) to check for content issues
        if (bytesRead > 0)
        {
            fprintf(gpFile, "First %zu bytes of %s: '%.10s'\n", bytesRead, filename, shaderSource);
        }
        free(shaderSource);
        fclose(file);
        return NULL;
    }

    fclose(file);
    fprintf(gpFile, "Successfully read shader file: %s\n", filename);
    
    return shaderSource;
}


GLchar *getShaderTypeName(GLenum SHADER_TYPE)
{
	switch (SHADER_TYPE)
	{
	case GL_VERTEX_SHADER:
		return (GLchar *)"Vertex";
	case GL_FRAGMENT_SHADER:
		return (GLchar *)"Fragment";
	case GL_GEOMETRY_SHADER:
		return (GLchar *)"Geometry";
	case GL_COMPUTE_SHADER:
		return (GLchar *)"Compute";
	case GL_TESS_CONTROL_SHADER:
		return (GLchar *)"Tessellation Control";
	case GL_TESS_EVALUATION_SHADER:
		return (GLchar *)"Tessellation Evaluation";
	default:
		return (GLchar *)"Unknown";
	}
}

Shader* shaderCompile(const GLchar *ShaderSourceCode, GLenum SHADER_TYPE)
{

	/* 

	 1. Write the shader source code in a string
	 2. Create a shader object
	 3. Give the shader source code to the shader object 
	 4. Compile the shader object
	 5. Check for compilation errors
	*/

	GLchar *sShaderName = getShaderTypeName(SHADER_TYPE);
    if (!ShaderSourceCode) {
        fprintf(gpFile, "Error: Shader source is NULL for %s shader\n", sShaderName);
        return NULL;
    }

    Shader* shader = (Shader*)malloc(sizeof(Shader));
    if (!shader) {
        fprintf(gpFile, "Error: Failed to allocate memory for %s shader\n", sShaderName);
        return NULL;
    }
    

	shader->id = glCreateShader(SHADER_TYPE);
    shader->type = SHADER_TYPE;
    shader->name = sShaderName;
	
    glShaderSource(shader->id, 1, (const GLchar**)&ShaderSourceCode, NULL);
	glCompileShader(shader->id);


	GLint iShaderCompileStatus = 0;
	GLint iInfoLogLength = 0;
	GLchar *szInfoLog = NULL;

	// Black Box For Shader Compilation
	glGetShaderiv(shader->id, GL_COMPILE_STATUS, &iShaderCompileStatus);

	if (iShaderCompileStatus == GL_FALSE)
	{
		// If Shader Compilation Failed
		glGetShaderiv(shader->id, GL_INFO_LOG_LENGTH, &iInfoLogLength);
		if (iInfoLogLength > 0)
		{
			szInfoLog = (GLchar *)malloc(iInfoLogLength);
			if (szInfoLog != NULL)
			{
				glGetShaderInfoLog(shader->id, iInfoLogLength, NULL, szInfoLog);
				fprintf(gpFile, "BLACK BOX for %s Shader (Shader Compilation Log) : %s\n", sShaderName, szInfoLog);
				free(szInfoLog);
				szInfoLog = NULL;
			}
			fprintf(gpFile, "++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++\n");
		}
		else
		{
			fprintf(gpFile, "%s Shader Compilation Failed But No Info Log Available\n", sShaderName);
		}
        freeThyShader(shader);
    	return NULL;
	}

	return shader;
}

Bool shaderLink(Shader** shaders, int shaderCount, ShaderProgram* program, const char** attribNames, GLint* attribIndices, int attribCount)
{
    if (!program || !shaders || shaderCount <= 0) {
        fprintf(gpFile,"Error: Invalid parameters for shader linking\n");
        return False;
    }

	/* 

	 1. Create shader program object
	 2. Attach vertex shader object to shader program object
	 3. Attach fragment shader object to shader program object
	 4. Tell OpenGL to link the shaders to shader program object
	 5. Check for linking errors

	*/

	// Create Shader Program Object
    program->id = glCreateProgram();
    program->shaderCount = shaderCount;
    for (int i = 0; i < shaderCount; i++)
    {
        if (shaders[i] && shaders[i]->id) {
            glAttachShader(program->id, shaders[i]->id);
            program->shaders[i] = shaders[i];
        } else {
            fprintf(gpFile, "Warning: Invalid shader at index %d\n", i);
        }
    }

    // Bind attributes (inspired by Unreal's FVertexFactory)
    for (int i = 0; i < attribCount; i++) 
    {
        if (attribNames[i] && attribIndices[i] >= 0) {
            glBindAttribLocation(program->id, attribIndices[i], attribNames[i]);
        } else {
            fprintf(gpFile, "Warning: Invalid attribute at index %d: name=%s, index=%d\n", 
                              i, attribNames[i] ? attribNames[i] : "NULL", attribIndices[i]);
        }
    }
	
	// Link Shader Program Object
	glLinkProgram(program->id);
	GLint iShaderProgramLinkStatus = 0;
	GLint iShaderProgramInfoLogLength = 0;
	GLchar *szShaderProgramInfoLog = NULL;

	// Black Box For Shader Program Linking
	glGetProgramiv(program->id, GL_LINK_STATUS, &iShaderProgramLinkStatus);
	if (iShaderProgramLinkStatus == GL_FALSE)
	{
		// If Shader Program Linking Failed
		glGetProgramiv(program->id, GL_INFO_LOG_LENGTH, &iShaderProgramInfoLogLength);
		if (iShaderProgramInfoLogLength > 0)
		{
			szShaderProgramInfoLog = (GLchar *)malloc(iShaderProgramInfoLogLength);
			if (szShaderProgramInfoLog != NULL)
			{
				glGetProgramInfoLog(program->id, iShaderProgramInfoLogLength, NULL, szShaderProgramInfoLog);
				fprintf(gpFile, "BLACK BOX (Shader Program Linking Log) : %s\n", szShaderProgramInfoLog);
				free(szShaderProgramInfoLog);
				szShaderProgramInfoLog = NULL;
			}
			fprintf(gpFile, "++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++\n");
		}
		else
		{
			fprintf(gpFile, "Shader Program Linking Failed But No Info Log Available\n");
		}
        
        freeThyShaderProgram(program);
        return False;

	}

	else
	{
		fprintf(gpFile, "Shader Program Linked Successfully\n");
	}

    // Detach shaders after linking to avoid keeping them bound
    for (int i = 0; i < shaderCount; i++) {
        if (shaders[i] && shaders[i]->id) {
            glDetachShader(program->id, shaders[i]->id);
        }
    }

    return True;
	
}

Bool buildShaderProgram(const GLchar **SourceCodes, GLenum* shaderTypes, int shaderCount, ShaderProgram** program, const char** attribNames, GLint* attribIndices, int attribCount)
{

    if (!SourceCodes || !shaderTypes || shaderCount <= 0 || !program) {
        fprintf(gpFile, "Error: Invalid parameters for buildShaderProgram\n");
        return False;
    }

    *program = (ShaderProgram*)malloc(sizeof(ShaderProgram));
    if (!*program) {
        fprintf(gpFile, "Error: Failed to allocate memory for ShaderProgram\n");
        return False;
    }

    (*program)->id = 0;
    (*program)->shaderCount = 0;

    Shader* shaders[6]; // Max 6 shader types (adjust as needed)
    for (int i = 0; i < shaderCount && i < 6; i++) {
        shaders[i] = shaderCompile(SourceCodes[i], shaderTypes[i]);
        if (!shaders[i]) {
            fprintf(gpFile, "Error: Failed to compile %s shader\n", getShaderTypeName(shaderTypes[i]));
            
            freeThyShaderProgram(*program);
            return False;
        }
    }

    if (!shaderLink(shaders, shaderCount, *program, attribNames, attribIndices, attribCount)) {
        freeThyShaderProgram(*program);
        return False;
    }

    return True;
}


// Utility to use a shader program
void useShaderProgram(ShaderProgram* program) {
    if (program && program->id) {
        glUseProgram(program->id);
    } else {
        fprintf(gpFile, "Warning: Attempted to use invalid shader program\n");
        glUseProgram(0);
    }
}

// Utility to get uniform location
GLint getUniformLocation(ShaderProgram* program, const char* name) {
    if (!program || !program->id || !name) {
        fprintf(gpFile, "Error: Invalid parameters for getUniformLocation\n");
        return -1;
    }
    GLint location = glGetUniformLocation(program->id, name);
    if (location == -1) {
        fprintf(gpFile, "Warning: Uniform '%s' not found in program\n", name);
    }
    return location;
}
