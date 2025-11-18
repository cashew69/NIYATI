cullfrustum extractFrustum(const mat4& vp) {
    cullfrustum frustum;
    
    // Extract planes using Gribb-Hartmann method
    // Note: vp[col][row] because vmath uses column-major ordering
    
    // Left plane: row3 + row0
    frustum.planes[0].normal[0] = vp[0][3] + vp[0][0];
    frustum.planes[0].normal[1] = vp[1][3] + vp[1][0];
    frustum.planes[0].normal[2] = vp[2][3] + vp[2][0];
    frustum.planes[0].distance = vp[3][3] + vp[3][0];
    
    // Right plane: row3 - row0
    frustum.planes[1].normal[0] = vp[0][3] - vp[0][0];
    frustum.planes[1].normal[1] = vp[1][3] - vp[1][0];
    frustum.planes[1].normal[2] = vp[2][3] - vp[2][0];
    frustum.planes[1].distance = vp[3][3] - vp[3][0];
    
    // Bottom plane: row3 + row1
    frustum.planes[2].normal[0] = vp[0][3] + vp[0][1];
    frustum.planes[2].normal[1] = vp[1][3] + vp[1][1];
    frustum.planes[2].normal[2] = vp[2][3] + vp[2][1];
    frustum.planes[2].distance = vp[3][3] + vp[3][1];
    
    // Top plane: row3 - row1
    frustum.planes[3].normal[0] = vp[0][3] - vp[0][1];
    frustum.planes[3].normal[1] = vp[1][3] - vp[1][1];
    frustum.planes[3].normal[2] = vp[2][3] - vp[2][1];
    frustum.planes[3].distance = vp[3][3] - vp[3][1];
    
    // Near plane: row3 + row2
    frustum.planes[4].normal[0] = vp[0][3] + vp[0][2];
    frustum.planes[4].normal[1] = vp[1][3] + vp[1][2];
    frustum.planes[4].normal[2] = vp[2][3] + vp[2][2];
    frustum.planes[4].distance = vp[3][3] + vp[3][2];
    
    // Far plane: row3 - row2
    frustum.planes[5].normal[0] = vp[0][3] - vp[0][2];
    frustum.planes[5].normal[1] = vp[1][3] - vp[1][2];
    frustum.planes[5].normal[2] = vp[2][3] - vp[2][2];
    frustum.planes[5].distance = vp[3][3] - vp[3][2];
    
    // Normalize all planes
    for (int i = 0; i < 6; i++) {
        float length = sqrt(frustum.planes[i].normal[0] * frustum.planes[i].normal[0] +
                           frustum.planes[i].normal[1] * frustum.planes[i].normal[1] +
                           frustum.planes[i].normal[2] * frustum.planes[i].normal[2]);
        frustum.planes[i].normal[0] /= length;
        frustum.planes[i].normal[1] /= length;
        frustum.planes[i].normal[2] /= length;
        frustum.planes[i].distance /= length;
    }
    
    return frustum;
}

void getcullfrustum()
{
    mat4 vp = perspectiveProjectionMatrix * viewMatrix;
                                                              
    viewFrustum = extractFrustum(vp);
}

bool isPointInside(vec3 point, Plane plane)
{
    // Plane equation: normal.x * x + normal.y * y + normal.z * z + distance
    float dotProduct = plane.normal[0] * point[0] + 
                      plane.normal[1] * point[1] + 
                      plane.normal[2] * point[2] + 
                      plane.distance;
    
    return (dotProduct >= 0);  // Positive or zero means inside
}

bool isBoxCompletelyOutsidePlane(boundingRect bound, Plane plane)
{
    // Test all 4 corners of the bounding rectangle
    // If ALL corners are outside the plane, the box is completely outside
    
    vec3 corners[4] = {
        bound.ox,  // bottom-left
        bound.ex,  // bottom-right
        bound.ez,  // top-right
        bound.oz   // top-left
    };
    
    // Check if all corners are on the negative side of the plane
    int outsideCount = 0;
    for (int i = 0; i < 4; i++) {
        float dotProduct = plane.normal[0] * corners[i][0] + 
                          plane.normal[1] * corners[i][1] + 
                          plane.normal[2] * corners[i][2] + 
                          plane.distance;
        
        if (dotProduct < 0) {
            outsideCount++;
        }
    }
    
    // If all 4 corners are outside, the box is completely outside this plane
    return (outsideCount == 4);
}

bool isChunkVisible(boundingRect bound, cullfrustum frustum)
{
    // A chunk is visible if it's NOT completely outside ANY of the 6 frustum planes
    // If the chunk is completely outside even one plane, it's not visible
    
    for (int i = 0; i < 6; i++) {
        if (isBoxCompletelyOutsidePlane(bound, frustum.planes[i])) {
            return false;  // Completely outside this plane, so not visible
        }
    }
    
    // Not completely outside any plane, so it's at least partially visible
    return true;
}
