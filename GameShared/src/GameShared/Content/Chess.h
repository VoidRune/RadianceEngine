#pragma once
#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace Game::Chess
{
	enum class Piece : uint8_t
	{
		None,
		Pawn,
		Knight,
		Bishop,
		Rook,
		Queen,
		King,
	};

	enum class Side : uint8_t
	{
		White,
		Black,
	};

	enum Rights : uint8_t
	{
		WhiteShort = 1 << 0,
		WhiteLong = 1 << 1,
		BlackShort = 1 << 2,
		BlackLong = 1 << 3,
		AllRights = WhiteShort | WhiteLong | BlackShort | BlackLong,
	};

	enum MoveFlags : uint8_t
	{
		MoveCapture = 1 << 0,
		MoveDouble = 1 << 1,
		MovePassant = 1 << 2,
		MoveCastle = 1 << 3,
	};

	enum class Outcome : uint8_t
	{
		Ongoing,
		Checkmate,
		Stalemate,
		Material,
		FiftyMoves,
		Repetition,
	};

	inline constexpr uint8_t Empty = 0;
	inline constexpr uint8_t BlackBit = 8;
	inline constexpr uint8_t NoSquare = 64;

	constexpr uint8_t MakeCell(Side side, Piece piece) { return uint8_t(uint8_t(piece) | (side == Side::Black ? BlackBit : 0u)); }
	constexpr Piece PieceOf(uint8_t cell) { return Piece(cell & 7u); }
	constexpr Side SideOf(uint8_t cell) { return (cell & BlackBit) != 0 ? Side::Black : Side::White; }
	constexpr Side Opponent(Side side) { return side == Side::White ? Side::Black : Side::White; }
	constexpr uint8_t SquareAt(int32_t file, int32_t rank) { return uint8_t(rank * 8 + file); }
	constexpr int32_t FileOf(uint8_t square) { return square & 7; }
	constexpr int32_t RankOf(uint8_t square) { return square >> 3; }
	constexpr bool IsOnBoard(int32_t file, int32_t rank) { return file >= 0 && file < 8 && rank >= 0 && rank < 8; }

	struct Position
	{
		std::array<uint8_t, 64> Board{};
		Side ToMove = Side::White;
		uint8_t Rights = 0;
		uint8_t EnPassant = NoSquare;
		uint16_t Quiet = 0;
		uint16_t Turn = 1;
	};

	struct Move
	{
		uint8_t From = 0;
		uint8_t To = 0;
		Piece Promotion = Piece::None;
		uint8_t Flags = 0;
	};

	struct Placement
	{
		std::array<uint8_t, 64> Board{};
		Side ToMove = Side::White;
		uint8_t Rights = 0;
		uint8_t EnPassant = NoSquare;
		bool operator==(const Placement&) const = default;
	};

	enum class Verdict : uint8_t
	{
		Playing,
		Checkmate,
		Resigned,
		Abandoned,
		Stalemate,
		Material,
		FiftyMoves,
		Repetition,
		Agreed,
		MoveLimit,
	};

	enum class Command : uint8_t
	{
		Sit,
		Stand,
		Move,
		Resign,
		Draw,
		Rematch,
		Count,
	};

	inline constexpr uint8_t NoSeat = 0xFF;
	inline constexpr uint8_t NoWinner = 2;
	inline constexpr size_t MoveLimit = 1200;
	inline constexpr float SquareSize = 0.22f;
	inline constexpr float TableTop = 0.62f;

	Position StartPosition();
	std::optional<Position> ParseFen(std::string_view fen);
	std::string ToFen(const Position& position);
	std::string SquareName(uint8_t square);

	bool IsAttacked(const Position& position, uint8_t square, Side by);
	uint8_t KingSquare(const Position& position, Side side);
	bool InCheck(const Position& position);
	void LegalMoves(const Position& position, std::vector<Move>& moves);
	std::optional<Move> FindLegal(const Position& position, uint8_t from, uint8_t to, Piece promotion);
	Position Play(const Position& position, const Move& move);

	bool IsInsufficientMaterial(const Position& position);
	Placement PlacementOf(const Position& position);
	Outcome Judge(const Position& position, std::span<const Placement> history);
	std::string Notation(const Position& position, const Move& move);
	uint64_t Perft(const Position& position, int32_t depth);

	struct Match
	{
		std::vector<uint16_t> Moves;
		std::array<uint8_t, 2> Seats{ NoSeat, NoSeat };
		Verdict Result = Verdict::Playing;
		uint8_t Winner = NoWinner;
		uint8_t DrawOffer = 0;
		uint32_t Revision = 0;
		Position Board = StartPosition();
		std::vector<Placement> History{ PlacementOf(StartPosition()) };
	};

	uint16_t PackMove(const Move& move);
	void ResetBoard(Match& match);
	bool PlayMove(Match& match, uint8_t from, uint8_t to, Piece promotion);
	bool Rebuild(Match& match);
	bool IsDraw(Verdict verdict);
	bool IsSeated(const Match& match, uint8_t player);
}
