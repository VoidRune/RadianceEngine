#include "Animation.h"
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <cmath>

namespace Game
{
	namespace
	{
		glm::quat Nlerp(const glm::quat& a, glm::quat b, float t)
		{
			if (glm::dot(a, b) < 0.0f)
				b = -b;
			return glm::normalize(glm::quat(a.w + (b.w - a.w) * t, a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, a.z + (b.z - a.z) * t));
		}

		JointPose Mix(const JointPose& a, const JointPose& b, float t)
		{
			return { glm::mix(a.Translation, b.Translation, t), Nlerp(a.Rotation, b.Rotation, t), glm::mix(a.Scale, b.Scale, t) };
		}
	}

	void RestPose(const ModelSkeleton& skeleton, Pose& pose)
	{
		std::copy(skeleton.Rest.begin(), skeleton.Rest.begin() + std::min<size_t>(skeleton.Rest.size(), MaxModelJoints), pose.begin());
	}

	void SampleClip(const ModelClip& clip, uint32_t joints, float time, bool loop, Pose& pose)
	{
		joints = std::min(joints, MaxModelJoints);
		if (clip.FrameCount == 0 || clip.Frames.size() < size_t(clip.FrameCount) * joints)
			return;
		const float duration = clip.Duration();
		if (duration <= 0.0f)
		{
			std::copy(clip.Frames.begin(), clip.Frames.begin() + joints, pose.begin());
			return;
		}
		time = loop ? time - duration * std::floor(time / duration) : std::clamp(time, 0.0f, duration);
		const float position = time * clip.FrameRate;
		const uint32_t first = std::min(uint32_t(position), clip.FrameCount - 1);
		const uint32_t second = std::min(first + 1, clip.FrameCount - 1);
		const float t = position - float(first);
		const JointPose* a = clip.Frames.data() + size_t(first) * joints;
		const JointPose* b = clip.Frames.data() + size_t(second) * joints;
		for (uint32_t joint = 0; joint < joints; joint++)
			pose[joint] = Mix(a[joint], b[joint], t);
	}

	void BlendPose(Pose& target, const Pose& source, float weight, uint32_t joints, const JointWeights* mask)
	{
		if (weight <= 0.0f)
			return;
		joints = std::min(joints, MaxModelJoints);
		for (uint32_t joint = 0; joint < joints; joint++)
		{
			const float amount = std::min(weight * (mask ? (*mask)[joint] : 1.0f), 1.0f);
			if (amount >= 1.0f)
				target[joint] = source[joint];
			else if (amount > 0.0f)
				target[joint] = Mix(target[joint], source[joint], amount);
		}
	}

	void ComputeGlobals(const ModelSkeleton& skeleton, const Pose& pose, JointMatrices& globals)
	{
		const uint32_t joints = std::min(skeleton.JointCount(), MaxModelJoints);
		for (uint32_t joint = 0; joint < joints; joint++)
		{
			const JointPose& local = pose[joint];
			const glm::mat4 matrix = glm::translate(glm::mat4(1.0f), local.Translation) * glm::mat4_cast(local.Rotation) * glm::scale(glm::mat4(1.0f), local.Scale);
			const int32_t parent = skeleton.Parents[joint];
			globals[joint] = parent >= 0 ? globals[size_t(parent)] * matrix : matrix;
		}
	}

	void ComputeSkin(const ModelSkeleton& skeleton, const JointMatrices& globals, JointMatrices& skin)
	{
		const uint32_t joints = std::min(skeleton.JointCount(), MaxModelJoints);
		for (uint32_t joint = 0; joint < joints; joint++)
			skin[joint] = globals[joint] * skeleton.InverseBind[joint];
	}
}
