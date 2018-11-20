#ifndef MATH_UTIL_H
#define MATH_UTIL_H

#include "Core.h"
#include <DirectXMath.h>
using namespace DirectX;

namespace Loxodonta
{

const float M_PI  = DirectX::XM_PI;
const float M_2PI = DirectX::XM_2PI;
const float M_PI_OVER_2 = DirectX::XM_PIDIV2;
const float M_PI_OVER_4 = DirectX::XM_PIDIV4;
const float M_1_OVER_PI = DirectX::XM_1DIVPI;
const float M_1_OVER_2PI = DirectX::XM_1DIV2PI;

typedef DirectX::XMVECTOR vect;
typedef DirectX::XMMATRIX vect4;

typedef DirectX::XMFLOAT4X4 float4x4; // change to XMFLOAT4X4A to set to 16-byte alignment
typedef DirectX::XMFLOAT4X3 float4x3; // change to XMFLOAT4X3A to set to 16-byte alignment
typedef DirectX::XMFLOAT3X3 float3x3;
typedef DirectX::XMFLOAT4   float4;   // change to XMFLOAT4A to set to 16-byte alignment
typedef DirectX::XMFLOAT3   float3;   // change to XMFLOAT3A to set to 16-byte alignment
typedef DirectX::XMFLOAT2   float2;


namespace Math
{
	// Returns a random 32-bit unsigned integer
	inline u32 Xorshift128()
	{
		static u32 x = 123456789;
		static u32 y = 3624396069;
		static u32 z = 521288629;
		static u32 w = 88675123;
		u32 s, t = w;
		t ^= t << 11;
		t ^= t >> 8;
		w = z; z = y; y = s = x;
		t ^= s;
		t ^= s >> 19;
		x = t;
		return t;
	}

	// Returns a random 32-bit signed integer
	inline i32 Rand()
	{
		return (i32) Xorshift128();
	}

	// Returns a random 32-bit signed integer [a,b)
	inline i32 Rand(const i32 a, const i32 b)
	{
		return a + Xorshift128() % ((b - a) + 1);
	}

	// Returns a random 32 bit float [0,1)
	inline float Randf()
	{
		float r = Xorshift128() / 4294967296.0f;
		if (r != 1)
			return r;
		else
			return Randf();
		return r;
	}

	// Returns a random 32 bit float in [a, b).
	inline float Randf(const float a, const float b)
	{
		return a + Randf()*(b - a);
	}

	template<typename T>
	inline T Min(const T& a, const T& b)
	{
		return a < b ? a : b;
	}

	template<typename T>
	inline T Max(const T& a, const T& b)
	{
		return a > b ? a : b;
	}

	template<typename T>
	inline T Lerp(const T& a, const T& b, float t)
	{
		return a + (b - a)*t;
	}

	template<typename T>
	inline T Clamp(const T& x, const T& low, const T& high)
	{
		return x < low ? low : (x > high ? high : x);
	}

	// "Wrap" an angle in range -pi...pi by adding the correct multiple of 2 pi
	inline float WrapPi(const float _theta)
	{
		float theta = _theta + M_PI;
		theta -= floorf(theta * M_1_OVER_2PI) * M_2PI;
		theta -= M_PI;
		return theta;
	}

	// Same as acos(x), but if x is out of range, it is "clamped" to the nearest 
	// valid value. The value returned is in range 0...pi, the same as the
	// standard C acos() function
	inline float SafeAcosf(const float x)
	{
		// Check limit conditions

		if (x <= -1.0f)
		{
			return M_PI;
		}
		if (x >= 1.0f)
		{
			return 0.0f;
		}
		// Value is in the domain - use standard C function

		return acosf(x);
	}

	// Converts from degrees to radians
	inline float DegreesToRadians(const float degrees)
	{
		return degrees * (M_PI / 180.0f);
	}

	// Converts from radians to degrees
	inline float RadiansToDegrees(const float radians)
	{
		return radians * (180.0f * M_1_OVER_PI);
	}
}


namespace Matrix
{
	inline float4x4 SetFloat4x4(
		float m00, float m01, float m02, float m03,
		float m10, float m11, float m12, float m13, 
		float m20, float m21, float m22, float m23, 
		float m30, float m31, float m32, float m33)
	{
		return XMFLOAT4X4(
			m00, m01, m02, m03,
			m10, m11, m12, m13, 
			m20, m21, m22, m23, 
			m30, m31, m32, m33);
	}

	inline void StoreFloat4x4(float4x4* pDestination, vect4& M)
	{
		XMStoreFloat4x4(pDestination, M);
	}
	inline vect4 LoadFloat4x4(float4x4* pSource)
	{
		return XMLoadFloat4x4(pSource);
	}
	inline float4x4 Identity4x4()
	{
		static float4x4 I(
			1.0f, 0.0f, 0.0f, 0.0f,
			0.0f, 1.0f, 0.0f, 0.0f,
			0.0f, 0.0f, 1.0f, 0.0f,
			0.0f, 0.0f, 0.0f, 1.0f);

		return I;
	}

	inline vect4 SetupScale(float scaleX, float scaleY, float scaleZ)
	{
		return XMMatrixScaling(scaleX, scaleY, scaleZ);
	}
	inline vect4 SetupScale(vect* scale)
	{
		return XMMatrixScalingFromVector(*scale);
	}
	inline vect4 SetupTranslation(float transX, float transY, float transZ)
	{
		return XMMatrixTranslation(transX, transY, transZ);
	}
	inline vect4 SetupTranslation(vect* trans)
	{
		return XMMatrixTranslationFromVector(*trans);
	}
	inline vect4 SetupRotation(float rotX, float rotY, float rotZ)
	{
		return XMMatrixRotationRollPitchYaw(rotX, rotY, rotZ);
	}
	inline vect4 SetupRotation(vect* rot)
	{
		return XMMatrixRotationRollPitchYawFromVector(*rot);
	}

