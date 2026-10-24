#include "Chess.h"
#include <algorithm>
#include <cctype>

namespace Game::Chess
{
	namespace
	{
		using Step = std::array<int32_t, 2>;
		constexpr Step KnightSteps[] = { { 1, 2 }, { 2, 1 }, { 2, -1 }, { 1, -2 }, { -1, -2 }, { -2, -1 }, { -2, 1 }, { -1, 2 } };
		constexpr Step KingSteps[] = { { 1, 0 }, { 1, 1 }, { 0, 1 }, { -1, 1 }, { -1, 0 }, { -1, -1 }, { 0, -1 }, { 1, -1 } };
		constexpr Step RookRays[] = { { 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 } };
		constexpr Step BishopRays[] = { { 1, 1 }, { 1, -1 }, { -1, 1 }, { -1, -1 } };
		constexpr Piece Promotions[] = { Piece::Queen, Piece::Rook, Piece::Bishop, Piece::Knight };
		constexpr std::string_view PieceLetters = "pnbrqk";

		constexpr uint8_t A1 = 0;
		constexpr uint8_t E1 = 4;
		constexpr uint8_t H1 = 7;
		constexpr uint8_t A8 = 56;
		constexpr uint8_t E8 = 60;
		constexpr uint8_t H8 = 63;

		uint8_t RightsKept(uint8_t square)
		{
			switch (square)
			{
			case A1:
				return uint8_t(AllRights & ~WhiteLong);
			case E1:
				return uint8_t(AllRights & ~(WhiteShort | WhiteLong));
			case H1:
				return uint8_t(AllRights & ~WhiteShort);
			case A8:
				return uint8_t(AllRights & ~BlackLong);
			case E8:
				return uint8_t(AllRights & ~(BlackShort | BlackLong));
			case H8:
				return uint8_t(AllRights & ~BlackShort);
			default:
				return AllRights;
			}
		}

		int32_t Forward(Side side) { return side == Side::White ? 1 : -1; }
		int32_t HomeRank(Side side) { return side == Side::White ? 0 : 7; }

		void AddPawnMove(std::vector<Move>& moves, uint8_t from, uint8_t to, uint8_t flags, bool promotes)
		{
			if (!promotes)
			{
				moves.push_back({ from, to, Piece::None, flags });
				return;
			}
			for (const Piece piece : Promotions)
				moves.push_back({ from, to, piece, flags });
		}

		bool CanCastle(const Position& position, Side side, bool kingside)
		{
			const int32_t rank = HomeRank(side);
			const uint8_t right = side == Side::White ? (kingside ? WhiteShort : WhiteLong) : (kingside ? BlackShort : BlackLong);
			if ((position.Rights & right) == 0 || position.Board[SquareAt(4, rank)] != MakeCell(side, Piece::King) || position.Board[SquareAt(kingside ? 7 : 0, rank)] != MakeCell(side, Piece::Rook))
				return false;
			for (int32_t file = kingside ? 5 : 1; file <= (kingside ? 6 : 3); file++)
				if (position.Board[SquareAt(file, rank)] != Empty)
					return false;
			for (int32_t file = kingside ? 4 : 2; file <= (kingside ? 6 : 4); file++)
				if (IsAttacked(position, SquareAt(file, rank), Opponent(side)))
					return false;
			return true;
		}

