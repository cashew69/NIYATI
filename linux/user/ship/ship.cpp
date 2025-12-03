float shipSpeed = 0.0f;
const float SHIP_ACCELERATION = 0.1f;
const float SHIP_MAX_SPEED = 25.0f;
const float SHIP_DRAG = 0.98f;
const float SHIP_TURN_SPEED = 0.5f;

// Camera Settings
const float CAM_OFFSET_DISTANCE = 28.0f; // Distance behind ship
const float CAM_OFFSET_HEIGHT = 0.0f;    // Height above ship
                                         //
float smoothFactor = 0.5f; 

// Initial Orientation: Identity (No rotation, pointing -Z in logical space)
quaternion shipOrientation = quaternion(0.0f, 0.0f, 0.0f, 1.0f);

// Propulsion Globals
Mesh* propulsionMeshes = NULL;
int propulsionMeshCount = 0;
ShaderProgram* propulsionShader = NULL;
GLuint noiseTexture = 0;

vec3 propulsionOffset = vec3(0.5f, 0.37f, 2.7f);
vec3 propulsionScale = vec3(0.2f, 0.2f, 1.0f);
vec3 propulsionRotation = vec3(0.0f, 0.0f, 0.0f);

// rotate the MESH to align (Nose = -Z).
mat4 getMeshCorrectionMatrix()
{
    // Rotate 90 degrees around Y to turn Nose from +X to -Z
    mat4 fixYaw = rotate(90.0f, 0.0f, 1.0f, 0.0f);
    
    // Rotate -90 degrees around Z to fix the "Clockwise Tilt"
    mat4 fixRoll = rotate(90.0f, 0.0f, 0.0f, 1.0f);

    return fixRoll * fixYaw;
}

extern const char* attribNames[4];
extern GLint attribIndices[4];

quaternion eulerToQuaternion(float pitch, float yaw, float roll)
{
    float p = radians(pitch) * 0.5f;
    float y = radians(yaw) * 0.5f;
    float r = radians(roll) * 0.5f;

    float sp = sin(p);
    float cp = cos(p);
    float sy = sin(y);
    float cy = cos(y);
    float sr = sin(r);
    float cr = cos(r);

    quaternion q;
    q[0] = sr * cp * cy - cr * sp * sy; // x
    q[1] = cr * sp * cy + sr * cp * sy; // y
    q[2] = cr * cp * sy - sr * sp * cy; // z
    q[3] = cr * cp * cy + sr * sp * sy; // w
    return q;
}

void initPropulsion()
{
    // Load Model
    if (!loadModel("user/models/propulsion.fbx", &propulsionMeshes, &propulsionMeshCount, 1.0f))
    {
        fprintf(gpFile, "Failed to load propulsion model\n");
    }

    // Load Texture
    loadPNGTexture(&noiseTexture, const_cast<char*>("user/ship/Jet_Propulsion_with_texture/noiseTexture.png"), 0, 1);

    // Create Shader
    const char* propulsionShaderFiles[5] = {
        "user/ship/propulsion_vs.glsl",
        NULL,
        NULL,
        NULL,
        "user/ship/Jet_Propulsion_with_texture/code.glsl"
    };
    
    if (!buildShaderProgramFromFiles(propulsionShaderFiles, 5, 
                &propulsionShader, attribNames, attribIndices, 4))
    {
        fprintf(gpFile, "Failed to build propulsion shader program\n");
    }
}

void renderPropulsion()
{
    if (!propulsionMeshes || propulsionMeshCount < 1 || !propulsionShader) return;

    
    glUseProgram(propulsionShader->id);

    // Uniforms
    GLint loc;
    loc = glGetUniformLocation(propulsionShader->id, "u_time");
    if (loc != -1) glUniform1f(loc, (float)glfwGetTime());

    loc = glGetUniformLocation(propulsionShader->id, "u_speed");
    if (loc != -1) glUniform1f(loc, 1.0f); // Default speed

    loc = glGetUniformLocation(propulsionShader->id, "u_textures[0]");
    if (loc != -1) glUniform1i(loc, 0);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, noiseTexture);

    // Matrices
    loc = glGetUniformLocation(propulsionShader->id, "uView");
    if (loc != -1) glUniformMatrix4fv(loc, 1, GL_FALSE, viewMatrix);

    loc = glGetUniformLocation(propulsionShader->id, "uProjection");
    if (loc != -1) glUniformMatrix4fv(loc, 1, GL_FALSE, perspectiveProjectionMatrix);

    // Render 2 engines
    for (int i = 0; i < 2; i++)
    {
        if (propulsionMeshes[0].transform == NULL) {
            propulsionMeshes[0].transform = createTransform();
        }

        vec3 finalOffset = propulsionOffset;
        if (i == 1) finalOffset[0] = -finalOffset[0]; // Mirror X for second engine

        mat4 shipRotMat = shipOrientation.asMatrix();
        vec4 tempOffset = vec4(finalOffset, 1.0f) * shipRotMat.transpose();
        vec3 worldOffset = vec3(tempOffset[0], tempOffset[1], tempOffset[2]);
        
        vec3 enginePos = shipPosition + worldOffset;

        setPosition(propulsionMeshes[0].transform, enginePos);
        
        // Combine ship orientation with local rotation
        quaternion localRot = eulerToQuaternion(propulsionRotation[0], propulsionRotation[1], propulsionRotation[2]);
        setRotation(propulsionMeshes[0].transform, shipOrientation * localRot);
        setScale(propulsionMeshes[0].transform, propulsionScale);

        mat4 modelMatrix = getWorldMatrix(propulsionMeshes[0].transform);
        
        
        loc = glGetUniformLocation(propulsionShader->id, "uModel");
        if (loc != -1) glUniformMatrix4fv(loc, 1, GL_FALSE, modelMatrix);

        glBindVertexArray(propulsionMeshes[0].vao);
        glDrawElements(GL_TRIANGLES, propulsionMeshes[0].indexCount, GL_UNSIGNED_INT, NULL);
        glBindVertexArray(0);
    }
    
    glUseProgram(0);
}

