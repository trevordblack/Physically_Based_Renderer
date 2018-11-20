#include "Camera.h"

namespace Loxodonta
{

// fov is expected in RADIANS
Camera::Camera(float fovY, uint screenWidth, uint screenHeight, float nearZ, float farZ)
{
	m_fovY = fovY;
	m_aspectRatio = ((float) screenWidth) / screenHeight;
	m_nearZ = nearZ;
	m_farZ = farZ;

	m_yaw = 0.0f;
	m_pitch = 0.0f;

	// Move Vector set to Mathutil functions to allow for API independence
	m_Position = Vector::Set3(0.0f, 0.0f, 0.0f);
	m_Forward = Vector::Forward();
	m_Side = Vector::Side();
	m_Up = Vector::Set3(0.0f, 1.0f, 0.0f);
	m_WorldUp = Vector::Set3(0.0f, 1.0f, 0.0f);

	m_mouseSensitivity = 1.0f;

	DeriveProjectionMatrix();
}

void Camera::OnResize(uint screenWidth, uint screenHeight)
{
	m_aspectRatio = ((float) screenWidth) / screenHeight;
	DeriveProjectionMatrix();
}

void Camera::SetPosition(const vect& Pos)
{
	m_Position = Pos;
}

vect Camera::GetPosition()
{
	return m_Position;
}
vect Camera::GetForward()
{
	return m_Forward;
}
vect Camera::GetSide()
{
	return m_Side;
}
vect Camera::GetUp()
{
	return m_Up;
}
float Camera::GetNearZ()
{
	return m_nearZ;
}
float Camera::GetFarZ()
{
	return m_farZ;
}
vect4 Camera::GetProjectionMatrix()
{
	return m_Projection;
}
vect4 Camera::GetViewMatrix()
{
	return m_View;
}


void Camera::ProcessKeyboardInput(float deltaSide, float deltaForward)
{
	m_Position = m_Position + m_Side * deltaSide;
	m_Position = m_Position + m_Forward * deltaForward;
}

void Camera::ProcessMouseMovement(float deltaYaw, float deltaPitch)
{
	deltaYaw *= m_mouseSensitivity;
	deltaPitch *= m_mouseSensitivity;

	m_yaw += deltaYaw;
	m_pitch += deltaPitch;

	// Make sure pitch doesn't extend to perfectly up or perfectly down
	m_pitch = Math::Clamp(m_pitch, -M_PI_OVER_2 + 0.1f, M_PI_OVER_2 - 0.1f);
	// Wrap yaw so it stays constrained to -pi ... pi
	m_yaw = Math::WrapPi(m_yaw);
	
	// Update Front, Right and Up Vectors using the updated Euler angles
	DeriveLocalDirectionVectors();
}

void Camera::DeriveLocalDirectionVectors()
{
	m_Forward = Vector::YawPitchToCartesian(m_yaw, m_pitch);
	m_Side = Vector::Normalize3(Vector::CrossProduct3(m_Forward, m_WorldUp));
	m_Up = Vector::CrossProduct3(m_Side, m_Forward); 
}

void Camera::DeriveProjectionMatrix()
{
	m_Projection = Matrix::PerspectiveFov(m_fovY, m_aspectRatio, m_nearZ, m_farZ);
}

void Camera::DeriveViewMatrix()
{
	m_View = Matrix::LookTo(m_Position, m_Forward, m_WorldUp);
}

}