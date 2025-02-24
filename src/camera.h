//
// Created by Klemen Plestenjak on 2/19/25.
//

#ifndef CAMERA_H
#define CAMERA_H

#include <cglm/cglm.h>

typedef struct OrbitCamera {
    vec3 pos;
    vec3 center;
    vec3 up;

    float distance;
    float yaw;
    float pitch;

    float fov;
    float aspect;
    float near;
    float far;

    mat4 view;
    mat4 proj;
    mat4 viewProj;
} OrbitCamera;

static const OrbitCamera ORBIT_CAMERA_DEFAULT = {
    .pos = {0.0f, 0.0f, 0.0f},
    .center = {0.0f, 0.0f, 0.0f},
    .up = {0.0f, 1.0f, 0.0f},

    .distance = 5.0f,
    .yaw = 0.0f,
    .pitch = 0.0f,

    .fov = 45.0f * GLM_PIf / 180.0f,
    .aspect = 16.0f / 9.0f,
    .near = 0.1f,
    .far = 100.0f,
};

static inline void orbitCameraUpdate(OrbitCamera *camera) {
    camera->pos[0] = camera->distance * cosf(camera->pitch) * cosf(camera->yaw);
    camera->pos[1] = camera->distance * sinf(camera->pitch);
    camera->pos[2] = camera->distance * cosf(camera->pitch) * sinf(camera->yaw);

    glm_lookat(camera->pos, camera->center, camera->up, camera->view);
    glm_perspective(camera->fov, camera->aspect, camera->near, camera->far, camera->proj);
    glm_mat4_mul(camera->proj, camera->view, camera->viewProj);
}

static inline void orbitCameraMove(OrbitCamera *camera, float dx, float dy) {
    float sensitivity = 0.01f;
    camera->yaw += dx * sensitivity;
    camera->pitch = glm_clamp(camera->pitch + dy * sensitivity, -GLM_PI_2 + 0.1f, GLM_PI_2 - 0.1f);
}

#endif //CAMERA_H
