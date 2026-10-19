#include "glinterface.h"
#include <cmath>

glinterface::glinterface() {}
glinterface::~glinterface() {}

void glinterface::square_3D(GLfloat x, GLfloat y, GLfloat z, int r, int g, int b, int a, GLfloat size, int axis) {
    float fVar8 = size * 0.5f;
    GLfloat vertices[12];

    if (axis == 2) {
        // Y-axis plane (horizontal / XZ)
        vertices[0]  = -fVar8; vertices[1]  = 0.0f; vertices[2]  = fVar8;
        vertices[3]  = -fVar8; vertices[4]  = 0.0f; vertices[5]  = -fVar8;
        vertices[6]  = fVar8;  vertices[7]  = 0.0f; vertices[8]  = fVar8;
        vertices[9]  = fVar8;  vertices[10] = 0.0f; vertices[11] = -fVar8;
    } else if (axis == 3) {
        // Z-axis plane (XY)
        vertices[0]  = -fVar8; vertices[1]  = -fVar8; vertices[2]  = 0.0f;
        vertices[3]  = -fVar8; vertices[4]  = fVar8;  vertices[5]  = 0.0f;
        vertices[6]  = fVar8;  vertices[7]  = -fVar8; vertices[8]  = 0.0f;
        vertices[9]  = fVar8;  vertices[10] = fVar8;  vertices[11] = 0.0f;
    } else {
        // X-axis plane (YZ)
        float fVar7 = -fVar8;
        vertices[0]  = 0.0f; vertices[1]  = fVar7; vertices[2]  = fVar8;
        vertices[3]  = 0.0f; vertices[4]  = fVar7; vertices[5]  = fVar7;
        vertices[6]  = 0.0f; vertices[7]  = fVar8; vertices[8]  = fVar8;
        vertices[9]  = 0.0f; vertices[10] = fVar8; vertices[11] = fVar7;
    }

    glPushMatrix();
    glTranslatef(x + fVar8, y + fVar8, z + fVar8);
    glColor4ub((GLubyte)r, (GLubyte)g, (GLubyte)b, (GLubyte)a);
    glVertexPointer(3, GL_FLOAT, 0, vertices);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glPopMatrix();
}

void glinterface::square(GLfloat x, GLfloat y, GLfloat z, int r, int g, int b, int a, GLfloat size) {
    GLfloat vertices[12] = {
        -size, -size, 0.0f,
         size, -size, 0.0f,
        -size,  size, 0.0f,
         size,  size, 0.0f
    };

    glPushMatrix();
    glTranslatef(x, y, z);
    glColor4ub((GLubyte)r, (GLubyte)g, (GLubyte)b, (GLubyte)a);
    glVertexPointer(3, GL_FLOAT, 0, vertices);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glPopMatrix();
}

void glinterface::line(GLfloat x_origin, GLfloat y_origin, GLfloat z_origin,
                       GLfloat x_destination, GLfloat y_destination, GLfloat z_destination,
                       const int* material) {
    GLfloat lineVerts[6] = {
        x_origin, y_origin, z_origin,
        x_destination, y_destination, z_destination
    };

    glPushMatrix();
    glColor4ub((GLubyte)material[0], (GLubyte)material[1], (GLubyte)material[2], (GLubyte)material[3]);
    glVertexPointer(3, GL_FLOAT, 0, lineVerts);
    glDrawArrays(GL_LINES, 0, 2);
    glPopMatrix();
}

void glinterface::cube_skeleton(GLfloat x, GLfloat y, GLfloat z, int r, int g, int b, int a, GLfloat size) {
    float s = size;
    GLfloat box[72] = {
        // Bottom 4 lines
        -s, -s, -s,   s, -s, -s,
         s, -s, -s,   s, -s,  s,
         s, -s,  s,  -s, -s,  s,
        -s, -s,  s,  -s, -s, -s,
        // Top 4 lines
        -s,  s, -s,   s,  s, -s,
         s,  s, -s,   s,  s,  s,
         s,  s,  s,  -s,  s,  s,
        -s,  s,  s,  -s,  s, -s,
        // 4 Vertical pillars
        -s, -s, -s,  -s,  s, -s,
         s, -s, -s,   s,  s, -s,
         s, -s,  s,   s,  s,  s,
        -s, -s,  s,  -s,  s,  s
    };

    glPushMatrix();
    glTranslatef(x, y, z);
    glColor4ub((GLubyte)r, (GLubyte)g, (GLubyte)b, (GLubyte)a);
    glVertexPointer(3, GL_FLOAT, 0, box);
    glDrawArrays(GL_LINES, 0, 24);
    glPopMatrix();
}

void glinterface::solid_cube(GLfloat x, GLfloat y, GLfloat z, int r, int g, int b, int a, GLfloat size) {
    float s = size;
    float ns = -size;

    glPushMatrix();
    glTranslatef(x, y, z);
    glColor4ub((GLubyte)r, (GLubyte)g, (GLubyte)b, (GLubyte)a);

    glBegin(GL_QUADS);
    // Front face
    glVertex3f(ns, ns,  s);
    glVertex3f( s, ns,  s);
    glVertex3f( s,  s,  s);
    glVertex3f(ns,  s,  s);

    // Back face
    glVertex3f(ns, ns, ns);
    glVertex3f(ns,  s, ns);
    glVertex3f( s,  s, ns);
    glVertex3f( s, ns, ns);

    // Top face
    glVertex3f(ns,  s, ns);
    glVertex3f(ns,  s,  s);
    glVertex3f( s,  s,  s);
    glVertex3f( s,  s, ns);

    // Bottom face
    glVertex3f(ns, ns, ns);
    glVertex3f( s, ns, ns);
    glVertex3f( s, ns,  s);
    glVertex3f(ns, ns,  s);

    // Right face
    glVertex3f( s, ns, ns);
    glVertex3f( s,  s, ns);
    glVertex3f( s,  s,  s);
    glVertex3f( s, ns,  s);

    // Left face
    glVertex3f(ns, ns, ns);
    glVertex3f(ns, ns,  s);
    glVertex3f(ns,  s,  s);
    glVertex3f(ns,  s, ns);
    glEnd();

    glPopMatrix();
}

void glinterface::cube(GLfloat x, GLfloat y, GLfloat z, int r, int g, int b, int a, GLfloat size) {
    solid_cube(x, y, z, r, g, b, a, size);
}

void glinterface::lookat(GLfloat eyex, GLfloat eyey, GLfloat eyez,
                         GLfloat centerx, GLfloat centery, GLfloat centerz,
                         GLfloat upx, GLfloat upy, GLfloat upz) {
    gluLookAt(eyex, eyey, eyez, centerx, centery, centerz, upx, upy, upz);
}

void glinterface::perspective(GLfloat fovy, GLfloat aspect, GLfloat zNear, GLfloat zFar) {
    gluPerspective(fovy, aspect, zNear, zFar);
}
