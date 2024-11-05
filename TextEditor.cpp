#include "pch.h"
#include "TextEditor.h"
#include <unordered_map>

#undef min
#undef max

extern void AddLineDashed(ImDrawList* drawList, const ImVec2& a, const ImVec2& b, ImU32 col, float thickness = 1.0f, unsigned int num_segments = 10, unsigned int on_segments = 1, unsigned int off_segments = 1);
// TODO
// - multiline comments vs single-line: latter is blocking start of a ML
std::wstring MakeString(std::string& source)
{
	return std::wstring(source.begin(), source.end());
}
std::wstring MakeString(std::wstring& source)
{
	return source;
}
std::wstring MakeString(const char* sources)
{
	std::string source(sources);
	return std::wstring(source.begin(), source.end());
}
template<class InputIt1, class InputIt2, class BinaryPredicate>
bool equals(InputIt1 first1, InputIt1 last1,
	InputIt2 first2, InputIt2 last2, BinaryPredicate p)
{
	for (; first1 != last1 && first2 != last2; ++first1, ++first2)
	{
		if (!p(*first1, *first2))
			return false;
	}
	return first1 == last1 && first2 == last2;
}

auto TextEditor::LanguageDefinition::GetIdentifier(const _stdstr& key)
{
	/*if (mIdentifiers.find(key) != mIdentifiers.end())
		return mIdentifiers.find(key);
	else*/ if (mClasses.find(key) != mClasses.end())
		return mClasses.find(key);
	else if (mFunctions.find(key) != mFunctions.end())
		return mFunctions.find(key);
	else if (mEnums.find(key) != mEnums.end())
		return mEnums.find(key);
	else if (mStructs.find(key) != mStructs.end())
		return mStructs.find(key);
	return mIdentifiers.find(key);
}
auto TextEditor::LanguageDefinition::GetMultiIdentifier(const _stdstr& key)
{
	if (mParameterProperties.find(key) != mParameterProperties.end())
		return mParameterProperties.find(key);
	else if (mLocalProperties.find(key) != mLocalProperties.end())
		return mLocalProperties.find(key);
	else if (mMemberProperties.find(key) != mMemberProperties.end())
		return mMemberProperties.find(key);
	return mParameterProperties.end();
}
TextEditor::TextEditor()
	: mLineSpacing(1.0f)
	, mUndoIndex(0)
	, mTabSize(4)
	, mOverwrite(false)
	, mReadOnly(false)
	, mWithinRender(false)
	, mScrollToCursor(false)
	, mScrollToTop(false)
	, mTextChanged(false)
	, mColorizerEnabled(true)
	, mLineNoStart(20.0f)
	, mCollapsorStart(15.0f)
	, mTextStart(mLineNoStart + mCollapsorStart + 20.0f)
	, mLeftMargin(10)
	, mCursorPositionChanged(false)
	, mCursorOnABracket(false)
	, mMatchingBracket(Coordinates())
	, mColorRangeMin(0)
	, mColorRangeMax(0)
	, mSelectionMode(SelectionMode::Normal)
	, mCheckComments(true)
	, mLastClick(-1.0f)
	, mHandleKeyboardInputs(true)
	, mHandleMouseInputs(true)
	, mIgnoreImGuiChild(true)
	, mShowWhitespaces(false)
	, mAllowTooltips(true)
	, mDrawBars(true)
	, mShowCollapsables(true)
	, mOnBreakpointToggle(NULL)
	, mTriggeredBPLine(-1)
	, mOnPropertyIndentifierHover(NULL)
	, mOnMultiPropertyIndentifierHover(NULL)
	, mHaltRender(false)
	, mStartTime(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count())
{
	SetPalette(GetDarkPalette());
	SetLanguageDefinition(LanguageDefinition::CPlusPlus());
	mLines.push_back(Line());
}

TextEditor::~TextEditor()
{
}

void TextEditor::Clear()
{
	mActiveCollapsables.clear();
	mIgnoredActiveCollapsables.clear();
	mCollapsables.clear();
	mLines.clear();
	mLines.push_back(Line());
	SetCursorPosition(Coordinates(0, 0));

}

void TextEditor::SetLanguageDefinition(const LanguageDefinition& aLanguageDef)
{
	mLanguageDefinition = aLanguageDef;
	mRegexList.clear();

	for (auto& r : mLanguageDefinition.mTokenRegexStrings)
		mRegexList.push_back(std::make_pair(_regx(r.first, std::regex_constants::optimize), r.second));

	Colorize();
}

void TextEditor::AddIdentifier(const _stdstr& key, const Identifier& idf, IdentifierType idft)
{
	const_cast<LanguageDefinition&>(GetLanguageDefinition()).AddIdentifier(key, idf, idft);
}

void TextEditor::SetPalette(const Palette& aValue)
{
	mPaletteBase = aValue;
}

_stdstr TextEditor::GetText(const Coordinates& aStart, const Coordinates& aEnd) const
{
	_stdstr result;

	auto lstart = aStart.mLine;
	auto lend = aEnd.mLine;
	auto istart = GetCharacterIndex(aStart);
	auto iend = GetCharacterIndex(aEnd);
	size_t s = 0;

	for (size_t i = lstart; i < lend; i++)
		s += mLines[i].size();

	result.reserve(s + s / 8);

	while (istart < iend || lstart < lend)
	{
		if (lstart >= (int)mLines.size())
			break;

		auto& line = mLines[lstart];
		if (istart < (int)line.size())
		{
			result += line[istart].mChar;
			istart++;
		}
		else
		{
			istart = 0;
			++lstart;
			result += '\n';
		}
	}

	return result;
}

TextEditor::Coordinates TextEditor::GetActualCursorCoordinates() const
{
	return SanitizeCoordinates(mState.mCursorPosition);
}

TextEditor::Coordinates TextEditor::GetCollapsedCursordCoordinates(Coordinates& aActualCorrdinates) const
{
	Coordinates cd(aActualCorrdinates);
	if (mShowCollapsables)
	{
		for (auto& activeCollapsed : mActiveCollapsables)
		{
			if (mIgnoredActiveCollapsables.find(activeCollapsed.first) == mIgnoredActiveCollapsables.end())
			{
				if (aActualCorrdinates.mLine > activeCollapsed.first)
				{
					if (aActualCorrdinates.mLine > activeCollapsed.second) //if current is outside the collapsed box
						cd.mLine -= (activeCollapsed.second - activeCollapsed.first);
					else
						cd.mLine = activeCollapsed.first;
				}
			}
		}
	}
	return cd;
}

TextEditor::Coordinates TextEditor::SanitizeCoordinates(const Coordinates& aValue) const
{
	auto line = aValue.mLine;
	auto column = aValue.mColumn;
	if (line >= (int)mLines.size())
	{
		if (mLines.empty())
		{
			line = 0;
			column = 0;
		}
		else
		{
			line = (int)mLines.size() - 1;
			column = GetLineMaxColumn(line);
		}
		return Coordinates(line, column);
	}
	else
	{
		column = mLines.empty() ? 0 : std::min(column, GetLineMaxColumn(line));
		return Coordinates(line, column);
	}
}

// https://en.wikipedia.org/wiki/UTF-8
// We assume that the char is a standalone character (<128) or a leading byte of an UTF-8 code sequence (non-10xxxxxx code)
static int UTF8CharLength(TextEditor::Char c)
{
	if ((c & 0xFE) == 0xFC)
		return 6;
	if ((c & 0xFC) == 0xF8)
		return 5;
	if ((c & 0xF8) == 0xF0)
		return 4;
	else if ((c & 0xF0) == 0xE0)
		return 3;
	else if ((c & 0xE0) == 0xC0)
		return 2;
	return 1;
}

// "Borrowed" from ImGui source
static inline int ImTextCharToUtf8(char* buf, int buf_size, unsigned int c)
{
	if (c < 0x80)
	{
		buf[0] = (char)c;
		return 1;
	}
	if (c < 0x800)
	{
		if (buf_size < 2) return 0;
		buf[0] = (char)(0xc0 + (c >> 6));
		buf[1] = (char)(0x80 + (c & 0x3f));
		return 2;
	}
	if (c >= 0xdc00 && c < 0xe000)
	{
		return 0;
	}
	if (c >= 0xd800 && c < 0xdc00)
	{
		if (buf_size < 4) return 0;
		buf[0] = (char)(0xf0 + (c >> 18));
		buf[1] = (char)(0x80 + ((c >> 12) & 0x3f));
		buf[2] = (char)(0x80 + ((c >> 6) & 0x3f));
		buf[3] = (char)(0x80 + ((c) & 0x3f));
		return 4;
	}
	//else if (c < 0x10000)
	{
		if (buf_size < 3) return 0;
		buf[0] = (char)(0xe0 + (c >> 12));
		buf[1] = (char)(0x80 + ((c >> 6) & 0x3f));
		buf[2] = (char)(0x80 + ((c) & 0x3f));
		return 3;
	}
}

void TextEditor::Advance(Coordinates& aCoordinates) const
{
	if (aCoordinates.mLine < (int)mLines.size())
	{
		auto& line = mLines[aCoordinates.mLine];
		auto cindex = GetCharacterIndex(aCoordinates);

		if (cindex + 1 < (int)line.size())
		{
			auto delta = UTF8CharLength(line[cindex].mChar);
			cindex = std::min(cindex + delta, (int)line.size() - 1);
		}
		else
		{
			++aCoordinates.mLine;
			cindex = 0;
		}
		aCoordinates.mColumn = GetCharacterColumn(aCoordinates.mLine, cindex);
	}
}

void TextEditor::DeleteRange(const Coordinates& aStart, const Coordinates& aEnd)
{
	assert(aEnd >= aStart);
	assert(!mReadOnly);

	//printf("D(%d.%d)-(%d.%d)\n", aStart.mLine, aStart.mColumn, aEnd.mLine, aEnd.mColumn);

	if (aEnd == aStart)
		return;

	auto start = GetCharacterIndex(aStart);
	auto end = GetCharacterIndex(aEnd);

	if (aStart.mLine == aEnd.mLine)
	{
		auto& line = mLines[aStart.mLine];
		auto n = GetLineMaxColumn(aStart.mLine);
		if (aEnd.mColumn >= n)
			line.erase(line.begin() + start, line.end());
		else
			line.erase(line.begin() + start, line.begin() + end);
	}
	else
	{
		auto& firstLine = mLines[aStart.mLine];
		auto& lastLine = mLines[aEnd.mLine];

		firstLine.erase(firstLine.begin() + start, firstLine.end());
		lastLine.erase(lastLine.begin(), lastLine.begin() + end);

		if (aStart.mLine < aEnd.mLine)
			firstLine.insert(firstLine.end(), lastLine.begin(), lastLine.end());

		if (aStart.mLine < aEnd.mLine)
			RemoveLine(aStart.mLine + 1, aEnd.mLine + 1);
	}

	mTextChanged = true;
}

int TextEditor::InsertTextAt(Coordinates& /* inout */ aWhere, _constcharptr aValue)
{
	assert(!mReadOnly);

	int cindex = GetCharacterIndex(aWhere);
	int totalLines = 0;
	while (*aValue != mystr('\0'))
	{
		assert(!mLines.empty());

		if (*aValue == mystr('\r'))
		{
			// skip
			++aValue;
		}
		else if (*aValue == mystr('\n'))
		{
			if (cindex < (int)mLines[aWhere.mLine].size())
			{
				auto& newLine = InsertLine(aWhere.mLine + 1, cindex);
				auto& line = mLines[aWhere.mLine];
				newLine.insert(newLine.begin(), line.begin() + cindex, line.end());
				line.erase(line.begin() + cindex, line.end());
			}
			else
			{
				InsertLine(aWhere.mLine + 1, cindex);
			}
			++aWhere.mLine;
			aWhere.mColumn = 0;
			cindex = 0;
			++totalLines;
			++aValue;
		}
		else
		{
			auto& line = mLines[aWhere.mLine];
			auto d = UTF8CharLength(*aValue);
			while (d-- > 0 && *aValue != mystr('\0'))
				line.insert(line.begin() + cindex++, Glyph(*aValue++, PaletteIndex::Default));
			++aWhere.mColumn;
		}

		mTextChanged = true;
	}

	return totalLines;
}

void TextEditor::AddUndo(UndoRecord& aValue)
{
	assert(!mReadOnly);
	//printf("AddUndo: (@%d.%d) +\'%s' [%d.%d .. %d.%d], -\'%s', [%d.%d .. %d.%d] (@%d.%d)\n",
	//	aValue.mBefore.mCursorPosition.mLine, aValue.mBefore.mCursorPosition.mColumn,
	//	aValue.mAdded.c_str(), aValue.mAddedStart.mLine, aValue.mAddedStart.mColumn, aValue.mAddedEnd.mLine, aValue.mAddedEnd.mColumn,
	//	aValue.mRemoved.c_str(), aValue.mRemovedStart.mLine, aValue.mRemovedStart.mColumn, aValue.mRemovedEnd.mLine, aValue.mRemovedEnd.mColumn,
	//	aValue.mAfter.mCursorPosition.mLine, aValue.mAfter.mCursorPosition.mColumn
	//	);

	mUndoBuffer.resize((size_t)(mUndoIndex + 1));
	mUndoBuffer.back() = aValue;
	++mUndoIndex;
}

TextEditor::Coordinates TextEditor::ScreenPosToCoordinates(const ImVec2& aPosition, bool* isOverLineNumber, bool* isOverBPPoint, bool* isOverCollaper) const
{
	ImVec2 origin = ImGui::GetCursorScreenPos();
	ImVec2 local(aPosition.x - origin.x, aPosition.y - origin.y);
	if (!!isOverBPPoint)
		*isOverBPPoint = local.x < mLineNoStart;
	if (isOverLineNumber != nullptr)
		*isOverLineNumber = local.x < (mTextStart - mCollapsorStart) && local.x > mLineNoStart;
	if (!!isOverCollaper)
		*isOverCollaper = local.x < mTextStart && local.x > (mTextStart - mCollapsorStart);
	int lineNo = std::max(0, (int)floor(local.y / mCharAdvance.y));
	int cStart = 0, cEnd = 0;
	CalculateCurrentRealLine(lineNo);
	/*if (FindCurrentCollapsSize(lineNo, &cStart, &cEnd) && lineNo > cStart)
	{
		lineNo = (lineNo - cStart) + cEnd - 1;
	}*/
	int columnCoord = 0;

	if (lineNo >= 0 && lineNo < (int)mLines.size())
	{
		auto& line = mLines.at(lineNo);

		int columnIndex = 0;
		float columnX = 0.0f;

		while ((size_t)columnIndex < line.size())
		{
			float columnWidth = 0.0f;

			if (line[columnIndex].mChar == '\t')
			{
				float spaceSize = ImGui::GetFont()->CalcTextSizeA(ImGui::GetFontSize(), FLT_MAX, -1.0f, " ").x;
				float oldX = columnX;
				float newColumnX = (1.0f + std::floor((1.0f + columnX) / (float(mTabSize) * spaceSize))) * (float(mTabSize) * spaceSize);
				columnWidth = newColumnX - oldX;
				if (mTextStart + columnX + columnWidth * 0.5f > local.x)
					break;
				columnX = newColumnX;
				columnCoord = (columnCoord / mTabSize) * mTabSize + mTabSize;
				columnIndex++;
			}
			else
			{
				_justchar buf[7];
				auto d = UTF8CharLength(line[columnIndex].mChar);
				int i = 0;
				while (i < 6 && d-- > 0)
					buf[i++] = line[columnIndex++].mChar;
				buf[i] = '\0';
				columnWidth = ImGui::GetFont()->CalcTextSizeA(ImGui::GetFontSize(), FLT_MAX, -1.0f, (const char*)buf).x;
				if (mTextStart + columnX + columnWidth * 0.5f > local.x)
					break;
				columnX += columnWidth;
				columnCoord++;
			}
		}
	}

	return SanitizeCoordinates(Coordinates(lineNo, columnCoord));
}
#pragma optimize("", off)
TextEditor::Coordinates TextEditor::FindWordStart(const Coordinates& aFrom) const
{
	Coordinates at = aFrom;
	if (at.mLine >= (int)mLines.size())
		return at;

	Line& line = const_cast<Line&>(mLines[at.mLine]);
	auto cindex = GetCharacterIndex(at);

	if (cindex >= (int)line.size())
		return at;

	while (cindex > 0 && (_isspace_l(line[cindex].mChar, _get_current_locale()) || _isblank_l(line[cindex].mChar, _get_current_locale())))
		--cindex;

	auto cstart = (PaletteIndex)line[cindex].mColorIndex;
	while (cindex > 0)
	{
		auto c = line[cindex].mChar;
		if ((c & 0xC0) != 0x80)	// not UTF code sequence 10xxxxxx
		{
			if (c <= 32 && _isspace_l(c, _get_current_locale()))
			{
				cindex++;
				break;
			}
			/*if (cstart != (PaletteIndex)line[size_t(cindex - 1)].mColorIndex)
				break;*/
			if (!(c == mystr('_') || (c >= mystr('0') && c <= mystr('9')) || (c >= mystr('A') && c <= mystr('Z')) || (c >= mystr('a') && c <= mystr('z'))))
			{
				cindex++;
				break;	
			}
		}
		--cindex;
	}
	if (cindex == 0 && cindex+1 < line.size() && _isspace_l(line[cindex].mChar, _get_current_locale()))
		cindex++;
	return Coordinates(at.mLine, GetCharacterColumn(at.mLine, cindex));
}
#pragma optimize("", on)

TextEditor::Coordinates TextEditor::FindWordEnd(const Coordinates& aFrom) const
{
	Coordinates at = aFrom;
	if (at.mLine >= (int)mLines.size())
		return at;

	auto& line = mLines[at.mLine];
	auto cindex = GetCharacterIndex(at);

	if (cindex >= (int)line.size())
		return at;

	bool prevspace = (bool)_isspace_l(line[cindex].mChar, _get_current_locale());
	auto cstart = (PaletteIndex)line[cindex].mColorIndex;
	while (cindex < (int)line.size())
	{
		auto c = line[cindex].mChar;
		auto d = UTF8CharLength(c);
		/*if (cstart != (PaletteIndex)line[cindex].mColorIndex)
			break;*/
		if (!(c == mystr('_') || (c >= mystr('0') && c <= mystr('9')) || (c >= mystr('A') && c <= mystr('Z')) || (c >= mystr('a') && c <= mystr('z'))))
			break;
		if (prevspace != !!_isspace_l(c, _get_current_locale()))
		{
			if (_isspace_l(c, _get_current_locale()))
				while (cindex < (int)line.size() && _isspace_l(line[cindex].mChar, _get_current_locale()))
					++cindex;
			break;
		}
		cindex += d;
	}
	return Coordinates(aFrom.mLine, GetCharacterColumn(aFrom.mLine, cindex));
}

TextEditor::Coordinates TextEditor::FindNextWord(const Coordinates& aFrom) const
{
	Coordinates at = aFrom;
	if (at.mLine >= (int)mLines.size())
		return at;

	// skip to the next non-word character
	auto cindex = GetCharacterIndex(aFrom);
	bool isword = false;
	bool skip = false;
	if (cindex < (int)mLines[at.mLine].size())
	{
		auto& line = mLines[at.mLine];
		isword = _isalnum_l(line[cindex].mChar, _get_current_locale());
		skip = isword;
	}

	while (!isword || skip)
	{
		if (at.mLine >= mLines.size())
		{
			auto l = std::max(0, (int)mLines.size() - 1);
			return Coordinates(l, GetLineMaxColumn(l));
		}

		auto& line = mLines[at.mLine];
		if (cindex < (int)line.size())
		{
			isword = _isalnum_l(line[cindex].mChar, _get_current_locale());

			if (isword && !skip)
				return Coordinates(at.mLine, GetCharacterColumn(at.mLine, cindex));

			if (!isword)
				skip = false;

			cindex++;
		}
		else
		{
			cindex = 0;
			++at.mLine;
			skip = false;
			isword = false;
		}
	}

	return at;
}

int TextEditor::GetCharacterIndex(const Coordinates& aCoordinates) const
{
	if (aCoordinates.mLine >= mLines.size())
		return -1;
	auto& line = mLines[aCoordinates.mLine];
	int c = 0;
	int i = 0;
	for (; i < line.size() && c < aCoordinates.mColumn;)
	{
		if (line[i].mChar == '\t')
			c = (c / mTabSize) * mTabSize + mTabSize;
		else
			++c;
		i += UTF8CharLength(line[i].mChar);
	}
	return i;
}

int TextEditor::GetCharacterSize(const Coordinates& aCoordinates) const
{
	if (aCoordinates.mLine >= mLines.size())
		return 0;
	auto& line = mLines[aCoordinates.mLine];
	int c = 0;
	int i = 0;
	int l = 0;
	for (; i < line.size() && c <= aCoordinates.mColumn;)
	{
		if (line[i].mChar == '\t')
			c = (c / mTabSize) * mTabSize + mTabSize;
		else
			++c;
		if (c == aCoordinates.mColumn)
		{
			if (line[i].mChar == '\t')
				return mTabSize;
			else
				return UTF8CharLength(line[i].mChar);
		}
		i += UTF8CharLength(line[i].mChar);
	}
	return 0;
}

int TextEditor::GetCharacterColumn(int aLine, int aIndex) const
{
	if (aLine >= mLines.size())
		return 0;
	auto& line = mLines[aLine];
	int col = 0;
	int i = 0;
	while (i < aIndex && i < (int)line.size())
	{
		auto c = line[i].mChar;
		i += UTF8CharLength(c);
		if (c == '\t')
			col = (col / mTabSize) * mTabSize + mTabSize;
		else
			col++;
	}
	return col;
}

int TextEditor::GetMainCharacterIndex(Line& line) const
{
	for (int i = 0; i < line.size(); i++)
	{
		Char ch = line[i].mChar;
		if (ch != ' ' && ch != '\t' && ch != '\r' && ch != '\n')
			return i;
	}
	return 0;
}

int TextEditor::GetLineCharacterCount(int aLine) const
{
	if (aLine >= mLines.size())
		return 0;
	auto& line = mLines[aLine];
	int c = 0;
	for (unsigned i = 0; i < line.size(); c++)
		i += UTF8CharLength(line[i].mChar);
	return c;
}

int TextEditor::GetLineMaxColumn(int aLine) const
{
	if (aLine >= mLines.size())
		return 0;
	auto& line = mLines[aLine];
	int col = 0;
	for (unsigned i = 0; i < line.size(); )
	{
		auto c = line[i].mChar;
		if (c == '\t')
			col = (col / mTabSize) * mTabSize + mTabSize;
		else
			col++;
		i += UTF8CharLength(c);
	}
	return col;
}

bool TextEditor::IsOnWordBoundary(const Coordinates& aAt) const
{
	if (aAt.mLine >= (int)mLines.size() || aAt.mColumn == 0)
		return true;

	auto& line = mLines[aAt.mLine];
	auto cindex = GetCharacterIndex(aAt);
	if (cindex >= (int)line.size())
		return true;

	/*if (mColorizerEnabled)
		return line[cindex].mColorIndex != line[size_t(cindex - 1)].mColorIndex;*/

	return _isspace_l(line[cindex].mChar, _get_current_locale()) != _isspace_l(line[cindex - 1].mChar, _get_current_locale());
}

