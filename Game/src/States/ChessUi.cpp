#include "SessionState.h"
#include "App/GameApp.h"
#include "Audio/GameAudio.h"
#include "Rendering/Characters.h"
#include "Rendering/WorldRenderer.h"
#include "UI/Ui.h"
#include <algorithm>
#include <cmath>

namespace Game
{
	namespace
	{
		constexpr int ChessPayload = 20;
		constexpr float SquarePixels = 56.0f;
		constexpr float GlideSeconds = 0.42f;
		constexpr float SinkSeconds = 0.35f;
		constexpr float PendingSeconds = 1.5f;
		constexpr float HearingRange = 10.0f;
		constexpr Chess::Piece PromotionChoices[] = { Chess::Piece::Queen, Chess::Piece::Rook, Chess::Piece::Bishop, Chess::Piece::Knight };
		constexpr Chess::Piece MaterialOrder[] = { Chess::Piece::Queen, Chess::Piece::Rook, Chess::Piece::Bishop, Chess::Piece::Knight, Chess::Piece::Pawn };
		const UiColor LightSquare = Hex(0xD9CEB6);
		const UiColor DarkSquare = Hex(0x6B6391);

		UiIcon PieceIcon(Chess::Piece piece)
		{
			switch (piece)
			{
			case Chess::Piece::King:
				return UiIcon::ChessKing;
			case Chess::Piece::Queen:
				return UiIcon::ChessQueen;
			case Chess::Piece::Rook:
				return UiIcon::ChessRook;
			case Chess::Piece::Bishop:
				return UiIcon::ChessBishop;
			case Chess::Piece::Knight:
				return UiIcon::ChessKnight;
			default:
				return UiIcon::ChessPawn;
			}
		}

		IconStyle PieceStyle(Chess::Side side, float alpha = 1.0f)
		{
			if (side == Chess::Side::White)
				return { .Color = Hex(0xFBF6EA, alpha), .ColorBottom = Hex(0xD3C7AC, alpha), .Outline = 1.1f, .OutlineColor = Rgba(0.08f, 0.06f, 0.1f, 0.9f * alpha), .ShadowColor = Rgba(0.0f, 0.0f, 0.0f, 0.45f * alpha) };
			return { .Color = Hex(0x3B3447, alpha), .ColorBottom = Hex(0x16111E, alpha), .Outline = 1.1f, .OutlineColor = Rgba(0.93f, 0.88f, 0.78f, 0.8f * alpha), .ShadowColor = Rgba(0.0f, 0.0f, 0.0f, 0.45f * alpha) };
		}

		const char* SideName(uint32_t side)
		{
			return side == 0 ? "White" : "Black";
		}

		std::string Verdict(const Chess::Match& match)
		{
			const std::string winner = match.Winner < 2 ? SideName(match.Winner) : "";
			const std::string loser = match.Winner < 2 ? SideName(1u - match.Winner) : "";
			switch (match.Result)
			{
			case Chess::Verdict::Checkmate:
				return "Checkmate. " + winner + " wins.";
			case Chess::Verdict::Resigned:
				return loser + " resigned. " + winner + " wins.";
			case Chess::Verdict::Abandoned:
				return loser + " left the table. " + winner + " wins.";
			case Chess::Verdict::Stalemate:
				return "Stalemate. The game is drawn.";
			case Chess::Verdict::Material:
				return "Draw. Neither side has enough pieces left to checkmate.";
			case Chess::Verdict::FiftyMoves:
				return "Draw by the fifty-move rule.";
			case Chess::Verdict::Repetition:
				return "Draw. The same position came up three times.";
			case Chess::Verdict::Agreed:
				return "The players agreed to a draw.";
			case Chess::Verdict::MoveLimit:
				return "Draw. The game reached the move limit.";
			default:
				return "";
			}
		}

		float Glide(float t)
		{
			t = std::clamp(t, 0.0f, 1.0f);
			return t * t * (3.0f - 2.0f * t);
		}

