#include "SnapshotCodec.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace Game
{
	namespace
	{
		constexpr uint32_t MaxRecords = 4096;
		constexpr uint32_t MaxBlockSize = 16384;
		constexpr float NearRange = 20.0f;
		constexpr int32_t NearSlack = 2;
		constexpr int32_t FarSlack = 24;
		constexpr int32_t OrbSlack = 6;
		constexpr int32_t FarOrbSlack = 32;
		constexpr int32_t NearTurn = 32;
		constexpr int32_t FarTurn = 512;
		constexpr int32_t HitSlack = 4;
		constexpr int32_t WarpQuanta = int32_t(4.0f * NetPositionScale);
		constexpr int32_t MaxVelocity = 1 << 20;

		enum BodyBits : uint8_t
		{
			Removed = 1 << 0,
			Moved = 1 << 1,
			Turned = 1 << 2,
			Hurt = 1 << 3,
			Flagged = 1 << 4,
			Ident = 1 << 5,
			Extras = 1 << 6,
		};

		enum OrbBits : uint8_t
		{
			OrbRemoved = 1 << 0,
			OrbMoved = 1 << 1,
			OrbValued = 1 << 2,
		};

		const SnapshotFrame& EmptyFrame()
		{
			static const SnapshotFrame empty;
			return empty;
		}

		int32_t Extrapolate(int32_t anchor, int32_t velocity, uint32_t from, uint32_t to)
		{
			return int32_t(std::clamp<int64_t>(anchor + ((int64_t(velocity) * int64_t(to - from) + 8) >> 4), 0, 65535));
		}

		int32_t ExtrapolateX(const NetMotion& motion, uint32_t tick)
		{
			return Extrapolate(motion.X, motion.VelocityX, motion.Tick, tick);
		}

		int32_t ExtrapolateY(const NetMotion& motion, uint32_t tick)
		{
			return Extrapolate(motion.Y, motion.VelocityY, motion.Tick, tick);
		}

		Vec2 PositionAt(const NetMotion& motion, uint32_t tick)
		{
			return { DequantizePosition(uint16_t(ExtrapolateX(motion, tick))), DequantizePosition(uint16_t(ExtrapolateY(motion, tick))) };
		}

		template<typename T, typename Key>
		auto FindById(T& items, uint32_t id, Key key)
		{
			const auto it = std::lower_bound(items.begin(), items.end(), id, [&](const auto& item, uint32_t value) { return key(item) < value; });
			return it != items.end() && key(*it) == id ? &*it : nullptr;
		}

		// Re-anchors when the client's extrapolation drifts past the slack; the new velocity is the last tick's step.
		template<typename Sample>
		bool Track(NetMotion& motion, bool known, int32_t x, int32_t y, uint32_t tick, int32_t slack, const Sample* previous, uint32_t previousTick)
		{
			if (known && std::abs(x - ExtrapolateX(motion, tick)) <= slack && std::abs(y - ExtrapolateY(motion, tick)) <= slack)
				return false;
			int32_t vx = 0;
			int32_t vy = 0;
			if (previous && tick > previousTick && std::abs(x - previous->X) < WarpQuanta && std::abs(y - previous->Y) < WarpQuanta)
			{
				const float span = float(tick - previousTick);
				vx = std::clamp(int32_t(std::lround(float(x - previous->X) * 16.0f / span)), -MaxVelocity, MaxVelocity);
				vy = std::clamp(int32_t(std::lround(float(y - previous->Y) * 16.0f / span)), -MaxVelocity, MaxVelocity);
			}
			motion = { x, y, vx, vy, tick };
			return true;
		}

		void WriteMotion(ByteWriter& writer, const NetMotion& old, const NetMotion& motion, uint32_t tick)
		{
			writer.WriteSigned(motion.X - ExtrapolateX(old, tick));
			writer.WriteSigned(motion.Y - ExtrapolateY(old, tick));
			writer.WriteSigned(motion.VelocityX - old.VelocityX);
			writer.WriteSigned(motion.VelocityY - old.VelocityY);
		}

		bool ReadMotion(ByteReader& reader, const NetMotion& old, uint32_t tick, NetMotion& motion)
		{
			motion.X = ExtrapolateX(old, tick) + reader.ReadSigned();
			motion.Y = ExtrapolateY(old, tick) + reader.ReadSigned();
			motion.VelocityX = old.VelocityX + reader.ReadSigned();
			motion.VelocityY = old.VelocityY + reader.ReadSigned();
			motion.Tick = tick;
			return !reader.Failed() && motion.X >= 0 && motion.X <= 65535 && motion.Y >= 0 && motion.Y <= 65535 && std::abs(motion.VelocityX) <= MaxVelocity && std::abs(motion.VelocityY) <= MaxVelocity;
		}

		void WriteBlock(ByteWriter& writer, std::span<const uint8_t> base, std::span<const uint8_t> block)
		{
			const size_t size = block.size();
			const auto prior = [&](size_t i) { return i < base.size() ? base[i] : uint8_t(0); };
			const auto same = [&](size_t i) { return block[i] == prior(i); };
			writer.WriteVarint(uint32_t(size));
			size_t i = 0;
			while (i < size)
			{
				size_t start = i;
				while (start < size && same(start))
					start++;
				size_t end = start;
				while (end < size && !(same(end) && (end + 1 == size || same(end + 1))))
					end++;
				writer.WriteVarint(uint32_t(start - i));
				writer.WriteVarint(uint32_t(end - start));
				for (size_t k = start; k < end; k++)
					writer.Write(uint8_t(block[k] ^ prior(k)));
				i = end;
			}
		}

		bool ReadBlock(ByteReader& reader, std::span<const uint8_t> base, std::vector<uint8_t>& block)
		{
			const uint32_t size = reader.ReadVarint();
			if (reader.Failed() || size > MaxBlockSize)
				return false;
			block.assign(size, 0);
			std::copy_n(base.begin(), std::min<size_t>(base.size(), size), block.begin());
			size_t i = 0;
			while (i < size)
			{
				const uint32_t same = reader.ReadVarint();
				const uint32_t changed = reader.ReadVarint();
				if (reader.Failed() || (same == 0 && changed == 0) || same > size - i || changed > size - i - same)
					return false;
				i += same;
				const std::span<const uint8_t> bytes = reader.ReadBytes(changed);
				if (reader.Failed())
					return false;
				for (uint32_t k = 0; k < changed; k++)
					block[i + k] ^= bytes[k];
				i += changed;
			}
			return true;
		}

		NetBody BodyOf(const EntityState& state)
		{
			return {
				.NetId = state.NetId,
				.Kind = state.Kind,
				.Owner = QuantizeOwner(state.Owner),
				.Tag = state.Tag,
				.Variant = state.Variant,
				.Scale = state.Scale,
				.Health = state.Health,
				.Flags = state.Flags,
				.Charge = state.Charge,
				.Ward = state.Ward,
				.CastX = QuantizePosition(state.CastPoint.x),
				.CastY = QuantizePosition(state.CastPoint.y),
				.CastAim = QuantizeAngle(state.CastAim),
				.Facing = QuantizeAngle(state.Facing),
			};
		}

		EntityState StateOf(const NetBody& body, uint32_t tick)
		{
			return {
				.NetId = body.NetId,
				.Kind = body.Kind,
				.Position = PositionAt(body.Motion, tick),
				.Facing = DequantizeAngle(body.Facing),
				.Owner = DequantizeOwner(body.Owner),
				.Tag = body.Tag,
				.Variant = body.Variant,
				.Scale = body.Scale,
				.Health = body.Health,
				.Flags = body.Flags,
				.Charge = body.Charge,
				.Ward = body.Ward,
				.CastPoint = { DequantizePosition(body.CastX), DequantizePosition(body.CastY) },
				.CastAim = DequantizeAngle(body.CastAim),
			};
		}

		bool SameIdent(const NetBody& a, const NetBody& b)
		{
			return a.Kind == b.Kind && a.Owner == b.Owner && a.Tag == b.Tag && a.Variant == b.Variant && a.Scale == b.Scale;
		}

		bool SameExtras(const NetBody& a, const NetBody& b)
		{
			return a.Charge == b.Charge && a.Ward == b.Ward && a.CastX == b.CastX && a.CastY == b.CastY && a.CastAim == b.CastAim;
		}

		uint8_t HitFlags(const HitEvent& hit)
		{
			return uint8_t((hit.Critical ? 1 : 0) | (hit.OnPlayer ? 2 : 0) | (uint32_t(hit.Damages) << 2) | (hit.Guarded ? 0x40 : 0) | (hit.Warded ? 0x80 : 0));
		}
	}

	void MergeHits(std::vector<HitEvent>& hits)
	{
		std::vector<HitEvent> merged;
		merged.reserve(hits.size());
		for (const HitEvent& hit : hits)
		{
			const auto same = std::find_if(merged.begin(), merged.end(), [&](const HitEvent& other)
			{
				return other.Target == hit.Target && other.Attacker == hit.Attacker && other.Ability == hit.Ability && other.Damages == hit.Damages && other.OnPlayer == hit.OnPlayer && other.Guarded == hit.Guarded && other.Warded == hit.Warded;
			});
			if (same == merged.end())
			{
				merged.push_back(hit);
				continue;
			}
			same->Damage = uint16_t(std::min(uint32_t(same->Damage) + hit.Damage, 65535u));
			same->Critical = same->Critical || hit.Critical;
		}
		hits = std::move(merged);
	}

	void SnapshotEncoder::Reset()
	{
		m_Frames = {};
		m_Acked = 0;
		m_SampleTick = 0;
		m_Bodies.clear();
		m_Orbs.clear();
	}

	std::vector<uint8_t> SnapshotEncoder::Encode(const ServerSnapshot& snapshot, Vec2 focus)
	{
		const uint32_t tick = snapshot.Tick;
		const SnapshotFrame& acked = m_Frames[m_Acked % Depth];
		const bool delta = acked.Valid && acked.Tick == m_Acked && acked.Epoch == snapshot.Epoch && tick > m_Acked && tick - m_Acked < Depth;
		const SnapshotFrame& base = delta ? acked : EmptyFrame();
		SnapshotFrame& frame = m_Frames[tick % Depth];
		frame.Valid = true;
		frame.Epoch = snapshot.Epoch;
		frame.Tick = tick;
		ByteWriter block;
		WriteSnapshotBlock(block, snapshot);
		frame.Block = block.Take();

		ByteWriter writer;
		writer.Write(MessageType::ServerSnapshot);
		writer.Write(snapshot.Epoch);
		writer.Write(tick);
		writer.Write(uint8_t(delta ? tick - m_Acked : 0));
		writer.Write(snapshot.LastProcessedInput);
		WriteBlock(writer, base.Block, frame.Block);

		std::vector<const EntityState*> sorted;
		sorted.reserve(snapshot.Entities.size());
		for (const EntityState& state : snapshot.Entities)
			sorted.push_back(&state);
		std::sort(sorted.begin(), sorted.end(), [](const EntityState* a, const EntityState* b) { return a->NetId < b->NetId; });
		sorted.erase(std::unique(sorted.begin(), sorted.end(), [](const EntityState* a, const EntityState* b) { return a->NetId == b->NetId; }), sorted.end());
		if (sorted.size() > MaxRecords)
			sorted.resize(MaxRecords);

		const auto nearby = [&](Vec2 position) { return LengthSquared(position - focus) <= NearRange * NearRange; };
		const auto sampleOf = [](const std::vector<Sample>& samples, uint32_t id) { return FindById(samples, id, [](const Sample& sample) { return sample.Id; }); };
		ByteWriter records;
		uint32_t count = 0;
		uint32_t last = 0;
		const auto record = [&](uint32_t id, uint8_t mask)
		{
			records.WriteVarint(id - last);
			records.Write(mask);
			last = id;
			count++;
		};

		frame.Bodies.clear();
		m_NextBodies.clear();
		size_t b = 0;
		for (const EntityState* state : sorted)
		{
			while (b < base.Bodies.size() && base.Bodies[b].NetId < state->NetId)
				record(base.Bodies[b++].NetId, Removed);
			const bool known = b < base.Bodies.size() && base.Bodies[b].NetId == state->NetId;
			const NetBody old = known ? base.Bodies[b++] : NetBody{ .NetId = state->NetId, .Motion = { .Tick = tick } };
			const bool near = state->Kind == EntityKind::Player || nearby(state->Position);
			const int32_t x = QuantizePosition(state->Position.x);
			const int32_t y = QuantizePosition(state->Position.y);
			m_NextBodies.push_back({ state->NetId, x, y });

			NetBody body = BodyOf(*state);
			body.Motion = old.Motion;
			uint8_t mask = 0;
			if (Track(body.Motion, known, x, y, tick, near ? NearSlack : FarSlack, sampleOf(m_Bodies, state->NetId), m_SampleTick))
				mask |= Moved;
			if (std::abs(int32_t(int16_t(uint16_t(body.Facing - old.Facing)))) > (near ? NearTurn : FarTurn) || (!known && body.Facing != old.Facing))
				mask |= Turned;
			else
				body.Facing = old.Facing;
			mask |= body.Health != old.Health ? Hurt : 0;
			mask |= body.Flags != old.Flags ? Flagged : 0;
			mask |= SameIdent(body, old) ? 0 : Ident;
			mask |= SameExtras(body, old) ? 0 : Extras;
			frame.Bodies.push_back(body);
			if (mask == 0)
				continue;
			record(body.NetId, mask);
			if (mask & Moved)
				WriteMotion(records, old.Motion, body.Motion, tick);
			if (mask & Turned)
				records.WriteSigned(int16_t(uint16_t(body.Facing - old.Facing)));
			if (mask & Hurt)
				records.Write(body.Health);
			if (mask & Flagged)
				records.WriteVarint(body.Flags);
			if (mask & Ident)
			{
				records.Write(body.Kind);
				records.Write(body.Owner);
				records.WriteVarint(body.Tag);
				records.Write(body.Variant);
				records.Write(body.Scale);
			}
			if (mask & Extras)
			{
				records.Write(body.Charge);
				records.Write(body.Ward);
				records.Write(body.CastX);
				records.Write(body.CastY);
				records.Write(body.CastAim);
			}
		}
		while (b < base.Bodies.size())
			record(base.Bodies[b++].NetId, Removed);
		writer.WriteVarint(count);
		writer.WriteBytes(records.GetData());

		const size_t hits = std::min<size_t>(snapshot.Hits.size(), MaxRecords);
		writer.WriteVarint(uint32_t(hits));
		for (size_t i = 0; i < hits; i++)
		{
			const HitEvent& hit = snapshot.Hits[i];
			const NetBody* target = FindById(frame.Bodies, hit.Target, [](const NetBody& body) { return body.NetId; });
			const bool placed = target && std::abs(int32_t(QuantizePosition(hit.Position.x)) - ExtrapolateX(target->Motion, tick)) <= HitSlack && std::abs(int32_t(QuantizePosition(hit.Position.y)) - ExtrapolateY(target->Motion, tick)) <= HitSlack;
			const bool attacker = hit.Attacker < MaxPlayers;
			const bool ability = hit.Ability != UINT8_MAX;
			writer.WriteVarint(hit.Target);
			writer.WriteVarint(hit.Damage);
			writer.Write(HitFlags(hit));
			writer.Write(uint8_t((placed ? 0 : 1) | (attacker ? 2 : 0) | (ability ? 4 : 0)));
			if (attacker)
				writer.Write(uint8_t(hit.Attacker));
			if (ability)
				writer.Write(hit.Ability);
			if (!placed)
			{
				writer.Write(QuantizePosition(hit.Position.x));
				writer.Write(QuantizePosition(hit.Position.y));
			}
		}

		frame.Orbs.clear();
		m_NextOrbs.clear();
		if (snapshot.Status.Mode == GameMode::Shardfall)
		{
			std::vector<const Orb*> orbs;
			orbs.reserve(snapshot.Orbs.size());
			for (const Orb& orb : snapshot.Orbs)
				orbs.push_back(&orb);
			std::sort(orbs.begin(), orbs.end(), [](const Orb* a, const Orb* b) { return a->Id < b->Id; });
			orbs.erase(std::unique(orbs.begin(), orbs.end(), [](const Orb* a, const Orb* b) { return a->Id == b->Id; }), orbs.end());
			if (orbs.size() > MaxRecords)
				orbs.resize(MaxRecords);
			records = {};
			count = 0;
			last = 0;
			size_t o = 0;
			for (const Orb* orb : orbs)
			{
				while (o < base.Orbs.size() && base.Orbs[o].Id < orb->Id)
					record(base.Orbs[o++].Id, OrbRemoved);
				const bool known = o < base.Orbs.size() && base.Orbs[o].Id == orb->Id;
				const NetOrb old = known ? base.Orbs[o++] : NetOrb{ .Id = orb->Id, .Motion = { .Tick = tick } };
				const int32_t x = QuantizePosition(orb->Position.x);
				const int32_t y = QuantizePosition(orb->Position.y);
				m_NextOrbs.push_back({ orb->Id, x, y });
				NetOrb next{ .Id = orb->Id, .Value = orb->Value, .Motion = old.Motion };
				uint8_t mask = 0;
				if (Track(next.Motion, known, x, y, tick, nearby(orb->Position) ? OrbSlack : FarOrbSlack, sampleOf(m_Orbs, orb->Id), m_SampleTick))
					mask |= OrbMoved;
				mask |= next.Value != old.Value ? OrbValued : 0;
				frame.Orbs.push_back(next);
				if (mask == 0)
					continue;
				record(next.Id, mask);
				if (mask & OrbMoved)
					WriteMotion(records, old.Motion, next.Motion, tick);
				if (mask & OrbValued)
					records.WriteVarint(next.Value);
			}
			while (o < base.Orbs.size())
				record(base.Orbs[o++].Id, OrbRemoved);
			writer.WriteVarint(count);
			writer.WriteBytes(records.GetData());
		}

		m_Bodies.swap(m_NextBodies);
		m_Orbs.swap(m_NextOrbs);
		m_SampleTick = tick;
		return writer.Take();
	}

	void SnapshotDecoder::Reset()
	{
		m_Frames = {};
		m_Latest = 0;
		m_LatestEpoch = 0;
	}

	bool SnapshotDecoder::Fail()
	{
		m_Failures++;
		return false;
	}

	bool SnapshotDecoder::Decode(std::span<const uint8_t> data, ServerSnapshot& snapshot)
	{
		ByteReader reader(data);
		if (reader.Read<MessageType>() != MessageType::ServerSnapshot)
			return Fail();
		snapshot.Epoch = reader.Read<uint32_t>();
		snapshot.Tick = reader.Read<uint32_t>();
		const uint8_t age = reader.Read<uint8_t>();
		snapshot.LastProcessedInput = reader.Read<uint32_t>();
		if (reader.Failed() || age >= Depth)
			return Fail();
		const uint32_t tick = snapshot.Tick;
		const SnapshotFrame* base = &EmptyFrame();
		if (age > 0)
		{
			const uint32_t from = tick - age;
			base = &m_Frames[from % Depth];
			if (!base->Valid || base->Tick != from || base->Epoch != snapshot.Epoch)
				return Fail();
		}

		SnapshotFrame frame{ .Valid = true, .Epoch = snapshot.Epoch, .Tick = tick };
		if (!ReadBlock(reader, base->Block, frame.Block))
			return Fail();
		ByteReader block(frame.Block);
		if (!ReadSnapshotBlock(block, snapshot) || !block.AtEnd())
			return Fail();

		const uint32_t count = reader.ReadVarint();
		if (reader.Failed() || count > MaxRecords * 2)
			return Fail();
		frame.Bodies.reserve(base->Bodies.size() + count);
		size_t b = 0;
		uint32_t last = 0;
		for (uint32_t i = 0; i < count; i++)
		{
			const uint32_t step = reader.ReadVarint();
			const uint32_t id = last + step;
			if (reader.Failed() || (i > 0 && step == 0) || id < last)
				return Fail();
			last = id;
			while (b < base->Bodies.size() && base->Bodies[b].NetId < id)
				frame.Bodies.push_back(base->Bodies[b++]);
			const bool known = b < base->Bodies.size() && base->Bodies[b].NetId == id;
			const NetBody old = known ? base->Bodies[b++] : NetBody{ .NetId = id, .Motion = { .Tick = tick } };
			const uint8_t mask = reader.Read<uint8_t>();
			if (mask & Removed)
			{
				if (!known || mask != Removed)
					return Fail();
				continue;
			}
			NetBody body = old;
			if ((mask & Moved) && !ReadMotion(reader, old.Motion, tick, body.Motion))
				return Fail();
			if (mask & Turned)
				body.Facing = uint16_t(old.Facing + reader.ReadSigned());
			if (mask & Hurt)
				body.Health = reader.Read<uint8_t>();
			if (mask & Flagged)
				body.Flags = reader.ReadVarint();
			if (mask & Ident)
			{
				body.Kind = reader.Read<EntityKind>();
				body.Owner = reader.Read<uint8_t>();
				body.Tag = uint16_t(reader.ReadVarint());
				body.Variant = reader.Read<uint8_t>();
				body.Scale = reader.Read<uint8_t>();
				if (uint8_t(body.Kind) >= uint8_t(EntityKind::Count))
					return Fail();
			}
			if (mask & Extras)
			{
				body.Charge = reader.Read<uint8_t>();
				body.Ward = reader.Read<uint8_t>();
				body.CastX = reader.Read<uint16_t>();
				body.CastY = reader.Read<uint16_t>();
				body.CastAim = reader.Read<uint16_t>();
			}
			frame.Bodies.push_back(body);
		}
		while (b < base->Bodies.size())
			frame.Bodies.push_back(base->Bodies[b++]);
		if (frame.Bodies.size() > MaxRecords)
			return Fail();
		snapshot.Entities.resize(frame.Bodies.size());
		for (size_t i = 0; i < frame.Bodies.size(); i++)
			snapshot.Entities[i] = StateOf(frame.Bodies[i], tick);

		const uint32_t hits = reader.ReadVarint();
		if (reader.Failed() || hits > MaxRecords)
			return Fail();
		snapshot.Hits.resize(hits);
		for (HitEvent& hit : snapshot.Hits)
		{
			hit.Target = reader.ReadVarint();
			hit.Damage = uint16_t(std::min(reader.ReadVarint(), 65535u));
			const uint8_t flags = reader.Read<uint8_t>();
			hit.Critical = (flags & 1) != 0;
			hit.OnPlayer = (flags & 2) != 0;
			hit.Damages = Element(std::min<uint32_t>((flags >> 2) & 0xFu, uint32_t(Element::Count) - 1));
			hit.Guarded = (flags & 0x40) != 0;
			hit.Warded = (flags & 0x80) != 0;
			const uint8_t layout = reader.Read<uint8_t>();
			hit.Attacker = NoPlayer;
			if (layout & 2)
			{
				const uint8_t attacker = reader.Read<uint8_t>();
				hit.Attacker = attacker < MaxPlayers ? attacker : NoPlayer;
			}
			hit.Ability = (layout & 4) ? reader.Read<uint8_t>() : uint8_t(UINT8_MAX);
			if (layout & 1)
			{
				hit.Position.x = DequantizePosition(reader.Read<uint16_t>());
				hit.Position.y = DequantizePosition(reader.Read<uint16_t>());
			}
			else if (const NetBody* target = FindById(frame.Bodies, hit.Target, [](const NetBody& body) { return body.NetId; }))
				hit.Position = PositionAt(target->Motion, tick);
		}

		snapshot.Orbs.clear();
		if (snapshot.Status.Mode == GameMode::Shardfall)
		{
			const uint32_t orbs = reader.ReadVarint();
			if (reader.Failed() || orbs > MaxRecords * 2)
				return Fail();
			size_t o = 0;
			last = 0;
			for (uint32_t i = 0; i < orbs; i++)
			{
				const uint32_t step = reader.ReadVarint();
				const uint32_t id = last + step;
				if (reader.Failed() || (i > 0 && step == 0) || id > UINT16_MAX)
					return Fail();
				last = id;
				while (o < base->Orbs.size() && base->Orbs[o].Id < id)
					frame.Orbs.push_back(base->Orbs[o++]);
				const bool known = o < base->Orbs.size() && base->Orbs[o].Id == id;
				const NetOrb old = known ? base->Orbs[o++] : NetOrb{ .Id = uint16_t(id), .Motion = { .Tick = tick } };
				const uint8_t mask = reader.Read<uint8_t>();
				if (mask & OrbRemoved)
				{
					if (!known || mask != OrbRemoved)
						return Fail();
					continue;
				}
				NetOrb orb = old;
				if ((mask & OrbMoved) && !ReadMotion(reader, old.Motion, tick, orb.Motion))
					return Fail();
				if (mask & OrbValued)
					orb.Value = uint16_t(std::min(reader.ReadVarint(), 65535u));
				frame.Orbs.push_back(orb);
			}
			while (o < base->Orbs.size())
				frame.Orbs.push_back(base->Orbs[o++]);
			if (frame.Orbs.size() > MaxRecords)
				return Fail();
			snapshot.Orbs.reserve(frame.Orbs.size());
			for (const NetOrb& orb : frame.Orbs)
				snapshot.Orbs.push_back({ .Id = orb.Id, .Position = PositionAt(orb.Motion, tick), .Value = orb.Value });
		}
		if (reader.Failed() || !reader.AtEnd())
			return Fail();

		if (snapshot.Epoch != m_LatestEpoch || tick > m_Latest)
		{
			m_Latest = tick;
			m_LatestEpoch = snapshot.Epoch;
		}
		m_Frames[tick % Depth] = std::move(frame);
		return true;
	}
}