void TextEditor::RemoveLine(int aStart, int aEnd)
{
	assert(!mReadOnly);
	assert(aEnd >= aStart);
	assert(mLines.size() > (size_t)(aEnd - aStart));

	ErrorMarkers etmp;
	for (auto& i : mErrorMarkers)
	{
		ErrorMarkers::value_type e(i.first >= aStart ? i.first - 1 : i.first, i.second);
		if (e.first >= aStart && e.first <= aEnd)
			continue;
		etmp.insert(e);
	}
	mErrorMarkers = std::move(etmp);

	Breakpoints btmp;
	for (auto i : mBreakpoints)
	{
		if (i.mLine >= aStart && i.mLine <= aEnd)
			continue;
		btmp.insert(Breakpoint(i.mLine >= aStart ? i.mLine - 1 : i.mLine, i.mTriggered));
	}
	mBreakpoints = std::move(btmp);

	mLines.erase(mLines.begin() + aStart, mLines.begin() + aEnd);
	assert(!mLines.empty());
	if (aStart > 0)
	{
		//auto cc = GetActualCursorCoordinates();
		auto cc = Coordinates(aStart - 1, 0);
		UpdateCollapsables(cc);
	}
	mTextChanged = true;
	mCursorPositionChanged = true;
}

void TextEditor::RemoveLine(int aIndex)
{
	assert(!mReadOnly);
	assert(mLines.size() > 1);

	ErrorMarkers etmp;
	for (auto& i : mErrorMarkers)
	{
		ErrorMarkers::value_type e(i.first > aIndex ? i.first - 1 : i.first, i.second);
		if (e.first - 1 == aIndex)
			continue;
		etmp.insert(e);
	}
	mErrorMarkers = std::move(etmp);

	Breakpoints btmp;
	for (auto i : mBreakpoints)
	{
		if (i.mLine == aIndex)
			continue;
		btmp.insert(Breakpoint(i.mLine >= aIndex ? i.mLine - 1 : i.mLine, i.mTriggered));
	}
	mBreakpoints = std::move(btmp);

	mLines.erase(mLines.begin() + aIndex);
	assert(!mLines.empty());
	if (aIndex > 0)
	{
		//auto cc = GetActualCursorCoordinates();
		auto cc = Coordinates(aIndex - 1, 0);
		UpdateCollapsables(cc);
	}
	mTextChanged = true;
	mCursorPositionChanged = true;
}

TextEditor::Line& TextEditor::InsertLine(int aIndex, int aColumn)
{
	assert(!mReadOnly);
	
	auto& result = *mLines.insert(mLines.begin() + aIndex, Line());

	ErrorMarkers etmp;
	for (auto& i : mErrorMarkers)
		etmp.insert(ErrorMarkers::value_type(i.first >= aIndex ? i.first + 1 : i.first, i.second));
	mErrorMarkers = std::move(etmp);

	Breakpoints btmp;
	for (auto i : mBreakpoints)
	{
		bool bCond = i.mLine > aIndex || (i.mLine == aIndex && aColumn == 0);
		btmp.insert(Breakpoint(bCond ? i.mLine + 1 : i.mLine, i.mTriggered));
	}
	mBreakpoints = std::move(btmp);

	return result;
}

_stdstr TextEditor::GetWordUnderCursor() const
{
	auto c = GetCursorPosition();
	return GetWordAt(c);
}

_stdstr TextEditor::GetWordAt(const Coordinates& aCoords) const
{
	auto start = FindWordStart(aCoords);
	auto end = FindWordEnd(aCoords);

	_stdstr r;

	auto istart = GetCharacterIndex(start);
	auto iend = GetCharacterIndex(end);

	for (auto it = istart; it < iend; ++it)
		r.push_back(mLines[aCoords.mLine][it].mChar);

	return r;
}

ImU32 TextEditor::GetGlyphColor(const Glyph& aGlyph) const
{
	if (!mColorizerEnabled)
		return mPalette[(int)PaletteIndex::Default];
	if (aGlyph.mComment)
		return mPalette[(int)PaletteIndex::Comment];
	if (aGlyph.mMultiLineComment)
		return mPalette[(int)PaletteIndex::MultiLineComment];
	auto const color = mPalette[(int)aGlyph.mColorIndex];
	if (aGlyph.mPreprocessor)
	{
		const auto ppcolor = mPalette[(int)PaletteIndex::Preprocessor];
		const int c0 = ((ppcolor & 0xff) + (color & 0xff)) / 2;
		const int c1 = (((ppcolor >> 8) & 0xff) + ((color >> 8) & 0xff)) / 2;
		const int c2 = (((ppcolor >> 16) & 0xff) + ((color >> 16) & 0xff)) / 2;
		const int c3 = (((ppcolor >> 24) & 0xff) + ((color >> 24) & 0xff)) / 2;
		return ImU32(c0 | (c1 << 8) | (c2 << 16) | (c3 << 24));
	}
	return color;
}

bool TextEditor::BufferIsAChar(_constcharptr buffer)
{
	return (buffer && ((*buffer >= 'a' && *buffer <= 'z') || (*buffer >= 'A' && *buffer <= 'Z')));
}

bool TextEditor::BufferIsACStyleIdentifier(_constcharptr buffer, _constcharptr bufferend)
{
	bool bIs = false;
	if (buffer)
		while (buffer < bufferend)
		{
			bIs = ((*buffer >= 'a' && *buffer <= 'z') || (*buffer >= 'A' && *buffer <= 'Z') || *buffer == '_');
			buffer++;
		}
	
	return bIs;
}

void TextEditor::HandleKeyboardInputs()
{
	ImGuiIO& io = ImGui::GetIO();
	auto shift = io.KeyShift;
	auto ctrl = io.ConfigMacOSXBehaviors ? io.KeySuper : io.KeyCtrl;
	auto alt = io.ConfigMacOSXBehaviors ? io.KeyCtrl : io.KeyAlt;

	if (ImGui::IsWindowFocused())
	{
		if (ImGui::IsWindowHovered())
			ImGui::SetMouseCursor(ImGuiMouseCursor_TextInput);
		//ImGui::CaptureKeyboardFromApp(true);

		io.WantCaptureKeyboard = true;
		io.WantTextInput = true;

		if (!IsReadOnly() && ctrl && !shift && !alt && ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_Z)))
			Undo();
		else if (!IsReadOnly() && !ctrl && !shift && alt && ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_Backspace)))
			Undo();
		else if (!IsReadOnly() && ctrl && !shift && !alt && ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_Y)))
			Redo();
		else if (mShowCollapsables && !ctrl && !shift && alt && ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_0)))
			CollapsAll();
		else if (mShowCollapsables && ctrl && !shift && alt && ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_0)))
			ExpandAll();
		else if (!ctrl && !alt && ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_UpArrow)))
			MoveUp(1, shift);
		else if (!ctrl && !alt && ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_DownArrow)))
			MoveDown(1, shift);
		else if (!alt && ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_LeftArrow)))
			MoveLeft(1, shift, ctrl);
		else if (!alt && ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_RightArrow)))
			MoveRight(1, shift, ctrl);
		else if (!alt && ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_PageUp)))
			MoveUp(GetPageSize() - 4, shift);
		else if (!alt && ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_PageDown)))
			MoveDown(GetPageSize() - 4, shift);
		else if (!alt && ctrl && ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_Home)))
			MoveTop(shift);
		else if (ctrl && !alt && ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_End)))
			MoveBottom(shift);
		else if (!ctrl && !alt && ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_Home)))
			MoveHome(shift);
		else if (!ctrl && !alt && ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_End)))
			MoveEnd(shift);
		else if (!IsReadOnly() && !ctrl && !shift && !alt && ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_Delete)))
			Delete();
		else if (!IsReadOnly() && !ctrl && !shift && !alt && ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_Backspace)))
			Backspace();
		else if (!ctrl && !shift && !alt && ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_Insert)))
			mOverwrite ^= true;
		else if (ctrl && !shift && !alt && ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_Insert)))
			Copy();
		else if (ctrl && !shift && !alt && ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_C)))
			Copy();
		else if (!IsReadOnly() && !ctrl && shift && !alt && ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_Insert)))
			Paste();
		else if (!IsReadOnly() && ctrl && !shift && !alt && ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_V)))
			Paste();
		else if (ctrl && !shift && !alt && ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_X)))
			Cut();
		else if (!ctrl && shift && !alt && ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_Delete)))
			Cut();
		else if (ctrl && !shift && !alt && ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_A)))
			SelectAll();
		else if (!IsReadOnly() && !ctrl && !shift && !alt && ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_Enter)))
			EnterCharacter('\n', false);
		else if (!IsReadOnly() && !ctrl && !alt && ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_Tab)))
			EnterCharacter('\t', shift);

		if (!IsReadOnly() && !io.InputQueueCharacters.empty())
		{
			for (int i = 0; i < io.InputQueueCharacters.Size; i++)
			{
				auto c = io.InputQueueCharacters[i];
				if (c != 0 && (c == '\n' || c >= 32))
					EnterCharacter(c, shift);
			}
			io.InputQueueCharacters.resize(0);
		}
	}
}

void TextEditor::HandleMouseInputs()
{
	ImGuiIO& io = ImGui::GetIO();
	auto shift = io.KeyShift;
	auto ctrl = io.ConfigMacOSXBehaviors ? io.KeySuper : io.KeyCtrl;
	auto alt = io.ConfigMacOSXBehaviors ? io.KeyCtrl : io.KeyAlt;

	if (ImGui::IsWindowHovered())
	{
		if (!shift && !alt)
		{
			auto click = ImGui::IsMouseClicked(0);
			auto doubleClick = ImGui::IsMouseDoubleClicked(0);
			auto t = ImGui::GetTime();
			auto tripleClick = click && !doubleClick && (mLastClick != -1.0f && (t - mLastClick) < io.MouseDoubleClickTime);

			/*
			Left mouse button triple click
			*/

			if (tripleClick)
			{
				if (!ctrl)
				{
					mState.mCursorPosition = mInteractiveStart = mInteractiveEnd = ScreenPosToCoordinates(ImGui::GetMousePos());
					mSelectionMode = SelectionMode::Line;
					SetSelection(mInteractiveStart, mInteractiveEnd, mSelectionMode);
				}

				mLastClick = -1.0f;
			}

			/*
			Left mouse button double click
			*/

			else if (doubleClick)
			{
				if (!ctrl)
				{
					mState.mCursorPosition = mInteractiveStart = mInteractiveEnd = ScreenPosToCoordinates(ImGui::GetMousePos());
					if (mSelectionMode == SelectionMode::Line)
						mSelectionMode = SelectionMode::Normal;
					else
						mSelectionMode = SelectionMode::Word;
					SetSelection(mInteractiveStart, mInteractiveEnd, mSelectionMode);
				}

				mLastClick = (float)ImGui::GetTime();
			}

			/*
			Left mouse button click
			*/
			else if (click)
			{
				bool bOverLineNumber = false;
				bool bOverBPPoint = false;
				bool bOverCollapsor = false;
				Coordinates currentCoords;
				currentCoords = mInteractiveStart = mInteractiveEnd = ScreenPosToCoordinates(ImGui::GetMousePos(), &bOverLineNumber, &bOverBPPoint, &bOverCollapsor);
				if (bOverBPPoint)
				{
					/*Breakpoint pb;
					pb.mLine = currentCoords.mLine;
					pb.mTriggered = true;*/
					int line = currentCoords.mLine + 1;
					ToggleBreakpoint(line);
				}
				else if (bOverCollapsor && mShowCollapsables)
				{
					int line = currentCoords.mLine;
					int linesToAdd = 0;
					Collapsables::iterator clickedCollapser = mCollapsables.begin();
					for (auto cps = mCollapsables.begin(); cps < mCollapsables.end(); cps++)
					{
						if (cps->mStart.mLine == line)
						{
							clickedCollapser = cps;
							cps->mIsFoldEnabled = !cps->mIsFoldEnabled; //toggle fold
							if (cps->mIsFoldEnabled)
								AddCollapsedLine(cps->mStart.mLine, cps->mEnd.mLine);
							else
								RemoveCollapsedLine(cps->mStart.mLine);
							linesToAdd = cps->mEnd.mLine - cps->mStart.mLine;
							if (cps->mIsFoldEnabled)
								linesToAdd *= -1;
						}
					}
				}
				else if (bOverLineNumber)
				{
					Coordinates targetCursorPos = currentCoords.mLine < mLines.size() - 1 ?
						Coordinates{ currentCoords.mLine + 1, 0 } :
						Coordinates{ currentCoords.mLine, GetLineMaxColumn(currentCoords.mLine) };
					SetSelection({ currentCoords.mLine, 0 }, targetCursorPos);
					SetCursorPosition(targetCursorPos);
					mState.mCursorPosition = currentCoords;
				}
				else 
				{
					if (ctrl)
						mSelectionMode = SelectionMode::Word;
					else
						mSelectionMode = SelectionMode::Normal;
					SetSelection(mInteractiveStart, mInteractiveEnd, mSelectionMode);
					mState.mCursorPosition = currentCoords;
				}

				mLastClick = (float)ImGui::GetTime();
			}
			// Mouse left button dragging (=> update selection)
			else if (ImGui::IsMouseDragging(0) && ImGui::IsMouseDown(0))
			{
				io.WantCaptureMouse = true;
				mState.mCursorPosition = mInteractiveEnd = ScreenPosToCoordinates(ImGui::GetMousePos());
				SetSelection(mInteractiveStart, mInteractiveEnd, mSelectionMode);
			}
		}
	}
}

bool IsCharUnicode(TextEditor::Char c)
{
	return (c > 0x80);
}

void TextEditor::Render(const char* aTitle, const ImVec2& aSize, bool aBorder)
{
	/*if (mHaltRender)
		return;*/
	mTextChanged = false;
	if (mCursorPositionChanged)
		UpdateBrackets();
	mCursorPositionChanged = false;
	ImGui::PushStyleColor(ImGuiCol_ChildBg, ImGui::ColorConvertU32ToFloat4(mPalette[(int)PaletteIndex::Background]));
	ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, 0.0f));
	ImGui::Begin(aTitle, 0, ImGuiWindowFlags_NoSavedSettings);
	if (mHaltRender)
	{
		mWithinRender = false;
		ImGui::End();
		ImGui::PopStyleVar();
		ImGui::PopStyleColor();
		return;
	}
	mWithinRender = true;
	if (!mIgnoreImGuiChild)
		ImGui::BeginChild(aTitle, aSize, aBorder, ImGuiWindowFlags_HorizontalScrollbar | ImGuiWindowFlags_AlwaysHorizontalScrollbar | ImGuiWindowFlags_NoMove);

	if (mHandleKeyboardInputs)
	{
		HandleKeyboardInputs();
		ImGui::PushAllowKeyboardFocus(true);
	}

	if (mHandleMouseInputs)
		HandleMouseInputs();

	ColorizeInternal();
	Render();
	if (ImGui::Shortcut(ImGuiKey_ModCtrl | ImGuiKey_F, ImGuiInputFlags_RouteFocused))
	{
		_stdstr text = GetSelectedText();
		if (text.size() > 0)
			mSearchStr = std::string(text.begin(), text.end());
		mShowSearchBar = true;
	}
	RenderSearch();
	if (mHandleKeyboardInputs)
		ImGui::PopAllowKeyboardFocus();

	if (!mIgnoreImGuiChild)
		ImGui::EndChild();
	ImGui::End();
	ImGui::PopStyleVar();
	ImGui::PopStyleColor();

	mWithinRender = false;
}

