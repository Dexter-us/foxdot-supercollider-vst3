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

inline bool findPlayerAssignmentAt(std::string_view source,
                                   std::size_t position,
                                   PlayerAssignment& result);
inline std::size_t skipSpace(std::string_view text, std::size_t position,
                             std::size_t end);

inline bool findPlayerLineAt(std::string_view source, std::size_t position,
                             PlayerAssignment& result,
                             bool allowBareLine = false) {
    if (findPlayerAssignmentAt(source, position, result))
        return true;
    if (!allowBareLine || position > source.size())
        return false;
    const auto lineStart = position == 0 ? 0 :
        source.rfind('\n', position - 1) == std::string_view::npos ? 0 :
        source.rfind('\n', position - 1) + 1;
    auto lineEnd = source.find('\n', position);
    if (lineEnd == std::string_view::npos)
        lineEnd = source.size();
    auto cursor = skipSpace(source, lineStart, lineEnd);
    if (cursor >= lineEnd || source[cursor++] != 'p')
        return false;
    const auto digitsStart = cursor;
    while (cursor < lineEnd &&
           std::isdigit(static_cast<unsigned char>(source[cursor])) != 0)
        ++cursor;
    if (digitsStart == cursor)
        return false;
    int player = 0;
    for (auto index = digitsStart; index < cursor; ++index) {
        player = player * 10 + source[index] - '0';
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
    if (cursor < lineEnd && source[cursor] != '#')
        return false;
    result = {};
    result.lineStart = lineStart;
    result.lineEnd = lineEnd;
    result.synthStart = result.synthEnd = cursor;
    result.callOpen = result.callClose = cursor;
    result.playerIndex = player - 1;
    return true;
}

inline bool insertPlayerCall(std::string& source,
                             const PlayerAssignment& assignment,
                             std::string_view call) {
    if (assignment.callOpen != assignment.callClose ||
        assignment.callOpen > source.size() || call.empty())
        return false;
    source.insert(assignment.callOpen, call);
    return true;
}

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

inline bool findTopLevelItemBounds(std::string_view source,
                                   std::size_t open, std::size_t close,
                                   std::size_t wanted,
                                   std::size_t& itemStart,
                                   std::size_t& itemEnd) {
    if (open >= close || source[open] != '[' || source[close] != ']')
        return false;
    std::size_t start = open + 1;
    std::size_t item = 0;
    int parens = 0;
    int brackets = 0;
    int braces = 0;
    char quote = 0;
    bool escaped = false;
    for (std::size_t index = start; index <= close; ++index) {
        if (index < close) {
            const char current = source[index];
            if (quote != 0) {
                if (escaped) escaped = false;
                else if (current == '\\') escaped = true;
                else if (current == quote) quote = 0;
                continue;
            }
            if (current == '\'' || current == '"') {
                quote = current;
                continue;
            }
            if (current == '#') {
                while (index < close && source[index] != '\n')
                    ++index;
                if (index >= close)
                    break;
            }
            if (current == '(') ++parens;
            else if (current == ')' && parens > 0) --parens;
            else if (current == '[') ++brackets;
            else if (current == ']' && brackets > 0) --brackets;
            else if (current == '{') ++braces;
            else if (current == '}' && braces > 0) --braces;
            else if (current == ',' && parens == 0 && brackets == 0 &&
                     braces == 0) {
                if (item == wanted) {
                    itemStart = skipSpace(source, start, index);
                    itemEnd = index;
                    while (itemEnd > itemStart &&
                           std::isspace(static_cast<unsigned char>(
                               source[itemEnd - 1])) != 0)
                        --itemEnd;
                    return itemStart < itemEnd;
                }
                ++item;
                start = index + 1;
            }
            continue;
        }
        if (item == wanted) {
            itemStart = skipSpace(source, start, close);
            itemEnd = close;
            while (itemEnd > itemStart &&
                   std::isspace(static_cast<unsigned char>(
                       source[itemEnd - 1])) != 0)
                --itemEnd;
            return itemStart < itemEnd;
        }
    }
    return false;
}

inline bool setPlayerEffectControl(std::string& source,
                                   const PlayerAssignment& assignment,
                                   std::size_t effectIndex,
                                   std::string_view controlName,
                                   std::string_view value) {
    if (!isSimpleIdentifier(controlName) || controlName.size() < 3 ||
        controlName.substr(0, 3) != "fx_")
        return false;
    std::size_t listStart = 0;
    std::size_t listEnd = 0;
    if (!findArgumentValue(source, assignment, "effects", true,
                           listStart, listEnd) ||
        listStart >= listEnd || source[listStart] != '[') {
        if (effectIndex != 0)
            return false;
        return setPlayerArgument(source, assignment, controlName, value);
    }
    std::size_t listClose = 0;
    if (!findMatchingDelimiter(source, listStart, listEnd, '[', ']',
                               listClose))
        return false;
    std::size_t tupleStart = 0;
    std::size_t tupleEnd = 0;
    if (!findTopLevelItemBounds(source, listStart, listClose, effectIndex,
                                tupleStart, tupleEnd) ||
        source[tupleStart] != '(')
        return false;
    std::size_t tupleClose = 0;
    if (!findMatchingDelimiter(source, tupleStart, tupleEnd, '(', ')',
                               tupleClose))
        return false;
    auto dictStart = source.find('{', tupleStart + 1);
    if (dictStart == std::string::npos || dictStart >= tupleClose)
        return false;
    std::size_t dictClose = 0;
    if (!findMatchingDelimiter(source, dictStart, tupleClose + 1, '{', '}',
                               dictClose))
        return false;

    const auto dictionary = std::string_view(source);
    std::size_t entryStart = dictStart + 1;
    for (std::size_t index = entryStart; index <= dictClose; ++index) {
        if (index < dictClose) {
            const char current = dictionary[index];
            if (current == '\'' || current == '"') {
                const char quote = current;
                std::size_t keyEnd = index + 1;
                bool escaped = false;
                for (; keyEnd < dictClose; ++keyEnd) {
                    if (escaped) escaped = false;
                    else if (dictionary[keyEnd] == '\\') escaped = true;
                    else if (dictionary[keyEnd] == quote) break;
                }
                if (keyEnd >= dictClose)
                    return false;
                const auto colon = skipSpace(dictionary, keyEnd + 1, dictClose);
                if (colon < dictClose && dictionary[colon] == ':') {
                    const auto key = dictionary.substr(
                        index + 1, keyEnd - index - 1);
                    if (key == controlName) {
                        auto valueStart = skipSpace(
                            dictionary, colon + 1, dictClose);
                        auto valueEnd = valueStart;
                        int parens = 0;
                        int brackets = 0;
                        int braces = 0;
                        char valueQuote = 0;
                        bool valueEscaped = false;
                        for (; valueEnd < dictClose; ++valueEnd) {
                            const char item = dictionary[valueEnd];
                            if (valueQuote != 0) {
                                if (valueEscaped) valueEscaped = false;
                                else if (item == '\\') valueEscaped = true;
                                else if (item == valueQuote) valueQuote = 0;
                                continue;
                            }
                            if (item == '\'' || item == '"') valueQuote = item;
                            else if (item == '(') ++parens;
                            else if (item == ')' && parens > 0) --parens;
                            else if (item == '[') ++brackets;
                            else if (item == ']' && brackets > 0) --brackets;
                            else if (item == '{') ++braces;
                            else if (item == '}' && braces > 0) --braces;
                            else if (item == ',' && parens == 0 &&
                                     brackets == 0 && braces == 0)
                                break;
                        }
                        while (valueEnd > valueStart &&
                               std::isspace(static_cast<unsigned char>(
                                   dictionary[valueEnd - 1])) != 0)
                            --valueEnd;
                        if (valueStart >= valueEnd)
                            return false;
                        source.replace(valueStart, valueEnd - valueStart,
                                       value);
                        return true;
                    }
                    index = colon;
                } else {
                    index = keyEnd;
                }
            } else if (current == '#') {
                while (index < dictClose && dictionary[index] != '\n')
                    ++index;
                entryStart = index + 1;
            }
            continue;
        }
        const auto entry = dictionary.substr(entryStart, dictClose-entryStart);
        const auto first = entry.find_first_not_of(" \t\r\n");
        const bool empty = first == std::string_view::npos;
        const auto last = entry.find_last_not_of(" \t\r\n");
        const bool trailingComma = !empty && entry[last] == ',';
        const std::string prefix = empty ? "\"" + std::string(controlName) +
            "\": " : trailingComma ? " \"" + std::string(controlName) +
            "\": " : ", \"" + std::string(controlName) + "\": ";
        source.insert(dictClose, prefix + std::string(value));
        return true;
    }
    return false;
}

inline bool setPlayerListItem(std::string& source,
                              const PlayerAssignment& assignment,
                              std::string_view listName,
                              std::string_view value) {
    if (listName != "effects" || !isSimpleIdentifier(value))
        return false;
    std::size_t valueStart = 0;
    std::size_t valueEnd = 0;
    if (!findArgumentValue(source, assignment, listName, true,
                           valueStart, valueEnd))
        return false;
    const auto listOpen = source.find('[', valueStart);
    if (listOpen == std::string::npos || listOpen >= valueEnd)
        return false;
    std::size_t listClose = 0;
    if (!findMatchingDelimiter(source, listOpen, valueEnd,
                               '[', ']', listClose))
        return false;
    if (countTopLevelItems(source, listOpen, listClose) >= 4)
        return false;
    const auto last = source.find_last_not_of(" \t\r\n", listClose - 1);
    const bool hasItems = last != std::string::npos && last != listOpen &&
                          source[listOpen + 1] != ']';
    const auto separator = !hasItems ? "" :
        source[last] == ',' ? " " : ", ";
    source.insert(listClose, std::string(separator) + "(\"" +
                 std::string(value) + "\", {})");
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

inline constexpr std::string_view kEditorScaleNames[] = {
    "chromatic", "major", "minor", "natural_minor", "aeolian", "dorian",
    "dorian2", "mixolydian", "major_pentatonic", "majorPentatonic",
    "minor_pentatonic", "minorPentatonic", "melodic_minor", "melodicMinor",
    "melodic_major", "melodicMajor", "harmonic_minor", "harmonicMinor",
    "harmonic_major", "harmonicMajor", "diminished", "egyptian", "yu", "zhi",
    "phrygian", "prometheus", "indian", "locrian", "locrian_major",
    "locrianMajor", "lydian", "lydian_minor", "lydianMinor", "custom",
    "hungarian_minor", "hungarianMinor", "romanian_minor", "romanianMinor",
    "chinese", "whole_tone", "wholeTone", "half_whole", "halfWhole",
    "whole_half", "wholeHalf", "bebop_maj", "bebopMaj", "bebop_dorian",
    "bebopDorian", "bebop_dom", "bebopDom", "bebop_mel_min", "bebopMelMin",
    "blues", "min_maj", "minMaj", "susb9", "lydian_aug", "lydianAug",
    "lydian_dom", "lydianDom", "mel_min5th", "melMin5th", "half_dim",
    "halfDim", "altered"
};

inline bool isEditorScaleName(std::string_view name) {
    for (const auto candidate : kEditorScaleNames)
        if (candidate == name)
            return true;
    return false;
}

inline std::size_t pythonCommentStart(std::string_view source,
                                      std::size_t start,
                                      std::size_t end) {
    char quote = 0;
    bool escaped = false;
    for (std::size_t index = start; index < end; ++index) {
        const char value = source[index];
        if (quote != 0) {
            if (escaped)
                escaped = false;
            else if (value == '\\')
                escaped = true;
            else if (value == quote)
                quote = 0;
        } else if (value == '\'' || value == '"') {
            quote = value;
        } else if (value == '#') {
            return index;
        }
    }
    return end;
}

inline bool scaleDefaultLineAt(std::string_view source, std::size_t position,
                               std::size_t& lineStart,
                               std::size_t& lineEnd,
                               std::size_t& statementStart) {
    if (position > source.size())
        return false;
    if (position == 0) {
        lineStart = 0;
    } else {
        const auto previousNewline = source.rfind('\n', position - 1);
        lineStart = previousNewline == std::string_view::npos
            ? 0 : previousNewline + 1;
    }
    lineEnd = source.find('\n', position);
    if (lineEnd == std::string_view::npos)
        lineEnd = source.size();
    const auto codeEnd = pythonCommentStart(source, lineStart, lineEnd);
    statementStart = skipSpace(source, lineStart, codeEnd);
    constexpr std::string_view prefix = "Scale.default";
    return codeEnd - statementStart >= prefix.size() &&
           source.substr(statementStart, prefix.size()) == prefix &&
           (statementStart + prefix.size() == codeEnd ||
            !isIdentifierChar(source[statementStart + prefix.size()]));
}

inline bool scaleContextTargetAt(std::string_view source,
                                 std::size_t position) {
    std::size_t lineStart = 0;
    std::size_t lineEnd = 0;
    std::size_t statementStart = 0;
    return scaleDefaultLineAt(source, position, lineStart, lineEnd,
                              statementStart);
}

inline std::string scaleDefaultNameAt(std::string_view source,
                                     std::size_t position) {
    std::size_t lineStart = 0;
    std::size_t lineEnd = 0;
    std::size_t statementStart = 0;
    if (!scaleDefaultLineAt(source, position, lineStart, lineEnd,
                            statementStart))
        return {};
    const auto codeEnd = pythonCommentStart(source, lineStart, lineEnd);
    constexpr std::string_view prefix = "Scale.default";
    auto cursor = skipSpace(source, statementStart + prefix.size(), codeEnd);
    std::size_t valueStart = 0;
    if (cursor < codeEnd && source[cursor] == '=') {
        cursor = skipSpace(source, cursor + 1, codeEnd);
        valueStart = cursor;
    } else if (source.substr(cursor, 5) == ".set(") {
        cursor = skipSpace(source, cursor + 5, codeEnd);
        if (cursor >= codeEnd)
            return {};
        if (source[cursor] == '\'' || source[cursor] == '"') {
            const char quote = source[cursor++];
            valueStart = cursor;
            while (cursor < codeEnd && source[cursor] != quote)
                ++cursor;
            if (cursor >= codeEnd)
                return {};
            const auto name = source.substr(valueStart, cursor - valueStart);
            return isEditorScaleName(name) ? std::string(name) : std::string{};
        }
        valueStart = cursor;
    } else {
        return {};
    }

    if (valueStart >= codeEnd)
        return {};
    constexpr std::string_view scalePrefix = "Scale.";
    if (source.substr(valueStart, scalePrefix.size()) == scalePrefix)
        valueStart += scalePrefix.size();
    else if (source[valueStart] == '\'' || source[valueStart] == '"') {
        const char quote = source[valueStart++];
        const auto nameStart = valueStart;
        while (valueStart < codeEnd && source[valueStart] != quote)
            ++valueStart;
        if (valueStart >= codeEnd)
            return {};
        const auto name = source.substr(nameStart, valueStart - nameStart);
        return isEditorScaleName(name) ? std::string(name) : std::string{};
    } else {
        return {};
    }

    auto nameEnd = valueStart;
    while (nameEnd < codeEnd && isIdentifierChar(source[nameEnd]))
        ++nameEnd;
    const auto name = source.substr(valueStart, nameEnd - valueStart);
    return isEditorScaleName(name) ? std::string(name) : std::string{};
}

inline std::string currentEditorScale(std::string_view source) {
    std::string current;
    std::size_t lineStart = 0;
    while (lineStart < source.size()) {
        const auto lineEnd = source.find('\n', lineStart);
        std::size_t statementStart = 0;
        std::size_t ignoredStart = 0;
        std::size_t ignoredEnd = 0;
        if (scaleDefaultLineAt(source, lineStart, ignoredStart, ignoredEnd,
                               statementStart))
            current = scaleDefaultNameAt(source, lineStart);
        if (lineEnd == std::string_view::npos)
            break;
        lineStart = lineEnd + 1;
    }
    return current;
}

inline bool setEditorScaleAt(std::string& source, std::size_t position,
                             std::string_view scaleName) {
    if (!isEditorScaleName(scaleName) || position > source.size())
        return false;
    std::size_t lineStart = 0;
    std::size_t lineEnd = 0;
    std::size_t statementStart = 0;
    if (scaleDefaultLineAt(source, position, lineStart, lineEnd,
                           statementStart)) {
        const auto codeEnd = pythonCommentStart(source, lineStart, lineEnd);
        auto replaceEnd = codeEnd;
        while (replaceEnd > statementStart &&
               std::isspace(static_cast<unsigned char>(
                   source[replaceEnd - 1])) != 0)
            --replaceEnd;
        source.replace(statementStart, replaceEnd - statementStart,
                       "Scale.default = Scale." + std::string(scaleName));
        return true;
    }
    source.insert(0, "Scale.default = Scale." +
                     std::string(scaleName) + "\n");
    return true;
}

} // namespace dexter::editor
