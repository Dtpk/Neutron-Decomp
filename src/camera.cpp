#include "camera.h"
#include <cmath>

camera::camera()
    : x(0.0f), y(0.0f), z(0.0f),
      x_dir(0.0f), y_dir(0.0f), z_dir(0.0f),
      x_ori(0.0f), y_ori(1.0f), z_ori(0.0f),
      last_x(0.0f), last_y(0.0f), last_z(0.0f),
      locator(new glinterface()), degrees(0) {}

camera::~camera() {
    delete locator;
}

void camera::set_coordinates(GLfloat nx, GLfloat ny, GLfloat nz, GLfloat nx_dir, GLfloat ny_dir, GLfloat nz_dir) {
    last_x = x;
    last_y = y;
    last_z = z;
    x = nx;
    y = ny;
    z = nz;
    x_dir = nx_dir;
    y_dir = ny_dir;
    z_dir = nz_dir;
}

void camera::update_pos(int cam_x, int cam_y, int cam_z, GLfloat nx, GLfloat ny, GLfloat nz) {
    last_x = x;
    last_y = y;
    last_z = z;
    x = (float)cam_x;
    y = (float)cam_y;
    z = (float)cam_z;
    x_dir = nx;
    y_dir = ny;
    z_dir = nz;
}

void camera::restart() {
    z_ori = 0.0f;
    x = 0.0f;
    y_ori = 1.0f;
    y = 0.0f;
    z = 0.0f;
    x_dir = 0.0f;
    y_dir = 0.0f;
    z_dir = 0.0f;
    x_ori = 0.0f;
}

void camera::locate() {
    if (!locator) return;

    float dx = x_dir - x;
    float dy = y_dir - y;
    float dz = z_dir - z;
    float len = std::sqrt(dx * dx + dy * dy + dz * dz);
    if (len < 0.0001f) {
        x_dir += 0.01f;
        dx = x_dir - x;
        dy = y_dir - y;
        dz = z_dir - z;
        len = std::sqrt(dx * dx + dy * dy + dz * dz);
    }

    float ux = x_ori;
    float uy = y_ori;
    float uz = z_ori;
    if (ux == 0.0f && uy == 0.0f && uz == 0.0f) {
        uy = 1.0f;
    }

    // Orthogonalize up vector against view direction vector for numerical stability
    float inv_len = 1.0f / len;
    float vx = dx * inv_len;
    float vy = dy * inv_len;
    float vz = dz * inv_len;

    float dot = vx * ux + vy * uy + vz * uz;
    if (std::fabs(dot) > 0.99f) {
        // Fallback up vector if nearly parallel
        ux = 0.0f;
        uy = 1.0f;
        uz = 0.0f;
        if (std::fabs(vy) > 0.95f) {
            ux = 0.0f;
            uy = 0.0f;
            uz = (vy > 0.0f) ? -1.0f : 1.0f;
        }
    }

    locator->lookat(x, y, z, x_dir, y_dir, z_dir, ux, uy, uz);
}
