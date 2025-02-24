//
// Created by Klemen Plestenjak on 2/20/25.
//

#ifndef INPUT_H
#define INPUT_H

#include <stdbool.h>

#include <cglm/cglm.h>

void inputInit(void *window);

void inputUpdate();

bool inputIsKeyDown(int key);
bool inputIsKeyPressed(int key);
bool inputIsKeyReleased(int key);

void inputGetMousePos(vec2 dst);
void inputGetMouseDelta(vec2 dst);
void inputGetMouseWheelDelta(vec2 dst);

bool inputIsButtonDown(int button);
bool inputIsButtonPressed(int button);
bool inputIsButtonReleased(int button);



#endif //INPUT_H