		bool Unpack(const Chess::Position& position, uint16_t packed, Chess::Move& move)
		{
			const std::optional<Chess::Move> found = Chess::FindLegal(position, uint8_t(packed & 63u), uint8_t((packed >> 6) & 63u), Chess::Piece((packed >> 12) & 7u));
			if (found)
				move = *found;
			return found.has_value();
		}

		void PlayMoveSound(GameAudio& audio, const Chess::Move& move, const Chess::Position& after, float volume, bool threatened, float delay)
		{
			const Cue cue = move.Promotion != Chess::Piece::None ? Cue::ChessPromote : (move.Flags & Chess::MoveCapture) != 0 ? Cue::ChessCapture : Cue::ChessMove;
			audio.PlayUi(cue, volume, 1.0f, delay);
			if (move.Flags & Chess::MoveCastle)
				audio.PlayUi(Cue::ChessMove, volume * 0.8f, 1.06f, delay + 0.15f);
			std::vector<Chess::Move> replies;
			Chess::LegalMoves(after, replies);
			if (Chess::InCheck(after) && !replies.empty())
				audio.PlayUi(Cue::ChessCheck, threatened ? 1.0f : 0.6f * volume, 1.0f, delay + 0.1f);
		}
	}

	Chess::Position SessionState::ShownChessPosition() const
	{
		const Chess::Match& match = m_Client.GetChess();
		if (m_ChessPending && m_ChessPending->Revision == match.Revision)
			return Chess::Play(match.Board, m_ChessPending->Move);
		return match.Board;
	}

	void SessionState::CommitChessMove(uint8_t from, uint8_t to, Chess::Piece promotion)
	{
		const Chess::Match& match = m_Client.GetChess();
		m_ChessPick = -1;
		m_ChessPromotion.reset();
		const std::optional<Chess::Move> move = Chess::FindLegal(match.Board, from, to, promotion);
		if (!move)
			return;
		m_Client.PlayChess(Chess::Command::Move, Chess::PackMove(*move));
		m_ChessPending = ChessPending{ *move, match.Revision, m_Time };
		m_ChessVoiced = std::pair(match.Moves.size() + 1, Chess::PackMove(*move));
		const Chess::Position after = Chess::Play(match.Board, *move);
		const uint8_t me = uint8_t(std::min<uint32_t>(m_Client.GetPlayerId(), Chess::NoSeat));
		PlayMoveSound(m_App.GetGameAudio(), *move, after, 1.0f, match.Seats[size_t(after.ToMove)] == me, 0.0f);
	}