void TextEditor::Render()
{
	/* Compute mCharAdvance regarding to scaled font size (Ctrl + mouse wheel)*/
	const float fontSize = ImGui::GetFont()->CalcTextSizeA(ImGui::GetFontSize(), FLT_MAX, -1.0f, "#", nullptr, nullptr).x;
	const float fontHeight = ImGui::GetTextLineHeightWithSpacing();
	mCharAdvance = ImVec2(fontSize, ImGui::GetTextLineHeightWithSpacing() * mLineSpacing);

	/* Update palette with the current alpha from style */
	for (int i = 0; i < (int)PaletteIndex::Max; ++i)
	{
		auto color = ImGui::ColorConvertU32ToFloat4(mPaletteBase[i]);
		color.w *= ImGui::GetStyle().Alpha;
		mPalette[i] = ImGui::ColorConvertFloat4ToU32(color);
	}

	assert(mLineBuffer.empty());

	auto contentSize = ImGui::GetWindowContentRegionMax();
	auto drawList = ImGui::GetWindowDrawList();
	float longest(mTextStart);

	if (mScrollToTop)
	{
		mScrollToTop = false;
		ImGui::SetScrollY(0.f);
	}

	ImVec2 cursorScreenPos = ImGui::GetCursorScreenPos();
	auto scrollX = ImGui::GetScrollX();
	auto scrollY = ImGui::GetScrollY();

	auto screenLinePos = (int)floor(scrollY / mCharAdvance.y);
	int lineNo = screenLinePos, lextraSkip = 0;
	CalculateCurrentRealLine(lineNo, &lextraSkip, false);
	auto globalLineMax = (int)mLines.size();
	auto lineMax = std::max(0, std::min((int)mLines.size() - 1, lineNo + (int)floor((scrollY + contentSize.y) / mCharAdvance.y)));
	// Deduce mTextStart by evaluating mLines size (global lineMax) plus two spaces as text width
	char buf[16];
	snprintf(buf, 16, " %d ", globalLineMax);
	mTextStart = ImGui::GetFont()->CalcTextSizeA(ImGui::GetFontSize(), FLT_MAX, -1.0f, buf, nullptr, nullptr).x + mLeftMargin + mLineNoStart;
	bool bHasSelection = HasSelection();
	_stdstr selectedtext;
	if (bHasSelection)
		selectedtext = GetSelectedText();
	if (!mLines.empty())
	{
		float spaceSize = ImGui::GetFont()->CalcTextSizeA(ImGui::GetFontSize(), FLT_MAX, -1.0f, " ", nullptr, nullptr).x;
		//lineNo += lextraSkip;
		while (lineNo <= lineMax)
		{
			ImVec2 lineStartScreenPos = ImVec2(cursorScreenPos.x, cursorScreenPos.y + screenLinePos * mCharAdvance.y);
			ImVec2 textScreenPos = ImVec2(lineStartScreenPos.x + mTextStart, lineStartScreenPos.y);
			screenLinePos++;
			auto& line = mLines[lineNo];

			size_t lineCount = line.size();
			int lStartLine = 0, lEndLine = 0;
			if (IsWithinCollapsedRange(lineNo, &lStartLine, &lEndLine))
			{
				if (lineNo > lStartLine)
				{
					//lineNo += lEndLine - lStartLine;
					lineNo = lEndLine + 1;
					continue;
				}
				float chardist = 0;
				if (line.size() > 0)
					chardist = GetCharacterColumn(lineNo, GetMainCharacterIndex(line));
				if (chardist < 1)
					chardist = 1;
				//Drawing Collapsing box:
				ImVec2 vstart(lineStartScreenPos.x + mTextStart + mCharAdvance.x * chardist, lineStartScreenPos.y);
				ImVec2 vend(vstart.x + mCharAdvance.x * 10, vstart.y + mCharAdvance.y);
				drawList->AddRect(vstart, vend, mPalette[(int)PaletteIndex::CollapsorBoxDisabled], 0, 0, 3);
				vstart.y = (vstart.y + vend.y) / 2;
				vend.y = vstart.y;
				AddLineDashed(drawList, vstart, vend, mPalette[(int)PaletteIndex::CollapsedRegion], 1, 10, 1, 0);
				//lineNo += lEndLine - lStartLine;
				lineNo = lEndLine + 1;
				lineMax = std::min(lineMax + lEndLine - lStartLine, (int)mLines.size() - 1);
				continue; //Because all the whole line is collapsed not just one character
			}
			longest = std::max(mTextStart + TextDistanceToLineStart(Coordinates(lineNo, GetLineMaxColumn(lineNo))), longest);
			auto columnNo = 0;
			Coordinates lineStartCoord(lineNo, 0);
			Coordinates lineEndCoord(lineNo, GetLineMaxColumn(lineNo));

			// Draw selection for the current line
			float sstart = -1.0f;
			float ssend = -1.0f;

			assert(mState.mSelectionStart <= mState.mSelectionEnd);
			if (mState.mSelectionStart <= lineEndCoord)
				sstart = mState.mSelectionStart > lineStartCoord ? TextDistanceToLineStart(mState.mSelectionStart) : 0.0f;
			if (mState.mSelectionEnd > lineStartCoord)
				ssend = TextDistanceToLineStart(mState.mSelectionEnd < lineEndCoord ? mState.mSelectionEnd : lineEndCoord);

			if (mState.mSelectionEnd.mLine > lineNo)
				ssend += mCharAdvance.x;

			if (sstart != -1 && ssend != -1 && sstart < ssend)
			{
				ImVec2 vstart(lineStartScreenPos.x + mTextStart + sstart, lineStartScreenPos.y);
				ImVec2 vend(lineStartScreenPos.x + mTextStart + ssend, lineStartScreenPos.y + mCharAdvance.y);
				drawList->AddRectFilled(vstart, vend, mPalette[(int)PaletteIndex::Selection]);
			}


			auto start = ImVec2(lineStartScreenPos.x + scrollX, lineStartScreenPos.y);

			// Draw error markers
			auto errorIt = mErrorMarkers.find(lineNo + 1);
			if (errorIt != mErrorMarkers.end())
			{
				auto end = ImVec2(lineStartScreenPos.x + contentSize.x + 2.0f * scrollX, lineStartScreenPos.y + mCharAdvance.y);
				drawList->AddRectFilled(start, end, mPalette[(int)PaletteIndex::ErrorMarker]);

				if (ImGui::IsMouseHoveringRect(lineStartScreenPos, end))
				{
					ImGui::BeginTooltip();
					ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.2f, 0.2f, 1.0f));
					ImGui::Text("Error at line %d:", errorIt->first);
					ImGui::PopStyleColor();
					ImGui::Separator();
					ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 0.2f, 1.0f));
					ImGui::Text("%s", errorIt->second.c_str());
					ImGui::PopStyleColor();
					ImGui::EndTooltip();
				}
			}

			// Draw line number (right aligned)
			snprintf(buf, 16, "%d  ", lineNo + 1);

			auto lineNoWidth = ImGui::GetFont()->CalcTextSizeA(ImGui::GetFontSize(), FLT_MAX, -1.0f, buf, nullptr, nullptr).x;
			ImU32 col = mState.mCursorPosition.mLine == lineNo ? mPalette[(int)PaletteIndex::LineNumberActive] : mPalette[(int)PaletteIndex::LineNumber];
			drawList->AddText(ImVec2(lineStartScreenPos.x + mTextStart - lineNoWidth, lineStartScreenPos.y), col, buf);

			// Draw breakpoints
			ImVec2 endpoint = ImVec2(start.x + ImGui::GetFontSize(), start.y + ImGui::GetFontSize());
			if (mBreakpoints.count(lineNo + 1) != 0)
			{
				//auto end = ImVec2(lineStartScreenPos.x + contentSize.x + 2.0f * scrollX, lineStartScreenPos.y + mCharAdvance.y);
				ImVec2 circleCenter = (start + endpoint) / 2;
				circleCenter.x -= scrollX;
				float circleRadius = (endpoint.y - start.y - 4) / 2;
				drawList->AddCircleFilled(circleCenter, circleRadius, mPalette[(int)PaletteIndex::Breakpoint]);
				
			}
			if (mTriggeredBPLine == lineNo)
			{
				ImGui::RenderArrow(drawList, lineStartScreenPos, IM_COL32(211, 151, 0, 255), ImGuiDir_Right);

				/*auto end = ImVec2(lineStartScreenPos.x + contentSize.x + 2.0f * scrollX, lineStartScreenPos.y + mCharAdvance.y);
				const ImVec2 circleCenter = (start + endpoint) / 2;
				float circleRadius = (endpoint.y - start.y - 4) / 2;
				ImVec2 lefttop = circleCenter;
				lefttop.x -= circleRadius * sin(3.1415 / 6);
				lefttop.y += circleRadius / 1.2;
				ImVec2 leftdown = circleCenter;
				leftdown.x -= circleRadius * sin(3.1415 / 6);
				leftdown.y -= circleRadius / 1.2;
				ImVec2 rightcenter = circleCenter;
				rightcenter.x += circleRadius;
				drawList->AddTriangleFilled(lefttop, leftdown, rightcenter, mPalette[(int)PaletteIndex::BreakpointEnabled]);*/
			}
			if (mState.mCursorPosition.mLine == lineNo)
			{
				auto focused = ImGui::IsWindowFocused();

				// Highlight the current line (where the cursor is)
				if (!HasSelection())
				{
					start.x += mTextStart - 2 - scrollX;
					auto end = ImVec2(start.x + contentSize.x + scrollX, start.y + mCharAdvance.y);
					drawList->AddRectFilled(start, end, mPalette[(int)(focused ? PaletteIndex::CurrentLineFill : PaletteIndex::CurrentLineFillInactive)]);
					drawList->AddRect(start, end, mPalette[(int)PaletteIndex::CurrentLineEdge], 2.0f);
				}

				// Render the cursor
				if (focused)
				{
					auto timeEnd = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
					auto elapsed = timeEnd - mStartTime;
					if (elapsed > 400)
					{
						float width = 1.0f;
						auto cindex = GetCharacterIndex(mState.mCursorPosition);
						float cx = TextDistanceToLineStart(mState.mCursorPosition);

						if (mOverwrite && cindex < (int)line.size())
						{
							auto c = line[cindex].mChar;
							if (c == '\t')
							{
								auto x = (1.0f + std::floor((1.0f + cx) / (float(mTabSize) * spaceSize))) * (float(mTabSize) * spaceSize);
								width = x - cx;
							}
							else
							{
								_justchar buf2[2];
								buf2[0] = line[cindex].mChar;
								buf2[1] = '\0';
								width = ImGui::GetFont()->CalcTextSizeA(ImGui::GetFontSize(), FLT_MAX, -1.0f, (const char*)buf2).x;
							}
						}
						ImVec2 cstart(textScreenPos.x + cx, lineStartScreenPos.y);
						ImVec2 cend(textScreenPos.x + cx + width, lineStartScreenPos.y + mCharAdvance.y);
						drawList->AddRectFilled(cstart, cend, mPalette[(int)PaletteIndex::Cursor]);
						if (elapsed > 800)
							mStartTime = timeEnd;
					}
				}
			}

			// Render colorized text
			auto prevColor = line.empty() ? mPalette[(int)PaletteIndex::Default] : GetGlyphColor(line[0]);
			ImVec2 bufferOffset;

			ImFont* nextfont = nullptr;
			for (int i = 0; i < line.size();)
			{
				auto& glyph = line[i];
				//Following code draws rect around any matching character as of the selected character!
				if (bHasSelection && DoesTextMatch(selectedtext, lineNo, i))
				{
					const Coordinates& startCoord = Coordinates(lineNo, GetCharacterColumn(lineNo, i));
					if (startCoord != mState.mSelectionStart)
					{
						const float startPos = TextDistanceToLineStart(SanitizeCoordinates(startCoord));
						const float endPos = TextDistanceToLineStart(SanitizeCoordinates(Coordinates(lineNo, GetCharacterColumn(lineNo, i + selectedtext.size()))));
						ImVec2 vstart(lineStartScreenPos.x + mTextStart + startPos, lineStartScreenPos.y);
						ImVec2 vend(lineStartScreenPos.x + mTextStart + endPos, lineStartScreenPos.y + mCharAdvance.y);
						drawList->AddRectFilled(vstart, vend, mPalette[(int)PaletteIndex::SelectionDuplicate]);
					}
				}
				auto color = GetGlyphColor(glyph);
				if ((color != prevColor || 
					glyph.mChar == mystr('\t') || 
					glyph.mChar == mystr(' ') || 
					IsCharUnicode(glyph.mChar) || 
					(mLineBuffer.size()>0  && IsCharUnicode(mLineBuffer[0]))) &&  //all unicode chars must be drawn separately
					!mLineBuffer.empty())
				{
					const ImVec2 newOffset(textScreenPos.x + bufferOffset.x, textScreenPos.y + bufferOffset.y);
					const char* chars = nullptr;
					size_t charsize = mLineBuffer.size();
#if UseWideStr
					if (charsize > 0)
					{
						chars = (const char*)alloca(charsize * 4);
						ImTextStrToUtf8((char*)chars, charsize * 4, (const ImWchar*)mLineBuffer.c_str(), nullptr);
					}
#else
					chars = mLineBuffer.c_str();
#endif
					if (nextfont)
						ImGui::PushFont(nextfont);
					drawList->AddText(newOffset, prevColor, chars);
					auto textSize = ImGui::GetFont()->CalcTextSizeA(ImGui::GetFontSize(), FLT_MAX, -1.0f, chars, nullptr, nullptr);
					if (nextfont)
						ImGui::PopFont();
					bufferOffset.x += textSize.x;
					mLineBuffer.clear();
					
				}
				prevColor = color;

				if (glyph.mChar == mystr('\t'))
				{
					auto oldX = bufferOffset.x;
					bufferOffset.x = (1.0f + std::floor((1.0f + bufferOffset.x) / (float(mTabSize) * spaceSize))) * (float(mTabSize) * spaceSize);
					++i;

					if (mShowWhitespaces)
					{
						const auto s = ImGui::GetFontSize();
						const auto x1 = textScreenPos.x + oldX + 1.0f;
						const auto x2 = textScreenPos.x + bufferOffset.x - 1.0f;
						const auto y = textScreenPos.y + bufferOffset.y + s * 0.5f;
						const ImVec2 p1(x1, y);
						const ImVec2 p2(x2, y);
						const ImVec2 p3(x2 - s * 0.2f, y - s * 0.2f);
						const ImVec2 p4(x2 - s * 0.2f, y + s * 0.2f);
						drawList->AddLine(p1, p2, 0x90909090);
						drawList->AddLine(p2, p3, 0x90909090);
						drawList->AddLine(p2, p4, 0x90909090);
					}
				}
				else if (glyph.mChar == mystr(' '))
				{
					if (mShowWhitespaces)
					{
						const auto s = ImGui::GetFontSize();
						const auto x = textScreenPos.x + bufferOffset.x + spaceSize * 0.5f;
						const auto y = textScreenPos.y + bufferOffset.y + s * 0.5f;
						drawList->AddCircleFilled(ImVec2(x, y), 1.5f, 0x80808080, 4);
					}
					bufferOffset.x += spaceSize;
					i++;
				}
				else
				{
					nextfont = nullptr;
					const ImFontGlyph* glph = ImGui::GetFont()->FindGlyph(glyph.mChar);
					if (glph == ImGui::GetFont()->FallbackGlyph)
					{
						for (ImFont* fnt : ImGui::GetIO().Fonts->Fonts)
						{
							glph = fnt->FindGlyph(glyph.mChar);
							if (glph != fnt->FallbackGlyph)
							{
								nextfont = fnt;
								break;
							}
						}
					}
					auto l = UTF8CharLength(glyph.mChar);
					/*size_t maxl = line.size();*/
					while (l-- > 0)
						mLineBuffer.push_back(line[i++].mChar);
				}
				++columnNo;
			}

			if (!mLineBuffer.empty())
			{
				const ImVec2 newOffset(textScreenPos.x + bufferOffset.x, textScreenPos.y + bufferOffset.y);
				const char* chars = nullptr;
				size_t bufsize = mLineBuffer.size();
				/*_stdstr astr = mystr("握");
				mLineBuffer = astr;*/
#if UseWideStr
				if (bufsize > 0)
				{
					chars = (const char*)alloca(bufsize * 4);
					ImTextStrToUtf8((char*)chars, bufsize * 4, (const ImWchar*)mLineBuffer.c_str(), nullptr);
				}
#else
				chars = mLineBuffer.c_str();
#endif
				drawList->AddText(newOffset, prevColor, chars);
				mLineBuffer.clear();
			}

			++lineNo;
		}

		// Draw a tooltip on known identifiers/preprocessor symbols
		if (ImGui::IsMousePosValid() && mAllowTooltips)
		{
			auto pos = ScreenPosToCoordinates(ImGui::GetMousePos());
			auto id = GetWordAt(pos);
			if (pos.mLine <= lineMax && !id.empty())
			{
				if (mOnPropertyIndentifierHover)
				{
					if (mOnPropertyIndentifierHover && mLanguageDefinition.HasIdentifier(id))
					{
						auto it = mLanguageDefinition.GetIdentifier(id);
						mOnPropertyIndentifierHover(&it->second, pos.mLine, it->first);
					}
					else if (mOnMultiPropertyIndentifierHover && mLanguageDefinition.HasMultiIdentifier(id))
					{
						auto it = mLanguageDefinition.GetMultiIdentifier(id);
						mOnMultiPropertyIndentifierHover(&it->second, pos.mLine, it->first);
					}

					/*int charsize = it->second.mDeclaration.size();
					if (charsize > 0)
					{
						ImGui::BeginTooltip();
						const char* chars = nullptr;
	#if UseWideStr
						if (charsize > 0)
						{
							chars = (const char*)alloca(charsize * 4);
							ImTextStrToUtf8((char*)chars, charsize + 3, (const ImWchar*)it->second.mDeclaration.c_str(), nullptr);
						}
	#else
						chars = mLineBuffer.c_str();
	#endif
						ImGui::TextUnformatted(chars);
						ImGui::EndTooltip();
					}*/
				}
				else
				{
					auto pi = mLanguageDefinition.mPreprocIdentifiers.find(id);
					if (pi != mLanguageDefinition.mPreprocIdentifiers.end())
					{
						ImGui::BeginTooltip();
						const char* chars = nullptr;
						size_t charsize = pi->second.mDeclaration.size();
#if UseWideStr
						if (charsize > 0)
						{
							chars = (const char*)alloca(charsize * 4);
							ImTextStrToUtf8((char*)chars, charsize + 3, (const ImWchar*)pi->second.mDeclaration.c_str(), nullptr);
						}
#else
						chars = mLineBuffer.c_str();
#endif
						ImGui::TextUnformatted(chars);
						ImGui::EndTooltip();
					}
				}
			}
		}

		//DrawBrackets
		if (mCursorOnABracket && !mCursorPositionChanged /*&& mMatchingBracket != Coordinates()*/)
		{
			auto drawhorizontalline = [&](const Coordinates& startpos, const Coordinates& endpos)
				{
					float cx = TextDistanceToLineStart(startpos); 
					int RealStart = startpos.mLine, RealEnd = endpos.mLine; bool bIsStart = false;
					if (!CalculateNextCollapsStartEnd(&RealStart, &RealEnd, bIsStart) || bIsStart)
						return;
					ImVec2 lineStartScreenPos = ImVec2(cursorScreenPos.x, cursorScreenPos.y + RealStart * mCharAdvance.y);
					ImVec2 textScreenPos = ImVec2(lineStartScreenPos.x + mTextStart, lineStartScreenPos.y);
					ImVec2 topLeft = { textScreenPos.x + cx, lineStartScreenPos.y + fontHeight + 1.0f };
					ImVec2 bottomRight = { topLeft.x + mCharAdvance.x, topLeft.y + 1.0f };
					const ImVec2 startposition = { (topLeft.x + bottomRight.x) / 2 , topLeft.y };

					lineStartScreenPos = ImVec2(cursorScreenPos.x, cursorScreenPos.y + RealEnd * mCharAdvance.y);
					const ImVec2 endposition = { startposition.x, lineStartScreenPos.y };
					auto plt = mPalette[(int)PaletteIndex::BracketsTrace];
					plt |= 0xFF0000FF;
					AddLineDashed(drawList, startposition, endposition, plt, 2.0f, (RealEnd - RealStart) * 5);
				};
			auto cpos = GetCursorPosition();
			if (cpos.mLine < mMatchingBracket.mLine)
				drawhorizontalline(cpos, mMatchingBracket);
			else if (cpos.mLine > mMatchingBracket.mLine)
				drawhorizontalline(mMatchingBracket, cpos);
		}
		if (mShowCollapsables)
		{
			auto drawCollapsableStuff = [&](Coordinates startpos, Coordinates endpos, bool bIsFoldEnabled)
				{
					if (cursorScreenPos.y + endpos.mLine * mCharAdvance.y < 0 ||  //if ending position lies out of range then dont bother
						startpos.mLine > lineNo)  // or is starting pos is after the show
						return;
					int RealStart = startpos.mLine, RealEnd = endpos.mLine; bool bIsStart = false;
					if (!CalculateNextCollapsStartEnd(&RealStart, &RealEnd, bIsStart))
						return;
					float textdist = TextDistanceToLineStart(Coordinates(RealStart, startpos.mColumn));
					ImVec2 lineStartScreenPos = ImVec2(cursorScreenPos.x, cursorScreenPos.y + RealStart * mCharAdvance.y);
					ImVec2 textScreenPos = ImVec2(lineStartScreenPos.x + mTextStart, lineStartScreenPos.y);
					ImVec2 pos = { textScreenPos.x, lineStartScreenPos.y };
					ImVec2 topLeft = { textScreenPos.x + textdist, lineStartScreenPos.y + fontHeight + 1.0f };
					ImVec2 bottomRight = { topLeft.x + mCharAdvance.x, topLeft.y + 1.0f };
					const ImVec2 startposition = { (topLeft.x + bottomRight.x) / 2 , topLeft.y };
					//Drawing dotted line:
					lineStartScreenPos = ImVec2(cursorScreenPos.x, cursorScreenPos.y + RealEnd * mCharAdvance.y);
					const ImVec2 endposition = { startposition.x, lineStartScreenPos.y };
					if (!bIsFoldEnabled)
						AddLineDashed(drawList, startposition, endposition, mPalette[(int)PaletteIndex::BracketsTrace], 1.0f, (RealEnd - RealStart) * 5);

					//Drawing Collapsing box
					pos.x -= mCollapsorStart;
					ImVec2 endingpos = pos;
					endingpos.x += mCharAdvance.x;
					endingpos.y += mCharAdvance.x;
					float boxlen = endingpos.y - pos.y;
					if (boxlen < mCharAdvance.y)
					{
						float boxstart = (mCharAdvance.y - boxlen) / 2;
						pos.y += boxstart;
						endingpos.y += boxstart;
					}
					drawList->AddRectFilled(pos, endingpos, ImColor(0xFF000000), 0); //Black spot to clear any line passing through
					ImColor col = bIsFoldEnabled ? mPalette[(int)PaletteIndex::CollapsorBoxEnabled] : mPalette[(int)PaletteIndex::CollapsorBoxDisabled];
					drawList->AddRect(pos, endingpos, col, 0);

					//Drawing + and - signs
					const float lineindent = 3;
					ImVec2 vertpos;	vertpos.x = (pos.x + endingpos.x) / 2;	vertpos.y = pos.y + lineindent - 1;
					ImVec2 vertendingpos; vertendingpos.x = vertpos.x; vertendingpos.y = endingpos.y - lineindent;
					if (bIsFoldEnabled)
						drawList->AddLine(vertpos, vertendingpos, col, 2); // vertical line
					vertpos.y = (pos.y + endingpos.y) / 2;	vertpos.x = pos.x + lineindent - 1;
					vertendingpos.y = vertpos.y; vertendingpos.x = endingpos.x - lineindent;
					drawList->AddLine(vertpos, vertendingpos, col, 2); // horizontal line

					//Drawing Collapsing box line
					if (!bIsFoldEnabled)
					{
						ImVec2 cblinestart = endingpos;
						cblinestart.x = (pos.x + endingpos.x) / 2;
						ImVec2 cblineend = cblinestart;
						cblineend.y = endposition.y + mCharAdvance.y / 2; //End of Dotted line + half of char height;
						drawList->AddLine(cblinestart, cblineend, col, 1);
						cblinestart = cblineend;
						cblineend.x += mCollapsorStart / 2;
						drawList->AddLine(cblinestart, cblineend, col, 1);
					}

				};
			for (auto& collapsable : mCollapsables)
			{
				if (collapsable.mStart < collapsable.mEnd)
				{
					drawCollapsableStuff(collapsable.mStart, collapsable.mEnd, collapsable.mIsFoldEnabled);
				}
			}
		}
	}

	//Drawing sections for Breakpoint and LineNo
	if (mDrawBars)
	{
		ImVec2 contentSizeMin = ImGui::GetWindowPos();
		auto startbpArea = ImVec2(cursorScreenPos.x, cursorScreenPos.y + scrollY);
		auto endbpArea = ImVec2(cursorScreenPos.x + mLineNoStart + 2, startbpArea.y + contentSize.y + contentSizeMin.y - cursorScreenPos.y);
		drawList->AddRectFilled(startbpArea, endbpArea, mPalette[(int)PaletteIndex::BreakpointBar]); //BP Bar Color
		startbpArea.x = endbpArea.x + 2;
		endbpArea.x = endbpArea.x > 0 ? mTextStart : endbpArea.x;
		drawList->AddRectFilled(startbpArea, endbpArea, mPalette[(int)PaletteIndex::LineNoBar]); //Line No Bar Color
	}

	size_t collapsedItemsCount = 0;
	if (mShowCollapsables)
	{
		for (auto& val : mActiveCollapsables)
		{
			if (mIgnoredActiveCollapsables.find(val.first) == mIgnoredActiveCollapsables.end())
				collapsedItemsCount += (val.second - val.first);

		}
	}
	ImGui::Dummy(ImVec2((longest + 2), (mLines.size() - collapsedItemsCount)* mCharAdvance.y));

	if (mScrollToCursor)
	{
		EnsureCursorVisible();
		ImGui::SetWindowFocus();
		mScrollToCursor = false;
	}
}

void TextEditor::RenderSearch()
{
	if (mShowSearchBar)
	{
		if (ImGui::Begin("##Searcher", &mShowSearchBar, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoDocking))
		{
			ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4, 0));
			ImGui::InputText("##Search text", &mSearchStr, ImGuiTextFlags_None);
			ImGui::SameLine();
			auto col = ImGui::GetStyle().Colors[ImGuiCol_WindowBg];
			ImGui::PushStyleColor(ImGuiCol_Button, ImColor(0).Value);
			ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(col.x + 0.2, col.y + 0.2, col.z + 0.2, col.w));
			ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(col.x + 0.4, col.y + 0.4, col.z + 0.4, col.w));
			if (ImGui::ArrowButton("##Backwards", ImGuiDir_Left) && mSearchStr.size() > 0)
			{
				std::wstring wstr(mSearchStr.begin(), mSearchStr.end());
				auto currentPos = GetCursorPosition();
				int lineNo = currentPos.mLine - 1;
				for (; lineNo >= 0; lineNo--)
				{
					size_t pos = GetText(Coordinates(lineNo, 0), Coordinates(lineNo, GetLineMaxColumn(lineNo))).find(wstr);
					if (pos != _stdstr::npos)
					{
						int clm = GetCharacterColumn(lineNo, pos);
						SetCursorPosition(Coordinates(lineNo, 0));
						if (clm >= 0 && clm < GetLineMaxColumn(lineNo) && wstr.size() + pos < GetLineCharacterCount(lineNo))
						{
							SetSelection(Coordinates(lineNo, clm), Coordinates(lineNo, GetCharacterColumn(lineNo, wstr.size() + pos)));
						}
						mScrollToCursor = true;
						break;
					}
				}
				if (lineNo == -1 && currentPos.mLine < mLines.size() - 1) //if backwards search has no result, start from end to the current line
				{
					for (lineNo = mLines.size() - 1; lineNo >= currentPos.mLine; lineNo--)
					{
						_stdstr txt = GetText(Coordinates(lineNo, 0), Coordinates(lineNo, GetLineMaxColumn(lineNo)));
						size_t pos = txt.find(wstr);
						if (pos != _stdstr::npos)
						{
							int clm = GetCharacterColumn(lineNo, pos);
							SetCursorPosition(Coordinates(lineNo, 0));
							if (clm >= 0 && clm < GetLineMaxColumn(lineNo) && wstr.size() + pos < GetLineCharacterCount(lineNo))
							{
								SetSelection(Coordinates(lineNo, clm), Coordinates(lineNo, GetCharacterColumn(lineNo, wstr.size() + pos)));
							}
							mScrollToCursor = true;
							break;
						}
					}
				}

			}
			ImGui::SameLine();
			if ((ImGui::ArrowButton("##Forewords", ImGuiDir_Right) || ImGui::Shortcut(ImGuiKey_Enter, ImGuiInputFlags_RouteFocused)) && mSearchStr.size() > 0)
			{
				std::wstring wstr(mSearchStr.begin(), mSearchStr.end());
				auto currentPos = GetCursorPosition();
				int lineNo = currentPos.mLine + 1;
				for (; lineNo < mLines.size(); lineNo++)
				{
					size_t pos = GetText(Coordinates(lineNo, 0), Coordinates(lineNo, GetLineMaxColumn(lineNo))).find(wstr);
					if (pos != _stdstr::npos)
					{
						int clm = GetCharacterColumn(lineNo, pos);
						SetCursorPosition(Coordinates(lineNo, 0));
						if (clm >= 0 && clm < GetLineMaxColumn(lineNo) && wstr.size() + pos < GetLineCharacterCount(lineNo))
						{
							SetSelection(Coordinates(lineNo, clm), Coordinates(lineNo, GetCharacterColumn(lineNo, wstr.size() + pos)));
						}
						mScrollToCursor = true;
						break;
					}
				}
				if (lineNo == mLines.size() && currentPos.mLine > 0) //if forward search has no result, start from beginning
				{
					for (lineNo = 0; lineNo <= currentPos.mLine; lineNo++)
					{
						size_t pos = GetText(Coordinates(lineNo, 0), Coordinates(lineNo, GetLineMaxColumn(lineNo))).find(wstr);
						if (pos != _stdstr::npos)
						{
							int clm = GetCharacterColumn(lineNo, pos);
							SetCursorPosition(Coordinates(lineNo, 0));
							if (clm >= 0 && clm < GetLineMaxColumn(lineNo) && wstr.size() + pos < GetLineCharacterCount(lineNo))
							{
								SetSelection(Coordinates(lineNo, clm), Coordinates(lineNo, GetCharacterColumn(lineNo, wstr.size() + pos)));
							}
							mScrollToCursor = true;
							break;
						}
					}
				}
			}
			ImGui::SameLine();
			if (ImGui::Button("X##closebtn"))
				mShowSearchBar = false;
			ImGui::PopStyleColor(3);
			ImGui::PopStyleVar();
		}
		ImGui::End();
	}

}


bool TextEditor::ToggleBreakpoint(int aLineNo)
{
	if (mBreakpoints.find(aLineNo) != mBreakpoints.end())
	{
		if (mOnBreakpointToggle)
			mOnBreakpointToggle(aLineNo, false);
		mBreakpoints.erase(aLineNo);
		return false;
	}
	bool bShouldAdd = !!mOnBreakpointToggle ? mOnBreakpointToggle(aLineNo, true) : true;
	if (bShouldAdd)
		mBreakpoints.insert(aLineNo);
	return true;
}

int TextEditor::GetTriggerLine()
{
	return mTriggeredBPLine;
}

void TextEditor::SetTriggerLine(int aLinNo)
{
	mTriggeredBPLine = aLinNo;
}