		void PseudoMoves(const Position& position, std::vector<Move>& moves)
		{
			const Side side = position.ToMove;
			for (uint8_t from = 0; from < 64; from++)
			{
				const uint8_t cell = position.Board[from];
				if (cell == Empty || SideOf(cell) != side)
					continue;
				const int32_t file = FileOf(from);
				const int32_t rank = RankOf(from);
				const auto step = [&](std::span<const Step> steps)
				{
					for (const auto& [df, dr] : steps)
					{
						if (!IsOnBoard(file + df, rank + dr))
							continue;
						const uint8_t to = SquareAt(file + df, rank + dr);
						if (position.Board[to] == Empty)
							moves.push_back({ from, to, Piece::None, 0 });
						else if (SideOf(position.Board[to]) != side)
							moves.push_back({ from, to, Piece::None, MoveCapture });
					}
				};
				const auto slide = [&](std::span<const Step> rays)
				{
					for (const auto& [df, dr] : rays)
						for (int32_t f = file + df, r = rank + dr; IsOnBoard(f, r); f += df, r += dr)
						{
							const uint8_t to = SquareAt(f, r);
							if (position.Board[to] == Empty)
							{
								moves.push_back({ from, to, Piece::None, 0 });
								continue;
							}
							if (SideOf(position.Board[to]) != side)
								moves.push_back({ from, to, Piece::None, MoveCapture });
							break;
						}
				};
				switch (PieceOf(cell))
				{
				case Piece::Pawn:
				{
					const int32_t ahead = rank + Forward(side);
					const int32_t last = side == Side::White ? 7 : 0;
					if (!IsOnBoard(file, ahead))
						break;
					if (position.Board[SquareAt(file, ahead)] == Empty)
					{
						AddPawnMove(moves, from, SquareAt(file, ahead), 0, ahead == last);
						const int32_t jump = rank + 2 * Forward(side);
						if (rank == (side == Side::White ? 1 : 6) && position.Board[SquareAt(file, jump)] == Empty)
							moves.push_back({ from, SquareAt(file, jump), Piece::None, MoveDouble });
					}
					for (const int32_t df : { -1, 1 })
					{
						if (!IsOnBoard(file + df, ahead))
							continue;
						const uint8_t to = SquareAt(file + df, ahead);
						const uint8_t victim = position.Board[to];
						if (victim != Empty && SideOf(victim) != side)
							AddPawnMove(moves, from, to, MoveCapture, ahead == last);
						else if (victim == Empty && to == position.EnPassant && rank == (side == Side::White ? 4 : 3) && position.Board[SquareAt(file + df, rank)] == MakeCell(Opponent(side), Piece::Pawn))
							moves.push_back({ from, to, Piece::None, MoveCapture | MovePassant });
					}
					break;
				}
				case Piece::Knight:
					step(KnightSteps);
					break;
				case Piece::Bishop:
					slide(BishopRays);
					break;
				case Piece::Rook:
					slide(RookRays);
					break;
				case Piece::Queen:
					slide(RookRays);
					slide(BishopRays);
					break;
				case Piece::King:
					step(KingSteps);
					if (from == SquareAt(4, HomeRank(side)))
						for (const bool kingside : { true, false })
							if (CanCastle(position, side, kingside))
								moves.push_back({ from, SquareAt(kingside ? 6 : 2, rank), Piece::None, MoveCastle });
					break;
				case Piece::None:
					break;
				}
			}
		}

		std::vector<std::string_view> Fields(std::string_view text)
		{
			std::vector<std::string_view> fields;
			size_t start = 0;
			while (start < text.size())
			{
				while (start < text.size() && text[start] == ' ')
					start++;
				size_t end = start;
				while (end < text.size() && text[end] != ' ')
					end++;
				if (end > start)
					fields.push_back(text.substr(start, end - start));
				start = end;
			}
			return fields;
		}

		std::optional<uint16_t> Number(std::string_view text)
		{
			if (text.empty() || text.size() > 4)
				return std::nullopt;
			uint32_t value = 0;
			for (const char c : text)
			{
				if (c < '0' || c > '9')
					return std::nullopt;
				value = value * 10u + uint32_t(c - '0');
			}
			return uint16_t(value);
		}
	}

