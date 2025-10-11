// Jai Shree Ram!!!
//
//

GLuint mvpMatrixUniform = 0; // Model-View-Projection Matrix Uniform

mat4 perspectiveProjectionMatrix; // Orthographic Projection Matrix

//  Get the uniformed location for shader

mvpMatrixUniform = glGetUniformLocation(iShaderProgramObject, "uMVPMatrix");

	/* 

	 1. Write the shader source code in a string
	 2. Create a shader object
	 3. Give the shader source code to the shader object 
	 4. Compile the shader object
	 5. Check for compilation errors
	*/

	// Vertex Shader Source Code
	const GLchar *vertexShaderSourceCode =
		"#version 460 core\n" \
		"in vec4 aPosition;\n" \
		"in vec4 aColor;\n" \
		"out vec4 out_color;\n" \
		"uniform mat4 uMVPMatrix;\n" \
		"void main(void)\n" \
		"{\n" \
		"	gl_Position = uMVPMatrix * aPosition;\n" \
		"	out_color = aColor;\n" \
		"}\n";

	
	// Fragment Shader Source Code
	const GLchar *fragmentShaderSourceCode =
		"#version 460 core\n" \
		"in vec4 out_color;\n" \
		"out vec4 FragColor;\n" \
		"void main(void)\n" \
	
		"{\n" \
		"	FragColor = out_color;\n" \
		"}\n";

	if(!buildShaderProgram(vertexShaderSourceCode, fragmentShaderSourceCode))
	{
		return -7;
	}

	// Provide vertex data (Position, Color, Normals, Texture Coordinates etc) to OpenGL
	const GLfloat triangle_position[] = {
		 00.0f,  1.0f, 0.0f,
		-1.0f, -1.0f, 0.0f,
		 1.0f, -1.0f, 0.0f
	};

	const GLfloat triangle_color[] = {
		 1.0f,  0.0f,  0.0f, //R
		 0.0f,  1.0f,  0.0f, //G
		 0.0f,  0.0f,  1.0f  //B
	};

	







