#include "pch.h"
#include "BoundBox.h"

BoundBox BoundBox::operator*(const glm::mat4 &mat)
{
    glm::vec3 center_new = glm::vec3(mat * glm::vec4(getCenter(), 1.0f));

    glm::vec3 old_extent = getSize() * 0.5f;

    glm::vec3 new_extent = glm::vec3(
        glm::dot(glm::abs(glm::vec3(mat[0][0], mat[1][0], mat[2][0])), old_extent),
        glm::dot(glm::abs(glm::vec3(mat[0][1], mat[1][1], mat[2][1])), old_extent),
        glm::dot(glm::abs(glm::vec3(mat[0][2], mat[1][2], mat[2][2])), old_extent)
    );

    return BoundBox(center_new - new_extent, center_new + new_extent);
}
