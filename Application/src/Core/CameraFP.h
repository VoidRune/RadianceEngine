#pragma once
#include "RadianceEngine/Window/Window.h"

#define GLM_FORCE_LEFT_HANDED
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>

class CameraFP
{
public:
	CameraFP(Rdn::Window* window);
	~CameraFP();

	void Update(double deltaTime);

	float Theta = 0.0f;
	float Phi = 0.0f;
	float Pitch = 0.0f;
	float Yaw = 90.0f;
	float Roll = 0.0f;
	float Fov = 70.0f;
	float NearPlane = 0.1f;
	float FarPlane = 1000.0f;

	float Sensitivity = 0.3f;
	float MovementSpeed = 2.0f;
	float AspectRatio = 1.0f;
	double LastMouseX;
	double LastMouseY;
	uint32_t HasMoved;

	glm::mat4 Projection;
	glm::mat4 View;
	glm::mat4 InverseProjection;
	glm::mat4 InverseView;

	glm::vec3 Velocity;

	glm::vec3 Position;
	glm::vec3 Forward;
	glm::vec3 Up;
	glm::vec3 Right;
	glm::vec3 WorldUp;

private:
	Rdn::Window* m_Window;
	void UpdateMatrices();
};