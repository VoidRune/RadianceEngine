#pragma once
#include "GameShared/Core/Math.h"
#include <algorithm>
#include <cmath>
#include <iterator>

namespace Game
{
	inline constexpr Vec2 EmberCenter{ 54.5f, 35.5f };
	inline constexpr float EmberRadius = 16.6f;
	inline constexpr float EmberHarmonics[] = { 2.0f, 0.5f, -0.35f, 5.0f, 0.12f, -0.18f, 7.0f, -0.2f, 0.1f, 11.0f, 0.12f, 0.1f, 13.0f, -0.08f, 0.12f };
	inline constexpr float EmberLobes[] = {
		262.0f, 34.0f, 2.4f,
		228.0f, 12.0f, 1.0f,
		196.0f, 18.0f, -0.7f,
		178.0f, 10.0f, 0.8f,
		145.0f, 20.0f, 2.0f,
		116.0f, 11.0f, -1.8f,
		92.0f, 16.0f, 1.0f,
		66.0f, 18.0f, -0.9f,
		44.0f, 5.0f, 0.9f,
		26.0f, 7.0f, -2.6f,
		0.0f, 12.0f, 1.2f,
		338.0f, 22.0f, -1.2f,
		306.0f, 9.0f, -1.6f,
	};
	inline constexpr float EmberRimLevel = -0.3f;
	inline constexpr float EmberRimBlend = 1.6f;
	inline constexpr float EmberRimSlope = 1.0f;
	inline constexpr float EmberWalkInset = 0.45f;

	inline constexpr float EmberGlassRadius = 4.4f;
	inline constexpr float EmberRingInner = 4.4f;
	inline constexpr float EmberRingOuter = 5.05f;
	inline constexpr float EmberHoleRadius = 4.85f;
	inline constexpr float EmberSurroundRadius = 7.95f;
	inline constexpr float EmberHeartDepth = 3.9f;
	inline constexpr float EmberHeartRadius = 1.35f;
	inline constexpr float EmberHarnessRadius = 1.86f;
	inline constexpr float EmberCavernDepths[] = { 0.0f, 0.6f, 1.4f, 2.6f, 4.4f, 6.6f, 8.8f, 11.0f, 12.8f, 14.5f };
	inline constexpr float EmberCavernRadii[] = { 4.85f, 5.0f, 6.4f, 7.5f, 8.1f, 7.4f, 6.2f, 4.8f, 3.8f, 3.0f };
	inline constexpr float EmberCavernLobes[] = { 2.0f, 0.06f, -0.04f, 3.0f, 0.05f, 0.05f, 5.0f, -0.03f, 0.025f };
	inline constexpr float EmberChainAzimuths[] = { 45.0f, 135.0f, 225.0f, 315.0f };
	inline constexpr float EmberChainDepth = 1.7f;
	inline constexpr float EmberRuneTiles = 5.0f;
	inline constexpr float EmberRuneInner = 5.062f;
	inline constexpr float EmberRuneOuter = 5.832f;
	inline constexpr float EmberMonolithAzimuths[] = { 27.0f, 63.0f, 117.0f, 153.0f, 207.0f, 243.0f, 297.0f, 333.0f };
	inline constexpr float EmberMonolithDepth = 3.3f;
	inline constexpr float EmberMonolithWidth = 1.15f;
	inline constexpr float EmberMonolithHeight = 2.1f;
	inline constexpr float EmberMonolithThickness = 1.0f;
	inline constexpr float EmberLavaFalls[] = { 80.0f, 12.0f, 168.0f };
	inline constexpr float EmberSpoutDepth = 1.5f;

	inline constexpr Vec2 EmberPadOffset{ 11.4f, 0.0f };
	inline constexpr float EmberPadRadius = 2.7f;
	inline constexpr float EmberCourtRadius = 10.4f;
	inline constexpr Vec2 EmberGateOffset{ 0.0f, 11.0f };
	inline constexpr Vec2 EmberMapleOffset{ -9.6f, 8.8f };
	inline constexpr Vec2 EmberBoardOffset{ 3.6f, -11.2f };
	inline constexpr Vec2 EmberWishOffset{ 11.6f, 10.4f };
	inline constexpr Vec2 EmberTrainingOffset{ 9.2f, -9.6f };

