#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>

struct CameraUBO {
	glm::mat4 viewProjInv;
	glm::vec4 position;
	glm::vec4 direction;
	float fov;
	float aspectRatio;
	float near;
	float far;
};

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
	void setNear(float near) {
		m_near = near;
	}
	void setFar(float far) {
		m_far = far;
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
	float getNear() const {
		return m_near;
	}
	float getFar() const {
		return m_far;
	}

	float getFOV() const {
		return m_fov;
	}

	float getSensitivity() const{
		return m_sensitivity;
	}

	glm::mat4 getViewMatrix() const {
		glm::mat4 rotMat = glm::toMat4(glm::conjugate(glm::normalize(m_rotation)));
		glm::mat4 trans = glm::translate(glm::mat4(1.0f), -m_position);
		return rotMat * trans;
	}
	
	glm::mat4 getProjMatrix(float aspectRatio) const {
		auto proj =  glm::perspective(glm::radians(m_fov), aspectRatio, m_near, m_far);
		proj[1][1] *= -1;
		return proj;
	}

	glm::mat4 getViewProjInv(float aspectRatio) const {
		glm::mat4 view = getViewMatrix();
		glm::mat4 proj = getProjMatrix(aspectRatio);
		return glm::inverse(proj * view);
	}

      private:
	glm::quat m_rotation;
	glm::vec3 m_position;
	float m_sensitivity{0.01f};
	float m_speed{2.5f};
	float m_near{0.1f};
	float m_far{100.0f};
	float m_fov{45.0f};
};


