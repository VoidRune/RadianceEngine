#pragma once
#include "Rendering/ModelData.h"
#include <array>

namespace Game
{
	using Pose = std::array<JointPose, MaxModelJoints>;
	using JointWeights = std::array<float, MaxModelJoints>;
	using JointMatrices = std::array<glm::mat4, MaxModelJoints>;

	void RestPose(const ModelSkeleton& skeleton, Pose& pose);
	void SampleClip(const ModelClip& clip, uint32_t joints, float time, bool loop, Pose& pose);
	void BlendPose(Pose& target, const Pose& source, float weight, uint32_t joints, const JointWeights* mask = nullptr);
	void ComputeGlobals(const ModelSkeleton& skeleton, const Pose& pose, JointMatrices& globals);
	void ComputeSkin(const ModelSkeleton& skeleton, const JointMatrices& globals, JointMatrices& skin);
	ModelClip RetargetClip(const ModelClip& clip, const ModelSkeleton& from, const ModelSkeleton& to, float armTuck = 0.0f);
}
