void getFrustum();
bool isPointInside(vec3 point, Plane plane);
bool isBoxCompletelyOutsidePlane(boundingRect bound, Plane plane);
bool isChunkVisible(boundingRect bound, cullfrustum frustum);