	inline void Decompose(vect* scale, vect* rot, vect* trans, vect4& M)
	{
		XMMatrixDecompose(scale, rot, trans, M);
	}

	inline vect4 Multiply(vect4& M1, vect4& M2)
	{
		return XMMatrixMultiply(M1, M2);
	}
	inline vect4 Transpose(vect4& M)
	{
		return XMMatrixTranspose(M);
	}
	inline vect4 Inverse(vect* pDestination, vect4& M)
	{
		return XMMatrixInverse(pDestination, M);
	}
	inline vect Determinant(vect4& M)
	{
		return XMMatrixDeterminant(M);
	}
	inline vect4 PerspectiveFov
	(float fovAngleY, float aspectRatio, float nearZ, float farZ)
	{
		return XMMatrixPerspectiveFovLH(fovAngleY, aspectRatio, nearZ, farZ);
	}
	inline vect4 LookAt(vect EyePosition, vect FocusPosition, vect UpDirection)
	{
		return XMMatrixLookAtLH(EyePosition, FocusPosition, UpDirection);
	}
	inline vect4 LookTo(vect EyePosition, vect EyeDirection, vect UpDirection)
	{
		return XMMatrixLookToLH(EyePosition, EyeDirection, UpDirection);
	}
}


namespace Vector
{
	inline void StoreFloat4(float4* pDestination, vect& M)
	{
		XMStoreFloat4(pDestination, M);
	}
	inline vect LoadFloat4(float4* pSource)
	{
		return XMLoadFloat4(pSource);
	}
	inline void StoreFloat3(float3* pDestination, vect& M)
	{
		XMStoreFloat3(pDestination, M);
	}
	inline vect LoadFloat3(const float3* pSource)
	{
		return XMLoadFloat3(pSource);
	}
	inline void StoreFloat2(float2* pDestination, vect& M)
	{
		XMStoreFloat2(pDestination, M);
	}
	inline vect LoadFloat2(const float2* pSource)
	{
		return XMLoadFloat2(pSource);
	}
	inline vect Set4(float x, float y, float z, float w)
	{
		return XMVectorSet(x, y, z, w);
	}
	inline vect Set3(float x, float y, float z)
	{
		return XMVectorSet(x, y, z, 0.0f);
	}
	inline vect Set2(float x, float y)
	{
		return XMVectorSet(x, y, 0.0f, 0.0f);
	}

	inline float4 SetFloat4(float x, float y, float z, float w)
	{
		return XMFLOAT4(x, y, z, w);
	}
	inline float3 SetFloat3(float x, float y, float z)
	{
		return XMFLOAT3(x, y, z);
	}
	inline float2 SetFloat2(float x, float y)
	{
		return XMFLOAT2(x, y);
	}

	inline vect Zero()
	{
		return XMVectorZero();
	}

	inline float GetX(vect V)
	{
		return XMVectorGetX(V);
	}
	inline float GetY(vect V)
	{
		return XMVectorGetY(V);
	}
	inline float GetZ(vect V)
	{
		return XMVectorGetZ(V);
	}
	inline float GetW(vect V)
	{
		return XMVectorGetW(V);
	}

	inline void SetX(vect V, float x)
	{
		XMVectorSetX(V, x);
	}
	inline void SetY(vect V, float y)
	{
		XMVectorSetY(V, y);
	}
	inline void SetZ(vect V, float z)
	{
		XMVectorSetZ(V, z);
	}
	inline void SetW(vect V, float w)
	{
		XMVectorSetW(V, w);
	}

	inline vect Forward()
	{
		return Vector::Set3(0.0f, 0.0f, 1.0f);
	}
	inline vect Side()
	{
		return Vector::Set3(-1.0f, 0.0f, 0.0f);
	}

	inline vect Normalize3(vect V)
	{
		return XMVector3Normalize(V);
	}
	inline vect CrossProduct3(vect& V1, vect& V2)
	{
		return XMVector3Cross(V1, V2);
	}

	inline vect YawPitchToCartesian(float yaw, float pitch)
	{
		// {0.0f, 0.0f}  is mapped to { 0.0f, 0.0f,  1.0f} for DX
		// {pi/2, 0.0f}  is mapped to { 1.0f, 0.0f,  0.0f} for DX
		// {-pi/2, 0.0f} is mapped to {-1.0f, 0.0f,  0.0f} for DX
		// {pi, 0.0f}    is mapped to { 0.0f, 0.0f, -1.0f} for DX
		// {0.0f, pi/4}  is mapped to { 0.0f, 0.7f, 0.7f} for DX

		float fx = cosf(pitch) * sinf(yaw);
		float fz = cosf(pitch) * cosf(yaw);
		float fy = sinf(pitch);
		return Normalize3(Set3(fx, fy, fz));

		// {0.0f, 0.0f} is mapped to {0.0f, 0.0f, -1.0f} for OGL
		// {pi/2, 0.0f} is mapped to {-1.0f, 0.0f, 0.0f} for OGL
	}

	inline vect Cos(vect& V)
	{
		return XMVectorCos(V);
	}
	inline vect Sin(vect& V)
	{
		return XMVectorSin(V);
	}

	inline vect SphericalToCartesian(float radius, float theta, float phi)
	{
		return DirectX::XMVectorSet(
			radius*sinf(phi)*cosf(theta),
			radius*cosf(phi),
			radius*sinf(phi)*sinf(theta),
			1.0f);
	}

}

}

#endif //!MATH_UTIL_H