void TextEditor::SetText(const _stdstr& aText)
{
	mLines.clear();
	mLines.emplace_back(Line());
	for (auto chr : aText)
	{
		if (chr == '\r')
		{
			// ignore the carriage return character
		}
		else if (chr == '\n')
			mLines.emplace_back(Line());
		else
		{
			mLines.back().emplace_back(Glyph(chr, PaletteIndex::Default));
		}
	}

	mTextChanged = true;
	mScrollToTop = true;

	mUndoBuffer.clear();
	mUndoIndex = 0;

	Colorize();
}

void TextEditor::SetTextLines(const std::vector<_stdstr>& aLines)
{
	mLines.clear();

	if (aLines.empty())
	{
		mLines.emplace_back(Line());
	}
	else
	{
		mLines.resize(aLines.size());

		for (size_t i = 0; i < aLines.size(); ++i)
		{
			const _stdstr& aLine = aLines[i];

			mLines[i].reserve(aLine.size());
			for (size_t j = 0; j < aLine.size(); ++j)
				mLines[i].emplace_back(Glyph(aLine[j], PaletteIndex::Default));
		}
	}

	mTextChanged = true;
	mScrollToTop = true;

	mUndoBuffer.clear();
	mUndoIndex = 0;

	Colorize();
}

void TextEditor::EnterCharacter(ImWchar aChar, bool aShift)
{
	assert(!mReadOnly);

	UndoRecord u;

	u.mBefore = mState;

	if (HasSelection())
	{
		if (aChar == '\t' && mState.mSelectionStart.mLine != mState.mSelectionEnd.mLine)
		{

			auto start = mState.mSelectionStart;
			auto end = mState.mSelectionEnd;
			auto originalEnd = end;

			if (start > end)
				std::swap(start, end);
			start.mColumn = 0;
			//			end.mColumn = end.mLine < mLines.size() ? mLines[end.mLine].size() : 0;
			if (end.mColumn == 0 && end.mLine > 0)
				--end.mLine;
			if (end.mLine >= (int)mLines.size())
				end.mLine = mLines.empty() ? 0 : (int)mLines.size() - 1;
			end.mColumn = GetLineMaxColumn(end.mLine);

			//if (end.mColumn >= GetLineMaxColumn(end.mLine))
			//	end.mColumn = GetLineMaxColumn(end.mLine) - 1;

			u.mRemovedStart = start;
			u.mRemovedEnd = end;
			u.mRemoved = GetText(start, end);

			bool modified = false;

			for (int i = start.mLine; i <= end.mLine; i++)
			{
				auto& line = mLines[i];
				if (aShift)
				{
					if (!line.empty())
					{
						if (line.front().mChar == '\t')
						{
							line.erase(line.begin());
							modified = true;
						}
						else
						{
							for (int j = 0; j < mTabSize && !line.empty() && line.front().mChar == ' '; j++)
							{
								line.erase(line.begin());
								modified = true;
							}
						}
					}
				}
				else
				{
					line.insert(line.begin(), Glyph('\t', TextEditor::PaletteIndex::Background));
					modified = true;
				}
			}

			if (modified)
			{
				start = Coordinates(start.mLine, GetCharacterColumn(start.mLine, 0));
				Coordinates rangeEnd;
				if (originalEnd.mColumn != 0)
				{
					end = Coordinates(end.mLine, GetLineMaxColumn(end.mLine));
					rangeEnd = end;
					u.mAdded = GetText(start, end);
				}
				else
				{
					end = Coordinates(originalEnd.mLine, 0);
					rangeEnd = Coordinates(end.mLine - 1, GetLineMaxColumn(end.mLine - 1));
					u.mAdded = GetText(start, rangeEnd);
				}

				u.mAddedStart = start;
				u.mAddedEnd = rangeEnd;
				u.mAfter = mState;

				mState.mSelectionStart = start;
				mState.mSelectionEnd = end;
				AddUndo(u);

				mTextChanged = true;

				EnsureCursorVisible();
			}

			UpdateCollapsables(start);
			return;
		} // c == '\t'
		else
		{
			u.mRemoved = GetSelectedText();
			u.mRemovedStart = mState.mSelectionStart;
			u.mRemovedEnd = mState.mSelectionEnd;
			DeleteSelection();
		}
	} // HasSelection

	auto coord = GetActualCursorCoordinates();
	u.mAddedStart = coord;
	auto cindex = GetCharacterIndex(coord);

	assert(!mLines.empty());

	if (aChar == '\n')
	{
		InsertLine(coord.mLine + 1, cindex);
		auto& line = mLines[coord.mLine];
		auto& newLine = mLines[coord.mLine + 1];
		if (mLanguageDefinition.mAutoIndentation)
			for (size_t it = 0; it < line.size() && isascii(line[it].mChar) && _isblank_l(line[it].mChar, _get_current_locale()); ++it)
				newLine.push_back(line[it]);

		const size_t whitespaceSize = newLine.size();
		auto cindex = GetCharacterIndex(coord);
		newLine.insert(newLine.end(), line.begin() + cindex, line.end());
		line.erase(line.begin() + cindex, line.begin() + line.size());
		SetCursorPosition(Coordinates(coord.mLine + 1, GetCharacterColumn(coord.mLine + 1, (int)whitespaceSize)));
		u.mAdded = (char)aChar;
	}
	else
	{
		char buf[7];
		int e = ImTextCharToUtf8(buf, 7, aChar);
		if (e > 0)
		{
			buf[e] = '\0';
			auto& line = mLines[coord.mLine];

			if (mOverwrite && cindex < (int)line.size())
			{
				auto d = UTF8CharLength(line[cindex].mChar);

				u.mRemovedStart = mState.mCursorPosition;
				u.mRemovedEnd = Coordinates(coord.mLine, GetCharacterColumn(coord.mLine, cindex + d));

				while (d-- > 0 && cindex < (int)line.size())
				{
					u.mRemoved += line[cindex].mChar;
					line.erase(line.begin() + cindex);
				}
			}

			for (auto p = buf; *p != '\0'; p++, ++cindex)
				line.insert(line.begin() + cindex, Glyph(*p, PaletteIndex::Default));
			u.mAdded = MakeString(buf);

			SetCursorPosition(Coordinates(coord.mLine, GetCharacterColumn(coord.mLine, cindex)));
		}
		else
			return;
	}

	mTextChanged = true;
	UpdateCollapsables(coord);
	u.mAddedEnd = GetActualCursorCoordinates();
	u.mAfter = mState;

	AddUndo(u);

	Colorize(coord.mLine - 1, 3);
	EnsureCursorVisible();
}

void TextEditor::SetReadOnly(bool aValue)
{
	mReadOnly = aValue;
}

void TextEditor::SetColorizerEnable(bool aValue)
{
	mColorizerEnabled = aValue;
}

void TextEditor::SetCursorPosition(const Coordinates& aPosition)
{
	if (mState.mCursorPosition != aPosition)
	{
		mState.mCursorPosition = aPosition;
		mCursorPositionChanged = true;
		EnsureCursorVisible();
	}
}

void TextEditor::GotoLine(int aLineNo)
{
	if (aLineNo == -1)
		SetCursorPosition(Coordinates(mLines.size() - 1, 0));
	else if (aLineNo < mLines.size())
		SetCursorPosition(Coordinates(aLineNo, 0));
}

void TextEditor::SetSelectionStart(const Coordinates& aPosition)
{
	mState.mSelectionStart = SanitizeCoordinates(aPosition);
	if (mState.mSelectionStart > mState.mSelectionEnd)
		std::swap(mState.mSelectionStart, mState.mSelectionEnd);
}

void TextEditor::SetSelectionEnd(const Coordinates& aPosition)
{
	mState.mSelectionEnd = SanitizeCoordinates(aPosition);
	if (mState.mSelectionStart > mState.mSelectionEnd)
		std::swap(mState.mSelectionStart, mState.mSelectionEnd);
}

void TextEditor::SetSelection(const Coordinates& aStart, const Coordinates& aEnd, SelectionMode aMode)
{
	auto oldSelStart = mState.mSelectionStart;
	auto oldSelEnd = mState.mSelectionEnd;

	mState.mSelectionStart = SanitizeCoordinates(aStart);
	mState.mSelectionEnd = SanitizeCoordinates(aEnd);
	if (mState.mSelectionStart > mState.mSelectionEnd)
		std::swap(mState.mSelectionStart, mState.mSelectionEnd);

	switch (aMode)
	{
	case TextEditor::SelectionMode::Normal:
		break;
	case TextEditor::SelectionMode::Word:
	{
		mState.mSelectionStart = FindWordStart(mState.mSelectionStart);
		if (!IsOnWordBoundary(mState.mSelectionEnd))
			mState.mSelectionEnd = FindWordEnd(FindWordStart(mState.mSelectionEnd));
		break;
	}
	case TextEditor::SelectionMode::Line:
	{
		const auto lineNo = mState.mSelectionEnd.mLine;
		const auto lineSize = (size_t)lineNo < mLines.size() ? mLines[lineNo].size() : 0;
		mState.mSelectionStart = Coordinates(mState.mSelectionStart.mLine, 0);
		mState.mSelectionEnd = Coordinates(lineNo, GetLineMaxColumn(lineNo));
		break;
	}
	default:
		break;
	}

	if (mState.mSelectionStart != oldSelStart ||
		mState.mSelectionEnd != oldSelEnd)
		mCursorPositionChanged = true;
}

void TextEditor::SetTabSize(int aValue)
{
	mTabSize = std::max(0, std::min(32, aValue));
}

void TextEditor::InsertText(const _stdstr& aValue)
{
	InsertText(aValue.c_str());
}

void TextEditor::InsertText(_constcharptr aValue)
{
	if (aValue == nullptr)
		return;

	auto pos = GetActualCursorCoordinates();
	auto start = std::min(pos, mState.mSelectionStart);
	int totalLines = pos.mLine - start.mLine;

	totalLines += InsertTextAt(pos, aValue);

	SetSelection(pos, pos);
	SetCursorPosition(pos);
	Colorize(start.mLine - 1, totalLines + 2);
}

void TextEditor::DeleteSelection()
{
	assert(mState.mSelectionEnd >= mState.mSelectionStart);

	if (mState.mSelectionEnd == mState.mSelectionStart)
		return;

	DeleteRange(mState.mSelectionStart, mState.mSelectionEnd);

	SetSelection(mState.mSelectionStart, mState.mSelectionStart);
	SetCursorPosition(mState.mSelectionStart);
	Colorize(mState.mSelectionStart.mLine, 1);
}

void TextEditor::MoveUp(int aAmount, bool aSelect)
{
	auto oldPos = mState.mCursorPosition;
	mState.mCursorPosition.mLine = std::max(0, mState.mCursorPosition.mLine - aAmount);
	if (oldPos != mState.mCursorPosition)
	{
		if (aSelect)
		{
			if (oldPos == mInteractiveStart)
				mInteractiveStart = mState.mCursorPosition;
			else if (oldPos == mInteractiveEnd)
				mInteractiveEnd = mState.mCursorPosition;
			else
			{
				mInteractiveStart = mState.mCursorPosition;
				mInteractiveEnd = oldPos;
			}
		}
		else
			mInteractiveStart = mInteractiveEnd = mState.mCursorPosition;
		SetSelection(mInteractiveStart, mInteractiveEnd);

		EnsureCursorVisible();
	}
}

void TextEditor::MoveDown(int aAmount, bool aSelect)
{
	assert(mState.mCursorPosition.mColumn >= 0);
	auto oldPos = mState.mCursorPosition;
	mState.mCursorPosition.mLine = std::max(0, std::min((int)mLines.size() - 1, mState.mCursorPosition.mLine + aAmount));

	if (mState.mCursorPosition != oldPos)
	{
		if (aSelect)
		{
			if (oldPos == mInteractiveEnd)
				mInteractiveEnd = mState.mCursorPosition;
			else if (oldPos == mInteractiveStart)
				mInteractiveStart = mState.mCursorPosition;
			else
			{
				mInteractiveStart = oldPos;
				mInteractiveEnd = mState.mCursorPosition;
			}
		}
		else
			mInteractiveStart = mInteractiveEnd = mState.mCursorPosition;
		SetSelection(mInteractiveStart, mInteractiveEnd);

		EnsureCursorVisible();
	}
}

static bool IsUTFSequence(char c)
{
	return (c & 0xC0) == 0x80;
}

void TextEditor::MoveLeft(int aAmount, bool aSelect, bool aWordMode)
{
	if (mLines.empty())
		return;

	auto oldPos = mState.mCursorPosition;
	mState.mCursorPosition = GetActualCursorCoordinates();
	auto line = mState.mCursorPosition.mLine;
	auto cindex = GetCharacterIndex(mState.mCursorPosition);

	while (aAmount-- > 0)
	{
		if (cindex == 0)
		{
			if (line > 0)
			{
				--line;
				if ((int)mLines.size() > line)
					cindex = (int)mLines[line].size();
				else
					cindex = 0;
			}
		}
		else
		{
			--cindex;
			if (cindex > 0)
			{
				if ((int)mLines.size() > line)
				{
					while (cindex > 0 && IsUTFSequence(mLines[line][cindex].mChar))
						--cindex;
				}
			}
		}

		mState.mCursorPosition = Coordinates(line, GetCharacterColumn(line, cindex));
		if (aWordMode)
		{
			mState.mCursorPosition = FindWordStart(mState.mCursorPosition);
			cindex = GetCharacterIndex(mState.mCursorPosition);
		}
	}

	mState.mCursorPosition = Coordinates(line, GetCharacterColumn(line, cindex));

	assert(mState.mCursorPosition.mColumn >= 0);
	if (aSelect)
	{
		if (oldPos == mInteractiveStart)
			mInteractiveStart = mState.mCursorPosition;
		else if (oldPos == mInteractiveEnd)
			mInteractiveEnd = mState.mCursorPosition;
		else
		{
			mInteractiveStart = mState.mCursorPosition;
			mInteractiveEnd = oldPos;
		}
	}
	else
		mInteractiveStart = mInteractiveEnd = mState.mCursorPosition;
	SetSelection(mInteractiveStart, mInteractiveEnd, aSelect && aWordMode ? SelectionMode::Word : SelectionMode::Normal);

	EnsureCursorVisible();
}

void TextEditor::MoveRight(int aAmount, bool aSelect, bool aWordMode)
{
	auto oldPos = mState.mCursorPosition;

	if (mLines.empty() || oldPos.mLine >= mLines.size())
		return;

	auto cindex = GetCharacterIndex(mState.mCursorPosition);
	while (aAmount-- > 0)
	{
		auto lindex = mState.mCursorPosition.mLine;
		auto& line = mLines[lindex];

		if (cindex >= line.size())
		{
			if (mState.mCursorPosition.mLine < mLines.size() - 1)
			{
				mState.mCursorPosition.mLine = std::max(0, std::min((int)mLines.size() - 1, mState.mCursorPosition.mLine + 1));
				mState.mCursorPosition.mColumn = 0;
			}
			else
				return;
		}
		else
		{
			cindex += UTF8CharLength(line[cindex].mChar);
			mState.mCursorPosition = Coordinates(lindex, GetCharacterColumn(lindex, cindex));
			if (aWordMode)
				mState.mCursorPosition = FindNextWord(mState.mCursorPosition);
		}
	}

	if (aSelect)
	{
		if (oldPos == mInteractiveEnd)
			mInteractiveEnd = SanitizeCoordinates(mState.mCursorPosition);
		else if (oldPos == mInteractiveStart)
			mInteractiveStart = mState.mCursorPosition;
		else
		{
			mInteractiveStart = oldPos;
			mInteractiveEnd = mState.mCursorPosition;
		}
	}
	else
		mInteractiveStart = mInteractiveEnd = mState.mCursorPosition;
	SetSelection(mInteractiveStart, mInteractiveEnd, aSelect && aWordMode ? SelectionMode::Word : SelectionMode::Normal);

	EnsureCursorVisible();
}

void TextEditor::MoveTop(bool aSelect)
{
	auto oldPos = mState.mCursorPosition;
	SetCursorPosition(Coordinates(0, 0));

	if (mState.mCursorPosition != oldPos)
	{
		if (aSelect)
		{
			mInteractiveEnd = oldPos;
			mInteractiveStart = mState.mCursorPosition;
		}
		else
			mInteractiveStart = mInteractiveEnd = mState.mCursorPosition;
		SetSelection(mInteractiveStart, mInteractiveEnd);
	}
}

void TextEditor::TextEditor::MoveBottom(bool aSelect)
{
	auto oldPos = GetCursorPosition();
	auto newPos = Coordinates((int)mLines.size() - 1, 0);
	SetCursorPosition(newPos);
	if (aSelect)
	{
		mInteractiveStart = oldPos;
		mInteractiveEnd = newPos;
	}
	else
		mInteractiveStart = mInteractiveEnd = newPos;
	SetSelection(mInteractiveStart, mInteractiveEnd);
}

void TextEditor::MoveHome(bool aSelect)
{
	auto oldPos = mState.mCursorPosition;
	SetCursorPosition(Coordinates(mState.mCursorPosition.mLine, 0));

	if (mState.mCursorPosition != oldPos)
	{
		if (aSelect)
		{
			if (oldPos == mInteractiveStart)
				mInteractiveStart = mState.mCursorPosition;
			else if (oldPos == mInteractiveEnd)
				mInteractiveEnd = mState.mCursorPosition;
			else
			{
				mInteractiveStart = mState.mCursorPosition;
				mInteractiveEnd = oldPos;
			}
		}
		else
			mInteractiveStart = mInteractiveEnd = mState.mCursorPosition;
		SetSelection(mInteractiveStart, mInteractiveEnd);
	}
}

void TextEditor::MoveEnd(bool aSelect)
{
	auto oldPos = mState.mCursorPosition;
	SetCursorPosition(Coordinates(mState.mCursorPosition.mLine, GetLineMaxColumn(oldPos.mLine)));

	if (mState.mCursorPosition != oldPos)
	{
		if (aSelect)
		{
			if (oldPos == mInteractiveEnd)
				mInteractiveEnd = mState.mCursorPosition;
			else if (oldPos == mInteractiveStart)
				mInteractiveStart = mState.mCursorPosition;
			else
			{
				mInteractiveStart = oldPos;
				mInteractiveEnd = mState.mCursorPosition;
			}
		}
		else
			mInteractiveStart = mInteractiveEnd = mState.mCursorPosition;
		SetSelection(mInteractiveStart, mInteractiveEnd);
	}
}

bool TextEditor::Move(int& aLine, int& aCharIndex, bool aLeft)
{
	if (aLine >= mLines.size())
		return false;
	if (aLeft)
	{
		if (aCharIndex <= 0)
		{
			if (aLine == 0)
				return false;
			aLine--;
			aCharIndex = mLines[aLine].size();
		}
		else
		{
			aCharIndex--;
		}
	}
	else
	{
		if (aCharIndex >= mLines[aLine].size()) //proceed to next line after the last char of this line
		{
			if (aLine == mLines.size() - 1)
				return false;
			aLine++;
			aCharIndex = 0;
		}
		else
		{
			aCharIndex++;
		}
	}
	return true;
}

bool TextEditor::FindMatchingBracket(int aLine, int aCharIndex, Coordinates& outcorrd)
{
	if (aLine > mLines.size() - 1)
		return false;
	int maxCharIndex = mLines[aLine].size() - 1;
	if (aCharIndex > maxCharIndex)
		return false;
	int currentLine = aLine;
	int currentCharIndex = aCharIndex;
	int counter = 1;
	_justchar currentbracketchar = mLines[aLine][aCharIndex].mChar;
	if (mLanguageDefinition.mBracketsOffOn.find(currentbracketchar) != mLanguageDefinition.mBracketsOffOn.end())
	{
		_justchar openingcharacter = mLanguageDefinition.mBracketsOffOn.at(currentbracketchar);
		while (Move(currentLine, currentCharIndex, true))
		{
			if (currentCharIndex < mLines[currentLine].size())
			{
				_justchar currentchar = mLines[currentLine][currentCharIndex].mChar;
				if (currentchar == currentbracketchar)
					counter++;
				else if (currentchar == openingcharacter)
					counter--;
				if (counter == 0)
				{
					outcorrd = { currentLine , GetCharacterColumn(currentLine, currentCharIndex) };
					return true;
				}
			}
		}
	}
	else if (mLanguageDefinition.mBracketsOnOff.find(currentbracketchar) != mLanguageDefinition.mBracketsOnOff.end())
	{
		_justchar closingcharacter = mLanguageDefinition.mBracketsOnOff.at(currentbracketchar);
		while (Move(currentLine, currentCharIndex))
		{
			if (currentCharIndex < mLines[currentLine].size())
			{
				_justchar currentchar = mLines[currentLine][currentCharIndex].mChar;
				if (currentchar == currentbracketchar)
					counter++;
				else if (currentchar == closingcharacter)
					counter--;
				if (counter == 0)
				{
					outcorrd = { currentLine , GetCharacterColumn(currentLine, currentCharIndex) };
					return true;
				}
			}
		}
	}

	return false;
}

bool TextEditor::LineHasAnyOpeningCollapsable(int aLine, int& i, int* OutCharIndex, _stdstr& outchar)
{
	if (aLine >= mLines.size())
		return false;
	auto& line = mLines[aLine];
	auto& allcollapsables = mLanguageDefinition.mCollapsablesOnOff;
	for (; i < line.size(); i++)
	{
		outchar = _stdstr(1, line[i].mChar);
		if (OutCharIndex)
			*OutCharIndex = i;
		if (allcollapsables.find(outchar) != allcollapsables.end())
		{
			i++;
			return true;
		}
		if (i < line.size() - 1)
		{
			outchar += line[i + 1].mChar;
			if (allcollapsables.find(outchar) != allcollapsables.end())
			{
				i++;
				return true;
			}
		}
	}

	return false;
	/*CollapsableType outval = CollapsableType::None;
	if (aLine < mLines.size())
	{
		auto& line = mLines[aLine];
		size_t totalcharsinline = line.size();
		for (int i = 0; i < totalcharsinline; i++)
		{
			auto& chr = line[i];
			if (OutCharIndex != nullptr)
				*OutCharIndex = i;
			if (outchar != nullptr)
				*outchar = chr.mChar;
			_stdstr sss = _stdstr(1, chr.mChar);
			if ((chr.mChar == mystr('/') || chr.mChar == mystr('*')) && i < totalcharsinline-1)
				sss+= line[i+1].mChar;
			if (mLanguageDefinition.mOpeningCollapsables.find(sss) != mLanguageDefinition.mOpeningCollapsables.end())
				outval = (CollapsableType)(outval | CollapsableType::Starting);
			else if (mLanguageDefinition.mClosingCollapsables.find(sss) != mLanguageDefinition.mClosingCollapsables.end())
				outval = (CollapsableType)(outval | CollapsableType::Ending);
		}
	}
	return outval;*/
}

void TextEditor::UpdateBrackets()
{
	mCursorOnABracket = FindMatchingBracket(mState.mCursorPosition.mLine, GetCharacterIndex(mState.mCursorPosition), mMatchingBracket);

	//Update all collapsables after current position
	//UpdateCollapsables(mState.mCursorPosition);
}

