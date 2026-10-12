#pragma once

#include <string>
#include <vector>

namespace mt {

	/*! Result of a fuzzy match */
	struct FuzzyMatch {
		int score = 0;					 ///< higher is better
		std::vector<size_t> positions{}; ///< matched characters of the text, sorted
	};

	/*! Match of one of the searched texts */
	struct FuzzyResult {
		size_t index = 0; ///< index of the text in the searched list
		FuzzyMatch match{};
	};

	/**
	 * Fuzzy match a pattern against a text (like file finders do): the characters of the pattern have to appear in the
	 * text in order, ignoring the case. Words of the pattern (separated by spaces) all have to match, in any order.
	 * Consecutive characters, characters that start a folder, the file name or a word (after / _ - . or a capital)
	 * and characters of the file name score higher; skipped characters lower the score.
	 * @param pattern searched text
	 * @param text text to match, like a file path
	 * @param out score and matched characters
	 * @return false if the text does not match (or the pattern is empty)
	 */
	bool fuzzyMatch(const std::string &pattern, const std::string &text, FuzzyMatch &out);

	/**
	 * Fuzzy match a pattern against texts
	 * @param pattern searched text
	 * @param texts texts to search
	 * @param limit keep only the best results, 0 for every match
	 * @return matches, best first (equal scores: shorter, then alphabetical text first)
	 */
	std::vector<FuzzyResult> fuzzyFind(const std::string &pattern, const std::vector<std::string> &texts, size_t limit = 0);
}
