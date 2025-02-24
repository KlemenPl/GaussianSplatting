//
// Created by Klemen Plestenjak on 2/20/25.
//

#include "input.h"

#include <GLFW/glfw3.h>

struct Input {
    struct {
        vec2 cursorPos;
        vec2 scrollMove;
        bool keys[GLFW_KEY_LAST];
        bool btns[GLFW_MOUSE_BUTTON_LAST];
    } prev, curr;
} input;

void eventButton(GLFWwindow *window, int btn, int action, int mods) {
    input.curr.btns[btn] = action != 0;
}
void eventCursor(GLFWwindow *window, double xPos, double yPos) {
    input.curr.cursorPos[0] = (float) xPos;
    input.curr.cursorPos[1] = (float) yPos;
}
void eventScroll(GLFWwindow *window, double xOffset, double yOffset) {
    input.curr.scrollMove[0] = (float) xOffset;
    input.curr.scrollMove[1] = (float) yOffset;
}
void eventKey(GLFWwindow *window, int key, int scancode, int action, int mods) {
    input.curr.keys[key] = action != 0;
}

void inputInit(void *window) {
    GLFWwindow *win = window;
    glfwSetMouseButtonCallback(win, eventButton);
    glfwSetCursorPosCallback(win, eventCursor);
    glfwSetScrollCallback(win, eventScroll);
    glfwSetKeyCallback(win, eventKey);


}

void inputUpdate() {
    input.prev = input.curr;
    input.curr.scrollMove[0] = 0;
    input.curr.scrollMove[1] = 0;
}

bool inputIsKeyDown(int key) {
    return input.curr.keys[key];
}
bool inputIsKeyPressed(int key) {
    return !input.prev.keys[key] && input.curr.keys[key];
}
bool inputIsKeyReleased(int key) {
    return input.prev.keys[key] && !input.curr.keys[key];
}

void inputGetMousePos(vec2 dst) {
    dst[0] = input.curr.cursorPos[0];
    dst[1] = input.curr.cursorPos[1];
}
void inputGetMouseDelta(vec2 dst) {
    dst[0] = input.curr.cursorPos[0] - input.prev.cursorPos[0];
    dst[1] = input.curr.cursorPos[1] - input.prev.cursorPos[1];
}
void inputGetMouseWheelDelta(vec2 dst) {
    dst[0] = input.curr.scrollMove[0];
    dst[1] = input.curr.scrollMove[1];
}

bool inputIsButtonDown(int button) {
    return input.curr.btns[button];
}
bool inputIsButtonPressed(int button) {
    return !input.curr.btns[button] && input.curr.btns[button];
}
bool inputIsButtonReleased(int button) {
    return input.curr.btns[button] && !input.curr.btns[button];
}

