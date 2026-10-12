#include "fuzzy.h"

#include <algorithm>
#include <cctype>
#include <climits>
#include <sstream>

namespace mt {

	namespace {
		constexpr int SCORE_MATCH = 16;			  ///< every matched character
		constexpr int BONUS_CONSECUTIVE = 24;	  ///< the character follows the previous match
		constexpr int BONUS_FOLDER = 30;		  ///< first character of a folder or the file name
		constexpr int BONUS_FILE_NAME_START = 40; ///< first character of the file name (on top of the folder bonus)
		constexpr int BONUS_WORD = 20;			  ///< after _ - . space or a lower to upper case change
		constexpr int BONUS_IN_FILE_NAME = 8;	  ///< the character is in the file name
		constexpr int PENALTY_GAP = 3;			  ///< every skipped character between matches
		constexpr int PENALTY_LEADING = 1;		  ///< every skipped character before the first match
		constexpr int MAX_LEADING_PENALTY = 15;
		constexpr int NONE = INT_MIN / 2;

		char lower(char c)
		{
			return char(std::tolower(static_cast<unsigned char>(c)));
		}

		int positionBonus(const std::string &text, size_t index, size_t fileNameStart)
		{
			int bonus = index >= fileNameStart ? BONUS_IN_FILE_NAME : 0;
			if (index == 0 || text[index - 1] == '/')
			{
				bonus += BONUS_FOLDER;
			}
			else
			{
				const char before = text[index - 1];
				const char current = text[index];
				if (before == '_' || before == '-' || before == '.' || before == ' '
					|| (std::islower(static_cast<unsigned char>(before)) && std::isupper(static_cast<unsigned char>(current))))
				{
					bonus += BONUS_WORD;
				}
			}
			if (index == fileNameStart)
			{
				bonus += BONUS_FILE_NAME_START;
			}
			return bonus;
		}

		/**
		 * Best match of one word: dynamic programming over the pattern and text characters
		 */
		bool matchWord(const std::string &pattern, const std::string &text, FuzzyMatch &out)
		{
			const size_t m = pattern.size(), n = text.size();
			if (m == 0 || m > n)
			{
				return m == 0;
			}

			// Quick check: the characters appear in order
			size_t found = 0;
			for (size_t j = 0; j < n && found < m; j++)
			{
				if (lower(text[j]) == lower(pattern[found]))
				{
					found++;
				}
			}
			if (found < m)
			{
				return false;
			}

			const size_t slash = text.find_last_of('/');
			const size_t fileNameStart = slash == std::string::npos ? 0 : slash + 1;

			// score[i][j]: best score with pattern[i] matched at text[j]; from[i][j]: where pattern[i - 1] was matched
			std::vector<int> score(m * n, NONE);
			std::vector<int> from(m * n, -1);
			auto at = [n](size_t i, size_t j) { return i * n + j; };

			for (size_t i = 0; i < m; i++)
			{
				const char wanted = lower(pattern[i]);
				int bestGap = NONE; // best previous score that leaves a gap, with the gap penalty applied
				int bestGapFrom = -1;
				for (size_t j = i; j < n; j++)
				{
					if (i > 0 && j >= 2)
					{
						// The previous match at j - 2 is now a gap of one, older ones grow by one
						if (bestGap != NONE)
						{
							bestGap -= PENALTY_GAP;
						}
						const int candidate = score[at(i - 1, j - 2)];
						if (candidate != NONE && candidate - PENALTY_GAP > bestGap)
						{
							bestGap = candidate - PENALTY_GAP;
							bestGapFrom = int(j - 2);
						}
					}

					if (lower(text[j]) != wanted)
					{
						continue;
					}

					const int here = SCORE_MATCH + positionBonus(text, j, fileNameStart);
					if (i == 0)
					{
						score[at(i, j)] = here - std::min(int(j) * PENALTY_LEADING, MAX_LEADING_PENALTY);
						continue;
					}

					int best = NONE, bestFrom = -1;
					if (j >= 1 && score[at(i - 1, j - 1)] != NONE)
					{
						best = score[at(i - 1, j - 1)] + BONUS_CONSECUTIVE;
						bestFrom = int(j - 1);
					}
					if (bestGap != NONE && bestGap > best)
					{
						best = bestGap;
						bestFrom = bestGapFrom;
					}
					if (best != NONE)
					{
						score[at(i, j)] = best + here;
						from[at(i, j)] = bestFrom;
					}
				}
			}

			// The best end, then walk back for the positions
			int bestScore = NONE;
			int bestEnd = -1;
			for (size_t j = m - 1; j < n; j++)
			{
				if (score[at(m - 1, j)] > bestScore)
				{
					bestScore = score[at(m - 1, j)];
					bestEnd = int(j);
				}
			}
			if (bestEnd < 0)
			{
				return false;
			}

			out.score += bestScore;
			int j = bestEnd;
			for (size_t i = m; i-- > 0;)
			{
				out.positions.push_back(size_t(j));
				j = from[at(i, size_t(j))];
			}
			return true;
		}
	}

	bool fuzzyMatch(const std::string &pattern, const std::string &text, FuzzyMatch &out)
	{
		out = {};
		std::istringstream words(pattern);
		bool any = false;
		for (std::string word; words >> word;)
		{
			any = true;
			if (!matchWord(word, text, out))
			{
				out = {};
				return false;
			}
		}
		if (!any)
		{
			return false;
		}

		std::sort(out.positions.begin(), out.positions.end());
		out.positions.erase(std::unique(out.positions.begin(), out.positions.end()), out.positions.end());
		return true;
	}

	std::vector<FuzzyResult> fuzzyFind(const std::string &pattern, const std::vector<std::string> &texts, size_t limit)
	{
		std::vector<FuzzyResult> results;
		for (size_t i = 0; i < texts.size(); i++)
		{
			FuzzyMatch match;
			if (fuzzyMatch(pattern, texts[i], match))
			{
				results.push_back({i, std::move(match)});
			}
		}

		auto better = [&](const FuzzyResult &a, const FuzzyResult &b) {
			if (a.match.score != b.match.score)
			{
				return a.match.score > b.match.score;
			}
			if (texts[a.index].size() != texts[b.index].size())
			{
				return texts[a.index].size() < texts[b.index].size();
			}
			return texts[a.index] < texts[b.index];
		};

		if (limit > 0 && results.size() > limit)
		{
			std::partial_sort(results.begin(), results.begin() + long(limit), results.end(), better);
			results.resize(limit);
		}
		else
		{
			std::sort(results.begin(), results.end(), better);
		}
		return results;
	}
}
