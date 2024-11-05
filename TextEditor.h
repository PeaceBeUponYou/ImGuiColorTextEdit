#pragma once
#ifndef UseWideStr
	#define UseWideStr 1
	#if UseWideStr == 1
	#define _stdstr std::wstring
	#define _constcharptr const wchar_t*
	#define _constchar const wchar_t
	#define _justchar wchar_t
	#define _regx std::wregex
	#define _cmatch std::wcmatch
	#define mystr(x) L##x
	#define _strcmp(x, y) wcscmp(x,y)
	#else
	#define _stdstr std::string
	#define _constcharptr const char*
	#define _constchar const char
	#define _justchar char
	#define _regx std::regex
	#define _cmatch std::cmatch
	#define mystr(x) ##x
	#define _strcmp(x, y) strcmp(x,y)
	#endif
#endif

#include <string>
#include <vector>
#include <array>
#include <memory>
#include <unordered_set>
#include <unordered_map>
#include <map>
#include <regex>
#include <future>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <Windows.h>
#include <locale.h>
#ifndef IMGUI_DEFINE_MATH_OPERATORS
	#define IMGUI_DEFINE_MATH_OPERATORS
#endif
#include "imgui.h"
#include "imgui_internal.h"
#include "imgui_stdlib.h"

struct Breakpoint
{
	int mLine;
	bool mTriggered;
	//_stdstr mCondition;

	Breakpoint()
		: mLine(-1)
		, mTriggered(false)
	{}
	Breakpoint(int line)
		: mLine(line)
		, mTriggered(false)
	{}
	Breakpoint(int line, bool enabled)
		: mLine(line)
		, mTriggered(enabled)
	{}
	Breakpoint(Breakpoint& ref)
		: mLine(ref.mLine)
		, mTriggered(ref.mTriggered)
	{}
	Breakpoint(const Breakpoint& ref)
		: mLine(ref.mLine)
		, mTriggered(ref.mTriggered)
	{}
	bool operator==(Breakpoint& other)
	{
		return this->mLine == other.mLine;
	}
	
	Breakpoint& operator=(const Breakpoint& other)
	{
		this->mLine = other.mLine;
		this->mTriggered = other.mTriggered;
		return *this;
	}
	size_t operator()(const Breakpoint& pointToHash) const {
		size_t hash = std::hash<int>()(pointToHash.mLine) + 10 * std::hash<bool>()(pointToHash.mTriggered);
		return hash;
	};
	/*bool operator==(int index){ return this->mLine == index; }
	bool operator>(int index){ return this->mLine > index; }
	bool operator>=(int index){ return this->mLine >= index; }
	bool operator<(int index){ return this->mLine < index; }
	bool operator<=(int index){ return this->mLine <= index; }

	int operator+(int index) { this->mLine += index; return this->mLine; }
	int operator-(int index) { this->mLine -= index; return this->mLine; }*/
};
static bool operator==(const Breakpoint& one, const Breakpoint& other)
{
	return one.mLine == other.mLine;
}
namespace std {
	template<> struct hash<Breakpoint>
	{
		std::size_t operator()(const Breakpoint& p) const noexcept
		{
			return p(p);
		}
	};
};

typedef bool(__stdcall* OnBreakpoint)(int aLineNo, bool ToggleOn);
typedef void(__stdcall* OnHoverOverProperty)(void* ptr, int aLineNo, const _stdstr& keyword); //ptr = Identifier

class TextEditor
{
public:
	enum class PaletteIndex
	{
		Default,
		Keyword,
		Number,
		String,
		CharLiteral,
		Punctuation,
		Preprocessor,
		Identifier,
		KnownIdentifier,
		PreprocIdentifier,
		Comment,
		MultiLineComment,
		Background,
		Cursor,
		Selection,
		SelectionDuplicate,
		ErrorMarker,
		Breakpoint,
		BreakpointEnabled,
		LineNumber,
		LineNumberActive,
		CurrentLineFill,
		CurrentLineFillInactive,
		CurrentLineEdge,
		Function,
		Class,
		EnumFields,
		LocalProps,
		MemberProps,
		ParamProps,
		BracketsTrace,
		CollapsorBoxDisabled,
		CollapsorBoxEnabled,
		CollapsedRegion,
		BreakpointBar,
		LineNoBar,
		Max
	};