	void SessionState::UpdateChess()
	{
		const Chess::Match& match = m_Client.GetChess();
		std::erase_if(m_ChessGlides, [&](const ChessGlide& glide) { return m_Time - glide.Start > GlideSeconds; });
		std::erase_if(m_ChessSinks, [&](const ChessSink& sink) { return m_Time - sink.Start > SinkSeconds; });
		if (m_ChessPending && (m_ChessPending->Revision != match.Revision || m_Time - m_ChessPending->Time > PendingSeconds))
			m_ChessPending.reset();
		if (m_Client.GetChessVersion() == m_ChessSeen)
			return;
		const bool first = m_ChessSeen == UINT32_MAX;
		m_ChessSeen = m_Client.GetChessVersion();
		m_ChessPick = -1;
		m_ChessPromotion.reset();
		const bool extends = match.Moves.size() == m_ChessSeenMoves.size() + 1 && std::equal(m_ChessSeenMoves.begin(), m_ChessSeenMoves.end(), match.Moves.begin());
		const Level* level = m_Client.GetLevel();
		const EntityState* self = m_Client.FindEntity(m_Client.GetPlayerNetId());
		const bool watching = m_Station == StationKind::Chess;
		const float distance = level && level->ChessTable && self ? Length(self->Position - level->ChessTable->Center) : HearingRange;
		const bool audible = !first && (watching || distance < HearingRange);
		const float presence = watching ? 1.0f : std::clamp(1.25f - distance / HearingRange, 0.35f, 1.0f);
		const float landing = watching ? 0.0f : GlideSeconds * 0.85f;
		const uint8_t me = uint8_t(std::min<uint32_t>(m_Client.GetPlayerId(), Chess::NoSeat));
		GameAudio& audio = m_App.GetGameAudio();
		Chess::Move move;
		if (extends && Unpack(m_ChessSeenPosition, match.Moves.back(), move))
		{
			const std::array<uint8_t, 64>& before = m_ChessSeenPosition.Board;
			const uint8_t cell = before[move.From];
			m_ChessNotation.push_back(Chess::Notation(m_ChessSeenPosition, move));
			if (move.Flags & Chess::MoveCapture)
				m_ChessCaptured[size_t(Chess::SideOf(cell))].push_back(Chess::PieceOf(before[(move.Flags & Chess::MovePassant) != 0 ? Chess::SquareAt(Chess::FileOf(move.To), Chess::RankOf(move.From)) : move.To]));
			m_ChessGlides.push_back({ cell, move.From, move.To, m_Time });
			if (move.Flags & Chess::MovePassant)
			{
				const uint8_t victim = Chess::SquareAt(Chess::FileOf(move.To), Chess::RankOf(move.From));
				m_ChessSinks.push_back({ before[victim], victim, m_Time + GlideSeconds * 0.5f });
			}
			else if (move.Flags & Chess::MoveCapture)
				m_ChessSinks.push_back({ before[move.To], move.To, m_Time + GlideSeconds * 0.5f });
			if (move.Flags & Chess::MoveCastle)
			{
				const int32_t rank = Chess::RankOf(move.From);
				const bool kingside = Chess::FileOf(move.To) == 6;
				const uint8_t rook = Chess::SquareAt(kingside ? 7 : 0, rank);
				m_ChessGlides.push_back({ before[rook], rook, Chess::SquareAt(kingside ? 5 : 3, rank), m_Time + 0.15f });
			}
			const bool voiced = m_ChessVoiced == std::pair(match.Moves.size(), match.Moves.back());
			if (voiced)
				m_ChessVoiced.reset();
			else if (audible)
				PlayMoveSound(audio, move, match.Board, presence, match.Seats[size_t(match.Board.ToMove)] == me, landing);
		}
		else
		{
			m_ChessVoiced.reset();
			m_ChessGlides.clear();
			m_ChessSinks.clear();
			m_ChessNotation.clear();
			m_ChessCaptured = {};
			Chess::Position position = Chess::StartPosition();
			for (const uint16_t packed : match.Moves)
			{
				if (!Unpack(position, packed, move))
					break;
				m_ChessNotation.push_back(Chess::Notation(position, move));
				if (move.Flags & Chess::MoveCapture)
					m_ChessCaptured[size_t(position.ToMove)].push_back(Chess::PieceOf(position.Board[(move.Flags & Chess::MovePassant) != 0 ? Chess::SquareAt(Chess::FileOf(move.To), Chess::RankOf(move.From)) : move.To]));
				position = Chess::Play(position, move);
			}
		}
		if (audible && m_ChessSeenResult == Chess::Verdict::Playing && match.Result != Chess::Verdict::Playing)
		{
			const bool seated = Chess::IsSeated(match, me);
			const float delay = landing + 0.15f;
			if (Chess::IsDraw(match.Result))
				audio.PlayUi(Cue::UiNotify, 0.6f, 1.0f, delay);
			else if (seated && match.Seats[match.Winner] == me)
				audio.PlayUi(Cue::UiSuccess, 0.7f, 1.0f, delay);
			else if (seated)
				audio.PlayUi(Cue::UiFail, 0.6f, 1.0f, delay);
			else
				audio.PlayUi(Cue::Bell, 0.35f * presence, 1.2f, delay);
		}
		m_ChessSeenMoves = match.Moves;
		m_ChessSeenPosition = match.Board;
		m_ChessSeenResult = match.Result;
	}

