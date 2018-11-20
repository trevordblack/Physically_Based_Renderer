#pragma once

#include "MathUtil.h"

class Component
{
	Component()
	{
		static uint newID = 1; // not thread safe
		m_uniqueID = newID++;
		m_isActive = true;
	}

	//virtual ~Component() {}

	uint GetID() { return m_uniqueID; }
	void SetID(uint newID) { m_uniqueID = newID; }

	bool GetActive() { return m_isActive; }
	void SetActive(bool active) { m_isActive = active; }

protected:
	uint m_uniqueID;
	bool m_isActive;
};