	enum class SelectionMode
	{
		Normal,
		Word,
		Line
	};

	enum CollapsableType
	{
		None = 0,
		Starting = 1,
		Ending = 2,
		Both = Starting | Ending
	};

	enum IdentifierType
	{
		General,
		Preprocessor,
		Class,
		Function,
		Struct,
		ParameterProperty,
		LocalProperty,
		MemberProperty,
		Any
	};

	// Represents a character coordinate from the user's point of view,
	// i. e. consider an uniform grid (assuming fixed-width font) on the
	// screen as it is rendered, and each cell has its own coordinate, starting from 0.
	// Tabs are counted as [1..mTabSize] count empty spaces, depending on
	// how many space is necessary to reach the next tab stop.
	// For example, coordinate (1, 5) represents the character 'B' in a line "\tABC", when mTabSize = 4,
	// because it is rendered as "    ABC" on the screen.
	struct Coordinates
	{
		int mLine, mColumn;
		Coordinates() : mLine(0), mColumn(0) {}
		Coordinates(int aLine, int aColumn) : mLine(aLine), mColumn(aColumn)
		{
			/*assert(aLine >= 0);
			assert(aColumn >= 0);*/
		}
		static Coordinates Invalid() { static Coordinates invalid(-1, -1); return invalid; }

		bool operator ==(const Coordinates& o) const
		{
			return
				mLine == o.mLine &&
				mColumn == o.mColumn;
		}

		bool operator !=(const Coordinates& o) const
		{
			return
				mLine != o.mLine ||
				mColumn != o.mColumn;
		}

		bool operator <(const Coordinates& o) const
		{
			if (mLine != o.mLine)
				return mLine < o.mLine;
			return mColumn < o.mColumn;
		}

		bool operator >(const Coordinates& o) const
		{
			if (mLine != o.mLine)
				return mLine > o.mLine;
			return mColumn > o.mColumn;
		}

		bool operator <=(const Coordinates& o) const
		{
			if (mLine != o.mLine)
				return mLine < o.mLine;
			return mColumn <= o.mColumn;
		}

		bool operator >=(const Coordinates& o) const
		{
			if (mLine != o.mLine)
				return mLine > o.mLine;
			return mColumn >= o.mColumn;
		}
	};

	struct Identifier
	{
		void* mAssociatedData = nullptr;
		Coordinates mLocation;
		_stdstr mDeclaration;
	};

	struct Collapsable
	{
		Coordinates mStart;
		Coordinates mEnd;
		_stdstr mInfoStart = mystr("");
		_stdstr mInfoEnd = mystr("");
		bool mIsBracket : 1;
		bool mIsFoldEnabled : 1;
		bool mWithinAnotherFold : 1;
		Collapsable() :
			  mStart(Coordinates())
			, mEnd(Coordinates())
			, mIsBracket(false)
			, mIsFoldEnabled(false)
			, mWithinAnotherFold(false)
		{

		}
		Collapsable(Coordinates& start, Coordinates& end, bool bIsBracket = false) :
			  mStart(start)
			, mEnd(end)
			, mIsBracket(bIsBracket)
			, mIsFoldEnabled(false)
		{

		}
		bool operator=(Collapsable& other)
		{
			return mStart == other.mStart && mEnd == other.mEnd;
		}
		bool operator=(const Collapsable& other) const
		{
			return mStart == other.mStart && mEnd == other.mEnd;
		}

	};

