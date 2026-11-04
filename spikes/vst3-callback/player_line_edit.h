#pragma once

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <string>
#include <string_view>

namespace dexter::editor {

struct PlayerAssignment {
    std::size_t lineStart = 0;
    std::size_t lineEnd = 0;
    std::size_t synthStart = 0;
    std::size_t synthEnd = 0;
    std::size_t callOpen = 0;
    std::size_t callClose = 0;
    int playerIndex = -1;
};

inline bool isIdentifierStart(char value) {
    return std::isalpha(static_cast<unsigned char>(value)) != 0 || value == '_';
}

inline bool isIdentifierChar(char value) {
    return std::isalnum(static_cast<unsigned char>(value)) != 0 || value == '_';
}

inline bool isSimpleIdentifier(std::string_view name) {
    return !name.empty() && isIdentifierStart(name.front()) &&
           std::all_of(name.begin() + 1, name.end(), isIdentifierChar);
}

inline std::size_t skipSpace(std::string_view text, std::size_t position,
                             std::size_t end) {
    while (position < end &&
           std::isspace(static_cast<unsigned char>(text[position])) != 0)
        ++position;
    return position;
}

inline bool findMatchingDelimiter(std::string_view text, std::size_t open,
                                  std::size_t end, char opening, char closing,
                                  std::size_t& match) {
    if (open >= end || text[open] != opening)
        return false;
    int depth = 0;
    char quote = 0;
    bool escaped = false;
    for (std::size_t index = open; index < end; ++index) {
        const char value = text[index];
        if (quote != 0) {
            if (escaped) {
                escaped = false;
            } else if (value == '\\') {
                escaped = true;
            } else if (value == quote) {
                quote = 0;
            }
            continue;
        }
        if (value == '\'' || value == '"') {
            quote = value;
            continue;
        }
        if (value == '#')
            while (index < end && text[index] != '\n')
                ++index;
        if (value == opening) {
            ++depth;
        } else if (value == closing && --depth == 0) {
            match = index;
            return true;
        }
    }
    return false;
}

inline bool findPlayerAssignmentAt(std::string_view source,
                                   std::size_t position,
                                   PlayerAssignment& result) {
    if (position > source.size())
        return false;
    const auto lineStart = position == 0
        ? 0
        : source.rfind('\n', position - 1) == std::string_view::npos
              ? 0
              : source.rfind('\n', position - 1) + 1;
    auto lineEnd = source.find('\n', position);
    if (lineEnd == std::string_view::npos)
        lineEnd = source.size();
    auto cursor = skipSpace(source, lineStart, lineEnd);
    if (cursor >= lineEnd || source[cursor] != 'p')
        return false;
    ++cursor;
    const auto digitsStart = cursor;
    while (cursor < lineEnd &&
           std::isdigit(static_cast<unsigned char>(source[cursor])) != 0)
        ++cursor;
    if (digitsStart == cursor)
        return false;
    int player = 0;
    for (auto index = digitsStart; index < cursor; ++index) {
        player = player * 10 + (source[index] - '0');
        if (player > 8)
            return false;
    }
    if (player < 1)
        return false;
    cursor = skipSpace(source, cursor, lineEnd);
    if (cursor + 1 >= lineEnd || source[cursor] != '>' ||
        source[cursor + 1] != '>')
        return false;
    cursor = skipSpace(source, cursor + 2, lineEnd);
    if (cursor >= lineEnd || !isIdentifierStart(source[cursor]))
        return false;
    const auto synthStart = cursor++;
    while (cursor < lineEnd && isIdentifierChar(source[cursor]))
        ++cursor;
    const auto synthEnd = cursor;
    cursor = skipSpace(source, cursor, lineEnd);
    if (cursor >= lineEnd || source[cursor] != '(')
        return false;

    std::size_t callClose = 0;
    if (!findMatchingDelimiter(source, cursor, source.size(), '(', ')',
                               callClose))
        return false;
    result.lineStart = lineStart;
    result.lineEnd = lineEnd;
    result.synthStart = synthStart;
    result.synthEnd = synthEnd;
    result.callOpen = cursor;
    result.callClose = callClose;
    result.playerIndex = player - 1;
    return true;
}

inline bool findPlayerAssignmentForIndex(std::string_view source,
                                        int playerIndex,
                                        PlayerAssignment& result) {
    if (playerIndex < 0 || playerIndex >= 8)
        return false;
    std::size_t lineStart = 0;
    while (lineStart < source.size()) {
        if (findPlayerAssignmentAt(source, lineStart, result) &&
            result.playerIndex == playerIndex)
            return true;
        const auto newline = source.find('\n', lineStart);
        if (newline == std::string_view::npos)
            break;
        lineStart = newline + 1;
    }
    return false;
}

inline bool findArgumentValue(std::string_view source,
                              const PlayerAssignment& assignment,
                              std::string_view name, bool topLevelOnly,
                              std::size_t& valueStart, std::size_t& valueEnd) {
    int parens = 0;
    int brackets = 0;
    int braces = 0;
    char quote = 0;
    bool escaped = false;
    for (std::size_t index = assignment.callOpen + 1;
         index < assignment.callClose;) {
        const char value = source[index];
        if (quote != 0) {
            if (escaped) {
                escaped = false;
            } else if (value == '\\') {
                escaped = true;
            } else if (value == quote) {
                quote = 0;
            }
            ++index;
            continue;
        }
        if (value == '\'' || value == '"') {
            quote = value;
            ++index;
            continue;
        }
        if (value == '#') {
            while (index < assignment.callClose && source[index] != '\n')
                ++index;
            continue;
        }
        if (isIdentifierStart(value) &&
            (!topLevelOnly || (parens == 0 && brackets == 0 && braces == 0))) {
            const auto wordStart = index++;
            while (index < assignment.callClose &&
                   isIdentifierChar(source[index]))
                ++index;
            if (source.substr(wordStart, index - wordStart) == name) {
                const auto equals = skipSpace(source, index,
                                               assignment.callClose);
                if (equals < assignment.callClose && source[equals] == '=') {
                    valueStart = skipSpace(source, equals + 1,
                                           assignment.callClose);
                    auto scan = valueStart;
                    int valueParens = 0;
                    int valueBrackets = 0;
                    int valueBraces = 0;
                    char valueQuote = 0;
                    bool valueEscaped = false;
                    for (; scan < assignment.callClose; ++scan) {
                        const char current = source[scan];
                        if (valueQuote != 0) {
                            if (valueEscaped) {
                                valueEscaped = false;
                            } else if (current == '\\') {
                                valueEscaped = true;
                            } else if (current == valueQuote) {
                                valueQuote = 0;
                            }
                            continue;
                        }
                        if (current == '\'' || current == '"') {
                            valueQuote = current;
                            continue;
                        }
                        if (current == '(') ++valueParens;
                        else if (current == '[') ++valueBrackets;
                        else if (current == '{') ++valueBraces;
                        else if (current == ')' && valueParens > 0) --valueParens;
                        else if (current == ']' && valueBrackets > 0) --valueBrackets;
                        else if (current == '}' && valueBraces > 0) --valueBraces;
                        else if (current == ',' && valueParens == 0 &&
                                 valueBrackets == 0 && valueBraces == 0)
                            break;
                        else if (current == '#' && valueParens == 0 &&
                                 valueBrackets == 0 && valueBraces == 0)
                            break;
                    }
                    valueEnd = scan;
                    while (valueEnd > valueStart &&
                           std::isspace(static_cast<unsigned char>(
                               source[valueEnd - 1])) != 0)
                        --valueEnd;
                    return true;
                }
            }
            continue;
        }
        if (value == '(') ++parens;
        else if (value == ')' && parens > 0) --parens;
        else if (value == '[') ++brackets;
        else if (value == ']' && brackets > 0) --brackets;
        else if (value == '{') ++braces;
        else if (value == '}' && braces > 0) --braces;
        ++index;
    }
    return false;
}

inline std::size_t countTopLevelItems(std::string_view source,
                                      std::size_t open, std::size_t close) {
    const auto contentStart = open + 1;
    const auto first = source.find_first_not_of(" \t\r\n", contentStart);
    if (first == std::string_view::npos || first >= close)
        return 0;
    std::size_t count = 1;
    int parens = 0;
    int brackets = 0;
    int braces = 0;
    char quote = 0;
    bool escaped = false;
    for (std::size_t index = contentStart; index < close; ++index) {
        const char value = source[index];
        if (quote != 0) {
            if (escaped) escaped = false;
            else if (value == '\\') escaped = true;
            else if (value == quote) quote = 0;
            continue;
        }
        if (value == '\'' || value == '"') quote = value;
        else if (value == '(') ++parens;
        else if (value == ')' && parens > 0) --parens;
        else if (value == '[') ++brackets;
        else if (value == ']' && brackets > 0) --brackets;
        else if (value == '{') ++braces;
        else if (value == '}' && braces > 0) --braces;
        else if (value == ',' && parens == 0 && brackets == 0 &&
                 braces == 0)
            ++count;
    }
    const auto last = source.find_last_not_of(" \t\r\n", close - 1);
    if (last != std::string_view::npos && source[last] == ',' && count > 0)
        --count;
    return count;
}

inline bool setPlayerArgument(std::string& source,
                              const PlayerAssignment& assignment,
                              std::string_view name, std::string_view value) {
    std::size_t valueStart = 0;
    std::size_t valueEnd = 0;
    if (findArgumentValue(source, assignment, name, true,
                          valueStart, valueEnd)) {
        source.replace(valueStart, valueEnd - valueStart, value);
        return true;
    }

    const auto hasArguments = skipSpace(source, assignment.callOpen + 1,
                                        assignment.callClose) !=
                              assignment.callClose;
    const auto last = source.find_last_not_of(" \t\r\n",
                                              assignment.callClose - 1);
    const auto separator = !hasArguments ? "" :
        (last != std::string::npos && source[last] == ',') ? " " : ", ";
    const auto insertion = std::string(separator) +
                           std::string(name) + "=" + std::string(value);
    source.insert(assignment.callClose, insertion);
    return true;
}

inline bool replacePlayerSynth(std::string& source,
                               const PlayerAssignment& assignment,
                               std::string_view synth) {
    if (!isSimpleIdentifier(synth))
        return false;
    source.replace(assignment.synthStart,
                   assignment.synthEnd - assignment.synthStart, synth);
    return true;
}

inline bool setPlayerEffectMix(std::string& source,
                              const PlayerAssignment& assignment,
                              std::string_view value) {
    std::size_t effectsStart = 0;
    std::size_t effectsEnd = 0;
    if (!findArgumentValue(source, assignment, "effects", true,
                           effectsStart, effectsEnd) ||
        effectsStart >= effectsEnd || source[effectsStart] != '[')
        return setPlayerArgument(source, assignment, "fx_mix", value);

    std::size_t effectsClose = 0;
    if (!findMatchingDelimiter(source, effectsStart, effectsEnd, '[', ']',
                               effectsClose))
        return false;
    const auto dictOpen = source.find('{', effectsStart + 1);
    if (dictOpen == std::string::npos || dictOpen >= effectsClose)
        return false;
    std::size_t dictClose = 0;
    if (!findMatchingDelimiter(source, dictOpen, effectsClose + 1, '{', '}',
                               dictClose))
        return false;

    const auto dictContents =
        std::string_view(source).substr(dictOpen + 1, dictClose - dictOpen - 1);
    auto key = dictContents.find("\"fx_mix\"");
    std::size_t keyLength = 8;
    if (key == std::string_view::npos) {
        key = dictContents.find("'fx_mix'");
        keyLength = 8;
    }
    if (key != std::string_view::npos) {
        const auto colon = dictContents.find(':', key + keyLength);
        if (colon == std::string_view::npos)
            return false;
        auto relativeStart = skipSpace(dictContents, colon + 1,
                                       dictContents.size());
        auto relativeEnd = relativeStart;
        int parens = 0;
        int brackets = 0;
        int braces = 0;
        char quote = 0;
        bool escaped = false;
        for (; relativeEnd < dictContents.size(); ++relativeEnd) {
            const char current = dictContents[relativeEnd];
            if (quote != 0) {
                if (escaped) escaped = false;
                else if (current == '\\') escaped = true;
                else if (current == quote) quote = 0;
                continue;
            }
            if (current == '\'' || current == '"') quote = current;
            else if (current == '(') ++parens;
            else if (current == ')' && parens > 0) --parens;
            else if (current == '[') ++brackets;
            else if (current == ']' && brackets > 0) --brackets;
            else if (current == '{') ++braces;
            else if (current == '}' && braces > 0) --braces;
            else if ((current == ',' || current == '#') && parens == 0 &&
                     brackets == 0 && braces == 0)
                break;
        }
        while (relativeEnd > relativeStart &&
               std::isspace(static_cast<unsigned char>(
                   dictContents[relativeEnd - 1])) != 0)
            --relativeEnd;
        if (relativeEnd <= relativeStart)
            return false;
        const auto originalStart = dictOpen + 1 + relativeStart;
        source.replace(originalStart, relativeEnd - relativeStart, value);
        return true;
    }
    const auto first = source.find_first_not_of(" \t\r\n", dictOpen + 1);
    const bool empty = first == std::string::npos || first >= dictClose;
    const auto last = source.find_last_not_of(" \t\r\n", dictClose - 1);
    const std::string prefix = empty ? "\"fx_mix\": " :
        (last != std::string::npos && source[last] == ',')
            ? " \"fx_mix\": " : ", \"fx_mix\": ";
    source.insert(dictClose, prefix + std::string(value));
    return true;
}

inline bool setPlayerEffect(std::string& source,
                            const PlayerAssignment& assignment,
                            std::string_view effect) {
    if (effect.empty())
        return false;
    for (const char character : effect) {
        if (!isIdentifierChar(character))
            return false;
    }
    std::size_t effectsStart = 0;
    std::size_t effectsEnd = 0;
    if (findArgumentValue(source, assignment, "effects", true,
                          effectsStart, effectsEnd)) {
        auto listOpen = source.find('[', effectsStart);
        if (listOpen == std::string::npos || listOpen >= effectsEnd)
            return false;
        std::size_t listClose = 0;
        if (!findMatchingDelimiter(source, listOpen, effectsEnd,
                                   '[', ']', listClose))
            return false;
        if (countTopLevelItems(source, listOpen, listClose) >= 4)
            return false;
        const auto content = source.substr(listOpen + 1,
                                           listClose - listOpen - 1);
        const auto hasEntries = content.find_first_not_of(" \t\r\n") !=
                                std::string::npos;
        const auto last = source.find_last_not_of(" \t\r\n", listClose - 1);
        const auto separator = !hasEntries ? "" :
            (last != std::string::npos && source[last] == ',') ? " " : ", ";
        const auto addition = std::string(separator) +
                              "(\"" + std::string(effect) + "\", {})";
        source.insert(listClose, addition);
        return true;
    }

    std::size_t effectStart = 0;
    std::size_t effectEnd = 0;
    if (findArgumentValue(source, assignment, "effect", true,
                          effectStart, effectEnd)) {
        source.replace(effectStart, effectEnd - effectStart,
                       "\"" + std::string(effect) + "\"");
        return true;
    }
    return setPlayerArgument(source, assignment, "effect",
                             "\"" + std::string(effect) + "\"");
}

} // namespace dexter::editor
