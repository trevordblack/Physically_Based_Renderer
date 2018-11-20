#ifndef LIGHT_H
#define LIGHT_H


#include "Core.h"
#include "Component.h"
#include "Node.h"


// @todo move light properties to components?
class LightNode : public Node
{
public:
	float3 GetStrength() { return m_strength; }
	void SetStrength(float3 strength) { m_strength = strength; }

protected:
	float3 m_strength = { 0.5f, 0.5f, 0.5f };
	float m_falloffStart = 1.0f;                          // point/spot light only
	float3 m_direction = { 0.0f, -1.0f, 0.0f };// directional/spot light only
	float m_falloffEnd = 10.0f;                           // point/spot light only
	float3 m_position = { 0.0f, 0.0f, 0.0f };  // point/spot light only
	float m_spotPower = 64.0f;                            // spot light only
};


class DirectLightNode : public LightNode
{
public:
	float3 GetDirection() { return m_direction; }
	void SetDirection(float3 dir) { m_direction = dir; }
};

class PointLightNode : public LightNode
{
public:
	float GetFalloffStart() { return m_falloffStart; }
	void SetFalloffStart(float start) { m_falloffStart = start; }

	float GetFalloffEnd() { return m_falloffEnd; }
	void SetFalloffEnd(float end) { m_falloffEnd = end; }

	float3 GetPosition() { return m_position; }
	void SetPosition(float3 pos) { m_position = pos; }
};

class SpotLightNode : public LightNode
{
public:
	float GetFalloffStart() { return m_falloffStart; }
	void SetFalloffStart(float start) { m_falloffStart = start; }

	float3 GetDirection() { return m_direction; }
	void SetDirection(float3 dir) { m_direction = dir; }

	float GetFalloffEnd() { return m_falloffEnd; }
	void SetFalloffEnd(float end) { m_falloffEnd = end; }

	float3 GetPosition() { return m_position; }
	void SetPosition(float3 pos) { m_position = pos; }

	float GetSpotPower() { return m_spotPower; }
	void SetSpotPower(float power) { m_spotPower = power; }
};

#endif //!LIGHT_H