	typedef _stdstr String;
	typedef std::unordered_map<_stdstr, Identifier> Identifiers;
	typedef std::unordered_map<_stdstr, std::unordered_map<void*, Identifier>> MultiIdentifers;
	typedef std::unordered_set<_stdstr> Keywords;
	typedef std::map<int, _stdstr> ErrorMarkers;
	typedef std::unordered_set<Breakpoint> Breakpoints;
	typedef std::array<ImU32, (unsigned)PaletteIndex::Max> Palette;
	//typedef std::map<Coordinates, Coordinates> Brackets;
	typedef std::vector<Collapsable> Collapsables;
	typedef _justchar Char;

	struct Glyph
	{
		Char mChar;
		PaletteIndex mColorIndex = PaletteIndex::Default;
		bool mComment : 1;
		bool mMultiLineComment : 1;
		bool mPreprocessor : 1;
		bool mIsCollapsed : 1;
		Glyph(Char aChar, PaletteIndex aColorIndex) : mChar(aChar), mColorIndex(aColorIndex),
			mComment(false), mMultiLineComment(false), mPreprocessor(false), mIsCollapsed(false) 
		{}
		Glyph(Glyph& other) : mChar(other.mChar), mColorIndex(other.mColorIndex),
			mComment(other.mComment), mMultiLineComment(other.mMultiLineComment), 
			mPreprocessor(other.mPreprocessor), mIsCollapsed(other.mIsCollapsed)
		{}
		Glyph(const Glyph& other) : mChar(other.mChar), mColorIndex(other.mColorIndex),
			mComment(other.mComment), mMultiLineComment(other.mMultiLineComment), 
			mPreprocessor(other.mPreprocessor), mIsCollapsed(other.mIsCollapsed)
		{}
	};
	struct Line
	{
		std::vector<Glyph> mLine;
		bool mIsCollapsed : 1;
		bool mHasCollapedInfo : 1;
		Line() : mIsCollapsed(false), mHasCollapedInfo(false)
		{}
		Glyph& operator[](size_t index) { return mLine[index]; }
		const Glyph& operator[](size_t index) const { return mLine[index]; }
		//operator bool() { return mIsCollapsed; }
		//std::vector<Glyph>* operator->() { return &mLine; }
		size_t size() { return mLine.size(); }
		size_t size() const{ return mLine.size(); }
		bool empty() const{ return mLine.empty(); }
		auto emplace_back(Glyph& glph) { return mLine.emplace_back(glph); }
		auto emplace_back(const Glyph& glph) { return emplace_back(const_cast<Glyph&>(glph)); }
		auto push_back(Glyph& glph) { return mLine.push_back(glph); }
		auto reserve(const size_t& cap) { return mLine.reserve(cap); }
		auto reserve(size_t& cap) { return mLine.reserve(cap); }
		auto erase(const std::vector<Glyph>::iterator& it) { return mLine.erase(it); }
		auto erase(std::vector<Glyph>::iterator& it) { return mLine.erase(it); }
		auto erase(const std::vector<Glyph>::iterator& beg, const std::vector<Glyph>::iterator& end) { return mLine.erase(beg, end); }
		auto erase(std::vector<Glyph>::iterator& beg, std::vector<Glyph>::iterator& end) { return mLine.erase(beg, end); }
		auto begin() { return mLine.begin(); }
		auto end() { return mLine.end(); }
		auto front() { return mLine.front(); }
		auto insert(const std::vector<Glyph>::iterator& it, Glyph& gph) { return mLine.insert(it, gph); }
		auto insert(const std::vector<Glyph>::iterator& it, const Glyph& gph) { return insert(it, const_cast<Glyph&>(gph)); }
		auto insert(std::vector<Glyph>::iterator& it, Glyph& gph) { return mLine.insert(it, gph); }
		auto insert(std::vector<Glyph>::iterator& reffirst, const std::vector<Glyph>::iterator& sec, const std::vector<Glyph>::iterator& thrd) { return mLine.insert(reffirst, sec, thrd); }
		auto insert(const std::vector<Glyph>::iterator& reffirst, const std::vector<Glyph>::iterator& sec, const std::vector<Glyph>::iterator& thrd) { return mLine.insert(reffirst, sec, thrd); }
	};
	//typedef std::vector<Glyph> Line;
	typedef std::vector<Line> Lines;

