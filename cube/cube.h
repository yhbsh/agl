static float cube_vertices[] = {
    -0.2f, -0.2f, +0.2f, +1.0f, +0.0f, +0.0f, // Bottom-left
    +0.2f, -0.2f, +0.2f, +0.0f, +0.0f, +1.0f, // Bottom-right
    +0.2f, +0.2f, +0.2f, +0.0f, +1.0f, +0.0f, // Top-right
    -0.2f, +0.2f, +0.2f, +1.0f, +1.0f, +0.0f, // Top-left

    -0.2f, -0.2f, -0.2f, +0.0f, +1.0f, +0.0f, // Bottom-left
    +0.2f, -0.2f, -0.2f, +0.0f, +0.0f, +1.0f, // Bottom-right
    +0.2f, +0.2f, -0.2f, +1.0f, +1.0f, +0.0f, // Top-right
    -0.2f, +0.2f, -0.2f, +0.0f, +1.0f, +1.0f  // Top-left
};

static unsigned int cube_indices[] = {
    // Front face
    0, 1, 2, // Bottom-left, Bottom-right, Top-right
    2, 3, 0, // Top-right, Top-left, Bottom-left

    // Right face
    1, 5, 6, // Bottom-right, Back bottom-right, Back top-right
    6, 2, 1, // Back top-right, Top-right, Bottom-right

    // Back face
    5, 4, 7, // Back bottom-right, Back bottom-left, Back top-left
    7, 6, 5, // Back top-left, Back top-right, Back bottom-right

    // Left face
    4, 0, 3, // Back bottom-left, Bottom-left, Top-left
    3, 7, 4, // Top-left, Back top-left, Back bottom-left

    // Top face
    3, 2, 6, // Top-left, Top-right, Back top-right
    6, 7, 3, // Back top-right, Back top-left, Top-left

    // Bottom face
    4, 5, 1, // Back bottom-left, Back bottom-right, Bottom-right
    1, 0, 4  // Bottom-right, Bottom-left, Back bottom-left
};