bool TextEditor::FindMatchingCollabsable(int aStartLine, int aStartColumn, _stdstr& collapsableChar, 
	Coordinates& outcorrd, _stdstr& outcollapsable, bool bFindOpening, int endingLine)
{
	if (aStartLine >= mLines.size())
		return false;
	if (bFindOpening && aStartLine == 0)
		return false;
	int currentCharIndex = bFindOpening ? mLines[aStartLine].size() : aStartColumn;
	int currentLine = aStartLine;
	int ccharsize = collapsableChar.size();
	int counter = 1;
	if (ccharsize == 0)
		return false;
	if (!bFindOpening)
	{
		if (mLanguageDefinition.mCollapsablesOnOff.find(collapsableChar) != mLanguageDefinition.mCollapsablesOnOff.end())
		{
			if (endingLine == -1)
				endingLine = mLines.size() - 1;
			_stdstr closingCollaps = mLanguageDefinition.mCollapsablesOnOff.at(collapsableChar);
			while (Move(currentLine, currentCharIndex))
			{
				if (currentLine > endingLine)
					return false;
				auto& line = mLines[currentLine];
				if (currentCharIndex < line.size())
				{
					_stdstr currentchar = _stdstr(1, line[currentCharIndex].mChar);
					if (ccharsize == 2 && currentCharIndex < line.size() - 1)
						currentchar += line[currentCharIndex + 1].mChar;
					if (currentchar == collapsableChar)
						counter++;
					else if (currentchar == closingCollaps)
						counter--;
					if (counter <= 0)
					{
						outcorrd = { currentLine , GetCharacterColumn(currentLine, currentCharIndex) };
						outcollapsable = currentchar;
						return true;
					}
				}
			}
		}
	}
	else
	{
		if (mLanguageDefinition.mCollapsablesOffOn.find(collapsableChar) != mLanguageDefinition.mCollapsablesOffOn.end())
		{
			if (endingLine == -1)
				endingLine = 0;
			_stdstr openingCollaps = mLanguageDefinition.mCollapsablesOffOn.at(collapsableChar);
			while (Move(currentLine, currentCharIndex, true))
			{
				if (currentLine < endingLine)
					return false;
				auto& line = mLines[currentLine];
				if (currentCharIndex < line.size())
				{
					_stdstr currentchar = _stdstr(1, line[currentCharIndex].mChar);
					if (ccharsize == 2 && currentCharIndex < line.size() - 1)
						currentchar += line[currentCharIndex + 1].mChar;
					if (currentchar == collapsableChar)
						counter++;
					else if (currentchar == openingCollaps)
						counter--;
					if (counter <= 0)
					{
						outcollapsable = currentchar;
						outcorrd = { currentLine , GetCharacterColumn(currentLine, currentCharIndex) };
						return true;
					}
				}
			}
		}
	}
	return false;
}

bool TextEditor::FindMatchingOpeningCollabsable(int aStartLine, int aStartColumn, _stdstr& collapsableChar, Coordinates& outcorrd, _stdstr& outcollapsable, int endingLine)
{
	return FindMatchingCollabsable(aStartLine, aStartColumn, collapsableChar, outcorrd, outcollapsable, true, endingLine);
}

bool TextEditor::FindMatchingClosingCollabsable(int aStartLine, int aStartColumn, _stdstr& collapsableChar, Coordinates& outcorrd, _stdstr& outcollapsable, int endingLine)
{
	return FindMatchingCollabsable(aStartLine, aStartColumn, collapsableChar, outcorrd, outcollapsable, false, endingLine);
}
#pragma optimize(off, "")
void TextEditor::UpdateCollapsables(Coordinates& currentLine)
{
	if (!mShowCollapsables) return;
	static std::unordered_map<_stdstr, std::pair<int, int>> OpeningLastClose;
	static Coordinates invalid = Coordinates(-1, -1);
	static std::mutex collapsablesMutex;
	static bool forceAbort = true;
	forceAbort = true;
	if (OpeningLastClose.size() == 0)
	{
		for (auto& point : mLanguageDefinition.mCollapsablesOnOff)
		{
			OpeningLastClose.emplace(point.first, std::pair<int, int>(-1, -1));
		}
	}
	else
	{
		for (auto& val : OpeningLastClose)
		{
			val.second.first = -1; val.second.second = -1;
		}
	}

	auto asyncUpdate = [currentLine, this](TextEditor* teditor, Coordinates currentLine)
	{
		std::lock_guard<std::mutex> lock(collapsablesMutex);
		forceAbort = false;
		try
		{
			Coordinates currentCorrds = currentLine;
			for (Collapsables::iterator it = teditor->mCollapsables.begin(); it != teditor->mCollapsables.end(); it++)
			{
				if (forceAbort)
					return;
				else if (it->mEnd.mLine >= currentLine.mLine && it->mStart < currentLine)
				{
					//Correct the ending of current collapsable
					if (!teditor->FindMatchingClosingCollabsable(it->mStart.mLine, it->mStart.mColumn + 1, it->mInfoStart, it->mEnd, it->mInfoEnd))
					{
						//if fails to correct, erast current point
						goto erasePoint;
					}
				}
				else if (it->mEnd.mLine >= currentLine.mLine || /*&& it->mStart < currentLine*/
					(it->mEnd == invalid && it->mInfoEnd.size() == 0) // if ending was not availabe but only the start
					)
				{
				erasePoint:
					currentCorrds = currentLine < it->mStart ? currentLine : it->mStart;
					bool bChanged = false;
					for (auto iit = it; iit < teditor->mCollapsables.end(); iit++) //Uncollaps all next onces
					{
						if (teditor->mActiveCollapsables.find(iit->mStart.mLine) != teditor->mActiveCollapsables.end())
						{
							teditor->mActiveCollapsables.erase(teditor->mActiveCollapsables.find(iit->mStart.mLine));
							bChanged = true;
						}
					}
					if (bChanged)
					{
						teditor->mIgnoredActiveCollapsables.clear();
						teditor->UpdateIgnoreCollapsables(true);
					}
					teditor->mCollapsables.erase(it, teditor->mCollapsables.end());
					break;
				}

			}
			while (currentCorrds.mLine < teditor->mLines.size())
			{
				int outindex = 0;
				_stdstr outchar;
				int StartClm = 0;

				while (teditor->LineHasAnyOpeningCollapsable(currentCorrds.mLine, StartClm, &outindex, outchar))
				{
					if (forceAbort)
						return;
					Collapsable cb;
					cb.mStart = Coordinates(currentCorrds.mLine, teditor->GetCharacterColumn(currentCorrds.mLine, outindex));
					cb.mInfoStart = outchar;
					cb.mIsBracket = true;
					int lastpoint = -1;
					bool LastPointFound = false;
					if (OpeningLastClose.find(outchar) != OpeningLastClose.end())
					{
						LastPointFound = true;
						auto& startstop = OpeningLastClose.at(outchar);
						if (cb.mStart.mLine < startstop.second)
							lastpoint = startstop.second;
					}
					if (!teditor->FindMatchingClosingCollabsable(currentCorrds.mLine, outindex, outchar, cb.mEnd, cb.mInfoEnd, lastpoint))
					{
						cb.mEnd = invalid;
						cb.mInfoEnd.clear();
					}
					else if (LastPointFound)
					{
						auto& startstop = OpeningLastClose.at(outchar);
						startstop.first = cb.mStart.mLine;
						startstop.second = cb.mEnd.mLine;
					}
					if (cb.mStart.mLine != cb.mEnd.mLine)
					{
						cb.mIsFoldEnabled = teditor->mActiveCollapsables.find(cb.mStart.mLine) != teditor->mActiveCollapsables.end();
						teditor->mCollapsables.push_back(cb);
					}
					outchar.clear();
				}
				currentCorrds.mLine++;
			}
			if (teditor->mCollapsables.size() == 0)
			{
				teditor->mActiveCollapsables.clear();
				teditor->mIgnoredActiveCollapsables.clear();
			}
		}
		catch (...)
		{

		}
	};
	
	std::jthread(asyncUpdate, this, currentLine).detach();
}
#pragma optimize(on, "")
bool TextEditor::FindCurrentCollapsSize(int aCurrentLine, int* aOutCollapsStart, int* aOutCollapsEnd) const
{
	if (aCurrentLine >= mLines.size() || !mLines[aCurrentLine].mIsCollapsed)
		return false;
	if (!!aOutCollapsStart)
	{
		for (int i = aCurrentLine; i >= 0; i--) //reverse
		{
			if (!mLines[i].mIsCollapsed || mLines[i].mHasCollapedInfo)
			{
				*aOutCollapsStart = i;
				break;
			}
		}
	}
	if (!!aOutCollapsEnd)
	{
		for (int i = aCurrentLine; i < mLines.size(); i++) //forward
		{
			if (!mLines[i].mIsCollapsed)
			{
				*aOutCollapsEnd = i;
				break;
			}
		}
	}
	return true;
}

ImVec2 TextEditor::GetPositionAt(Coordinates& coords)
{
	ImVec2 cpos = ImGui::GetCursorPos();
	float cx = TextDistanceToLineStart(coords);
	ImVec2 vec(cpos.x + mTextStart + cx + coords.mColumn * mCharAdvance.x, cpos.y + coords.mLine * mCharAdvance.y);
	
	return vec;
}

void TextEditor::Delete()
{
	assert(!mReadOnly);

	if (mLines.empty())
		return;

	UndoRecord u;
	u.mBefore = mState;

	if (HasSelection())
	{
		u.mRemoved = GetSelectedText();
		u.mRemovedStart = mState.mSelectionStart;
		u.mRemovedEnd = mState.mSelectionEnd;

		DeleteSelection();
	}
	else
	{
		auto pos = GetActualCursorCoordinates();
		SetCursorPosition(pos);
		auto& line = mLines[pos.mLine];

		if (pos.mColumn == GetLineMaxColumn(pos.mLine))
		{
			if (pos.mLine == (int)mLines.size() - 1)
				return;

			u.mRemoved = '\n';
			u.mRemovedStart = u.mRemovedEnd = GetActualCursorCoordinates();
			Advance(u.mRemovedEnd);

			auto& nextLine = mLines[pos.mLine + 1];
			line.insert(line.end(), nextLine.begin(), nextLine.end());
			RemoveLine(pos.mLine + 1);
		}
		else
		{
			auto cindex = GetCharacterIndex(pos);
			u.mRemovedStart = u.mRemovedEnd = GetActualCursorCoordinates();
			u.mRemovedEnd.mColumn++;
			u.mRemoved = GetText(u.mRemovedStart, u.mRemovedEnd);

			auto d = UTF8CharLength(line[cindex].mChar);
			while (d-- > 0 && cindex < (int)line.size())
				line.erase(line.begin() + cindex);
		}

		mTextChanged = true;

		Colorize(pos.mLine, 1);
	}
	UpdateCollapsables(u.mBefore.mCursorPosition);
	u.mAfter = mState;
	AddUndo(u);
}

_stdstr TextEditor::GetClipboardData()
{
	_stdstr outstr;
	ImGuiContext& g = *GImGui;
	if (!::OpenClipboard(NULL))
		return outstr;
	HANDLE wbuf_handle = ::GetClipboardData(CF_UNICODETEXT);
	if (wbuf_handle == NULL)
	{
		::CloseClipboard();
		return outstr;
	}
	if (const WCHAR* wbuf_global = (const WCHAR*)::GlobalLock(wbuf_handle))
	{
		int buf_len = ::WideCharToMultiByte(CP_UTF8, 0, wbuf_global, -1, NULL, 0, NULL, NULL);
#if UseWideStr==1
		outstr = _stdstr((_constcharptr)wbuf_global);
#else
		outstr = MakeString(std::wstring((const wchar_t*)wbuf_global));
#endif
		//outstr.resize(buf_len);
		//::WideCharToMultiByte(CP_UTF8, 0, wbuf_global, -1, g.ClipboardHandlerData.Data, buf_len, NULL, NULL);
	}
	::GlobalUnlock(wbuf_handle);
	::CloseClipboard();
	return outstr;
}

void TextEditor::Backspace()
{
	assert(!mReadOnly);

	if (mLines.empty())
		return;

	UndoRecord u;
	u.mBefore = mState;

	if (HasSelection())
	{
		u.mRemoved = GetSelectedText();
		u.mRemovedStart = mState.mSelectionStart;
		u.mRemovedEnd = mState.mSelectionEnd;

		DeleteSelection();
	}
	else
	{
		auto pos = GetActualCursorCoordinates();
		SetCursorPosition(pos);

		if (mState.mCursorPosition.mColumn == 0)
		{
			if (mState.mCursorPosition.mLine == 0)
				return;

			u.mRemoved = '\n';
			u.mRemovedStart = u.mRemovedEnd = Coordinates(pos.mLine - 1, GetLineMaxColumn(pos.mLine - 1));
			Advance(u.mRemovedEnd);

			auto& line = mLines[mState.mCursorPosition.mLine];
			auto& prevLine = mLines[mState.mCursorPosition.mLine - 1];
			auto prevSize = GetLineMaxColumn(mState.mCursorPosition.mLine - 1);
			prevLine.insert(prevLine.end(), line.begin(), line.end());

			ErrorMarkers etmp;
			for (auto& i : mErrorMarkers)
				etmp.insert(ErrorMarkers::value_type(i.first - 1 == mState.mCursorPosition.mLine ? i.first - 1 : i.first, i.second));
			mErrorMarkers = std::move(etmp);

			RemoveLine(mState.mCursorPosition.mLine);
			--mState.mCursorPosition.mLine;
			mState.mCursorPosition.mColumn = prevSize;
		}
		else
		{
			auto& line = mLines[mState.mCursorPosition.mLine];
			auto cindex = GetCharacterIndex(pos) - 1;
			auto cend = cindex + 1;
			while (cindex > 0 && IsUTFSequence(line[cindex].mChar))
				--cindex;

			if (cindex > 0 && UTF8CharLength(line[cindex].mChar) > 1)
				--cindex;

			u.mRemovedStart = u.mRemovedEnd = GetActualCursorCoordinates();
			
			if (int v = GetCharacterSize(pos))
			{
				u.mRemovedStart.mColumn -= v;
				mState.mCursorPosition.mColumn -= v;
			}
			else
			{
				--u.mRemovedStart.mColumn;
				--mState.mCursorPosition.mColumn;
			}

			while (cindex < line.size() && cend-- > cindex)
			{
				auto& chr = line[cindex].mChar;
				u.mRemoved += chr;
				line.erase(line.begin() + cindex);
			}
		}

		mTextChanged = true;

		pos.mColumn = GetCharacterColumn(pos.mLine, pos.mColumn);
		EnsureCursorVisible();
		Colorize(mState.mCursorPosition.mLine, 1);
	}
	
	UpdateCollapsables(u.mBefore.mCursorPosition);
	u.mAfter = mState;
	AddUndo(u);
}

void TextEditor::SelectWordUnderCursor()
{
	auto c = GetCursorPosition();
	SetSelection(FindWordStart(c), FindWordEnd(c));
}

void TextEditor::SelectAll()
{
	SetSelection(Coordinates(0, 0), Coordinates((int)mLines.size(), 0));
}

bool TextEditor::HasSelection() const
{
	return mState.mSelectionEnd > mState.mSelectionStart;
}

void TextEditor::Copy()
{
	if (HasSelection())
	{
		const char* chars = nullptr;
		size_t charsize = GetSelectedText().size();
#if UseWideStr
		if (charsize > 0)
		{
			chars = (const char*)malloc(charsize * 4);
			ImTextStrToUtf8((char*)chars, charsize + 3, (const ImWchar*)GetSelectedText().c_str(), nullptr);
		}
#else
		chars = mLineBuffer.c_str();
#endif
		ImGui::SetClipboardText(chars);
		if (chars)
			free((void*)chars);
	}
	else
	{
		if (!mLines.empty())
		{
			_stdstr str;
			auto& line = mLines[GetActualCursorCoordinates().mLine];
			for (auto& g : line)
				str.push_back(g.mChar);

			const char* chars = nullptr;
			size_t charsize = str.size();
#if UseWideStr
			if (charsize > 0)
			{
				chars = (const char*)alloca(charsize * 4);
				ImTextStrToUtf8((char*)chars, charsize + 3, (const ImWchar*)str.c_str(), nullptr);
			}
#else
			chars = mLineBuffer.c_str();
#endif
			ImGui::SetClipboardText(chars);
		}
	}
}

void TextEditor::Cut()
{
	if (IsReadOnly())
	{
		Copy();
	}
	else
	{
		if (HasSelection())
		{
			UndoRecord u;
			u.mBefore = mState;
			u.mRemoved = GetSelectedText();
			u.mRemovedStart = mState.mSelectionStart;
			u.mRemovedEnd = mState.mSelectionEnd;

			Copy();
			DeleteSelection();

			u.mAfter = mState;
			AddUndo(u);
		}
	}
}

void TextEditor::Paste()
{
	if (IsReadOnly())
		return;

	auto clipText = GetClipboardData();
	if (clipText.size() > 0)
	{
		UndoRecord u;
		u.mBefore = mState;

		if (HasSelection())
		{
			u.mRemoved = GetSelectedText();
			u.mRemovedStart = mState.mSelectionStart;
			u.mRemovedEnd = mState.mSelectionEnd;
			DeleteSelection();
		}

		u.mAdded = clipText;
		u.mAddedStart = GetActualCursorCoordinates();

		InsertText(clipText);

		u.mAddedEnd = GetActualCursorCoordinates();
		u.mAfter = mState;
		AddUndo(u);
		UpdateCollapsables(u.mAddedStart);
	}
}

bool TextEditor::CanUndo() const
{
	return !mReadOnly && mUndoIndex > 0;
}

bool TextEditor::CanRedo() const
{
	return !mReadOnly && mUndoIndex < (int)mUndoBuffer.size();
}

void TextEditor::Undo(int aSteps)
{
	while (CanUndo() && aSteps-- > 0)
	{
		auto urec = mUndoBuffer[mUndoIndex-1];
		mUndoBuffer[--mUndoIndex].Undo(this);
		UpdateCollapsables(urec.mBefore.mCursorPosition);
	}

	
}

void TextEditor::Redo(int aSteps)
{
	while (CanRedo() && aSteps-- > 0)
		mUndoBuffer[mUndoIndex++].Redo(this);
}

const char* TextEditor::GetPaletteIndexName(PaletteIndex index)
{
#define CaseEnumName(EnumClass, Enum) case EnumClass::Enum: return #Enum
	switch (index)
	{
		CaseEnumName(PaletteIndex, Default);
		CaseEnumName(PaletteIndex, Keyword);
		CaseEnumName(PaletteIndex, Number);
		CaseEnumName(PaletteIndex, String);
		CaseEnumName(PaletteIndex, CharLiteral);
		CaseEnumName(PaletteIndex, Punctuation);
		CaseEnumName(PaletteIndex, Preprocessor);
		CaseEnumName(PaletteIndex, Identifier);
		CaseEnumName(PaletteIndex, KnownIdentifier);
		CaseEnumName(PaletteIndex, PreprocIdentifier);
		CaseEnumName(PaletteIndex, Comment);
		CaseEnumName(PaletteIndex, MultiLineComment);
		CaseEnumName(PaletteIndex, Background);
		CaseEnumName(PaletteIndex, Cursor);
		CaseEnumName(PaletteIndex, Selection);
		CaseEnumName(PaletteIndex, SelectionDuplicate);
		CaseEnumName(PaletteIndex, ErrorMarker);
		CaseEnumName(PaletteIndex, Breakpoint);
		CaseEnumName(PaletteIndex, BreakpointEnabled);
		CaseEnumName(PaletteIndex, LineNumber);
		CaseEnumName(PaletteIndex, LineNumberActive);
		CaseEnumName(PaletteIndex, CurrentLineFill);
		CaseEnumName(PaletteIndex, CurrentLineFillInactive);
		CaseEnumName(PaletteIndex, CurrentLineEdge);
		CaseEnumName(PaletteIndex, Function);
		CaseEnumName(PaletteIndex, Class);
		CaseEnumName(PaletteIndex, EnumFields);
		CaseEnumName(PaletteIndex, LocalProps);
		CaseEnumName(PaletteIndex, MemberProps);
		CaseEnumName(PaletteIndex, ParamProps);
		CaseEnumName(PaletteIndex, BracketsTrace);
		CaseEnumName(PaletteIndex, CollapsorBoxDisabled);
		CaseEnumName(PaletteIndex, CollapsorBoxEnabled);
		CaseEnumName(PaletteIndex, CollapsedRegion);
		CaseEnumName(PaletteIndex, BreakpointBar);
		CaseEnumName(PaletteIndex, LineNoBar);
	case TextEditor::PaletteIndex::Max:
	default:
		break;
	}
	return nullptr;
#undef CaseEnumName
}

const TextEditor::Palette& TextEditor::GetDarkPalette()
{
	const static Palette p = { {
			0xff7f7f7f,	// Default
			0xffd69c56,	// Keyword	
			0xff00ff00,	// Number
			0xff7070e0,	// String
			0xff70a0e0, // Char literal
			0xffffffff, // Punctuation
			0xff989898,	// Preprocessor
			0xffaaaaaa, // Identifier
			0xff9bc64d, // Known identifier
			0xffc040a0, // Preproc identifier
			0xff206020, // Comment (single line)
			0xff406020, // Comment (multi line)
			0xff101010, // Background
			0xffe0e0e0, // Cursor
			0x80a06020, // Selection
			0x40a06020, // SelectionDuplicate
			0x800020ff, // ErrorMarker
			IM_COL32(200,81,89,255), // Breakpoint
			0xFF0000FF, // BreakpointEnabled
			0xff7A7A7A, // LineNumber
			0xffEAEAEA, // LineNumberActive
			0x20000000, // Current line fill
			0x40808080, // Current line fill (inactive)
			0x40a0a0a0, // Current line edge
			0xffaadccf, // Function
			0xff6daa4d, // Class
			0x909cceb0, // EnumVars
			0xFFFEDC9C, // LocalProps,
			0xFFDADADA, // MemberProps
			0xFF9A9A9A, // ParamProps
			0xFF717171, // BracketsTrace
			0xFFE2E2E2, // CollapsorBoxDisabled
			0xFF4444FF, // CollapsorBoxEnabled
			0xFF4444FF, // CollapsedRegion
			0x44A5A5A5, // BreakpointBar
			0x11A5A5A5, // LineNoBar
		} };
	return p;
}

const TextEditor::Palette& TextEditor::GetLightPalette()
{
	const static Palette p = { {
			0xff7f7f7f,	// None
			0xffff0c06,	// Keyword	
			0xff008000,	// Number
			0xff2020a0,	// String
			0xff304070, // Char literal
			0xff000000, // Punctuation
			0xff406060,	// Preprocessor
			0xff404040, // Identifier
			0xff606010, // Known identifier
			0xffc040a0, // Preproc identifier
			0xff205020, // Comment (single line)
			0xff405020, // Comment (multi line)
			0xffffffff, // Background
			0xff000000, // Cursor
			0x80600000, // Selection
			0x40600000, // SelectionDuplicate
			0xa00010ff, // ErrorMarker
			0x80f08000, // Breakpoint
			0xFF0000FF, // BreakpointEnabled
			0xff505000, // LineNumber
			0xff505000, // LineNumberActive
			0x40000000, // Current line fill
			0x40808080, // Current line fill (inactive)
			0x40000000, // Current line edge
			0xffaadccf, // Function
			0xff6daa4d, // Class
			0x909cceb0, // EnumVars
			0xFF9CDCFE, // LocalProps,
			0xFFDADADA, // MemberProps
			0xFF9A9A9A, // ParamProps
			0xFF717171, // BracketsTrace
			0xFFE2E2E2, // CollapsorBoxDisabled
			0xFF4444FF, // CollapsorBoxEnabled
			0xFF4444FF, // CollapsedRegion
			0xA5A5A544, // BreakpointBar
			0xA5A5A511, // LineNoBar
		} };
	return p;
}