	void SessionState::DrawChessTable(WorldRenderer& renderer) const
	{
		const Level* level = m_Client.GetLevel();
		if (!level || !level->ChessTable || m_Client.GetSession().Mode != GameMode::Hub)
			return;
		const ChessSpot& spot = *level->ChessTable;
		const float top = level->Map.GetHeight(spot.Center) + Chess::TableTop;
		const std::array<uint8_t, 64>& board = m_Client.GetChess().Board.Board;
		const auto at = [&](uint8_t square)
		{
			const Vec2 center = ChessSquareCenter(spot, Chess::FileOf(square), Chess::RankOf(square));
			return glm::vec3(center.x, top, center.y);
		};
		const auto draw = [&](uint8_t cell, const glm::vec3& position, float scale)
		{
			if (Chess::PieceOf(cell) == Chess::Piece::None)
				return;
			const Chess::Side side = Chess::SideOf(cell);
			const std::vector<uint32_t>& meshes = m_ChessMeshes[(side == Chess::Side::Black ? 6u : 0u) + uint32_t(Chess::PieceOf(cell)) - 1u];
			const glm::mat4 transform = ModelTransform(position, spot.Yaw + (side == Chess::Side::Black ? Pi : 0.0f), scale);
			for (const uint32_t mesh : meshes)
				renderer.DrawModel(mesh, transform, NoPalette, glm::vec4(1.0f, 1.0f, 1.0f, 0.0f), 0.0f, top);
		};
		std::array<bool, 64> moving{};
		for (const ChessGlide& glide : m_ChessGlides)
		{
			moving[glide.To] = true;
			const float t = Glide((m_Time - glide.Start) / GlideSeconds);
			const float hop = Chess::PieceOf(glide.Cell) == Chess::Piece::Knight ? 0.14f : 0.04f;
			draw(glide.Cell, glm::mix(at(glide.From), at(glide.To), t) + glm::vec3(0.0f, hop * std::sin(t * Pi), 0.0f), 1.0f);
		}
		for (const ChessSink& sink : m_ChessSinks)
		{
			const float t = std::clamp((m_Time - sink.Start) / SinkSeconds, 0.0f, 1.0f);
			draw(sink.Cell, at(sink.Square) - glm::vec3(0.0f, 0.1f * t * t, 0.0f), 1.0f - 0.7f * t);
		}
		for (uint8_t square = 0; square < 64; square++)
			if (board[square] != Chess::Empty && !moving[square])
				draw(board[square], at(square), 1.0f);
	}