	inline constexpr float EmberPaths[] = {
		13.4f, 0.2f, 15.0f, 1.5f, 16.1f, -0.4f, 17.8f, 0.5f, 1.1f,
		-13.4f, -0.2f, -15.0f, -1.4f, -16.3f, 0.5f, -17.8f, -0.1f, 1.1f,
		0.3f, -9.9f, -1.6f, -12.8f, 1.8f, -15.4f, 0.1f, -19.2f, 1.35f,
		6.8f, -7.7f, 7.6f, -8.3f, 8.2f, -9.2f, 9.2f, -9.6f, 0.95f,
		7.3f, 7.4f, 8.9f, 9.6f, 10.0f, 9.1f, 11.5f, 10.3f, 0.85f,
	};
	inline constexpr float EmberPlazas[] = {
		3.6f, -11.2f, 1.6f,
		9.2f, -9.6f, 1.8f,
		11.5f, 10.4f, 1.15f,
	};
	inline constexpr int EmberPathSamples = 32;

	inline constexpr float EmberMounds[] = {
		-3.6f, -16.0f, 3.2f, 0.55f,
		5.2f, -15.6f, 2.6f, 0.45f,
		-11.0f, -11.4f, 3.4f, 0.8f,
		-14.6f, -4.0f, 2.8f, 0.6f,
		-14.8f, 4.6f, 2.4f, 0.5f,
		-11.6f, 13.0f, 3.0f, 0.5f,
		-4.8f, 14.8f, 2.8f, 0.6f,
		4.2f, 13.8f, 2.6f, 0.55f,
		13.8f, 4.6f, 2.2f, 0.4f,
		13.6f, -5.6f, 2.6f, 0.55f,
		-8.2f, -8.2f, 2.0f, -0.14f,
		8.6f, 8.4f, 2.2f, -0.12f,
	};
	inline constexpr float EmberOutcrops[] = {
		-10.4f, -10.8f, 1.4f, 1.6f,
		13.0f, -6.5f, 1.3f, 1.4f,
		-14.0f, 5.0f, 1.1f, 1.2f,
		5.0f, 13.2f, 1.0f, 1.0f,
		-3.6f, 13.6f, 1.0f, 1.0f,
		-4.2f, -15.6f, 1.2f, 1.4f,
		5.4f, -14.6f, 1.1f, 1.2f,
		-13.6f, -5.4f, 1.1f, 1.3f,
	};
	inline constexpr float EmberOutcropBlock = 0.62f;
	inline constexpr float EmberFlatBlend = 1.6f;
	inline constexpr float EmberFootLift = 0.02f;

	inline float EmberEdge(float angle)
	{
		float radius = EmberRadius;
		for (size_t i = 0; i + 2 < std::size(EmberHarmonics); i += 3)
			radius += EmberHarmonics[i + 1] * std::cos(EmberHarmonics[i] * angle) + EmberHarmonics[i + 2] * std::sin(EmberHarmonics[i] * angle);
		const float degrees = angle * 180.0f / Pi;
		for (size_t i = 0; i + 2 < std::size(EmberLobes); i += 3)
		{
			const float delta = std::remainder(degrees - EmberLobes[i], 360.0f) / EmberLobes[i + 1];
			radius += EmberLobes[i + 2] * std::exp(-delta * delta);
		}
		return radius;
	}

	inline Vec2 EmberPathPoint(size_t path, float t)
	{
		const float* p = EmberPaths + path * 9;
		const float u = 1.0f - t;
		const float a = u * u * u;
		const float b = 3.0f * u * u * t;
		const float c = 3.0f * u * t * t;
		const float d = t * t * t;
		return { a * p[0] + b * p[2] + c * p[4] + d * p[6], a * p[1] + b * p[3] + c * p[5] + d * p[7] };
	}