const TextEditor::Palette& TextEditor::GetRetroBluePalette()
{
	const static Palette p = { {
			0xff00ffff,	// None
			0xffffff00,	// Keyword	
			0xff00ff00,	// Number
			0xff808000,	// String
			0xff808000, // Char literal
			0xffffffff, // Punctuation
			0xff008000,	// Preprocessor
			0xff00ffff, // Identifier
			0xffffffff, // Known identifier
			0xffff00ff, // Preproc identifier
			0xff808080, // Comment (single line)
			0xff404040, // Comment (multi line)
			0xff800000, // Background
			0xff0080ff, // Cursor
			0x80ffff00, // Selection
			0x40ffff00, // SelectionDuplicate
			0xa00000ff, // ErrorMarker
			0x80ff8000, // Breakpoint
			0xFF0000FF, // BreakpointEnabled
			0xff808000, // LineNumber
			0xff808000, // LineNumberActive
			0x40000000, // Current line fill
			0x40808080, // Current line fill (inactive)
			0x40000000, // Current line edge
			0xffaadccf, // Function
			0xff6daa4d, // Class
			0x909cceb0, // EnumVars
			0xFF9CDCFE, // LocalProps,
			0xFFDADADA, // MemberProps
			0xFF9A9A9A, // ParamProps
			0xFF717171, // BracketsTrace
			0xFFE2E2E2, // CollapsorBoxDisabled
			0xFF4444FF, // CollapsorBoxEnabled
			0xFF4444FF, // CollapsedRegion
			0xA5A5A544, // BreakpointBar
			0xA5A5A511, // LineNoBar
		} };
	return p;
}

void TextEditor::CollapsAll()
{
	static auto InvalidCoord = Coordinates(-1, -1);
	for (auto& cb : mCollapsables)
	{
		if (cb.mEnd != InvalidCoord)
		{
			cb.mIsFoldEnabled = true;
			mActiveCollapsables.emplace(std::pair<int, int>(cb.mStart.mLine, cb.mEnd.mLine));
		}
	}
	UpdateIgnoreCollapsables(true);
}

void TextEditor::ExpandAll()
{
	for (auto& cb : mCollapsables)		
		cb.mIsFoldEnabled = false;

	mActiveCollapsables.clear();
	mIgnoredActiveCollapsables.clear();
}

void TextEditor::AddCollapsedLine(int aStartLine, int aEndLine)
{
	std::pair<int, int> val = std::pair<int, int>(aStartLine, aEndLine);
	mActiveCollapsables.emplace(val);
	UpdateIgnoreCollapsables(true);
}

void TextEditor::RemoveCollapsedLine(int aStartLine)
{
	CollapsedLines::iterator iterator = mActiveCollapsables.find(aStartLine);
	if (iterator != mActiveCollapsables.end())
		mActiveCollapsables.erase(iterator);
	UpdateIgnoreCollapsables(false);
}

void TextEditor::UpdateIgnoreCollapsables(bool bShouldAdd)
{
	for (auto& item : mActiveCollapsables)
	{
		if (!bShouldAdd)
		{
			CollapsedLinesUn::iterator iterator = mIgnoredActiveCollapsables.find(item.first);
			if (iterator != mIgnoredActiveCollapsables.end() && !DoesActiveCollapsableHasAnyParent((std::pair<int, int>&)item))
				mIgnoredActiveCollapsables.erase(iterator);
		}
		else if (DoesActiveCollapsableHasAnyParent((std::pair<int, int>&)item))
		{
			mIgnoredActiveCollapsables.emplace(item);
		}
	}
}

bool TextEditor::DoesActiveCollapsableHasAnyParent(std::pair<int, int>& aActiveCollapsable)
{
	for (auto& item : mActiveCollapsables)
	{
		if (aActiveCollapsable.second < item.second && aActiveCollapsable.first > item.first)
			return true;
	}
	return false;
}

bool TextEditor::IsWithinCollapsedRange(int aLineNo, int* aStartLine, int* aEndLine)
{
	for (auto& pair : mActiveCollapsables)
	{
		if (aLineNo >= pair.first && aLineNo <= pair.second)
		{
			if (!!aStartLine)
				*aStartLine = pair.first;
			if (!!aEndLine)
				*aEndLine = pair.second;
			return true;
		}
	}
	return false;
}

bool TextEditor::CalculateCurrentRealLine(int& aLineIndex, int* aPreviousExtraAdd, bool aIncludeCurrentOne) const
{
	//Returns the estimated line based on aLineIndex from the collapsedlines
	bool bFound = false;
	int pline = aLineIndex;
	for (auto& pair : mActiveCollapsables)
	{
		if (mIgnoredActiveCollapsables.find(pair.first) != mIgnoredActiveCollapsables.end())
			continue;
		if (aLineIndex > pair.first || (aIncludeCurrentOne && aLineIndex >= pair.first))
		{
			bFound = true;
			int toAdd = (pair.second - pair.first);
			if (!!aPreviousExtraAdd && pline > pair.second) //if this line is past the previous collapsed area
				*aPreviousExtraAdd += toAdd;
			aLineIndex += toAdd;
		}
	}
	return bFound;
}

bool TextEditor::CalculateNextCollapsStartEnd(int* aStartLinePtr, int* aEndLinePtr, bool& aIsStartOfACollapsable) 
{
	assert(!!aStartLinePtr, "Error: StartLine Cannot be null pointer");
	assert(!!aEndLinePtr, "Error: EndLine Cannot be null pointer");
	int startsub = 0, endsub = 0;
	if (mIgnoredActiveCollapsables.find(*aStartLinePtr) != mIgnoredActiveCollapsables.end())
		return false;
	for (auto& pair : mActiveCollapsables)
	{
		if (mIgnoredActiveCollapsables.find(pair.first) != mIgnoredActiveCollapsables.end())
			continue;
		if (*aStartLinePtr == pair.first && *aEndLinePtr == pair.second)
			aIsStartOfACollapsable = true;
		if (*aStartLinePtr > pair.first)
		{
			if (*aEndLinePtr < pair.second) //that is: within a collapsed region
				return false;
			int toAdd = (pair.second - pair.first);
			startsub += toAdd;
			endsub += toAdd;
		}
		else if (*aEndLinePtr > pair.second)
			endsub += (pair.second - pair.first);
	}
	*aStartLinePtr -= startsub;
	*aEndLinePtr -= endsub;
	return *aStartLinePtr >= 0 && *aEndLinePtr >= 0;
}

_stdstr TextEditor::GetText() const
{
	return GetText(Coordinates(), Coordinates((int)mLines.size(), 0));
}

std::vector<_stdstr> TextEditor::GetTextLines() const
{
	std::vector<_stdstr> result;

	result.reserve(mLines.size());

	for (auto& line : mLines)
	{
		_stdstr text;

		text.resize(line.size());

		for (size_t i = 0; i < line.size(); ++i)
			text[i] = line[i].mChar;

		result.emplace_back(std::move(text));
	}

	return result;
}

_stdstr TextEditor::GetSelectedText() const
{
	return GetText(mState.mSelectionStart, mState.mSelectionEnd);
}

_stdstr TextEditor::GetCurrentLineText()const
{
	auto lineLength = GetLineMaxColumn(mState.mCursorPosition.mLine);
	return GetText(
		Coordinates(mState.mCursorPosition.mLine, 0),
		Coordinates(mState.mCursorPosition.mLine, lineLength));
}

_stdstr TextEditor::GetLineText(int aLineNo) const
{
	if (aLineNo < mLines.size())
	{
		return GetText(
			Coordinates(aLineNo, 0),
			Coordinates(aLineNo, GetLineMaxColumn(aLineNo)));
	}
	return mystr("");
}

bool TextEditor::DoesTextMatch(_stdstr& aText, int aLine, int aColumn) const
{
	if (aLine >= mLines.size() || aText.size() == 0)
		return false;
	if (aLine >= mLines.size())
		return false;
	const auto& line = mLines[aLine];
	if ((aColumn + aText.size()) >= line.size())
		return false;
	for (int i = 0; i < aText.size(); i++)
	{
		if (aText[i] != line[aColumn + i].mChar)
			return false;
	}
	return true;
}


void TextEditor::ProcessInputs()
{
}

void TextEditor::Colorize(int aFromLine, int aLines)
{
	int toLine = aLines == -1 ? (int)mLines.size() : std::min((int)mLines.size(), aFromLine + aLines);
	mColorRangeMin = std::min(mColorRangeMin, aFromLine);
	mColorRangeMax = std::max(mColorRangeMax, toLine);
	mColorRangeMin = std::max(0, mColorRangeMin);
	mColorRangeMax = std::max(mColorRangeMin, mColorRangeMax);
	mCheckComments = true;
}

bool TextEditor::BufferHasCharacter(_constcharptr input, _constcharptr endinput, _constcharptr tomatch, bool bSkipWhiteSpaces, bool bAlsoSkipChars)
{
	if (!tomatch)
		return false;
	_constcharptr p = input;
	if (bSkipWhiteSpaces)
		while (p < endinput && *p == mystr(' ') || *p == mystr('\t'))
			p++;
	if (bAlsoSkipChars)
		while (p < endinput && (BufferIsAChar(p) || *p == mystr(' ') || *p == mystr('\t')))
			p++;
	return (p < endinput && _strcmp(tomatch, p) == 0);
}

bool TextEditor::BufferHadCharacter(_constcharptr input, _constcharptr startpoint, _constcharptr tomatch, bool bSkipWhiteSpaces, bool bAlsoSkipChars)
{
	if (!tomatch)
		return false;
	_constcharptr p = input;
	if (bSkipWhiteSpaces)
		while (p >= startpoint && *p == mystr(' ') || *p == mystr('\t'))
			p--;
	if (bAlsoSkipChars)
		while (p >= startpoint && BufferIsAChar(p))
			p--;
	p -= _stdstr(tomatch).size();
	return (p >= startpoint && *tomatch == *p);
}

void TextEditor::ColorizeRange(int aFromLine, int aToLine)
{
	if (mLines.empty() || aFromLine >= aToLine)
		return;

	_stdstr buffer;
	_cmatch results;
	_stdstr id;

	int endLine = std::max(0, std::min((int)mLines.size(), aToLine));
	for (int i = aFromLine; i < endLine; ++i)
	{
		auto& line = mLines[i];

		if (line.empty())
			continue;

		buffer.resize(line.size());
		for (size_t j = 0; j < line.size(); ++j)
		{
			auto& col = line[j];
			buffer[j] = col.mChar;
			col.mColorIndex = PaletteIndex::Default;
		}

		_constcharptr bufferBegin = &buffer.front();
		_constcharptr bufferEnd = bufferBegin + buffer.size();

		auto last = bufferEnd;

		for (auto first = bufferBegin; first != last; )
		{
			_constcharptr token_begin = nullptr;
			_constcharptr token_end = nullptr;
			PaletteIndex token_color = PaletteIndex::Default;

			bool hasTokenizeResult = false;

			if (mLanguageDefinition.mTokenize != nullptr)
			{
				if (mLanguageDefinition.mTokenize(first, last, token_begin, token_end, token_color))
					hasTokenizeResult = true;
			}

			if (hasTokenizeResult == false)
			{
				// todo : remove
				//printf("using regex for %.*s\n", first + 10 < last ? 10 : int(last - first), first);

				for (auto& p : mRegexList)
				{
					if (std::regex_search(first, last, results, p.first, std::regex_constants::match_continuous))
					{
						hasTokenizeResult = true;

						auto& v = *results.begin();
						token_begin = v.first;
						token_end = v.second;
						token_color = p.second;
						break;
					}
				}
			}

			if (hasTokenizeResult == false)
			{
				first++;
			}
			else
			{
				const size_t token_length = token_end - token_begin;

				if (token_color == PaletteIndex::Identifier)
				{
					id.assign(token_begin, token_end);

					// todo : allmost all language definitions use lower case to specify keywords, so shouldn't this use ::tolower ?
					if (!mLanguageDefinition.mCaseSensitive)
						std::transform(id.begin(), id.end(), id.begin(), ::toupper);

					if (!line[first - bufferBegin].mPreprocessor)
					{
						if (mLanguageDefinition.mKeywords.count(id) != 0)
							token_color = PaletteIndex::Keyword;
						else if (mLanguageDefinition.mIdentifiers.count(id) != 0)
							token_color = PaletteIndex::KnownIdentifier;
						else if (mLanguageDefinition.mPreprocIdentifiers.count(id) != 0)
							token_color = PaletteIndex::PreprocIdentifier;
						else if (mLanguageDefinition.mClasses.count(id) != 0)
							token_color = PaletteIndex::Class;
						/*else if (mLanguageDefinition.mEnums.count(id) != 0)
							token_color = PaletteIndex::Class;
						else if (mLanguageDefinition.mStructs.count(id) != 0)
							token_color = PaletteIndex::Class;*/
						else if (mLanguageDefinition.mFunctions.count(id) != 0)
							token_color = PaletteIndex::Function;
						else if (mLanguageDefinition.mParameterProperties.count(id) != 0)
							token_color = PaletteIndex::ParamProps;
						else if (mLanguageDefinition.mLocalProperties.count(id) != 0)
							token_color = PaletteIndex::LocalProps;
						else if (mLanguageDefinition.mMemberProperties.count(id) != 0)
							token_color = PaletteIndex::MemberProps;
						else if (token_end < last)
						{
							bool bIsChar = BufferIsACStyleIdentifier(token_begin, token_end);
							if (mLanguageDefinition.mFunctionBegin.size() > 0 && BufferHasCharacter(token_end, last, mLanguageDefinition.mFunctionBegin.c_str(), true, bIsChar))
								token_color = PaletteIndex::Function;
							else if (mLanguageDefinition.mStaticClassBegin.size() > 0 && 
								(BufferHasCharacter(token_end, last, mLanguageDefinition.mStaticClassBegin.c_str(), true, bIsChar) /*||
								BufferHasCharacter(token_end, last, mystr("*"), true, bIsChar) ||
								BufferHasCharacter(token_end, last, mystr("&"), true, bIsChar)*/
									)
								)
								token_color = PaletteIndex::Class;
							else if (BufferHadCharacter(first, bufferBegin, mystr("::"), true, bIsChar))
								token_color = PaletteIndex::EnumFields;

						}		
					}
					else
					{
						if (first > bufferBegin && first[-1] == mystr('#'))
						{
							token_color = PaletteIndex::Preprocessor;
						}
						else
						{
							bool bHasAnyNonPreProc = false;
							for (_constcharptr p = first; p > bufferBegin; p--)
							{
								auto& glyph = line[p - bufferBegin];
								if (glyph.mColorIndex != PaletteIndex::Default && glyph.mColorIndex != PaletteIndex::Preprocessor)
								{
									bHasAnyNonPreProc = true;
									break;
								}
							}
							if (!bHasAnyNonPreProc)
							{
								//Clear all preprocs at current line:
								std::vector<_stdstr> idsToRemove;
								for (auto& val : mLanguageDefinition.mPreprocIdentifiers)
								{
									if (val.second.mLocation.mLine == i)
										idsToRemove.push_back(val.first);
								}
								for (auto& val : idsToRemove)
									mLanguageDefinition.mPreprocIdentifiers.erase(val);
								Identifier idf;
								idf.mDeclaration = mystr("Preproc");
								idf.mLocation = Coordinates(i, 0);
								mLanguageDefinition.mPreprocIdentifiers.emplace(id, idf);
							}
						}

						if (mLanguageDefinition.mPreprocIdentifiers.count(id) != 0)
							token_color = PaletteIndex::PreprocIdentifier;
					}
				}

				for (size_t j = 0; j < token_length; ++j)
					line[(token_begin - bufferBegin) + j].mColorIndex = token_color;

				first = token_end;
			}
		}
	}
}

void TextEditor::ColorizeInternal()
{
	if (mLines.empty() || !mColorizerEnabled)
		return;
	static std::mutex colorizerMutex;
	static std::vector<std::jthread> colorizerFutures;
	auto asyncColorize = [](TextEditor* teditor)
	{
		std::lock_guard<std::mutex> lock(colorizerMutex);
		try
		{
			if (teditor->mCheckComments)
			{
				auto endLine = teditor->mLines.size();
				auto endIndex = 0;
				auto commentStartLine = endLine;
				auto commentStartIndex = endIndex;
				auto withinString = false;
				auto withinSingleLineComment = false;
				auto withinPreproc = false;
				auto firstChar = true;			// there is no other non-whitespace characters in the line before
				auto concatenate = false;		// '\' on the very end of the line
				auto currentLine = 0;
				auto currentIndex = 0;
				while (currentLine < endLine || currentIndex < endIndex)
				{
					auto& line = teditor->mLines[currentLine];

					if (currentIndex == 0 && !concatenate)
					{
						withinSingleLineComment = false;
						withinPreproc = false;
						firstChar = true;
					}

					concatenate = false;

					if (!line.empty() && currentIndex < line.size())
					{
						auto& g = line[currentIndex];
						auto c = g.mChar;

						if (c != teditor->mLanguageDefinition.mPreprocChar && !_isspace_l(c, _get_current_locale()))
							firstChar = false;

						if (currentIndex == (int)line.size() - 1 && line[line.size() - 1].mChar == '\\')
							concatenate = true;

						bool inComment = (commentStartLine < currentLine || (commentStartLine == currentLine && commentStartIndex <= currentIndex));

						if (withinString)
						{
							line[currentIndex].mMultiLineComment = inComment;

							if (c == '\"')
							{
								if (currentIndex + 1 < (int)line.size() && line[currentIndex + 1].mChar == '\"')
								{
									currentIndex += 1;
									if (currentIndex < (int)line.size())
										line[currentIndex].mMultiLineComment = inComment;
								}
								else
									withinString = false;
							}
							else if (c == '\\')
							{
								//currentIndex += 1;
								if (currentIndex + 1 < (int)line.size())
									line[currentIndex].mMultiLineComment = inComment;
							}
						}
						else
						{
							if (firstChar && c == teditor->mLanguageDefinition.mPreprocChar)
								withinPreproc = true;

							if (c == '\"')
							{
								withinString = true;
								line[currentIndex].mMultiLineComment = inComment;
							}
							else
							{
								auto pred = [](_constchar& a, const Glyph& b) { return a == b.mChar; };
								auto from = line.begin() + currentIndex;
								auto& startStr = teditor->mLanguageDefinition.mCommentStart;
								auto& singleStartStr = teditor->mLanguageDefinition.mSingleLineComment;

								if (singleStartStr.size() > 0 &&
									currentIndex + singleStartStr.size() <= line.size() &&
									equals(singleStartStr.begin(), singleStartStr.end(), from, from + singleStartStr.size(), pred))
								{
									withinSingleLineComment = true;
								}
								else if (!withinSingleLineComment && currentIndex + startStr.size() <= line.size() &&
									equals(startStr.begin(), startStr.end(), from, from + startStr.size(), pred))
								{
									commentStartLine = currentLine;
									commentStartIndex = currentIndex;
								}

								inComment = inComment = (commentStartLine < currentLine || (commentStartLine == currentLine && commentStartIndex <= currentIndex));

								line[currentIndex].mMultiLineComment = inComment;
								line[currentIndex].mComment = withinSingleLineComment;

								auto& endStr = teditor->mLanguageDefinition.mCommentEnd;
								if (currentIndex + 1 >= (int)endStr.size() &&
									equals(endStr.begin(), endStr.end(), from + 1 - endStr.size(), from + 1, pred))
								{
									commentStartIndex = endIndex;
									commentStartLine = endLine;
								}
							}
						}
						line[currentIndex].mPreprocessor = withinPreproc;
						currentIndex += UTF8CharLength(c);
						if (currentIndex >= (int)line.size())
						{
							currentIndex = 0;
							++currentLine;
						}
					}
					else
					{
						currentIndex = 0;
						++currentLine;
					}
				}
				teditor->mCheckComments = false;
			}

			if (teditor->mColorRangeMin < teditor->mColorRangeMax)
			{
				const int increment = (teditor->mLanguageDefinition.mTokenize == nullptr) ? 10 : 10000;
				const int to = std::min(teditor->mColorRangeMin + increment, teditor->mColorRangeMax);
				teditor->ColorizeRange(teditor->mColorRangeMin, to);
				teditor->mColorRangeMin = to;

				if (teditor->mColorRangeMax == teditor->mColorRangeMin)
				{
					teditor->mColorRangeMin = std::numeric_limits<int>::max();
					teditor->mColorRangeMax = 0;
				}
				return;
			}
		}
		catch(...)
		{
		}
	};
	if (mCheckComments || mColorRangeMin < mColorRangeMax)
	{
		for (auto& future : colorizerFutures)
		{
			if (!future.joinable())
				return;
		}
		colorizerFutures.clear();
		colorizerFutures.push_back(std::jthread( asyncColorize, this ) );
	}
}

float TextEditor::TextDistanceToLineStart(const Coordinates& aFrom) const
{
	auto& line = mLines[aFrom.mLine];
	float distance = 0.0f;
	float spaceSize = ImGui::GetFont()->CalcTextSizeA(ImGui::GetFontSize(), FLT_MAX, -1.0f, " ", nullptr, nullptr).x;
	int colIndex = GetCharacterIndex(aFrom);
	for (size_t it = 0u; it < line.size() && it < colIndex; )
	{
		if (line[it].mChar == '\t')
		{
			distance = (1.0f + std::floor((1.0f + distance) / (float(mTabSize) * spaceSize))) * (float(mTabSize) * spaceSize);
			++it;
		}
		else
		{
			auto d = UTF8CharLength(line[it].mChar);
			char tempCString[7];
			int i = 0;
			for (; i < 6 && d-- > 0 && it < (int)line.size(); i++, it++)
				tempCString[i] = line[it].mChar;

			tempCString[i] = '\0';
			distance += ImGui::GetFont()->CalcTextSizeA(ImGui::GetFontSize(), FLT_MAX, -1.0f, tempCString, nullptr, nullptr).x;
		}
	}

	return distance;
}

void TextEditor::EnsureCursorVisible()
{
	if (!mWithinRender)
	{
		mScrollToCursor = true;
		return;
	}

	float scrollX = ImGui::GetScrollX();
	float scrollY = ImGui::GetScrollY();

	auto height = ImGui::GetWindowHeight();
	auto width = ImGui::GetWindowWidth();

	auto top = 1 + (int)ceil(scrollY / mCharAdvance.y);
	auto bottom = (int)ceil((scrollY + height) / mCharAdvance.y);

	auto left = (int)ceil(scrollX / mCharAdvance.x);
	auto right = (int)ceil((scrollX + width) / mCharAdvance.x);

	auto pos = GetActualCursorCoordinates();
	pos = GetCollapsedCursordCoordinates(pos);
	auto len = TextDistanceToLineStart(pos);

	if (pos.mLine < top)
		ImGui::SetScrollY(std::max(0.0f, (pos.mLine - 1) * mCharAdvance.y));
	if (pos.mLine > bottom - 4)
		ImGui::SetScrollY(std::max(0.0f, (pos.mLine + 4) * mCharAdvance.y - height));
	if (len + mTextStart < left + 4)
		ImGui::SetScrollX(std::max(0.0f, len + mTextStart - 4));
	if (len + mTextStart > right - 4)
		ImGui::SetScrollX(std::max(0.0f, len + mTextStart + 4 - width));
}

int TextEditor::GetPageSize() const
{
	auto height = ImGui::GetWindowHeight() - 20.0f;
	return (int)floor(height / mCharAdvance.y);
}