	void SessionState::DrawChess()
	{
		Ui& ui = m_App.GetUi();
		constexpr float Width = 840.0f;
		constexpr float Height = 610.0f;
		const UiRect home{ (ui.GetSize().x - Width) * 0.5f, std::max((ui.GetSize().y - Height) * 0.5f - 24.0f, 12.0f), Width, Height };
		bool open = true;
		UiRect content = ui.Window(Ui::MakeId("chess"), "Chess Table", home, &open);
		if (!open)
		{
			m_Station.reset();
			return;
		}
		UiDrawList& draw = ui.Draw();
		const Chess::Match& match = m_Client.GetChess();
		const uint8_t me = uint8_t(std::min<uint32_t>(m_Client.GetPlayerId(), Chess::NoSeat));
		const bool white = match.Seats[0] == me;
		const bool black = match.Seats[1] == me;
		const bool seated = white || black;
		const bool full = match.Seats[0] != Chess::NoSeat && match.Seats[1] != Chess::NoSeat;
		const bool playing = match.Result == Chess::Verdict::Playing;
		const bool flipped = black && !white;
		const Chess::Position position = ShownChessPosition();
		const Chess::Side mover = position.ToMove;
		const bool turn = playing && full && match.Seats[size_t(mover)] == me && !m_ChessPending;
		std::vector<Chess::Move> legal;
		if (turn)
			Chess::LegalMoves(position, legal);
		const auto movable = [&](uint8_t square) { return std::any_of(legal.begin(), legal.end(), [&](const Chess::Move& move) { return move.From == square; }); };
		const auto reaches = [&](uint8_t from, uint8_t to) { return std::any_of(legal.begin(), legal.end(), [&](const Chess::Move& move) { return move.From == from && move.To == to; }); };
		const auto promotes = [&](uint8_t from, uint8_t to) { return std::any_of(legal.begin(), legal.end(), [&](const Chess::Move& move) { return move.From == from && move.To == to && move.Promotion != Chess::Piece::None; }); };
		const auto attempt = [&](uint8_t from, uint8_t to)
		{
			if (promotes(from, to))
				m_ChessPromotion = std::make_pair(from, to);
			else
				CommitChessMove(from, to, Chess::Piece::None);
		};

		UiRect left = content.CutLeft(SquarePixels * 8.0f + 30.0f);
		content.CutLeft(22.0f);
		UiRect side = content;
		const UiRect frame{ left.X + 22.0f, left.Y + 6.0f, SquarePixels * 8.0f, SquarePixels * 8.0f };
		const auto squareRect = [&](uint8_t square)
		{
			const int32_t column = flipped ? 7 - Chess::FileOf(square) : Chess::FileOf(square);
			const int32_t row = flipped ? Chess::RankOf(square) : 7 - Chess::RankOf(square);
			return UiRect{ frame.X + float(column) * SquarePixels, frame.Y + float(row) * SquarePixels, SquarePixels, SquarePixels };
		};
		draw.Rect(frame.Expand(7.0f), { .Fill = Hex(0x14102A), .Border = WithAlpha(Theme::Gold, 0.55f), .BorderWidth = 1.5f, .Glow = WithAlpha(Theme::Accent, 0.12f), .GlowSize = 18.0f, .Chamfer = 7.0f });
		for (int32_t i = 0; i < 8; i++)
		{
			const int32_t file = flipped ? 7 - i : i;
			const int32_t rank = flipped ? i : 7 - i;
			draw.TextAligned({ frame.X + float(i) * SquarePixels, frame.Bottom() + 8.0f, SquarePixels, 16.0f }, std::string(1, char('a' + file)), Theme::Body(13.5f, Theme::Muted));
			draw.TextAligned({ frame.X - 24.0f, frame.Y + float(i) * SquarePixels, 14.0f, SquarePixels }, std::string(1, char('1' + rank)), Theme::Body(13.5f, Theme::Muted));
		}
		const UiPayload* drag = ui.GetDrag();
		const int32_t dragged = drag && drag->Type == ChessPayload ? drag->Value : -1;
		const int32_t focus = dragged >= 0 ? dragged : m_ChessPick;
		std::optional<Chess::Move> last;
		if (!match.Moves.empty())
		{
			const uint16_t packed = match.Moves.back();
			last = Chess::Move{ uint8_t(packed & 63u), uint8_t((packed >> 6) & 63u) };
		}
		if (m_ChessPending && m_ChessPending->Revision == match.Revision)
			last = m_ChessPending->Move;
		const uint8_t checked = Chess::InCheck(position) ? Chess::KingSquare(position, mover) : Chess::NoSquare;
		for (uint8_t square = 0; square < 64; square++)
		{
			const UiRect rect = squareRect(square);
			const bool light = ((Chess::FileOf(square) + Chess::RankOf(square)) & 1) != 0;
			draw.Rect(rect, { .Fill = light ? LightSquare : DarkSquare });
			if (last && (square == last->From || square == last->To))
				draw.Rect(rect, { .Fill = WithAlpha(Theme::Gold, 0.34f) });
			if (int32_t(square) == focus)
				draw.Rect(rect, { .Fill = WithAlpha(Theme::Astral, 0.36f) });
			if (square == checked)
				draw.Circle(rect.Center(), SquarePixels * 0.48f, WithAlpha(Theme::Danger, 0.55f));
			const UiId id = Ui::MakeId("chesssquare", square);
			const UiInteraction interaction = ui.Interact(id, rect);
			const uint8_t cell = position.Board[square];
			const bool mine = turn && cell != Chess::Empty && Chess::SideOf(cell) == mover && movable(square);
			if (mine && !m_ChessPromotion)
				ui.OfferDrag(id, ChessPayload, square);
			if (interaction.Hovered && (mine || (m_ChessPick >= 0 && reaches(uint8_t(m_ChessPick), square))))
				draw.Rect(rect.Shrink(2.0f), { .Border = WithAlpha(Theme::AccentLight, 0.7f), .BorderWidth = 2.0f });
			if (interaction.Clicked && m_ChessPromotion)
				m_ChessPromotion.reset();
			else if (interaction.Clicked)
			{
				if (m_ChessPick >= 0 && reaches(uint8_t(m_ChessPick), square))
					attempt(uint8_t(m_ChessPick), square);
				else
					m_ChessPick = mine && m_ChessPick != int32_t(square) ? int32_t(square) : -1;
			}
			if (const std::optional<int> from = ui.AcceptDrop(rect, ChessPayload); from && reaches(uint8_t(*from), square))
				attempt(uint8_t(*from), square);
		}
		for (uint8_t square = 0; square < 64; square++)
		{
			const uint8_t cell = position.Board[square];
			if (cell != Chess::Empty && int32_t(square) != dragged)
				draw.Icon(PieceIcon(Chess::PieceOf(cell)), squareRect(square).Shrink(5.0f), PieceStyle(Chess::SideOf(cell)));
		}
		if (focus >= 0)
			for (const Chess::Move& move : legal)
			{
				if (move.From != uint8_t(focus) || move.Promotion > Chess::Piece::Queen)
					continue;
				const UiRect rect = squareRect(move.To);
				if (move.Flags & Chess::MoveCapture)
					draw.Circle(rect.Center(), SquarePixels * 0.44f, UiColor(0.0f), WithAlpha(Theme::Ink, 0.45f), 4.0f);
				else
					draw.Circle(rect.Center(), SquarePixels * 0.13f, WithAlpha(Theme::Ink, 0.4f));
			}
		if (dragged >= 0 && position.Board[size_t(dragged)] != Chess::Empty)
		{
			const uint8_t cell = position.Board[size_t(dragged)];
			ui.Draw(UiLayer::Drag).Icon(PieceIcon(Chess::PieceOf(cell)), UiRect::Centered(ui.GetMouse(), { SquarePixels - 10.0f, SquarePixels - 10.0f }), PieceStyle(Chess::SideOf(cell)));
		}
		if (m_ChessPromotion)
		{
			const auto [from, to] = *m_ChessPromotion;
			const UiRect target = squareRect(to);
			const float direction = target.Y < frame.Y + frame.H * 0.5f ? 1.0f : -1.0f;
			const UiRect column{ target.X - 4.0f, direction > 0.0f ? target.Y - 4.0f : target.Y - 3.0f * SquarePixels - 4.0f, SquarePixels + 8.0f, 4.0f * SquarePixels + 8.0f };
			ui.SetLayer(UiLayer::Popup);
			UiDrawList& popup = ui.Draw();
			popup.Rect(column, { .Fill = Theme::PanelTop, .FillBottom = Theme::PanelBottom, .Border = WithAlpha(Theme::Gold, 0.8f), .BorderWidth = 1.5f, .Glow = WithAlpha(Theme::Accent, 0.25f), .GlowSize = 22.0f, .Chamfer = 6.0f });
			for (size_t i = 0; i < std::size(PromotionChoices); i++)
			{
				const UiRect choice{ target.X, target.Y + direction * float(i) * SquarePixels, SquarePixels, SquarePixels };
				const UiId id = Ui::MakeId("chesspromote", i);
				const UiInteraction interaction = ui.Interact(id, choice);
				const float hover = ui.Animate(id, interaction.Hovered);
				popup.Rect(choice.Shrink(2.0f), { .Fill = WithAlpha(Theme::Accent, 0.12f + 0.3f * hover), .Chamfer = 4.0f });
				popup.Icon(PieceIcon(PromotionChoices[i]), choice.Shrink(5.0f), PieceStyle(mover));
				if (interaction.Clicked)
					CommitChessMove(from, to, PromotionChoices[i]);
			}
			ui.SetLayer(UiLayer::Windows);
		}

		const auto seat = [&](UiRect row, uint32_t color)
		{
			const uint8_t player = match.Seats[color];
			const bool active = playing && full && size_t(mover) == color;
			draw.Rect(row, { .Fill = WithAlpha(Theme::Accent, active ? 0.16f : 0.06f), .Border = WithAlpha(active ? Theme::Gold : Theme::Edge, active ? 0.75f : 0.45f), .BorderWidth = 1.2f, .Chamfer = 6.0f });
			row = row.Shrink(10.0f);
			draw.Icon(UiIcon::ChessKing, row.CutLeft(SquarePixels - 10.0f), PieceStyle(Chess::Side(color)));
			row.CutLeft(10.0f);
			UiRect text = row;
			if (player == Chess::NoSeat)
			{
				const UiRect button = text.CutRight(96.0f).Shrink(0.0f, 6.0f);
				if (ui.Button(Ui::MakeId("chesssit", color), button, "Sit", ButtonKind::Primary, !seated || match.Seats[1u - color] == me))
					m_Client.PlayChess(Chess::Command::Sit, color);
			}
			UiRect name = text.CutTop(22.0f);
			draw.TextFit(name, player == Chess::NoSeat ? std::string("Empty seat") : PlayerName(player) + (player == me ? " (you)" : ""), Theme::Bold(16.0f, player == Chess::NoSeat ? Theme::Faint : Theme::Text));
			const UiRect trophies = text.CutTop(22.0f);
			float x = trophies.X;
			for (const Chess::Piece piece : MaterialOrder)
			{
				const size_t count = size_t(std::count(m_ChessCaptured[color].begin(), m_ChessCaptured[color].end(), piece));
				for (size_t i = 0; i < count && x + 20.0f <= trophies.Right(); i++, x += 12.0f)
					draw.Icon(PieceIcon(piece), { x, trophies.Y + 1.0f, 20.0f, 20.0f }, PieceStyle(Chess::Side(1u - color), 0.9f));
				x += count > 0 ? 8.0f : 0.0f;
			}
			if (playing && (match.DrawOffer & (1u << color)) != 0)
				draw.TextAligned(name, "offers a draw", Theme::Body(13.0f, Theme::Warning), { 1.0f, 0.5f });
			else if (active)
				draw.TextAligned(name, "to move", Theme::Body(13.0f, Theme::Gold), { 1.0f, 0.5f });
		};
		seat(side.CutTop(70.0f), flipped ? 0u : 1u);
		side.CutTop(10.0f);
		const UiRect bottomRow = side.CutBottom(70.0f);
		side.CutBottom(10.0f);
		UiRect buttons = side.CutBottom(seated ? 82.0f : 0.0f);
		side.CutBottom(seated ? 8.0f : 0.0f);

		std::string status;
		UiColor tone = Theme::Text;
		if (!playing)
		{
			status = Verdict(match);
			tone = Chess::IsDraw(match.Result) ? Theme::Warning : (seated && match.Winner < 2 && match.Seats[match.Winner] == me ? Theme::Success : Theme::Gold);
		}
		else if (!full)
			status = seated ? "Waiting for someone to take the other seat." : "Take a seat to play. Anyone can watch.";
		else
		{
			status = turn ? "Your move." : std::string(SideName(uint32_t(mover))) + " to move.";
			if (checked != Chess::NoSquare)
			{
				status = (turn ? std::string("Check! ") : std::string("Check. ")) + status;
				tone = Theme::Danger;
			}
		}
		const UiRect banner = side.CutTop(48.0f);
		draw.Rect(banner, { .Fill = Hex(0x0C0A19, 0.85f), .Border = WithAlpha(Theme::Edge, 0.4f), .BorderWidth = 1.0f, .Chamfer = 5.0f });
		draw.TextWrapped(banner.Shrink(10.0f, 6.0f), status, Theme::Body(14.5f, tone));
		side.CutTop(8.0f);
		ui.Separator(side.CutTop(20.0f), "MOVES");
		side.CutTop(4.0f);
		const size_t lines = size_t(std::max(side.H / 21.0f, 1.0f));
		const size_t total = (m_ChessNotation.size() + 1) / 2;
		for (size_t line = total > lines ? total - lines : 0; line < total; line++)
		{
			UiRect row = side.CutTop(21.0f);
			draw.TextAligned(row.CutLeft(36.0f), std::to_string(line + 1) + ".", Theme::Body(14.0f, Theme::Faint), { 1.0f, 0.5f });
			row.CutLeft(10.0f);
			draw.TextAligned(row.CutLeft(84.0f), m_ChessNotation[line * 2], Theme::Body(14.5f, Theme::Text), { 0.0f, 0.5f });
			if (line * 2 + 1 < m_ChessNotation.size())
				draw.TextAligned(row, m_ChessNotation[line * 2 + 1], Theme::Body(14.5f, Theme::Text), { 0.0f, 0.5f });
		}
		if (total == 0)
			draw.TextAligned(side.CutTop(21.0f), "No moves yet.", Theme::Body(14.0f, Theme::Faint), { 0.0f, 0.5f });

		seat(bottomRow, flipped ? 1u : 0u);
		if (!seated)
			return;
		const float gap = 8.0f;
		const float half = (buttons.W - gap) * 0.5f;
		UiRect upper = buttons.CutTop(37.0f);
		buttons.CutTop(gap);
		const uint32_t mine = white && black ? uint32_t(mover) : white ? 0u : 1u;
		const bool offered = (match.DrawOffer & (1u << (1u - mine))) != 0;
		if (playing)
		{
			const bool started = !match.Moves.empty();
			if (white && black)
			{
				if (ui.Button(Ui::MakeId("chessrestart"), upper.CutLeft(half), "Restart", ButtonKind::Normal, started))
					m_Client.PlayChess(Chess::Command::Rematch);
			}
			else if (ui.Button(Ui::MakeId("chessdraw"), upper.CutLeft(half), offered ? "Accept draw" : (match.DrawOffer & (1u << mine)) != 0 ? "Draw offered" : "Offer draw", offered ? ButtonKind::Primary : ButtonKind::Normal, full && started && (match.DrawOffer & (1u << mine)) == 0))
				m_Client.PlayChess(Chess::Command::Draw);
			upper.CutLeft(gap);
			if (started)
			{
				if (ui.Button(Ui::MakeId("chessresign"), upper, "Resign", ButtonKind::Danger, full))
					m_Client.PlayChess(Chess::Command::Resign);
			}
			else if (ui.Button(Ui::MakeId("chessswap"), upper, "Swap sides", ButtonKind::Normal))
				m_Client.PlayChess(Chess::Command::Rematch);
		}
		else if (ui.Button(Ui::MakeId("chessrematch"), upper, "Rematch with colours swapped", ButtonKind::Primary))
			m_Client.PlayChess(Chess::Command::Rematch);
		if (ui.Button(Ui::MakeId("chessstand"), buttons, playing && full && !match.Moves.empty() && !(white && black) ? "Leave the table (forfeits the game)" : "Leave the table", ButtonKind::Normal))
			m_Client.PlayChess(Chess::Command::Stand);
	}
}