	Position StartPosition()
	{
		return *ParseFen("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
	}

	std::optional<Position> ParseFen(std::string_view fen)
	{
		const std::vector<std::string_view> fields = Fields(fen);
		if (fields.size() < 4)
			return std::nullopt;
		Position position;
		int32_t rank = 7;
		int32_t file = 0;
		for (const char c : fields[0])
		{
			if (c == '/')
			{
				if (file != 8 || rank == 0)
					return std::nullopt;
				rank--;
				file = 0;
				continue;
			}
			if (c >= '1' && c <= '8')
			{
				file += c - '0';
				if (file > 8)
					return std::nullopt;
				continue;
			}
			const size_t index = PieceLetters.find(char(std::tolower(static_cast<unsigned char>(c))));
			if (index == std::string_view::npos || file > 7)
				return std::nullopt;
			position.Board[SquareAt(file, rank)] = MakeCell(std::isupper(static_cast<unsigned char>(c)) ? Side::White : Side::Black, Piece(index + 1));
			file++;
		}
		if (rank != 0 || file != 8)
			return std::nullopt;
		if (fields[1] != "w" && fields[1] != "b")
			return std::nullopt;
		position.ToMove = fields[1] == "w" ? Side::White : Side::Black;
		if (fields[2] != "-")
			for (const char c : fields[2])
				switch (c)
				{
				case 'K':
					position.Rights |= WhiteShort;
					break;
				case 'Q':
					position.Rights |= WhiteLong;
					break;
				case 'k':
					position.Rights |= BlackShort;
					break;
				case 'q':
					position.Rights |= BlackLong;
					break;
				default:
					return std::nullopt;
				}
		if (fields[3] != "-")
		{
			if (fields[3].size() != 2 || fields[3][0] < 'a' || fields[3][0] > 'h' || (fields[3][1] != '3' && fields[3][1] != '6'))
				return std::nullopt;
			position.EnPassant = SquareAt(fields[3][0] - 'a', fields[3][1] - '1');
		}
		if (fields.size() >= 6)
		{
			const std::optional<uint16_t> quiet = Number(fields[4]);
			const std::optional<uint16_t> turn = Number(fields[5]);
			if (!quiet || !turn)
				return std::nullopt;
			position.Quiet = *quiet;
			position.Turn = std::max<uint16_t>(*turn, 1);
		}
		if (KingSquare(position, Side::White) == NoSquare || KingSquare(position, Side::Black) == NoSquare)
			return std::nullopt;
		return position;
	}

	std::string ToFen(const Position& position)
	{
		std::string fen;
		for (int32_t rank = 7; rank >= 0; rank--)
		{
			int32_t gap = 0;
			for (int32_t file = 0; file < 8; file++)
			{
				const uint8_t cell = position.Board[SquareAt(file, rank)];
				if (cell == Empty)
				{
					gap++;
					continue;
				}
				if (gap > 0)
					fen += char('0' + gap);
				gap = 0;
				const char letter = PieceLetters[size_t(PieceOf(cell)) - 1];
				fen += SideOf(cell) == Side::White ? char(std::toupper(static_cast<unsigned char>(letter))) : letter;
			}
			if (gap > 0)
				fen += char('0' + gap);
			if (rank > 0)
				fen += '/';
		}
		fen += position.ToMove == Side::White ? " w " : " b ";
		if (position.Rights == 0)
			fen += '-';
		if (position.Rights & WhiteShort)
			fen += 'K';
		if (position.Rights & WhiteLong)
			fen += 'Q';
		if (position.Rights & BlackShort)
			fen += 'k';
		if (position.Rights & BlackLong)
			fen += 'q';
		fen += ' ';
		fen += position.EnPassant == NoSquare ? std::string("-") : SquareName(position.EnPassant);
		fen += ' ' + std::to_string(position.Quiet) + ' ' + std::to_string(position.Turn);
		return fen;
	}

	std::string SquareName(uint8_t square)
	{
		return { char('a' + FileOf(square)), char('1' + RankOf(square)) };
	}

	bool IsAttacked(const Position& position, uint8_t square, Side by)
	{
		const int32_t file = FileOf(square);
		const int32_t rank = RankOf(square);
		const int32_t pawnRank = rank - Forward(by);
		for (const int32_t df : { -1, 1 })
			if (IsOnBoard(file + df, pawnRank) && position.Board[SquareAt(file + df, pawnRank)] == MakeCell(by, Piece::Pawn))
				return true;
		for (const auto& [df, dr] : KnightSteps)
			if (IsOnBoard(file + df, rank + dr) && position.Board[SquareAt(file + df, rank + dr)] == MakeCell(by, Piece::Knight))
				return true;
		for (const auto& [df, dr] : KingSteps)
			if (IsOnBoard(file + df, rank + dr) && position.Board[SquareAt(file + df, rank + dr)] == MakeCell(by, Piece::King))
				return true;
		const auto ray = [&](std::span<const Step> rays, Piece slider)
		{
			for (const auto& [df, dr] : rays)
				for (int32_t f = file + df, r = rank + dr; IsOnBoard(f, r); f += df, r += dr)
				{
					const uint8_t cell = position.Board[SquareAt(f, r)];
					if (cell == Empty)
						continue;
					if (cell == MakeCell(by, slider) || cell == MakeCell(by, Piece::Queen))
						return true;
					break;
				}
			return false;
		};
		return ray(RookRays, Piece::Rook) || ray(BishopRays, Piece::Bishop);
	}

	uint8_t KingSquare(const Position& position, Side side)
	{
		for (uint8_t square = 0; square < 64; square++)
			if (position.Board[square] == MakeCell(side, Piece::King))
				return square;
		return NoSquare;
	}

	bool InCheck(const Position& position)
	{
		const uint8_t king = KingSquare(position, position.ToMove);
		return king != NoSquare && IsAttacked(position, king, Opponent(position.ToMove));
	}

	void LegalMoves(const Position& position, std::vector<Move>& moves)
	{
		moves.clear();
		std::vector<Move> candidates;
		candidates.reserve(96);
		PseudoMoves(position, candidates);
		const Side side = position.ToMove;
		for (const Move& move : candidates)
		{
			const Position next = Play(position, move);
			const uint8_t king = KingSquare(next, side);
			if (king != NoSquare && !IsAttacked(next, king, Opponent(side)))
				moves.push_back(move);
		}
	}

	std::optional<Move> FindLegal(const Position& position, uint8_t from, uint8_t to, Piece promotion)
	{
		if (from >= 64 || to >= 64)
			return std::nullopt;
		std::vector<Move> moves;
		LegalMoves(position, moves);
		for (const Move& move : moves)
			if (move.From == from && move.To == to && move.Promotion == promotion)
				return move;
		return std::nullopt;
	}

	Position Play(const Position& position, const Move& move)
	{
		Position next = position;
		const uint8_t cell = position.Board[move.From];
		const Side side = position.ToMove;
		if (move.Flags & MovePassant)
			next.Board[SquareAt(FileOf(move.To), RankOf(move.From))] = Empty;
		next.Board[move.To] = move.Promotion != Piece::None ? MakeCell(side, move.Promotion) : cell;
		next.Board[move.From] = Empty;
		if (move.Flags & MoveCastle)
		{
			const int32_t rank = RankOf(move.From);
			const bool kingside = FileOf(move.To) == 6;
			next.Board[SquareAt(kingside ? 5 : 3, rank)] = next.Board[SquareAt(kingside ? 7 : 0, rank)];
			next.Board[SquareAt(kingside ? 7 : 0, rank)] = Empty;
		}
		next.Rights = uint8_t(position.Rights & RightsKept(move.From) & RightsKept(move.To));
		next.EnPassant = (move.Flags & MoveDouble) != 0 ? SquareAt(FileOf(move.From), (RankOf(move.From) + RankOf(move.To)) / 2) : NoSquare;
		next.Quiet = PieceOf(cell) == Piece::Pawn || (move.Flags & MoveCapture) != 0 ? 0 : uint16_t(position.Quiet + 1);
		if (side == Side::Black)
			next.Turn++;
		next.ToMove = Opponent(side);
		return next;
	}

	bool IsInsufficientMaterial(const Position& position)
	{
		int32_t minors = 0;
		int32_t knights = 0;
		uint8_t bishopShades = 0;
		for (uint8_t square = 0; square < 64; square++)
		{
			switch (PieceOf(position.Board[square]))
			{
			case Piece::Pawn:
			case Piece::Rook:
			case Piece::Queen:
				return false;
			case Piece::Knight:
				knights++;
				minors++;
				break;
			case Piece::Bishop:
				minors++;
				bishopShades |= uint8_t(((FileOf(square) + RankOf(square)) & 1) != 0 ? 1u : 2u);
				break;
			default:
				break;
			}
		}
		return minors <= 1 || (knights == 0 && bishopShades != 3);
	}

	Placement PlacementOf(const Position& position)
	{
		Placement placement{ position.Board, position.ToMove, position.Rights, NoSquare };
		if (position.EnPassant != NoSquare)
		{
			std::vector<Move> moves;
			LegalMoves(position, moves);
			if (std::any_of(moves.begin(), moves.end(), [](const Move& move) { return (move.Flags & MovePassant) != 0; }))
				placement.EnPassant = position.EnPassant;
		}
		return placement;
	}

	Outcome Judge(const Position& position, std::span<const Placement> history)
	{
		std::vector<Move> moves;
		LegalMoves(position, moves);
		if (moves.empty())
			return InCheck(position) ? Outcome::Checkmate : Outcome::Stalemate;
		if (IsInsufficientMaterial(position))
			return Outcome::Material;
		if (position.Quiet >= 100)
			return Outcome::FiftyMoves;
		if (!history.empty() && std::count(history.begin(), history.end(), history.back()) >= 3)
			return Outcome::Repetition;
		return Outcome::Ongoing;
	}

	std::string Notation(const Position& position, const Move& move)
	{
		const uint8_t cell = position.Board[move.From];
		const Piece piece = PieceOf(cell);
		std::string text;
		if (move.Flags & MoveCastle)
			text = FileOf(move.To) == 6 ? "O-O" : "O-O-O";
		else
		{
			if (piece != Piece::Pawn)
			{
				text += char(std::toupper(static_cast<unsigned char>(PieceLetters[size_t(piece) - 1])));
				std::vector<Move> moves;
				LegalMoves(position, moves);
				bool ambiguous = false;
				bool sameFile = false;
				bool sameRank = false;
				for (const Move& other : moves)
				{
					if (other.To != move.To || other.From == move.From || position.Board[other.From] != cell)
						continue;
					ambiguous = true;
					sameFile = sameFile || FileOf(other.From) == FileOf(move.From);
					sameRank = sameRank || RankOf(other.From) == RankOf(move.From);
				}
				if (ambiguous && (!sameFile || sameRank))
					text += char('a' + FileOf(move.From));
				if (ambiguous && sameFile)
					text += char('1' + RankOf(move.From));
			}
			if (move.Flags & MoveCapture)
			{
				if (piece == Piece::Pawn)
					text += char('a' + FileOf(move.From));
				text += 'x';
			}
			text += SquareName(move.To);
			if (move.Promotion != Piece::None)
			{
				text += '=';
				text += char(std::toupper(static_cast<unsigned char>(PieceLetters[size_t(move.Promotion) - 1])));
			}
		}
		const Position next = Play(position, move);
		if (InCheck(next))
		{
			std::vector<Move> replies;
			LegalMoves(next, replies);
			text += replies.empty() ? '#' : '+';
		}
		return text;
	}

	uint64_t Perft(const Position& position, int32_t depth)
	{
		if (depth <= 0)
			return 1;
		std::vector<Move> moves;
		LegalMoves(position, moves);
		if (depth == 1)
			return moves.size();
		uint64_t nodes = 0;
		for (const Move& move : moves)
			nodes += Perft(Play(position, move), depth - 1);
		return nodes;
	}

	uint16_t PackMove(const Move& move)
	{
		return uint16_t(move.From | (move.To << 6) | (uint32_t(move.Promotion) << 12));
	}

	void ResetBoard(Match& match)
	{
		match.Moves.clear();
		match.Board = StartPosition();
		match.History.assign(1, PlacementOf(match.Board));
		match.Result = Verdict::Playing;
		match.Winner = NoWinner;
		match.DrawOffer = 0;
	}

	bool PlayMove(Match& match, uint8_t from, uint8_t to, Piece promotion)
	{
		if (match.Result != Verdict::Playing)
			return false;
		const std::optional<Move> move = FindLegal(match.Board, from, to, promotion);
		if (!move)
			return false;
		match.Moves.push_back(PackMove(*move));
		match.Board = Play(match.Board, *move);
		match.History.push_back(PlacementOf(match.Board));
		match.DrawOffer = 0;
		switch (Judge(match.Board, match.History))
		{
		case Outcome::Checkmate:
			match.Result = Verdict::Checkmate;
			match.Winner = uint8_t(Opponent(match.Board.ToMove));
			break;
		case Outcome::Stalemate:
			match.Result = Verdict::Stalemate;
			break;
		case Outcome::Material:
			match.Result = Verdict::Material;
			break;
		case Outcome::FiftyMoves:
			match.Result = Verdict::FiftyMoves;
			break;
		case Outcome::Repetition:
			match.Result = Verdict::Repetition;
			break;
		case Outcome::Ongoing:
			if (match.Moves.size() >= MoveLimit)
				match.Result = Verdict::MoveLimit;
			break;
		}
		return true;
	}

	bool Rebuild(Match& match)
	{
		const std::vector<uint16_t> moves = match.Moves;
		match.Board = StartPosition();
		match.History.assign(1, PlacementOf(match.Board));
		match.Moves.clear();
		for (const uint16_t packed : moves)
		{
			const std::optional<Move> move = FindLegal(match.Board, uint8_t(packed & 63u), uint8_t((packed >> 6) & 63u), Piece((packed >> 12) & 7u));
			if (!move)
				return false;
			match.Moves.push_back(packed);
			match.Board = Play(match.Board, *move);
			match.History.push_back(PlacementOf(match.Board));
		}
		return true;
	}

	bool IsDraw(Verdict verdict)
	{
		return verdict != Verdict::Playing && verdict != Verdict::Checkmate && verdict != Verdict::Resigned && verdict != Verdict::Abandoned;
	}

	bool IsSeated(const Match& match, uint8_t player)
	{
		return player != NoSeat && (match.Seats[0] == player || match.Seats[1] == player);
	}
}