TextEditor::UndoRecord::UndoRecord(
	const _stdstr& aAdded,
	const TextEditor::Coordinates aAddedStart,
	const TextEditor::Coordinates aAddedEnd,
	const _stdstr& aRemoved,
	const TextEditor::Coordinates aRemovedStart,
	const TextEditor::Coordinates aRemovedEnd,
	TextEditor::EditorState& aBefore,
	TextEditor::EditorState& aAfter)
	: mAdded(aAdded)
	, mAddedStart(aAddedStart)
	, mAddedEnd(aAddedEnd)
	, mRemoved(aRemoved)
	, mRemovedStart(aRemovedStart)
	, mRemovedEnd(aRemovedEnd)
	, mBefore(aBefore)
	, mAfter(aAfter)
{
	assert(mAddedStart <= mAddedEnd);
	assert(mRemovedStart <= mRemovedEnd);
}

void TextEditor::UndoRecord::Undo(TextEditor* aEditor)
{
	if (!mAdded.empty())
	{
		aEditor->DeleteRange(mAddedStart, mAddedEnd);
		aEditor->Colorize(mAddedStart.mLine - 1, mAddedEnd.mLine - mAddedStart.mLine + 2);
	}

	if (!mRemoved.empty())
	{
		auto start = mRemovedStart;
		aEditor->InsertTextAt(start, mRemoved.c_str());
		aEditor->Colorize(mRemovedStart.mLine - 1, mRemovedEnd.mLine - mRemovedStart.mLine + 2);
	}

	aEditor->mState = mBefore;
	aEditor->EnsureCursorVisible();

}

void TextEditor::UndoRecord::Redo(TextEditor* aEditor)
{
	if (!mRemoved.empty())
	{
		aEditor->DeleteRange(mRemovedStart, mRemovedEnd);
		aEditor->Colorize(mRemovedStart.mLine - 1, mRemovedEnd.mLine - mRemovedStart.mLine + 1);
	}

	if (!mAdded.empty())
	{
		auto start = mAddedStart;
		aEditor->InsertTextAt(start, mAdded.c_str());
		aEditor->Colorize(mAddedStart.mLine - 1, mAddedEnd.mLine - mAddedStart.mLine + 1);
	}

	aEditor->mState = mAfter;
	aEditor->EnsureCursorVisible();
}

static bool TokenizeCStyleString(_constcharptr in_begin, _constcharptr in_end, _constcharptr& out_begin, _constcharptr& out_end)
{
	_constcharptr p = in_begin;

	if (*p == '"' || (*p++ == mystr('L') && *p == '"'))
	{
		p++;

		while (p < in_end)
		{
			// handle end of string
			if (*p == '"')
			{
				out_begin = in_begin;
				out_end = p + 1;
				return true;
			}

			// handle escape character for "
			if (*p == '\\' && p + 1 < in_end && p[1] == '"')
				p++;

			p++;
		}
	}

	return false;
}

static bool TokenizeCStyleCharacterLiteral(_constcharptr in_begin, _constcharptr in_end, _constcharptr& out_begin, _constcharptr& out_end)
{
	_constcharptr p = in_begin;

	if (*p == '\'')
	{
		p++;

		// handle escape characters
		if (p < in_end && *p == '\\')
			p++;

		if (p < in_end)
			p++;

		// handle end of character literal
		if (p < in_end && *p == '\'')
		{
			out_begin = in_begin;
			out_end = p + 1;
			return true;
		}
	}

	return false;
}

