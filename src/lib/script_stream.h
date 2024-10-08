#pragma once

#include "mt.h"
#include "filesystem.h"

namespace mt {

	/*! Script parser and streamer, parses IDTech games script files and model files */
	class ScriptStream {
	  public:
		/**
		 * Create an empty ScriptStream
		 */
		ScriptStream() = default;

		/**
		 * Create and stream and use the input string as the script
		 * @param value to be parsed
		 */
		explicit ScriptStream(const std::string &value);

		/**
		 * Create and stream and use the input file as the script
		 * @param file to be parsed
		 */
		explicit ScriptStream(const Ref<io::MTFile> &file);

		~ScriptStream() = default;

		/**
		 * use the input file as the script
		 * @param file data to parse
		 */
		void load(const Ref<io::MTFile> &file);

		/**
		 * use the input string as the script
		 * @param value to be parsed
		 */
		void load(const std::string &value);

		/**
		 * Check if the current status is valid
		 * @return true if valid
		 */
		[[nodiscard]] bool valid() const;

		/**
		 * Returns the next token from the script
		 * @param allowLineBreaks are line breaks allowed when getting the next token
		 * @return the next token parsed
		 */
		std::string token(bool allowLineBreaks = true);

		/**
		 * Parses the next token from the script
		 * @param allowLineBreaks are line breaks allowed when getting the next token
		 * @return self
		 */
		ScriptStream &parse(bool allowLineBreaks = true);

		/**
		 * Checks if the token is a block start char '{'
		 * @param parse should we parse the next token
		 * @return if the char a block start char
		 */
		bool isBlockStart(bool parse = false);

		/**
		 * Checks if the token is a block end char '}'
		 * @param parse should we parse the next token
		 * @return if the char a block end char
		 */
		bool isBlockEnd(bool parse = false);

		/**
		 * Skip the current line
		 */
		void skipLine();

		/**
		 * Skip the current braced section ( until '}' )
		 */
		void skipSection();

		/**
		 * Check if the current token matches value
		 * @param match value to be matched
		 * @return true if the value matches
		 */
		[[nodiscard]] bool matches(const std::string &match) const;

		/**
		 * Last parsed token value
		 * @return token value
		 */
		std::string last();

		/**
		 * Previous parsed token value
		 * @return token value
		 */
		std::string previous();

		/**
		 * Peek the next token, but do not keep the parsed pointing to that token
		 * @return token value
		 */
		std::string peekNext();

		/**
		 * Parses a one dimentional matrix (vector) of floats
		 * @param columns number of columns
		 * @param output float array for the values
		 */
		void parse1DMatrix(int columns, float *output);

		/**
		 * Converts and returns an integer from the token string
		 * @param allowLineBreaks are line breaks allowed when getting the next token
		 * @return integer value of the token
		 */
		int parseInt(bool allowLineBreaks = true);

		/**
		 * Converts and returns a float from the token string
		 * @param allowLineBreaks are line breaks allowed when getting the next token
		 * @return float value of the token
		 */
		float parseFloat(bool allowLineBreaks = true);

		/**
		 * Converts and returns a bool from the token string
		 * @param allowLineBreaks are line breaks allowed when getting the next token
		 * @return bool value of the token
		 */
		bool parseBool(bool allowLineBreaks = true);

		/**
		 * Does the stream have a next token
		 * @return true if more tokens are available
		 */
		bool hasNext();

		bool operator==(const std::string &other) const;

		explicit operator float();

		explicit operator int();

		explicit operator bool();

	  private:
		/**
		 * Current character in point
		 * @return character
		 */
		char current();

		/**
		 * Next character after point
		 * @return character
		 */
		char next();

		/**
		 * Parse the next token
		 * @param linebreaks are line breaks allowed when getting the next token
		 */
		void parseNext(bool linebreaks);

		/**
		 * Skip a block comment
		 * @return was the skip needed
		 */
		bool skipBlockComment();

		/**
		 * Skip all whitespaces until token
		 * @return was the skip needed
		 */
		bool skipWhitespace();

		std::string fullScript{};	 ///< the script that is used for parsing
		unsigned int offset = 0;	 ///< current parsing offset
		unsigned int lines = 0;		 ///< line counter
		std::string currentToken{};	 ///< current token value
		std::string previousToken{}; ///< previous token value
	};
}
