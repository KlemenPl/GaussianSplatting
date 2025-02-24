//
// Created by Klemen Plestenjak on 2/19/25.
//

#ifndef CAMERA_H
#define CAMERA_H

#include <cglm/cglm.h>

typedef struct ArcballCamera {
    vec3 pos;
    vec3 center;
    vec3 up;

    float distance;

    // Quaternion
    versor rotation;

    // For mouse to sphere mapping
    float radius;

    float fov;
    float aspect;
    float near;
    float far;

    mat4 view;
    mat4 proj;
    mat4 viewProj;
} ArcballCamera;

static const ArcballCamera CAMERA_ARCBALL_DEFAULT = {
    .pos = {0.0f, 0.0f, 0.0f},
    .center = {0.0f, 0.0f, 0.0f},
    .up = {0.0f, -1.0f, 0.0f},

    .distance = 5.0f,
    .rotation = GLM_QUAT_IDENTITY_INIT,

    .radius = 0.8f,  // Normalized screen size

    .fov = 45.0f * GLM_PIf / 180.0f,
    .aspect = 16.0f / 9.0f,
    .near = 0.1f,
    .far = 100.0f,
};


static inline void arcballCameraUpdate(ArcballCamera *camera) {
    mat4 rotMat;
    glm_quat_mat4(camera->rotation, rotMat);

    vec3 offset = {0.0f, 0.0f, camera->distance};
    vec3 rotatedOffset;

    glm_mat4_mulv3(rotMat, offset, 1.0f, rotatedOffset);

    glm_vec3_add(camera->center, rotatedOffset, camera->pos);
    glm_lookat(camera->pos, camera->center, camera->up, camera->view);
    glm_perspective(camera->fov, camera->aspect, camera->near, camera->far, camera->proj);
    glm_mat4_mul(camera->proj, camera->view, camera->viewProj);
}

// Rotate the camera based on two points on the sphere
static inline void arcballCameraRotate(ArcballCamera *camera, vec2 delta) {
    float sensitivity = 0.01f;

    // Create rotation quaternions for x and y movements
    versor xRotation, yRotation;

    // Rotate around local up vector (typically Y-axis) for horizontal mouse movement
    glm_quatv(xRotation, -delta[0] * sensitivity, camera->up);

    // Calculate right vector for vertical rotation
    vec3 forward, right;
    glm_vec3_sub(camera->center, camera->pos, forward);
    glm_vec3_normalize(forward);
    glm_vec3_cross(camera->up, forward, right);
    glm_vec3_normalize(right);

    // Rotate around right vector for vertical mouse movement
    glm_quatv(yRotation, -delta[1] * sensitivity, right);

    // Combine rotations
    versor tempRotation;
    glm_quat_mul(xRotation, camera->rotation, tempRotation);
    glm_quat_mul(yRotation, tempRotation, camera->rotation);

    // Normalize quaternion to prevent drift
    glm_quat_normalize(camera->rotation);
}

static inline void arcballCameraZoom(ArcballCamera *camera, float amount) {
    camera->distance = glm_max(camera->distance + amount, 0.1f);
}

static inline void arcballCameraReset(ArcballCamera *camera) {
    glm_quat_identity(camera->rotation);
    camera->distance = CAMERA_ARCBALL_DEFAULT.distance;
}

#endif //CAMERA_H