	struct LanguageDefinition
	{
		typedef std::pair<_stdstr, PaletteIndex> TokenRegexString;
		typedef std::vector<TokenRegexString> TokenRegexStrings;
		typedef bool(*TokenizeCallback)(_constcharptr in_begin, _constcharptr in_end, _constcharptr& out_begin, _constcharptr& out_end, PaletteIndex & paletteIndex);

		_stdstr mName;
		Keywords mKeywords;
		Identifiers mIdentifiers;
		Identifiers mPreprocIdentifiers;
		Identifiers mClasses;
		Identifiers mStructs;
		Identifiers mEnums;
		Identifiers mFunctions;
		MultiIdentifers mParameterProperties;
		MultiIdentifers mLocalProperties;
		MultiIdentifers mMemberProperties;
		//MultiIdentifers OtherIdentifiers;
		_stdstr mCommentStart, mCommentEnd, mSingleLineComment, mFunctionBegin, mStaticClassBegin;
		char mPreprocChar;
		bool mAutoIndentation;

		TokenizeCallback mTokenize;

		TokenRegexStrings mTokenRegexStrings;

		bool mCaseSensitive;
		std::unordered_map<_justchar, _justchar> mBracketsOnOff;
		std::unordered_map<_justchar, _justchar> mBracketsOffOn;
		std::unordered_map<_stdstr, _stdstr> mCollapsablesOnOff;
		std::unordered_map<_stdstr, _stdstr> mCollapsablesOffOn;

		LanguageDefinition()
			: mPreprocChar('#'), mAutoIndentation(true), mTokenize(nullptr), mCaseSensitive(true)
		{
		}
		const void AddIdentifier(const _stdstr& key, const Identifier& idf, IdentifierType idft);
		bool HasIdentifier(const _stdstr& key, IdentifierType idft = IdentifierType::Any);
		auto GetIdentifier(const _stdstr& key);
		bool HasMultiIdentifier(const _stdstr& key, IdentifierType idft = IdentifierType::Any);
		auto GetMultiIdentifier(const _stdstr& key);
		static const LanguageDefinition& CPlusPlus();
		static const LanguageDefinition& HLSL();
		static const LanguageDefinition& GLSL();
		static const LanguageDefinition& C();
		static const LanguageDefinition& SQL();
		static const LanguageDefinition& AngelScript();
		static const LanguageDefinition& Lua();
	};

	TextEditor();
	~TextEditor();
	
	void Clear();

	void SetLanguageDefinition(const LanguageDefinition& aLanguageDef);
	const LanguageDefinition& GetLanguageDefinition() const { return mLanguageDefinition; }
	LanguageDefinition* GetLanguageDef() { return &mLanguageDefinition; }
	void AddIdentifier(const _stdstr& key, const Identifier& idf, IdentifierType idft);
	void ToggleIdentifierTips() { mAllowTooltips = !mAllowTooltips; }
	bool GetTipsState() { return mAllowTooltips; }

	const Palette& GetPalette() const { return mPaletteBase; }
	void SetPalette(const Palette& aValue);

	void SetErrorMarkers(const ErrorMarkers& aMarkers) { mErrorMarkers = aMarkers; }
	void SetBreakpoints(const Breakpoints& aMarkers) { mBreakpoints = aMarkers; }
	void MoveBreakpoints(Breakpoints& aMarkers) { mBreakpoints = std::move(aMarkers); }
	void SetOnBreakpointToggle(OnBreakpoint obp) { mOnBreakpointToggle = obp; }
	bool ToggleBreakpoint(int aLineNo);
	int	 GetTriggerLine();
	void SetTriggerLine(int aLinNo);

