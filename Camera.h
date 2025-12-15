#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>

class Camera {

      public:
	Camera() : m_rotation(glm::quat(1.0f, 0.0f, 0.0f, 0.0f)), m_position(0.0f) {}
	~Camera() = default;

	void setPosition(const glm::vec3& pos) {
		m_position = pos;
	}
	void setRotation(const glm::quat& q) {
		m_rotation = glm::normalize(q);
	}
	void setRotationEuler(const glm::vec3& euler) {
		m_rotation = glm::quat(euler);
	}
	void setSpeed(float speed) {
		m_speed = speed;
	}

	// getters
	const glm::vec3& getPosition() const {
		return m_position;
	}
	const glm::quat& getRotation() const {
		return m_rotation;
	}

	glm::vec3 getForward() const {
		return glm::normalize(m_rotation * glm::vec3(0.0f, 0.0f, -1.0f));
	}
	glm::vec3 getRight() const {
		return glm::normalize(m_rotation * glm::vec3(1.0f, 0.0f, 0.0f));
	}
	glm::vec3 getUp() const {
		return glm::normalize(m_rotation * glm::vec3(0.0f, 1.0f, 0.0f));
	}

	float getSpeed() const {
		return m_speed;
	}

	glm::mat4 getViewMatrix() const {
		glm::mat4 rotMat = glm::toMat4(glm::normalize(m_rotation));
		glm::mat4 trans = glm::translate(glm::mat4(1.0f), -m_position);
		return rotMat * trans;
	}

      private:
	glm::quat m_rotation;
	glm::vec3 m_position;
	float m_speed{2.5f};
};
