#ifndef BETTERCAMERA_H
#define BETTERCAMERA_H

#include "types.h"

typedef struct {
    bool isActive;
    bool LCentering;
} NewCamera;

extern NewCamera gNewCamera;

void newcam_init_settings(void);
void romhack_camera_init_settings(void);

#endif