void renderShipUI()
{
    ImGui::Begin("Ship Controls");
    ImGui::Text("Propulsion Settings");
    ImGui::DragFloat3("Offset", &propulsionOffset[0], 0.1f);
    ImGui::DragFloat3("Scale", &propulsionScale[0], 0.1f);
    ImGui::DragFloat3("Rotation", &propulsionRotation[0], 1.0f);
    ImGui::End();
}

void renderShip()
{
    if (!sceneMeshes || meshCount < 1) return;
    
    if (sceneMeshes[0].transform == NULL) {
        sceneMeshes[0].transform = createTransform();
    }
    
    setPosition(sceneMeshes[0].transform, shipPosition);

    setRotation(sceneMeshes[0].transform, shipOrientation);
    
    mat4 logicMatrix = getWorldMatrix(sceneMeshes[0].transform);

    mat4 finalModelMatrix = logicMatrix * getMeshCorrectionMatrix();
    
    modelLocUniform = getUniformLocation(mainShaderProgram, "uModel");
    if (modelLocUniform != -1) {
        glUniformMatrix4fv(modelLocUniform, 1, GL_FALSE, finalModelMatrix);
    }
    setMaterialUniforms(mainShaderProgram, &sceneMeshes[0].material);

    glBindVertexArray(sceneMeshes[0].vao);
    glDrawElements(GL_TRIANGLES, sceneMeshes[0].indexCount, GL_UNSIGNED_INT, NULL);
    glBindVertexArray(0);

    
}

void handleShipInput(int key, bool isKeyDown)
{
    if (!isKeyDown) return;

    float rollInput = 0.0f;
    float pitchInput = 0.0f;
    float yawInput = 0.0f;

    // Acceleration
    if (key == 'W' || key == 'w') {
        shipSpeed += SHIP_ACCELERATION;
        if (shipSpeed > SHIP_MAX_SPEED) shipSpeed = SHIP_MAX_SPEED;
    }
    if (key == 'S' || key == 's') {
        shipSpeed -= SHIP_ACCELERATION;
        if (shipSpeed < -SHIP_MAX_SPEED / 2.0f) shipSpeed = -SHIP_MAX_SPEED / 2.0f;
    }

    // Turning Inputs
    if (key == 'A' || key == 'a') rollInput = -SHIP_TURN_SPEED;
    if (key == 'D' || key == 'd') rollInput = SHIP_TURN_SPEED;
    
    if (key == 'J' || key == 'j') pitchInput = SHIP_TURN_SPEED;
    if (key == 'K' || key == 'k') pitchInput = -SHIP_TURN_SPEED;
    
    if (key == 'H' || key == 'h') yawInput = -SHIP_TURN_SPEED;
    if (key == 'L' || key == 'l') yawInput = SHIP_TURN_SPEED;

    // Apply Rotation to Logic Quaternion
    if (rollInput != 0.0f) {
        float angle = radians(rollInput) / 2.0f;
        quaternion delta = quaternion(0.0f, 0.0f, sin(angle), cos(angle));
        shipOrientation =  delta * shipOrientation; 
    }
    
    if (pitchInput != 0.0f) {
        float angle = radians(pitchInput) / 2.0f;
        quaternion delta = quaternion(sin(angle), 0.0f, 0.0f, cos(angle));
        shipOrientation =  delta * shipOrientation; 
    }
    
    if (yawInput != 0.0f) {
        float angle = radians(yawInput) / 2.0f;
        quaternion delta = quaternion(0.0f, sin(angle), 0.0f, cos(angle));

        shipOrientation =  delta * shipOrientation; 
    }
    
    shipOrientation = normalize(shipOrientation);
}

void shipUpdate()
{
    shipSpeed *= SHIP_DRAG;

    mat4 rotationMatrix = shipOrientation.asMatrix();

    vec3 forwardDir;
    forwardDir[0] = -rotationMatrix[2][0];
    forwardDir[1] = -rotationMatrix[2][1];
    forwardDir[2] = -rotationMatrix[2][2];
    
    shipPosition = shipPosition + forwardDir * shipSpeed * 0.05f;
}

void shipCam(float pOffSetX, float pOffSetY, float pOffSetZ)
{

    mat4 rotationMatrix = shipOrientation.asMatrix();
    vec3 shipUp    = vec3(rotationMatrix[1][0], rotationMatrix[1][1], rotationMatrix[1][2]);
    vec3 shipBack  = vec3(rotationMatrix[2][0], rotationMatrix[2][1], rotationMatrix[2][2]);

    vec3 desiredOffset = (shipBack * CAM_OFFSET_DISTANCE) + (shipUp * CAM_OFFSET_HEIGHT);
    vec3 desiredPos = shipPosition + desiredOffset;


    
    // Lerp 
    camera_pos = camera_pos + (desiredPos - camera_pos) * smoothFactor;

    camera_target = shipPosition;
    
    camera_orientation = shipOrientation;
    use_camera_quaternion = true;
}