	void Render(const char* aTitle, const ImVec2& aSize = ImVec2(), bool aBorder = false);
	void SetText(const _stdstr& aText);
	_stdstr GetText() const;

	void SetTextLines(const std::vector<_stdstr>& aLines);
	std::vector<_stdstr> GetTextLines() const;

	_stdstr GetSelectedText() const;
	_stdstr GetCurrentLineText()const;
	_stdstr GetLineText(int aLineNo)const;
	bool DoesTextMatch(_stdstr& aText, int aLine, int aColumn)const;

	int GetTotalLines() const { return (int)mLines.size(); }
	bool IsOverwrite() const { return mOverwrite; }

	void SetReadOnly(bool aValue);
	bool IsReadOnly() const { return mReadOnly; }
	bool IsTextChanged() const { return mTextChanged; }
	bool IsCursorPositionChanged() const { return mCursorPositionChanged; }

	bool IsColorizerEnabled() const { return mColorizerEnabled; }
	void SetColorizerEnable(bool aValue);

	Coordinates GetCursorPosition() const { return GetActualCursorCoordinates(); }
	void SetCursorPosition(const Coordinates& aPosition);
	void GotoLine(int aLineNo);

	inline void SetHandleMouseInputs    (bool aValue){ mHandleMouseInputs    = aValue;}
	inline bool IsHandleMouseInputsEnabled() const { return mHandleKeyboardInputs; }

	inline void SetHandleKeyboardInputs (bool aValue){ mHandleKeyboardInputs = aValue;}
	inline bool IsHandleKeyboardInputsEnabled() const { return mHandleKeyboardInputs; }

	inline void SetImGuiChildIgnored    (bool aValue){ mIgnoreImGuiChild     = aValue;}
	inline bool IsImGuiChildIgnored() const { return mIgnoreImGuiChild; }

	inline void SetShowWhitespaces(bool aValue) { mShowWhitespaces = aValue; }
	inline bool IsShowingWhitespaces() const { return mShowWhitespaces; }

	void SetTabSize(int aValue);
	inline int GetTabSize() const { return mTabSize; }

	void InsertText(const _stdstr& aValue);
	void InsertText(_constcharptr aValue);

	void MoveUp(int aAmount = 1, bool aSelect = false);
	void MoveDown(int aAmount = 1, bool aSelect = false);
	void MoveLeft(int aAmount = 1, bool aSelect = false, bool aWordMode = false);
	void MoveRight(int aAmount = 1, bool aSelect = false, bool aWordMode = false);
	void MoveTop(bool aSelect = false);
	void MoveBottom(bool aSelect = false);
	void MoveHome(bool aSelect = false);
	void MoveEnd(bool aSelect = false);
	bool Move(int& aLine, int& aCharIndex, bool aLeft = false);

	bool FindMatchingBracket(int aCurrentLine, int aBracketCharIndex, Coordinates& outcorrd);
	void UpdateBrackets();
	bool FindMatchingCollabsable(int aCurrentLine, int aCurrentColumn, _stdstr& collapsableChar, Coordinates& outcorrd, _stdstr& outcollapsable, bool bFindOpening = false, int endingLine = -1);
	bool FindMatchingOpeningCollabsable(int aCurrentLine, int aCurrentColumn, _stdstr& collapsableChar, Coordinates& outcorrd, _stdstr& outcollapsable, int endingLine = -1);
	bool FindMatchingClosingCollabsable(int aCurrentLine, int aCurrentColumn, _stdstr& collapsableChar, Coordinates& outcorrd, _stdstr& outcollapsable, int endingLine = -1);
	bool LineHasAnyOpeningCollapsable(int aLine, int& startColumn, int* OutCharIndex, _stdstr& outchar);
	void UpdateCollapsables(Coordinates& after);
	bool FindCurrentCollapsSize(int aCurrentLine, int* aOutCollapsStart, int* aOutCollapsEnd) const;

	ImVec2 GetPositionAt(Coordinates& coords);

