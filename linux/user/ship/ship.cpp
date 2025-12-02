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

// rotate the MESH to align (Nose = -Z).
mat4 getMeshCorrectionMatrix()
{
    // Rotate 90 degrees around Y to turn Nose from +X to -Z
    mat4 fixYaw = rotate(90.0f, 0.0f, 1.0f, 0.0f);
    
    // Rotate -90 degrees around Z to fix the "Clockwise Tilt"
    mat4 fixRoll = rotate(90.0f, 0.0f, 0.0f, 1.0f);

    return fixRoll * fixYaw;
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
