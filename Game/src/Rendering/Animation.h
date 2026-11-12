#pragma once
#include "Rendering/ModelData.h"
#include <array>

namespace Game
{
	using Pose = std::array<JointPose, MaxModelJoints>;
	using JointWeights = std::array<float, MaxModelJoints>;
	using JointMatrices = std::array<glm::mat4, MaxModelJoints>;
	using JointRotations = std::array<glm::quat, MaxModelJoints>;

	void RestPose(const ModelSkeleton& skeleton, Pose& pose);
	void SampleClip(const ModelClip& clip, uint32_t joints, float time, bool loop, Pose& pose);
	void BlendPose(Pose& target, const Pose& source, float weight, uint32_t joints, const JointWeights* mask = nullptr);
	void AddPose(Pose& target, const Pose& reference, const Pose& source, float weight, uint32_t joints, const JointWeights* mask = nullptr);
	void ComputeGlobals(const ModelSkeleton& skeleton, const Pose& pose, JointMatrices& globals);
	void ComputeRotations(const ModelSkeleton& skeleton, const Pose& pose, JointRotations& rotations);
	void ComputeSkin(const ModelSkeleton& skeleton, const JointMatrices& globals, JointMatrices& skin);
	ModelClip RetargetClip(const ModelClip& clip, const ModelSkeleton& from, const ModelSkeleton& to, float armTuck = 0.0f);

	glm::mat4 JointMatrix(const JointPose& pose);
	glm::quat GlobalRotation(const ModelSkeleton& skeleton, const Pose& pose, int32_t joint);
	void TurnJoint(const ModelSkeleton& skeleton, Pose& pose, uint32_t joint, const glm::quat& turn);
	glm::quat RotationBetween(const glm::vec3& from, const glm::vec3& to);
	glm::vec3 RotationVector(glm::quat rotation);
	glm::quat FromRotationVector(const glm::vec3& vector);

	struct LimbSolve
	{
		glm::quat Upper{ 1.0f, 0.0f, 0.0f, 0.0f };
		glm::quat Lower{ 1.0f, 0.0f, 0.0f, 0.0f };
	};

	LimbSolve SolveLimb(const glm::vec3& root, const glm::vec3& middle, const glm::vec3& end, const glm::vec3& target, const glm::quat& rootRotation, const glm::quat& middleRotation, const glm::vec3& hinge);
}