	void SetSelectionStart(const Coordinates& aPosition);
	void SetSelectionEnd(const Coordinates& aPosition);
	void SetSelection(const Coordinates& aStart, const Coordinates& aEnd, SelectionMode aMode = SelectionMode::Normal);
	void SelectWordUnderCursor();
	void SelectAll();
	bool HasSelection() const;

	void Copy();
	void Cut();
	void Paste();
	void Delete();
	_stdstr GetClipboardData();
	bool CanUndo() const;
	bool CanRedo() const;
	void Undo(int aSteps = 1);
	void Redo(int aSteps = 1);
	Palette& GetBasePalette() { return mPaletteBase; }
	const char* GetPaletteIndexName(PaletteIndex index);

	static const Palette& GetDarkPalette();
	static const Palette& GetLightPalette();
	static const Palette& GetRetroBluePalette();
public:
	OnHoverOverProperty mOnPropertyIndentifierHover;
	OnHoverOverProperty mOnMultiPropertyIndentifierHover;
	bool mHaltRender;
private:
	typedef std::vector<std::pair<_regx, PaletteIndex>> RegexList;
	typedef std::map<int, int> CollapsedLines;
	typedef std::unordered_map<int, int> CollapsedLinesUn;
	void CollapsAll();
	void ExpandAll();
	void AddCollapsedLine(int, int);
	void RemoveCollapsedLine(int);
	void UpdateIgnoreCollapsables(bool bShouldAdd);
	bool DoesActiveCollapsableHasAnyParent(std::pair<int, int>& aActiveCollapsable);
	bool IsWithinCollapsedRange(int, int*, int*);
	bool CalculateCurrentRealLine(int&, int* = nullptr, bool = false) const;
	bool CalculateNextCollapsStartEnd(int*, int*, bool&);
	struct EditorState
	{
		Coordinates mSelectionStart;
		Coordinates mSelectionEnd;
		Coordinates mCursorPosition;
	};

	class UndoRecord
	{
	public:
		UndoRecord() {}
		~UndoRecord() {}

		UndoRecord(
			const _stdstr& aAdded,
			const TextEditor::Coordinates aAddedStart,
			const TextEditor::Coordinates aAddedEnd,

			const _stdstr& aRemoved,
			const TextEditor::Coordinates aRemovedStart,
			const TextEditor::Coordinates aRemovedEnd,

			TextEditor::EditorState& aBefore,
			TextEditor::EditorState& aAfter);

		void Undo(TextEditor* aEditor);
		void Redo(TextEditor* aEditor);

		_stdstr mAdded;
		Coordinates mAddedStart;
		Coordinates mAddedEnd;

		_stdstr mRemoved;
		Coordinates mRemovedStart;
		Coordinates mRemovedEnd;

		EditorState mBefore;
		EditorState mAfter;
	};

	typedef std::vector<UndoRecord> UndoBuffer;

