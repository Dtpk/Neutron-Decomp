#pragma once

#include <GL/gl.h>
#include <GL/glu.h>

class glinterface {
public:
    glinterface();
    ~glinterface();

    void square_3D(GLfloat x, GLfloat y, GLfloat z, int r, int g, int b, int a, GLfloat size, int axis);
    void square(GLfloat x, GLfloat y, GLfloat z, int r, int g, int b, int a, GLfloat size);
    void line(GLfloat x_origin, GLfloat y_origin, GLfloat z_origin,
              GLfloat x_destination, GLfloat y_destination, GLfloat z_destination,
              const int* material);
    void cube_skeleton(GLfloat x, GLfloat y, GLfloat z, int r, int g, int b, int a, GLfloat size);
    void solid_cube(GLfloat x, GLfloat y, GLfloat z, int r, int g, int b, int a, GLfloat size);
    void cube(GLfloat x, GLfloat y, GLfloat z, int r, int g, int b, int a, GLfloat size);
    void lookat(GLfloat eyex, GLfloat eyey, GLfloat eyez,
                GLfloat centerx, GLfloat centery, GLfloat centerz,
                GLfloat upx, GLfloat upy, GLfloat upz);
    void perspective(GLfloat fovy, GLfloat aspect, GLfloat zNear, GLfloat zFar);
};