static bool TokenizeCStyleIdentifier(_constcharptr in_begin, _constcharptr in_end, _constcharptr& out_begin, _constcharptr& out_end)
{
	_constcharptr p = in_begin;

	if ((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z') || *p == '_')
	{
		p++;

		while ((p < in_end) && ((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z') || (*p >= '0' && *p <= '9') || *p == '_'))
			p++;

		out_begin = in_begin;
		out_end = p;
		return true;
	}

	return false;
}

static bool TokenizeCStyleNumber(_constcharptr in_begin, _constcharptr in_end, _constcharptr& out_begin, _constcharptr& out_end)
{
	_constcharptr p = in_begin;

	const bool startsWithNumber = *p >= '0' && *p <= '9';

	if (*p != '+' && *p != '-' && !startsWithNumber)
		return false;

	p++;

	bool hasNumber = startsWithNumber;

	while (p < in_end && (*p >= '0' && *p <= '9'))
	{
		hasNumber = true;

		p++;
	}

	if (hasNumber == false)
		return false;

	bool isFloat = false;
	bool isHex = false;
	bool isBinary = false;

	if (p < in_end)
	{
		if (*p == '.')
		{
			isFloat = true;

			p++;

			while (p < in_end && (*p >= '0' && *p <= '9'))
				p++;
		}
		else if (*p == 'x' || *p == 'X')
		{
			// hex formatted integer of the type 0xef80

			isHex = true;

			p++;

			while (p < in_end && ((*p >= '0' && *p <= '9') || (*p >= 'a' && *p <= 'f') || (*p >= 'A' && *p <= 'F')))
				p++;
		}
		else if (*p == 'b' || *p == 'B')
		{
			// binary formatted integer of the type 0b01011101

			isBinary = true;

			p++;

			while (p < in_end && (*p >= '0' && *p <= '1'))
				p++;
		}
	}

	if (isHex == false && isBinary == false)
	{
		// floating point exponent
		if (p < in_end && (*p == 'e' || *p == 'E'))
		{
			isFloat = true;

			p++;

			if (p < in_end && (*p == '+' || *p == '-'))
				p++;

			bool hasDigits = false;

			while (p < in_end && (*p >= '0' && *p <= '9'))
			{
				hasDigits = true;

				p++;
			}

			if (hasDigits == false)
				return false;
		}

		// single precision floating point type
		if (p < in_end && *p == 'f')
			p++;
	}

	if (isFloat == false)
	{
		// integer size type
		while (p < in_end && (*p == 'u' || *p == 'U' || *p == 'l' || *p == 'L'))
			p++;
	}

	out_begin = in_begin;
	out_end = p;
	return true;
}

static bool TokenizeCStylePunctuation(_constcharptr in_begin, _constcharptr in_end, _constcharptr& out_begin, _constcharptr& out_end)
{
	(void)in_end;

	switch (*in_begin)
	{
	case '[':
	case ']':
	case '{':
	case '}':
	case '!':
	case '%':
	case '^':
	case '&':
	case '*':
	case '(':
	case ')':
	case '-':
	case '+':
	case '=':
	case '~':
	case '|':
	case '<':
	case '>':
	case '?':
	case ':':
	case '/':
	case ';':
	case ',':
	case '.':
		out_begin = in_begin;
		out_end = in_begin + 1;
		return true;
	}

	return false;
}

const void TextEditor::LanguageDefinition::AddIdentifier(const _stdstr& key, const Identifier& idf, IdentifierType idft)
{
	if (mKeywords.find(key) != mKeywords.end()) //if identifier is a keyword, exit
		return;
	switch (idft)
	{
	case TextEditor::Preprocessor:
		mPreprocIdentifiers[key] = idf;
		break;
	case TextEditor::Class:
		mClasses[key] = idf;
		break;
	case TextEditor::Function:
		mFunctions[key] = idf;
		break;
	case TextEditor::Struct:
		mStructs[key] = idf;
		break;
	case TextEditor::ParameterProperty:
		mParameterProperties[key][idf.mAssociatedData] = idf;
		break;
	case TextEditor::LocalProperty:
		mLocalProperties[key][idf.mAssociatedData] = idf;
		break;
	case TextEditor::MemberProperty:
		mMemberProperties[key][idf.mAssociatedData] = idf;
		break;
	default:
		break;
	}
}

bool TextEditor::LanguageDefinition::HasIdentifier(const _stdstr& key, IdentifierType idft)
{
	if (idft != IdentifierType::Any)
	{
		switch (idft)
		{
		case TextEditor::General:
			return mIdentifiers.find(key) != mIdentifiers.end();
		case TextEditor::Preprocessor:
			return mPreprocIdentifiers.find(key) != mPreprocIdentifiers.end();
		case TextEditor::Class:
			return mClasses.find(key) != mClasses.end();
		case TextEditor::Function:
			return mFunctions.find(key) != mFunctions.end();
		/*case TextEditor::Struct:
			return mPreprocIdentifiers.find(key) != mPreprocIdentifiers.end();*/
		case TextEditor::ParameterProperty:
			return mParameterProperties.find(key) != mParameterProperties.end();
		case TextEditor::LocalProperty:
			return mLocalProperties.find(key) != mLocalProperties.end();
		case TextEditor::MemberProperty:
			return mMemberProperties.find(key) != mMemberProperties.end();
		default:
			break;
		}
	}
	return /*mIdentifiers.find(key) != mIdentifiers.end()
		||*/ mClasses.find(key) != mClasses.end()
		|| mFunctions.find(key) != mFunctions.end()
		|| mEnums.find(key) != mEnums.end()
		|| mStructs.find(key) != mStructs.end();
}
bool TextEditor::LanguageDefinition::HasMultiIdentifier(const _stdstr& key, IdentifierType idft)
{
	return
		mParameterProperties.find(key) != mParameterProperties.end()
		|| mLocalProperties.find(key) != mLocalProperties.end()
		|| mMemberProperties.find(key) != mMemberProperties.end();
}
const TextEditor::LanguageDefinition& TextEditor::LanguageDefinition::CPlusPlus()
{
	static bool inited = false;
	static LanguageDefinition langDef;
	if (!inited)
	{
		static _constcharptr const cppKeywords[] = {
			mystr("alignas"), mystr("alignof"), mystr("and"), mystr("and_eq"), mystr("asm"), mystr("atomic_cancel"), mystr("atomic_commit"), mystr("atomic_noexcept"), mystr("auto"), mystr("bitand"), mystr("bitor"), mystr("bool"), mystr("break"), mystr("case"), mystr("catch"), mystr("char"), mystr("char16_t"), mystr("char32_t"), mystr("class"),
			mystr("compl"), mystr("concept"), mystr("const"), mystr("constexpr"), mystr("const_cast"), mystr("continue"), mystr("decltype"), mystr("default"), mystr("delete"), mystr("do"), mystr("double"), mystr("dynamic_cast"), mystr("else"), mystr("enum"), mystr("explicit"), mystr("export"), mystr("extern"), mystr("false"), mystr("float"),
			mystr("for"), mystr("friend"), mystr("goto"), mystr("if"), mystr("import"), mystr("inline"), mystr("int"), mystr("long"), mystr("module"), mystr("mutable"), mystr("namespace"), mystr("new"), mystr("noexcept"), mystr("not"), mystr("not_eq"), mystr("nullptr"), mystr("operator"), mystr("or"), mystr("or_eq"), mystr("private"), mystr("protected"), mystr("public"),
			mystr("register"), mystr("reinterpret_cast"), mystr("requires"), mystr("return"), mystr("short"), mystr("signed"), mystr("sizeof"), mystr("static"), mystr("static_assert"), mystr("static_cast"), mystr("struct"), mystr("switch"), mystr("synchronized"), mystr("template"), mystr("this"), mystr("thread_local"),
			mystr("throw"), mystr("true"), mystr("try"), mystr("typedef"), mystr("typeid"), mystr("typename"), mystr("union"), mystr("unsigned"), mystr("using"), mystr("virtual"), mystr("void"), mystr("volatile"), mystr("wchar_t"), mystr("while"), mystr("xor"), mystr("xor_eq"), mystr("interface")
		};
		for (auto& k : cppKeywords)
			langDef.mKeywords.insert(k);

		static _constcharptr const identifiers[] = {
			mystr("abort"), mystr("abs"), mystr("acos"), mystr("asin"), mystr("atan"), mystr("atexit"), mystr("atof"), mystr("atoi"), mystr("atol"), mystr("ceil"), mystr("clock"), mystr("cosh"), mystr("ctime"), mystr("div"), mystr("exit"), mystr("fabs"), mystr("floor"), mystr("fmod"), mystr("getchar"), mystr("getenv"), mystr("isalnum"), mystr("isalpha"), mystr("isdigit"), mystr("isgraph"),
			mystr("ispunct"), mystr("isspace"), mystr("isupper"), mystr("kbhit"), mystr("log10"), mystr("log2"), mystr("log"), mystr("memcmp"), mystr("modf"), mystr("pow"), mystr("printf"), mystr("sprintf"), mystr("snprintf"), mystr("putchar"), mystr("putenv"), mystr("puts"), mystr("rand"), mystr("remove"), mystr("rename"), mystr("sinh"), mystr("sqrt"), mystr("srand"), mystr("strcat"), mystr("strcmp"), mystr("strerror"), mystr("time"), mystr("tolower"), mystr("toupper"),
			mystr("std"), mystr("string"), mystr("vector"), mystr("map"), mystr("unordered_map"), mystr("set"), mystr("unordered_set"), mystr("min"), mystr("max")
		};
		for (auto& k : identifiers)
		{
			Identifier id;
			id.mDeclaration = mystr("Built-in function");
			langDef.mIdentifiers.insert(std::make_pair(_stdstr(k), id));
		}

		langDef.mTokenize = [](_constcharptr in_begin, _constcharptr in_end, _constcharptr& out_begin, _constcharptr& out_end, PaletteIndex& paletteIndex) -> bool
			{
				paletteIndex = PaletteIndex::Max;

				while (in_begin < in_end && isascii(*in_begin) && _isblank_l(*in_begin, _get_current_locale()))
					in_begin++;

				if (in_begin == in_end)
				{
					out_begin = in_end;
					out_end = in_end;
					paletteIndex = PaletteIndex::Default;
				}
				else if (TokenizeCStyleString(in_begin, in_end, out_begin, out_end))
					paletteIndex = PaletteIndex::String;
				else if (TokenizeCStyleCharacterLiteral(in_begin, in_end, out_begin, out_end))
					paletteIndex = PaletteIndex::CharLiteral;
				else if (TokenizeCStyleIdentifier(in_begin, in_end, out_begin, out_end))
					paletteIndex = PaletteIndex::Identifier;
				else if (TokenizeCStyleNumber(in_begin, in_end, out_begin, out_end))
					paletteIndex = PaletteIndex::Number;
				else if (TokenizeCStylePunctuation(in_begin, in_end, out_begin, out_end))
					paletteIndex = PaletteIndex::Punctuation;

				return paletteIndex != PaletteIndex::Max;
			};

		langDef.mCommentStart = mystr("/*");
		langDef.mCommentEnd = mystr("*/");
		langDef.mSingleLineComment = mystr("//");
		langDef.mFunctionBegin = mystr("(");
		langDef.mStaticClassBegin = mystr("::");

		langDef.mCaseSensitive = true;
		langDef.mAutoIndentation = true;

		langDef.mName = mystr("C++");

		langDef.mBracketsOnOff = { {mystr('{'), mystr('}')}, /*{mystr('['), mystr(']')}, {mystr('('), mystr(')')},*/ };
		langDef.mBracketsOffOn = { {mystr('}'), mystr('{')}, /*{mystr(']'), mystr('[')}, {mystr(')'), mystr('(')},*/ };
		langDef.mCollapsablesOnOff = { {mystr("{"), mystr("}")}, /*{mystr("["), mystr("]")}, {mystr("("), mystr(")")},*/ {mystr("/*"), mystr("*/")} };
		langDef.mCollapsablesOffOn = { {mystr("}"), mystr("{")}, /*{mystr("]"), mystr("[")}, {mystr(")"), mystr("(")},*/ {mystr("*/"), mystr("/*")} };

		inited = true;
	}
	return langDef;
}

const TextEditor::LanguageDefinition& TextEditor::LanguageDefinition::HLSL()
{
	static bool inited = false;
	static LanguageDefinition langDef;
	/*if (!inited)
	{
		static _constcharptr const keywords[] = {
			"AppendStructuredBuffer", "asm", "asm_fragment", "BlendState", "bool", "break", "Buffer", "ByteAddressBuffer", "case", "cbuffer", "centroid", "class", "column_major", "compile", "compile_fragment",
			"CompileShader", "const", "continue", "ComputeShader", "ConsumeStructuredBuffer", "default", "DepthStencilState", "DepthStencilView", "discard", "do", "double", "DomainShader", "dword", "else",
			"export", "extern", "false", "float", "for", "fxgroup", "GeometryShader", "groupshared", "half", "Hullshader", "if", "in", "inline", "inout", "InputPatch", "int", "interface", "line", "lineadj",
			"linear", "LineStream", "matrix", "min16float", "min10float", "min16int", "min12int", "min16uint", "namespace", "nointerpolation", "noperspective", "NULL", "out", "OutputPatch", "packoffset",
			"pass", "pixelfragment", "PixelShader", "point", "PointStream", "precise", "RasterizerState", "RenderTargetView", "return", "register", "row_major", "RWBuffer", "RWByteAddressBuffer", "RWStructuredBuffer",
			"RWTexture1D", "RWTexture1DArray", "RWTexture2D", "RWTexture2DArray", "RWTexture3D", "sample", "sampler", "SamplerState", "SamplerComparisonState", "shared", "snorm", "stateblock", "stateblock_state",
			"static", "string", "struct", "switch", "StructuredBuffer", "tbuffer", "technique", "technique10", "technique11", "texture", "Texture1D", "Texture1DArray", "Texture2D", "Texture2DArray", "Texture2DMS",
			"Texture2DMSArray", "Texture3D", "TextureCube", "TextureCubeArray", "true", "typedef", "triangle", "triangleadj", "TriangleStream", "uint", "uniform", "unorm", "unsigned", "vector", "vertexfragment",
			"VertexShader", "void", "volatile", "while",
			"bool1","bool2","bool3","bool4","double1","double2","double3","double4", "float1", "float2", "float3", "float4", "int1", "int2", "int3", "int4", "in", "out", "inout",
			"uint1", "uint2", "uint3", "uint4", "dword1", "dword2", "dword3", "dword4", "half1", "half2", "half3", "half4",
			"float1x1","float2x1","float3x1","float4x1","float1x2","float2x2","float3x2","float4x2",
			"float1x3","float2x3","float3x3","float4x3","float1x4","float2x4","float3x4","float4x4",
			"half1x1","half2x1","half3x1","half4x1","half1x2","half2x2","half3x2","half4x2",
			"half1x3","half2x3","half3x3","half4x3","half1x4","half2x4","half3x4","half4x4",
		};
		for (auto& k : keywords)
			langDef.mKeywords.insert(k);

		static _constcharptr const identifiers[] = {
			"abort", "abs", "acos", "all", "AllMemoryBarrier", "AllMemoryBarrierWithGroupSync", "any", "asdouble", "asfloat", "asin", "asint", "asint", "asuint",
			"asuint", "atan", "atan2", "ceil", "CheckAccessFullyMapped", "clamp", "clip", "cos", "cosh", "countbits", "cross", "D3DCOLORtoUBYTE4", "ddx",
			"ddx_coarse", "ddx_fine", "ddy", "ddy_coarse", "ddy_fine", "degrees", "determinant", "DeviceMemoryBarrier", "DeviceMemoryBarrierWithGroupSync",
			"distance", "dot", "dst", "errorf", "EvaluateAttributeAtCentroid", "EvaluateAttributeAtSample", "EvaluateAttributeSnapped", "exp", "exp2",
			"f16tof32", "f32tof16", "faceforward", "firstbithigh", "firstbitlow", "floor", "fma", "fmod", "frac", "frexp", "fwidth", "GetRenderTargetSampleCount",
			"GetRenderTargetSamplePosition", "GroupMemoryBarrier", "GroupMemoryBarrierWithGroupSync", "InterlockedAdd", "InterlockedAnd", "InterlockedCompareExchange",
			"InterlockedCompareStore", "InterlockedExchange", "InterlockedMax", "InterlockedMin", "InterlockedOr", "InterlockedXor", "isfinite", "isinf", "isnan",
			"ldexp", "length", "lerp", "lit", "log", "log10", "log2", "mad", "max", "min", "modf", "msad4", "mul", "noise", "normalize", "pow", "printf",
			"Process2DQuadTessFactorsAvg", "Process2DQuadTessFactorsMax", "Process2DQuadTessFactorsMin", "ProcessIsolineTessFactors", "ProcessQuadTessFactorsAvg",
			"ProcessQuadTessFactorsMax", "ProcessQuadTessFactorsMin", "ProcessTriTessFactorsAvg", "ProcessTriTessFactorsMax", "ProcessTriTessFactorsMin",
			"radians", "rcp", "reflect", "refract", "reversebits", "round", "rsqrt", "saturate", "sign", "sin", "sincos", "sinh", "smoothstep", "sqrt", "step",
			"tan", "tanh", "tex1D", "tex1D", "tex1Dbias", "tex1Dgrad", "tex1Dlod", "tex1Dproj", "tex2D", "tex2D", "tex2Dbias", "tex2Dgrad", "tex2Dlod", "tex2Dproj",
			"tex3D", "tex3D", "tex3Dbias", "tex3Dgrad", "tex3Dlod", "tex3Dproj", "texCUBE", "texCUBE", "texCUBEbias", "texCUBEgrad", "texCUBElod", "texCUBEproj", "transpose", "trunc"
		};
		for (auto& k : identifiers)
		{
			Identifier id;
			id.mDeclaration = "Built-in function";
			langDef.mIdentifiers.insert(std::make_pair(_stdstr(k), id));
		}

		langDef.mTokenRegexStrings.push_back(std::make_pair<_stdstr, PaletteIndex>("[ \\t]*#[ \\t]*[a-zA-Z_]+", PaletteIndex::Preprocessor));
		langDef.mTokenRegexStrings.push_back(std::make_pair<_stdstr, PaletteIndex>("L?\\\"(\\\\.|[^\\\"])*\\\"", PaletteIndex::String));
		langDef.mTokenRegexStrings.push_back(std::make_pair<_stdstr, PaletteIndex>("\\'\\\\?[^\\']\\'", PaletteIndex::CharLiteral));
		langDef.mTokenRegexStrings.push_back(std::make_pair<_stdstr, PaletteIndex>("[+-]?([0-9]+([.][0-9]*)?|[.][0-9]+)([eE][+-]?[0-9]+)?[fF]?", PaletteIndex::Number));
		langDef.mTokenRegexStrings.push_back(std::make_pair<_stdstr, PaletteIndex>("[+-]?[0-9]+[Uu]?[lL]?[lL]?", PaletteIndex::Number));
		langDef.mTokenRegexStrings.push_back(std::make_pair<_stdstr, PaletteIndex>("0[0-7]+[Uu]?[lL]?[lL]?", PaletteIndex::Number));
		langDef.mTokenRegexStrings.push_back(std::make_pair<_stdstr, PaletteIndex>("0[xX][0-9a-fA-F]+[uU]?[lL]?[lL]?", PaletteIndex::Number));
		langDef.mTokenRegexStrings.push_back(std::make_pair<_stdstr, PaletteIndex>("[a-zA-Z_][a-zA-Z0-9_]*", PaletteIndex::Identifier));
		langDef.mTokenRegexStrings.push_back(std::make_pair<_stdstr, PaletteIndex>("[\\[\\]\\{\\}\\!\\%\\^\\&\\*\\(\\)\\-\\+\\=\\~\\|\\<\\>\\?\\/\\;\\,\\.]", PaletteIndex::Punctuation));

		langDef.mCommentStart = "/*";*/
		//langDef.mCommentEnd = "*/";
		/*langDef.mSingleLineComment = "//";

		langDef.mCaseSensitive = true;
		langDef.mAutoIndentation = true;

		langDef.mName = "HLSL";

		inited = true;
	}*/
	return langDef;
}

const TextEditor::LanguageDefinition& TextEditor::LanguageDefinition::GLSL()
{
	static bool inited = false;
	static LanguageDefinition langDef;
	/*if (!inited)
	{
		static _constcharptr const keywords[] = {
			"auto", "break", "case", "char", "const", "continue", "default", "do", "double", "else", "enum", "extern", "float", "for", "goto", "if", "inline", "int", "long", "register", "restrict", "return", "short",
			"signed", "sizeof", "static", "struct", "switch", "typedef", "union", "unsigned", "void", "volatile", "while", "_Alignas", "_Alignof", "_Atomic", "_Bool", "_Complex", "_Generic", "_Imaginary",
			"_Noreturn", "_Static_assert", "_Thread_local"
		};
		for (auto& k : keywords)
			langDef.mKeywords.insert(k);

		static _constcharptr const identifiers[] = {
			"abort", "abs", "acos", "asin", "atan", "atexit", "atof", "atoi", "atol", "ceil", "clock", "cosh", "ctime", "div", "exit", "fabs", "floor", "fmod", "getchar", "getenv", "isalnum", "isalpha", "isdigit", "isgraph",
			"ispunct", "isspace", "isupper", "kbhit", "log10", "log2", "log", "memcmp", "modf", "pow", "putchar", "putenv", "puts", "rand", "remove", "rename", "sinh", "sqrt", "srand", "strcat", "strcmp", "strerror", "time", "tolower", "toupper"
		};
		for (auto& k : identifiers)
		{
			Identifier id;
			id.mDeclaration = "Built-in function";
			langDef.mIdentifiers.insert(std::make_pair(_stdstr(k), id));
		}

		langDef.mTokenRegexStrings.push_back(std::make_pair<_stdstr, PaletteIndex>("[ \\t]*#[ \\t]*[a-zA-Z_]+", PaletteIndex::Preprocessor));
		langDef.mTokenRegexStrings.push_back(std::make_pair<_stdstr, PaletteIndex>("L?\\\"(\\\\.|[^\\\"])*\\\"", PaletteIndex::String));
		langDef.mTokenRegexStrings.push_back(std::make_pair<_stdstr, PaletteIndex>("\\'\\\\?[^\\']\\'", PaletteIndex::CharLiteral));
		langDef.mTokenRegexStrings.push_back(std::make_pair<_stdstr, PaletteIndex>("[+-]?([0-9]+([.][0-9]*)?|[.][0-9]+)([eE][+-]?[0-9]+)?[fF]?", PaletteIndex::Number));
		langDef.mTokenRegexStrings.push_back(std::make_pair<_stdstr, PaletteIndex>("[+-]?[0-9]+[Uu]?[lL]?[lL]?", PaletteIndex::Number));
		langDef.mTokenRegexStrings.push_back(std::make_pair<_stdstr, PaletteIndex>("0[0-7]+[Uu]?[lL]?[lL]?", PaletteIndex::Number));
		langDef.mTokenRegexStrings.push_back(std::make_pair<_stdstr, PaletteIndex>("0[xX][0-9a-fA-F]+[uU]?[lL]?[lL]?", PaletteIndex::Number));
		langDef.mTokenRegexStrings.push_back(std::make_pair<_stdstr, PaletteIndex>("[a-zA-Z_][a-zA-Z0-9_]*", PaletteIndex::Identifier));
		langDef.mTokenRegexStrings.push_back(std::make_pair<_stdstr, PaletteIndex>("[\\[\\]\\{\\}\\!\\%\\^\\&\\*\\(\\)\\-\\+\\=\\~\\|\\<\\>\\?\\/\\;\\,\\.]", PaletteIndex::Punctuation));

		langDef.mCommentStart = "/*";*/
		//langDef.mCommentEnd = "*/";
		/*langDef.mSingleLineComment = "//";

		langDef.mCaseSensitive = true;
		langDef.mAutoIndentation = true;

		langDef.mName = "GLSL";

		inited = true;
	}*/
	return langDef;
}

const TextEditor::LanguageDefinition& TextEditor::LanguageDefinition::C()
{
	static bool inited = false;
	static LanguageDefinition langDef;
	/*if (!inited)
	{
		static _constcharptr const keywords[] = {
			"auto", "break", "case", "char", "const", "continue", "default", "do", "double", "else", "enum", "extern", "float", "for", "goto", "if", "inline", "int", "long", "register", "restrict", "return", "short",
			"signed", "sizeof", "static", "struct", "switch", "typedef", "union", "unsigned", "void", "volatile", "while", "_Alignas", "_Alignof", "_Atomic", "_Bool", "_Complex", "_Generic", "_Imaginary",
			"_Noreturn", "_Static_assert", "_Thread_local"
		};
		for (auto& k : keywords)
			langDef.mKeywords.insert(k);

		static _constcharptr const identifiers[] = {
			"abort", "abs", "acos", "asin", "atan", "atexit", "atof", "atoi", "atol", "ceil", "clock", "cosh", "ctime", "div", "exit", "fabs", "floor", "fmod", "getchar", "getenv", "isalnum", "isalpha", "isdigit", "isgraph",
			"ispunct", "isspace", "isupper", "kbhit", "log10", "log2", "log", "memcmp", "modf", "pow", "putchar", "putenv", "puts", "rand", "remove", "rename", "sinh", "sqrt", "srand", "strcat", "strcmp", "strerror", "time", "tolower", "toupper"
		};
		for (auto& k : identifiers)
		{
			Identifier id;
			id.mDeclaration = "Built-in function";
			langDef.mIdentifiers.insert(std::make_pair(_stdstr(k), id));
		}

		langDef.mTokenize = [](_constcharptr in_begin, _constcharptr in_end, _constcharptr& out_begin, _constcharptr& out_end, PaletteIndex& paletteIndex) -> bool
			{
				paletteIndex = PaletteIndex::Max;

				while (in_begin < in_end && isascii(*in_begin) && isblank(*in_begin))
					in_begin++;

				if (in_begin == in_end)
				{
					out_begin = in_end;
					out_end = in_end;
					paletteIndex = PaletteIndex::Default;
				}
				else if (TokenizeCStyleString(in_begin, in_end, out_begin, out_end))
					paletteIndex = PaletteIndex::String;
				else if (TokenizeCStyleCharacterLiteral(in_begin, in_end, out_begin, out_end))
					paletteIndex = PaletteIndex::CharLiteral;
				else if (TokenizeCStyleIdentifier(in_begin, in_end, out_begin, out_end))
					paletteIndex = PaletteIndex::Identifier;
				else if (TokenizeCStyleNumber(in_begin, in_end, out_begin, out_end))
					paletteIndex = PaletteIndex::Number;
				else if (TokenizeCStylePunctuation(in_begin, in_end, out_begin, out_end))
					paletteIndex = PaletteIndex::Punctuation;

				return paletteIndex != PaletteIndex::Max;
			};

		langDef.mCommentStart = "/*";*/
		//langDef.mCommentEnd = "*/";
		/*langDef.mSingleLineComment = "//";

		langDef.mCaseSensitive = true;
		langDef.mAutoIndentation = true;

		langDef.mName = "C";

		inited = true;
	}*/
	return langDef;
}

const TextEditor::LanguageDefinition& TextEditor::LanguageDefinition::SQL()
{
	static bool inited = false;
	static LanguageDefinition langDef;
	/*if (!inited)
	{
		static _constcharptr const keywords[] = {
			"ADD", "EXCEPT", "PERCENT", "ALL", "EXEC", "PLAN", "ALTER", "EXECUTE", "PRECISION", "AND", "EXISTS", "PRIMARY", "ANY", "EXIT", "PRINT", "AS", "FETCH", "PROC", "ASC", "FILE", "PROCEDURE",
			"AUTHORIZATION", "FILLFACTOR", "PUBLIC", "BACKUP", "FOR", "RAISERROR", "BEGIN", "FOREIGN", "READ", "BETWEEN", "FREETEXT", "READTEXT", "BREAK", "FREETEXTTABLE", "RECONFIGURE",
			"BROWSE", "FROM", "REFERENCES", "BULK", "FULL", "REPLICATION", "BY", "FUNCTION", "RESTORE", "CASCADE", "GOTO", "RESTRICT", "CASE", "GRANT", "RETURN", "CHECK", "GROUP", "REVOKE",
			"CHECKPOINT", "HAVING", "RIGHT", "CLOSE", "HOLDLOCK", "ROLLBACK", "CLUSTERED", "IDENTITY", "ROWCOUNT", "COALESCE", "IDENTITY_INSERT", "ROWGUIDCOL", "COLLATE", "IDENTITYCOL", "RULE",
			"COLUMN", "IF", "SAVE", "COMMIT", "IN", "SCHEMA", "COMPUTE", "INDEX", "SELECT", "CONSTRAINT", "INNER", "SESSION_USER", "CONTAINS", "INSERT", "SET", "CONTAINSTABLE", "INTERSECT", "SETUSER",
			"CONTINUE", "INTO", "SHUTDOWN", "CONVERT", "IS", "SOME", "CREATE", "JOIN", "STATISTICS", "CROSS", "KEY", "SYSTEM_USER", "CURRENT", "KILL", "TABLE", "CURRENT_DATE", "LEFT", "TEXTSIZE",
			"CURRENT_TIME", "LIKE", "THEN", "CURRENT_TIMESTAMP", "LINENO", "TO", "CURRENT_USER", "LOAD", "TOP", "CURSOR", "NATIONAL", "TRAN", "DATABASE", "NOCHECK", "TRANSACTION",
			"DBCC", "NONCLUSTERED", "TRIGGER", "DEALLOCATE", "NOT", "TRUNCATE", "DECLARE", "NULL", "TSEQUAL", "DEFAULT", "NULLIF", "UNION", "DELETE", "OF", "UNIQUE", "DENY", "OFF", "UPDATE",
			"DESC", "OFFSETS", "UPDATETEXT", "DISK", "ON", "USE", "DISTINCT", "OPEN", "USER", "DISTRIBUTED", "OPENDATASOURCE", "VALUES", "DOUBLE", "OPENQUERY", "VARYING","DROP", "OPENROWSET", "VIEW",
			"DUMMY", "OPENXML", "WAITFOR", "DUMP", "OPTION", "WHEN", "ELSE", "OR", "WHERE", "END", "ORDER", "WHILE", "ERRLVL", "OUTER", "WITH", "ESCAPE", "OVER", "WRITETEXT"
		};

		for (auto& k : keywords)
			langDef.mKeywords.insert(k);

		static _constcharptr const identifiers[] = {
			"ABS",  "ACOS",  "ADD_MONTHS",  "ASCII",  "ASCIISTR",  "ASIN",  "ATAN",  "ATAN2",  "AVG",  "BFILENAME",  "BIN_TO_NUM",  "BITAND",  "CARDINALITY",  "CASE",  "CAST",  "CEIL",
			"CHARTOROWID",  "CHR",  "COALESCE",  "COMPOSE",  "CONCAT",  "CONVERT",  "CORR",  "COS",  "COSH",  "COUNT",  "COVAR_POP",  "COVAR_SAMP",  "CUME_DIST",  "CURRENT_DATE",
			"CURRENT_TIMESTAMP",  "DBTIMEZONE",  "DECODE",  "DECOMPOSE",  "DENSE_RANK",  "DUMP",  "EMPTY_BLOB",  "EMPTY_CLOB",  "EXP",  "EXTRACT",  "FIRST_VALUE",  "FLOOR",  "FROM_TZ",  "GREATEST",
			"GROUP_ID",  "HEXTORAW",  "INITCAP",  "INSTR",  "INSTR2",  "INSTR4",  "INSTRB",  "INSTRC",  "LAG",  "LAST_DAY",  "LAST_VALUE",  "LEAD",  "LEAST",  "LENGTH",  "LENGTH2",  "LENGTH4",
			"LENGTHB",  "LENGTHC",  "LISTAGG",  "LN",  "LNNVL",  "LOCALTIMESTAMP",  "LOG",  "LOWER",  "LPAD",  "LTRIM",  "MAX",  "MEDIAN",  "MIN",  "MOD",  "MONTHS_BETWEEN",  "NANVL",  "NCHR",
			"NEW_TIME",  "NEXT_DAY",  "NTH_VALUE",  "NULLIF",  "NUMTODSINTERVAL",  "NUMTOYMINTERVAL",  "NVL",  "NVL2",  "POWER",  "RANK",  "RAWTOHEX",  "REGEXP_COUNT",  "REGEXP_INSTR",
			"REGEXP_REPLACE",  "REGEXP_SUBSTR",  "REMAINDER",  "REPLACE",  "ROUND",  "ROWNUM",  "RPAD",  "RTRIM",  "SESSIONTIMEZONE",  "SIGN",  "SIN",  "SINH",
			"SOUNDEX",  "SQRT",  "STDDEV",  "SUBSTR",  "SUM",  "SYS_CONTEXT",  "SYSDATE",  "SYSTIMESTAMP",  "TAN",  "TANH",  "TO_CHAR",  "TO_CLOB",  "TO_DATE",  "TO_DSINTERVAL",  "TO_LOB",
			"TO_MULTI_BYTE",  "TO_NCLOB",  "TO_NUMBER",  "TO_SINGLE_BYTE",  "TO_TIMESTAMP",  "TO_TIMESTAMP_TZ",  "TO_YMINTERVAL",  "TRANSLATE",  "TRIM",  "TRUNC", "TZ_OFFSET",  "UID",  "UPPER",
			"USER",  "USERENV",  "VAR_POP",  "VAR_SAMP",  "VARIANCE",  "VSIZE "
		};
		for (auto& k : identifiers)
		{
			Identifier id;
			id.mDeclaration = "Built-in function";
			langDef.mIdentifiers.insert(std::make_pair(_stdstr(k), id));
		}

		langDef.mTokenRegexStrings.push_back(std::make_pair<_stdstr, PaletteIndex>("L?\\\"(\\\\.|[^\\\"])*\\\"", PaletteIndex::String));
		langDef.mTokenRegexStrings.push_back(std::make_pair<_stdstr, PaletteIndex>("\\\'[^\\\']*\\\'", PaletteIndex::String));
		langDef.mTokenRegexStrings.push_back(std::make_pair<_stdstr, PaletteIndex>("[+-]?([0-9]+([.][0-9]*)?|[.][0-9]+)([eE][+-]?[0-9]+)?[fF]?", PaletteIndex::Number));
		langDef.mTokenRegexStrings.push_back(std::make_pair<_stdstr, PaletteIndex>("[+-]?[0-9]+[Uu]?[lL]?[lL]?", PaletteIndex::Number));
		langDef.mTokenRegexStrings.push_back(std::make_pair<_stdstr, PaletteIndex>("0[0-7]+[Uu]?[lL]?[lL]?", PaletteIndex::Number));
		langDef.mTokenRegexStrings.push_back(std::make_pair<_stdstr, PaletteIndex>("0[xX][0-9a-fA-F]+[uU]?[lL]?[lL]?", PaletteIndex::Number));
		langDef.mTokenRegexStrings.push_back(std::make_pair<_stdstr, PaletteIndex>("[a-zA-Z_][a-zA-Z0-9_]*", PaletteIndex::Identifier));
		langDef.mTokenRegexStrings.push_back(std::make_pair<_stdstr, PaletteIndex>("[\\[\\]\\{\\}\\!\\%\\^\\&\\*\\(\\)\\-\\+\\=\\~\\|\\<\\>\\?\\/\\;\\,\\.]", PaletteIndex::Punctuation));

		langDef.mCommentStart = "/*";*/
		//langDef.mCommentEnd = "*/";
		/*langDef.mSingleLineComment = "//";

		langDef.mCaseSensitive = false;
		langDef.mAutoIndentation = false;

		langDef.mName = "SQL";

		inited = true;
	}*/
	return langDef;
}

const TextEditor::LanguageDefinition& TextEditor::LanguageDefinition::AngelScript()
{
	static bool inited = false;
	static LanguageDefinition langDef;
	/*if (!inited)
	{
		static _constcharptr const keywords[] = {
			"and", "abstract", "auto", "bool", "break", "case", "cast", "class", "const", "continue", "default", "do", "double", "else", "enum", "false", "final", "float", "for",
			"from", "funcdef", "function", "get", "if", "import", "in", "inout", "int", "interface", "int8", "int16", "int32", "int64", "is", "mixin", "namespace", "not",
			"null", "or", "out", "override", "private", "protected", "return", "set", "shared", "super", "switch", "this ", "true", "typedef", "uint", "uint8", "uint16", "uint32",
			"uint64", "void", "while", "xor"
		};

		for (auto& k : keywords)
			langDef.mKeywords.insert(k);

		static _constcharptr const identifiers[] = {
			"cos", "sin", "tab", "acos", "asin", "atan", "atan2", "cosh", "sinh", "tanh", "log", "log10", "pow", "sqrt", "abs", "ceil", "floor", "fraction", "closeTo", "fpFromIEEE", "fpToIEEE",
			"complex", "opEquals", "opAddAssign", "opSubAssign", "opMulAssign", "opDivAssign", "opAdd", "opSub", "opMul", "opDiv"
		};
		for (auto& k : identifiers)
		{
			Identifier id;
			id.mDeclaration = "Built-in function";
			langDef.mIdentifiers.insert(std::make_pair(_stdstr(k), id));
		}

		langDef.mTokenRegexStrings.push_back(std::make_pair<_stdstr, PaletteIndex>("L?\\\"(\\\\.|[^\\\"])*\\\"", PaletteIndex::String));
		langDef.mTokenRegexStrings.push_back(std::make_pair<_stdstr, PaletteIndex>("\\'\\\\?[^\\']\\'", PaletteIndex::String));
		langDef.mTokenRegexStrings.push_back(std::make_pair<_stdstr, PaletteIndex>("[+-]?([0-9]+([.][0-9]*)?|[.][0-9]+)([eE][+-]?[0-9]+)?[fF]?", PaletteIndex::Number));
		langDef.mTokenRegexStrings.push_back(std::make_pair<_stdstr, PaletteIndex>("[+-]?[0-9]+[Uu]?[lL]?[lL]?", PaletteIndex::Number));
		langDef.mTokenRegexStrings.push_back(std::make_pair<_stdstr, PaletteIndex>("0[0-7]+[Uu]?[lL]?[lL]?", PaletteIndex::Number));
		langDef.mTokenRegexStrings.push_back(std::make_pair<_stdstr, PaletteIndex>("0[xX][0-9a-fA-F]+[uU]?[lL]?[lL]?", PaletteIndex::Number));
		langDef.mTokenRegexStrings.push_back(std::make_pair<_stdstr, PaletteIndex>("[a-zA-Z_][a-zA-Z0-9_]*", PaletteIndex::Identifier));
		langDef.mTokenRegexStrings.push_back(std::make_pair<_stdstr, PaletteIndex>("[\\[\\]\\{\\}\\!\\%\\^\\&\\*\\(\\)\\-\\+\\=\\~\\|\\<\\>\\?\\/\\;\\,\\.]", PaletteIndex::Punctuation));

		langDef.mCommentStart = "/*";*/
		//langDef.mCommentEnd = "*/";
		/*langDef.mSingleLineComment = "//";

		langDef.mCaseSensitive = true;
		langDef.mAutoIndentation = true;

		langDef.mName = "AngelScript";

		inited = true;
	}*/
	return langDef;
}

const TextEditor::LanguageDefinition& TextEditor::LanguageDefinition::Lua()
{
	static bool inited = false;
	static LanguageDefinition langDef;
	/*if (!inited)
	{
		static _constcharptr const keywords[] = {
			"and", "break", "do", "", "else", "elseif", "end", "false", "for", "function", "if", "in", "", "local", "nil", "not", "or", "repeat", "return", "then", "true", "until", "while"
		};

		for (auto& k : keywords)
			langDef.mKeywords.insert(k);

		static _constcharptr const identifiers[] = {
			"assert", "collectgarbage", "dofile", "error", "getmetatable", "ipairs", "loadfile", "load", "loadstring",  "next",  "pairs",  "pcall",  "print",  "rawequal",  "rawlen",  "rawget",  "rawset",
			"select",  "setmetatable",  "tonumber",  "tostring",  "type",  "xpcall",  "_G",  "_VERSION","arshift", "band", "bnot", "bor", "bxor", "btest", "extract", "lrotate", "lshift", "replace",
			"rrotate", "rshift", "create", "resume", "running", "status", "wrap", "yield", "isyieldable", "debug","getuservalue", "gethook", "getinfo", "getlocal", "getregistry", "getmetatable",
			"getupvalue", "upvaluejoin", "upvalueid", "setuservalue", "sethook", "setlocal", "setmetatable", "setupvalue", "traceback", "close", "flush", "input", "lines", "open", "output", "popen",
			"read", "tmpfile", "type", "write", "close", "flush", "lines", "read", "seek", "setvbuf", "write", "__gc", "__tostring", "abs", "acos", "asin", "atan", "ceil", "cos", "deg", "exp", "tointeger",
			"floor", "fmod", "ult", "log", "max", "min", "modf", "rad", "random", "randomseed", "sin", "sqrt", "string", "tan", "type", "atan2", "cosh", "sinh", "tanh",
			"pow", "frexp", "ldexp", "log10", "pi", "huge", "maxinteger", "mininteger", "loadlib", "searchpath", "seeall", "preload", "cpath", "path", "searchers", "loaded", "module", "require", "clock",
			"date", "difftime", "execute", "exit", "getenv", "remove", "rename", "setlocale", "time", "tmpname", "byte", "char", "dump", "find", "format", "gmatch", "gsub", "len", "lower", "match", "rep",
			"reverse", "sub", "upper", "pack", "packsize", "unpack", "concat", "maxn", "insert", "pack", "unpack", "remove", "move", "sort", "offset", "codepoint", "char", "len", "codes", "charpattern",
			"coroutine", "table", "io", "os", "string", "utf8", "bit32", "math", "debug", "package"
		};
		for (auto& k : identifiers)
		{
			Identifier id;
			id.mDeclaration = "Built-in function";
			langDef.mIdentifiers.insert(std::make_pair(_stdstr(k), id));
		}

		langDef.mTokenRegexStrings.push_back(std::make_pair<_stdstr, PaletteIndex>("L?\\\"(\\\\.|[^\\\"])*\\\"", PaletteIndex::String));
		langDef.mTokenRegexStrings.push_back(std::make_pair<_stdstr, PaletteIndex>("\\\'[^\\\']*\\\'", PaletteIndex::String));
		langDef.mTokenRegexStrings.push_back(std::make_pair<_stdstr, PaletteIndex>("0[xX][0-9a-fA-F]+[uU]?[lL]?[lL]?", PaletteIndex::Number));
		langDef.mTokenRegexStrings.push_back(std::make_pair<_stdstr, PaletteIndex>("[+-]?([0-9]+([.][0-9]*)?|[.][0-9]+)([eE][+-]?[0-9]+)?[fF]?", PaletteIndex::Number));
		langDef.mTokenRegexStrings.push_back(std::make_pair<_stdstr, PaletteIndex>("[+-]?[0-9]+[Uu]?[lL]?[lL]?", PaletteIndex::Number));
		langDef.mTokenRegexStrings.push_back(std::make_pair<_stdstr, PaletteIndex>("[a-zA-Z_][a-zA-Z0-9_]*", PaletteIndex::Identifier));
		langDef.mTokenRegexStrings.push_back(std::make_pair<_stdstr, PaletteIndex>("[\\[\\]\\{\\}\\!\\%\\^\\&\\*\\(\\)\\-\\+\\=\\~\\|\\<\\>\\?\\/\\;\\,\\.]", PaletteIndex::Punctuation));

		langDef.mCommentStart = "--[[";
		langDef.mCommentEnd = "]]";
		langDef.mSingleLineComment = "--";

		langDef.mCaseSensitive = true;
		langDef.mAutoIndentation = false;

		langDef.mName = "Lua";

		inited = true;
	}*/
	return langDef;
}