	inline float EmberPathDistance(Vec2 local)
	{
		float best = 1e9f;
		for (size_t path = 0; path * 9 + 8 < std::size(EmberPaths); path++)
		{
			Vec2 previous = EmberPathPoint(path, 0.0f);
			float nearest = 1e9f;
			for (int s = 1; s <= EmberPathSamples; s++)
			{
				const Vec2 next = EmberPathPoint(path, float(s) / float(EmberPathSamples));
				const Vec2 span = next - previous;
				const float t = std::clamp(Dot(local - previous, span) / std::max(LengthSquared(span), 1e-6f), 0.0f, 1.0f);
				nearest = std::min(nearest, Length(local - (previous + span * t)));
				previous = next;
			}
			best = std::min(best, nearest - EmberPaths[path * 9 + 8]);
		}
		return best;
	}

	inline float EmberEdgeDistance(Vec2 position)
	{
		const Vec2 offset = position - EmberCenter;
		return EmberEdge(std::atan2(offset.y, offset.x)) - Length(offset);
	}

	inline float EmberFlatness(Vec2 local)
	{
		const float r = Length(local);
		float distance = r - (EmberCourtRadius + 0.4f);
		distance = std::min(distance, EmberPathDistance(local) - 0.3f);
		for (size_t i = 0; i + 2 < std::size(EmberPlazas); i += 3)
			distance = std::min(distance, Length(local - Vec2(EmberPlazas[i], EmberPlazas[i + 1])) - EmberPlazas[i + 2] - 0.3f);
		const struct { Vec2 Center; float Radius; } zones[] = {
			{ EmberPadOffset, EmberPadRadius + 0.5f },
			{ -EmberPadOffset, EmberPadRadius + 0.5f },
			{ EmberGateOffset, 4.8f },
			{ EmberMapleOffset, 3.4f },
			{ EmberBoardOffset, 1.5f },
			{ EmberWishOffset, 1.9f },
			{ EmberTrainingOffset, 1.9f },
		};
		for (const auto& zone : zones)
			distance = std::min(distance, Length(local - zone.Center) - zone.Radius);
		const float t = std::clamp(distance / EmberFlatBlend, 0.0f, 1.0f);
		return t * t * (3.0f - 2.0f * t);
	}

	inline float EmberBump(Vec2 local, float x, float y, float radius, float height)
	{
		const float d = LengthSquared(local - Vec2(x, y)) / (radius * radius);
		if (d >= 1.0f)
			return 0.0f;
		return height * (1.0f - d) * (1.0f - d);
	}

	inline float EmberRim(Vec2 local, float height)
	{
		const float inside = EmberEdge(std::atan2(local.y, local.x)) - Length(local);
		if (inside < 0.0f)
			return EmberRimLevel + EmberRimSlope * inside;
		const float t = std::clamp(inside / EmberRimBlend, 0.0f, 1.0f);
		return EmberRimLevel + (height - EmberRimLevel) * t * t * (3.0f - 2.0f * t);
	}

	inline float EmberGround(Vec2 local)
	{
		const float flat = EmberFlatness(local);
		if (flat <= 0.0f)
			return EmberRim(local, 0.0f);
		float height = 0.05f * std::sin(1.3f * local.x + 0.7f) * std::sin(1.1f * local.y - 0.4f) + 0.03f * std::sin(2.9f * local.x - 1.7f * local.y);
		for (size_t i = 0; i + 3 < std::size(EmberMounds); i += 4)
			height += EmberBump(local, EmberMounds[i], EmberMounds[i + 1], EmberMounds[i + 2], EmberMounds[i + 3]);
		for (size_t i = 0; i + 3 < std::size(EmberOutcrops); i += 4)
			height += EmberBump(local, EmberOutcrops[i], EmberOutcrops[i + 1], EmberOutcrops[i + 2] + 1.6f, 0.45f);
		return EmberRim(local, flat * height);
	}
}