	void ProcessInputs();
	void Colorize(int aFromLine = 0, int aCount = -1);
	void ColorizeRange(int aFromLine = 0, int aToLine = 0);	//Lines are colorized here
	void ColorizeInternal();
	float TextDistanceToLineStart(const Coordinates& aFrom) const;
	void EnsureCursorVisible();
	int GetPageSize() const;
	_stdstr GetText(const Coordinates& aStart, const Coordinates& aEnd) const;
	Coordinates GetActualCursorCoordinates() const;
	Coordinates GetCollapsedCursordCoordinates(Coordinates& aActualCorrdinates) const;
	Coordinates SanitizeCoordinates(const Coordinates& aValue) const;
	void Advance(Coordinates& aCoordinates) const;
	void DeleteRange(const Coordinates& aStart, const Coordinates& aEnd);
	int InsertTextAt(Coordinates& aWhere, _constcharptr aValue);
	void AddUndo(UndoRecord& aValue);
	Coordinates ScreenPosToCoordinates(const ImVec2& aPosition, bool* isOverLineNumber = nullptr, bool* isOverBPPoint = nullptr, bool* isOverCollaper = nullptr ) const;
	Coordinates FindWordStart(const Coordinates& aFrom) const;
	Coordinates FindWordEnd(const Coordinates& aFrom) const;
	Coordinates FindNextWord(const Coordinates& aFrom) const;
	int GetCharacterIndex(const Coordinates& aCoordinates) const;
	int GetCharacterSize(const Coordinates& aCoordinates) const;
	int GetCharacterColumn(int aLine, int aIndex) const;
	int GetMainCharacterIndex(Line& line) const;
	int GetLineCharacterCount(int aLine) const;
	int GetLineMaxColumn(int aLine) const;
	bool IsOnWordBoundary(const Coordinates& aAt) const;
	void RemoveLine(int aStart, int aEnd);
	void RemoveLine(int aIndex);
	Line& InsertLine(int aIndex, int aColumn);
	void EnterCharacter(ImWchar aChar, bool aShift);
	void Backspace();
	void DeleteSelection();
	_stdstr GetWordUnderCursor() const;
	_stdstr GetWordAt(const Coordinates& aCoords) const;
	ImU32 GetGlyphColor(const Glyph& aGlyph) const;
	bool BufferIsAChar(_constcharptr buffer);
	bool BufferIsACStyleIdentifier(_constcharptr buffer, _constcharptr bufferend);
	bool BufferHasCharacter(_constcharptr input, _constcharptr endinput, _constcharptr tomatch, bool bSkipWhiteSpaces, bool bAlsoSkipChars); //Checks if there is a related character in the upcomming
	bool BufferHadCharacter(_constcharptr input, _constcharptr startpoint, _constcharptr tomatch, bool bSkipWhiteSpaces, bool bAlsoSkipChars); //Checks if there is a related character in the past

	void HandleKeyboardInputs();
	void HandleMouseInputs();
	void Render();
	void RenderSearch();
public:
	int mTabSize;
	int  mLeftMargin;
	bool mReadOnly;
	bool mShowWhitespaces;
	bool mAllowTooltips;
	bool mDrawBars;
	bool mShowCollapsables;
	bool mColorizerEnabled;

private:
	bool mShowSearchBar;
	std::string mSearchStr;
	ImVec2 mCursorScreenPosAtStart;

	float mLineSpacing;
	Lines mLines;
	EditorState mState;
	UndoBuffer mUndoBuffer;
	int mUndoIndex;

	bool mOverwrite;
	bool mWithinRender;
	bool mScrollToCursor;
	bool mScrollToTop;
	bool mTextChanged;
	float mTextStart;                   // position (in pixels) where a code line starts relative to the left of the TextEditor.
	float mLineNoStart;                   // position (in pixels) where a line number starts relative to the left of the TextEditor.
	float mCollapsorStart;
	bool mCursorPositionChanged;
	int mColorRangeMin, mColorRangeMax;
	SelectionMode mSelectionMode;
	bool mHandleKeyboardInputs;
	bool mHandleMouseInputs;
	bool mIgnoreImGuiChild;
	bool mCursorOnABracket;
	Coordinates mMatchingBracket;

	Palette mPaletteBase;
	Palette mPalette;
	LanguageDefinition mLanguageDefinition;
	RegexList mRegexList;
	
	OnBreakpoint mOnBreakpointToggle;

	bool mCheckComments;
	Breakpoints mBreakpoints;
	int  mTriggeredBPLine;
	Collapsables mCollapsables;
	CollapsedLines mActiveCollapsables;
	CollapsedLinesUn mIgnoredActiveCollapsables;
	ErrorMarkers mErrorMarkers;
	ImVec2 mCharAdvance;
	Coordinates mInteractiveStart, mInteractiveEnd;
	_stdstr mLineBuffer;
	uint64_t mStartTime;

	float mLastClick;

	std::vector<std::future<void>> mFutures;
};
