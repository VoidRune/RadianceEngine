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

		using JointRotations = std::array<glm::quat, MaxModelJoints>;

		void GlobalRotations(const ModelSkeleton& skeleton, const Pose& pose, JointRotations& rotations)
		{
			const uint32_t joints = std::min(skeleton.JointCount(), MaxModelJoints);
			for (uint32_t joint = 0; joint < joints; joint++)
			{
				const int32_t parent = skeleton.Parents[joint];
				rotations[joint] = parent >= 0 ? glm::normalize(rotations[size_t(parent)] * pose[joint].Rotation) : pose[joint].Rotation;
			}
		}

		int32_t JointIndex(const ModelSkeleton& skeleton, const std::string& name)
		{
			const uint32_t joints = std::min(skeleton.JointCount(), MaxModelJoints);
			for (uint32_t joint = 0; joint < joints; joint++)
				if (skeleton.Names[joint] == name)
					return int32_t(joint);
			return -1;
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

	ModelClip RetargetClip(const ModelClip& clip, const ModelSkeleton& from, const ModelSkeleton& to, float armTuck)
	{
		ModelClip result;
		result.Name = clip.Name;
		result.FrameRate = clip.FrameRate;
		const uint32_t sourceJoints = std::min(from.JointCount(), MaxModelJoints);
		const uint32_t targetJoints = std::min(to.JointCount(), MaxModelJoints);
		if (clip.FrameCount == 0 || clip.Frames.size() < size_t(clip.FrameCount) * sourceJoints)
			return result;

		std::array<int32_t, MaxModelJoints> pairs{};
		for (uint32_t joint = 0; joint < targetJoints; joint++)
			pairs[joint] = JointIndex(from, to.Names[joint]);
		const int32_t sourceHips = JointIndex(from, "hips");
		const int32_t targetHips = JointIndex(to, "hips");

		Pose sourceRest;
		Pose targetRest;
		RestPose(from, sourceRest);
		RestPose(to, targetRest);
		JointMatrices sourceRestGlobals;
		JointMatrices targetRestGlobals;
		ComputeGlobals(from, sourceRest, sourceRestGlobals);
		ComputeGlobals(to, targetRest, targetRestGlobals);
		JointRotations sourceRestRotations;
		JointRotations targetRestRotations;
		GlobalRotations(from, sourceRest, sourceRestRotations);
		GlobalRotations(to, targetRest, targetRestRotations);
		float stride = 1.0f;
		if (sourceHips >= 0 && targetHips >= 0 && sourceRestGlobals[size_t(sourceHips)][3].y > 1e-3f)
			stride = targetRestGlobals[size_t(targetHips)][3].y / sourceRestGlobals[size_t(sourceHips)][3].y;

		std::array<float, MaxModelJoints> tuckSide{};
		std::array<glm::vec3, MaxModelJoints> boneAxis{};
		for (uint32_t joint = 0; joint < targetJoints && armTuck > 0.0f; joint++)
		{
			if (to.Names[joint].rfind("upperarm", 0) != 0)
				continue;
			for (uint32_t child = joint + 1; child < targetJoints; child++)
				if (to.Parents[child] == int32_t(joint) && glm::length(to.Rest[child].Translation) > 1e-4f)
				{
					boneAxis[joint] = glm::normalize(to.Rest[child].Translation);
					tuckSide[joint] = targetRestGlobals[joint][3].x >= 0.0f ? 1.0f : -1.0f;
					break;
				}
		}

		result.FrameCount = clip.FrameCount;
		result.Frames.resize(size_t(clip.FrameCount) * targetJoints);
		Pose source;
		JointMatrices sourceGlobals;
		JointMatrices targetGlobals;
		JointRotations sourceRotations;
		JointRotations deltas;
		JointRotations corrections;
		JointRotations targetRotations;
		for (uint32_t frame = 0; frame < clip.FrameCount; frame++)
		{
			const JointPose* frames = clip.Frames.data() + size_t(frame) * sourceJoints;
			std::copy(frames, frames + sourceJoints, source.begin());
			ComputeGlobals(from, source, sourceGlobals);
			GlobalRotations(from, source, sourceRotations);
			for (uint32_t joint = 0; joint < targetJoints; joint++)
			{
				const int32_t parent = to.Parents[joint];
				const int32_t paired = pairs[joint];
				if (paired >= 0)
					deltas[joint] = sourceRotations[size_t(paired)] * glm::inverse(sourceRestRotations[size_t(paired)]);
				else
					deltas[joint] = parent >= 0 ? deltas[size_t(parent)] : glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
				corrections[joint] = parent >= 0 ? corrections[size_t(parent)] : glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
				if (tuckSide[joint] != 0.0f && parent >= 0)
				{
					const glm::quat torso = deltas[size_t(parent)];
					const glm::vec3 direction = glm::inverse(torso) * (deltas[joint] * targetRestRotations[joint] * boneAxis[joint]);
					const float spread = std::atan2(tuckSide[joint] * direction.x, std::max(-direction.y, 1e-4f));
					const float hanging = std::clamp((-direction.y - 0.1f) / 0.6f, 0.0f, 1.0f);
					const float tuck = std::min(std::max(spread, 0.0f) * 0.5f, armTuck) * hanging * hanging * (3.0f - 2.0f * hanging);
					corrections[joint] = torso * glm::angleAxis(-tuckSide[joint] * tuck, glm::vec3(0.0f, 0.0f, 1.0f)) * glm::inverse(torso) * corrections[joint];
				}
				targetRotations[joint] = glm::normalize(corrections[joint] * deltas[joint] * targetRestRotations[joint]);

				JointPose pose = to.Rest[joint];
				pose.Rotation = parent >= 0 ? glm::normalize(glm::inverse(targetRotations[size_t(parent)]) * targetRotations[joint]) : targetRotations[joint];
				if (paired >= 0 && (parent < 0 || int32_t(joint) == targetHips))
				{
					const glm::vec3 offset = glm::vec3(sourceGlobals[size_t(paired)][3]) - glm::vec3(sourceRestGlobals[size_t(paired)][3]);
					const glm::vec3 position = glm::vec3(targetRestGlobals[joint][3]) + offset * stride;
					pose.Translation = parent >= 0 ? glm::vec3(glm::inverse(targetGlobals[size_t(parent)]) * glm::vec4(position, 1.0f)) : position;
				}
				const glm::mat4 matrix = glm::translate(glm::mat4(1.0f), pose.Translation) * glm::mat4_cast(pose.Rotation) * glm::scale(glm::mat4(1.0f), pose.Scale);
				targetGlobals[joint] = parent >= 0 ? targetGlobals[size_t(parent)] * matrix : matrix;
				result.Frames[size_t(frame) * targetJoints + joint] = pose;
			}
		}
		return result;
	}
}
