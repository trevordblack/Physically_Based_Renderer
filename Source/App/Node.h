#pragma once

#include <string>
#include <vector>
#include <memory>

#include "MathUtil.h"
#include "Component.h"

using namespace std;

class Node
{
public:
	Node()
	{
		static uint newID = 1; // not thread safe
		m_uniqueID = newID++;
		m_toWorldTransform = Matrix::Identity4x4();
		m_pParent = nullptr;
		m_name = "Node_" + std::to_string(m_uniqueID);
		m_isActive = true;
	}

	virtual ~Node() {}

	uint GetChildrenCount() { return m_children.size(); }

	shared_ptr<Node> GetChildByIndex(uint index)
	{
		if (index < m_children.size())
			return m_children[index];
		return nullptr;
	}

	shared_ptr<Node> GetChildByID(uint childID)
	{
		for (auto child : m_children)
		{
			if (child->GetID() == childID)
				return child;
		}
		return nullptr;
	}

	uint GetID() { return m_uniqueID; }
	void SetID(uint newID) { m_uniqueID = newID; }

	void SetTransform(float4x4 transform) { m_toWorldTransform = transform; }
	float4x4 GetTransform() { return m_toWorldTransform; }

	void SetName(string name) { m_name = name; }
	string GetName() { return m_name; }

	void SetParent(Node* parent) { m_pParent = parent; }
	Node* GetParent() { return m_pParent; }

	bool GetActive() { return m_isActive; }
	void SetActive(bool active) { m_isActive = active; }

protected:
	uint m_uniqueID;
	float4x4 m_toWorldTransform;
	vector<unique_ptr<Component>> m_components;
	vector<shared_ptr<Node>> m_children;
	Node* m_pParent;
	string m_name;
	bool m_isActive;
};
