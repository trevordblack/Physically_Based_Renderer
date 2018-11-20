#ifndef CAMERA_H
#define CAMERA_H

#include "Core.h"
#include "MathUtil.h"

namespace Loxodonta
{

class Camera
{
public:
	Camera() 
	{
		m_nearZ = 1.0f;
		m_farZ = 1000.0f;
	};
	// fov is expected in RADIANS
	Camera(float fov, uint screenWidth, uint screenHeight, float nearZ, float farZ);

	~Camera() {};

	void OnResize(uint screenWidth, uint screenHeight);

	// Setters
	void SetPosition(const vect& Pos);
	void SetLookAt(const vect& LookAt);

	// Input management
	void ProcessKeyboardInput(float deltaSide, float deltaForward);
	void ProcessMouseMovement(float deltaYaw, float deltaPitch);
	void ProcessMouseScroll(float deltaFOV);

	// Getters
	vect GetPosition();
	vect GetForward();
	vect GetSide();
	vect GetUp();
	float GetNearZ();
	float GetFarZ();
	vect4 GetProjectionMatrix();
	vect4 GetViewMatrix();

	// Compute new View Matrix
	void DeriveViewMatrix();

private:

	vect4 m_Projection;
	vect4 m_View;
	vect m_Position;
	vect m_Forward;
	vect m_Side; // points Left for LH, right for RH
	vect m_Up;
	vect m_WorldUp;

	float m_fovY;
	float m_aspectRatio;
	float m_nearZ;
	float m_farZ;

	float m_yaw;
	float m_pitch;
	float m_mouseSensitivity;

	// Compute new camera matrix and vectors
	void DeriveProjectionMatrix();
	void DeriveLocalDirectionVectors();
};

}

#endif //!CAMERA_H
