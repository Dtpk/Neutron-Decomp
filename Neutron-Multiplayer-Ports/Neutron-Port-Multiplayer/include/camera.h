#pragma once

#include "glinterface.h"

class camera {
public:
    float x;
    float y;
    float z;
    float x_dir;
    float y_dir;
    float z_dir;
    float x_ori;
    float y_ori;
    float z_ori;
    float last_x;
    float last_y;
    float last_z;
    glinterface* locator;
    int degrees;

    camera();
    ~camera();

    void set_coordinates(GLfloat x, GLfloat y, GLfloat z, GLfloat x_dir, GLfloat y_dir, GLfloat z_dir);
    void update_pos(int cam_x, int cam_y, int cam_z, GLfloat x, GLfloat y, GLfloat z);
    void restart();
    void locate();
};
