#include "CameraFP.h"
#include "RadianceEngine/Window/Input.h"
#include <algorithm>

#include <glm/gtc/matrix_transform.hpp>

CameraFP::CameraFP(Rdn::Window* window)
{
    Position = { 0.0f, 0.0f, -3.0f };
    Forward = glm::vec3(0.0f, 0.0f, 1.0f);
    WorldUp = { 0.0f, 1.0f, 0.0f };

    this->m_Window = window;
    UpdateMatrices();
}

CameraFP::~CameraFP()
{

}

void CameraFP::Update(double deltaTime)
{
    HasMoved = false;
    double mouseX = Rdn::Input::GetMouseX();
    double mouseY = Rdn::Input::GetMouseY();

    if (Rdn::Input::IsKeyDown(Rdn::KeyCode::MouseRight))
    {
        Yaw -= (mouseX - LastMouseX) * Sensitivity;
        Pitch -= (mouseY - LastMouseY) * Sensitivity;

        Yaw = fmod(Yaw, 360.0f);
        Pitch = std::clamp(Pitch, -89.9f, 89.9f);
        if (LastMouseX != mouseX || LastMouseY != mouseY)
            HasMoved = true;
    }
    LastMouseX = mouseX;
    LastMouseY = mouseY;

    Forward = glm::normalize(glm::vec3{
        cos(glm::radians(Yaw)) * cos(glm::radians(Pitch)),
        sin(glm::radians(Pitch)),
        sin(glm::radians(Yaw)) * cos(glm::radians(Pitch))
        });
    Right = glm::cross(-Forward, WorldUp);
    Right = glm::normalize(Right);
    Up = glm::cross(Forward, Right);

    glm::vec3 velocity = { 0.0f, 0.0f, 0.0f };

    if (Rdn::Input::IsKeyDown(Rdn::KeyCode::W)) { velocity += Forward; HasMoved = true; }
    if (Rdn::Input::IsKeyDown(Rdn::KeyCode::S)) { velocity -= Forward; HasMoved = true; }
    if (Rdn::Input::IsKeyDown(Rdn::KeyCode::D)) { velocity += Right; HasMoved = true; }
    if (Rdn::Input::IsKeyDown(Rdn::KeyCode::A)) { velocity -= Right; HasMoved = true; }

    velocity.y = 0.0f;
    if (velocity != glm::vec3{ 0.0f, 0.0f, 0.0f })
        velocity = glm::normalize(velocity);

    if (Rdn::Input::IsKeyDown(Rdn::KeyCode::Space)) { velocity.y++; HasMoved = true; }
    if (Rdn::Input::IsKeyDown(Rdn::KeyCode::LeftShift)) { velocity.y--; HasMoved = true; }

    Velocity = velocity;

    float speedBoost = 1.0f;
    if (Rdn::Input::IsKeyDown(Rdn::KeyCode::F)) { speedBoost = 10.0f; }

    Position += velocity * MovementSpeed * speedBoost * (float)deltaTime;
    UpdateMatrices();
}

void CameraFP::UpdateMatrices()
{
    Projection = glm::perspective(glm::radians(Fov), AspectRatio, NearPlane, FarPlane);
    Projection[1][1] *= -1;
    View = glm::lookAt(Position, Position + Forward, WorldUp);
    InverseView = glm::inverse(View);
    InverseProjection = glm::inverse(Projection);
}