#include <textmate/textmate.hpp>
#include <regex/regex.hpp>

#if 1

namespace orbiter {

using namespace ion;

static RegEx HAS_BACK_REFERENCES  = RegEx(utf16(R"(\\(\d+))"), RegEx::Behaviour::none);
static RegEx BACK_REFERENCING_END = RegEx(utf16(R"(\\(\d+))"), RegEx::Behaviour::global);

using ScopeName 		= str;
using ScopePath 		= str;
using ScopePattern 		= str;
using Uint32Array  		= array<uint32_t>;
using EncodedTokenAttr 	= num;

/**
 * Helpers to manage the "collapsed" metadata of an entire StackElement stack.
 * The following assumptions have been made:
 *  - languageId < 256 => needs 8 bits
 *  - unique color count < 512 => needs 9 bits
 *
 * The binary format is:
 * - -------------------------------------------
 *     3322 2222 2222 1111 1111 1100 0000 0000
 *     1098 7654 3210 9876 5432 1098 7654 3210
 * - -------------------------------------------
 *     xxxx xxxx xxxx xxxx xxxx xxxx xxxx xxxx
 *     bbbb bbbb ffff ffff fFFF FBTT LLLL LLLL
 * - -------------------------------------------
 *  - L = LanguageId (8 bits)
 *  - T = StandardTokenType (2 bits)
 *  - B = Balanced bracket (1 bit)
 *  - F = FontStyle (4 bits)
 *  - f = foreground color (9 bits)
 *  - b = background color (9 bits)
 */
enum EncodedTokenDataConsts {
	LANGUAGEID_MASK = 0b00000000000000000000000011111111,
	TOKEN_TYPE_MASK = 0b00000000000000000000001100000000,
	BALANCED_BRACKETS_MASK = 0b00000000000000000000010000000000,
	FONT_STYLE_MASK = 0b00000000000000000111100000000000,
	FOREGROUND_MASK = 0b00000000111111111000000000000000,
	BACKGROUND_MASK = 0b11111111000000000000000000000000,

	LANGUAGEID_OFFSET = 0,
	TOKEN_TYPE_OFFSET = 8,
	BALANCED_BRACKETS_OFFSET = 10,
	FONT_STYLE_OFFSET = 11,
	FOREGROUND_OFFSET = 15,
	BACKGROUND_OFFSET = 24
};

enum StandardTokenType {
	Other = 0,
	Comment = 1,
	String = 2,
	_RegEx = 3
};

// Must have the same values as `StandardTokenType`!
enum OptionalStandardTokenType {
	_Other = 0,
	_Comment = 1,
	_String = 2,
	__RegEx = 3,
	// Indicates that no token type is set.
	NotSet = 8
};

enums(FindOption, None,
	None,
	NotBeginString,
	NotEndString,
	NotBeginPosition,
	DebugCall,
);

enums(FontStyle, _NotSet, 
	_NotSet,
	None,
	Italic,
	Bold,
	Underline,
	Strikethrough
);

struct IOnigCaptureIndex {
	num start;
	num end;
	num length; // redundant i think -- check its usage in vscode
};

OptionalStandardTokenType toOptionalTokenType(StandardTokenType standardType) {
	return (OptionalStandardTokenType)standardType;
}

StandardTokenType fromOptionalTokenType(OptionalStandardTokenType standardType) {
	return (StandardTokenType)standardType;
}

bool _matchesScope(ScopeName scopeName, ScopeName scopePattern) {
	return scopePattern == scopeName || (scopeName.has_prefix(scopePattern) && scopeName[scopePattern.len()] == '.');
}

num strArrCmp(array<str> a, array<str> b) {
	if (!a && !b) {
		return 0;
	}
	if (!a) {
		return -1;
	}
	if (!b) {
		return 1;
	}
	num len1 = a.len();
	num len2 = b.len();
	if (len1 == len2) {
		for (num i = 0; i < len1; i++) {
			int res = strcmp(a[i].cs(), b[i].cs());
			if (res != 0) {
				return res;
			}
		}
		return 0;
	}
	return len1 - len2;
}

template <typename T>
map<T> mergeObjects(map<T> dst, map<T> src0, map<T> src1) {
	for (field<T> &f: src0) {
		dst[f.key] = f.value;
	}
	for (field<T> &f: src1) {
		dst[f.key] = f.value;
	}
	return dst;
}

str basename(str path) {
	num i0  = path.index_of("/", -1);
	num i1  = path.index_of("\\", -1);
	num idx = i0 < 0 ? ~i0 : ~i1;
	if (idx == 0) {
		return path;
	} else if (~idx == path.len() - 1) {
		return basename(path.mid(0, path.len() - 1));
	} else {
		return path.mid(~idx + 1);
	}
}

RegEx CAPTURING_REGEX_SOURCE = { R"($(\d+)|\${(\d+):\/(downcase|upcase)})", RegEx::Behaviour::global };

struct RegexSource {
	static bool hasCaptures(str regexSource) {
		return false; // not working!  needs the same lambda interface for regex replace on string.  the var arg is a bit silly to support

		if (!regexSource) {
			return false;
		}
		CAPTURING_REGEX_SOURCE.reset();
		return CAPTURING_REGEX_SOURCE.test(regexSource);
	}

	/// trace this usage in js; we dont quite have this one interfaced the same way.

	static str replaceCaptures(str regexSource, str captureSource, array<IOnigCaptureIndex> captureIndices) {
		return str();
		#if 0
		return regexSource.replace(CAPTURING_REGEX_SOURCE, [&](str match, str index, str commandIndex, str command) -> str  {
			i64 i = index ? index.integer_value() : command_index.integer_value();
			IOnigCaptureIndex &capture = captureIndices[i];
			if (capture) {
				str result = captureSource.mid(capture.start, capture.end - capture.start);
				// Remove leading dots that would make the selector invalid
				while (result[0] == '.') {
					result = result.mid(1);
				}
				if (command == "downcase") return result.lcase();
				if (command == "upcase")   return result.ucase();
				return result;
			} else {
				return match;
			}
		});
		#endif
	}
};

bool isValidHexColor(str hex) {
	if (!hex) return false;
    RegEx rgbRegex     ("^#[0-9a-fA-F]{3}$");
    RegEx rgbaRegex    ("^#[0-9a-fA-F]{4}$");
    RegEx rrggbbRegex  ("^#[0-9a-fA-F]{6}$");
    RegEx rrggbbaaRegex("^#[0-9a-fA-F]{8}$");

    return rgbRegex.exec(hex)    || 
           rgbaRegex.exec(hex)   ||
           rrggbbRegex.exec(hex) ||
           rrggbbaaRegex.exec(hex);
}

namespace EncodedTokenAttributes {

	num getBackground(EncodedTokenAttr encodedTokenAttributes) {
		return (
			(encodedTokenAttributes & EncodedTokenDataConsts::BACKGROUND_MASK) >>
			EncodedTokenDataConsts::BACKGROUND_OFFSET
		);
	}

	num getLanguageId(EncodedTokenAttr encodedTokenAttributes) {
		return (
			(encodedTokenAttributes & EncodedTokenDataConsts::LANGUAGEID_MASK) >>
			EncodedTokenDataConsts::LANGUAGEID_OFFSET
		);
	}

	num getForeground(EncodedTokenAttr encodedTokenAttributes) {
		return (
			(encodedTokenAttributes & EncodedTokenDataConsts::FOREGROUND_MASK) >>
			EncodedTokenDataConsts::FOREGROUND_OFFSET
		);
	}

	StandardTokenType getTokenType(EncodedTokenAttr encodedTokenAttributes) {
		return StandardTokenType (
			(encodedTokenAttributes & EncodedTokenDataConsts::TOKEN_TYPE_MASK) >>
			EncodedTokenDataConsts::TOKEN_TYPE_OFFSET
		);
	}

	num getFontStyle(EncodedTokenAttr encodedTokenAttributes) {
		return (
			(encodedTokenAttributes & EncodedTokenDataConsts::FONT_STYLE_MASK) >>
			EncodedTokenDataConsts::FONT_STYLE_OFFSET
		);
	}

	str toBinaryStr(EncodedTokenAttr encodedTokenAttributes) {
		return str((i64)encodedTokenAttributes, 2, 32); /// generic base conversion in str
	}

	void print(EncodedTokenAttr encodedTokenAttributes) {
		auto languageId = EncodedTokenAttributes::getLanguageId(encodedTokenAttributes);
		auto tokenType  = EncodedTokenAttributes::getTokenType (encodedTokenAttributes);
		auto fontStyle  = EncodedTokenAttributes::getFontStyle (encodedTokenAttributes);
		auto foreground = EncodedTokenAttributes::getForeground(encodedTokenAttributes);
		auto background = EncodedTokenAttributes::getBackground(encodedTokenAttributes);

		//console.log({
		//	languageId: languageId,
		//	tokenType: tokenType,
		//	fontStyle: fontStyle,
		//	foreground: foreground,
		//	background: background,
		//});
	}

	bool containsBalancedBrackets(EncodedTokenAttr encodedTokenAttributes) {
		return (encodedTokenAttributes & EncodedTokenDataConsts::BALANCED_BRACKETS_MASK) != 0;
	}

	/**
	 * Updates the fields in `metadata`.
	 * A value of `0`, `NotSet` or `null` indicates that the corresponding field should be left as is.
	 */
	num set(
		EncodedTokenAttr encodedTokenAttributes,
		num languageId,
		OptionalStandardTokenType tokenType,
		mx containsBalancedBrackets,
		FontStyle fontStyle,
		num foreground,
		num background)
	{
		auto _languageId = EncodedTokenAttributes::getLanguageId(encodedTokenAttributes);
		auto _tokenType = EncodedTokenAttributes::getTokenType(encodedTokenAttributes);
		auto _containsBalancedBracketsBit =
			EncodedTokenAttributes::containsBalancedBrackets(encodedTokenAttributes) ? 1 : 0;
		auto _fontStyle = EncodedTokenAttributes::getFontStyle(encodedTokenAttributes);
		auto _foreground = EncodedTokenAttributes::getForeground(encodedTokenAttributes);
		auto _background = EncodedTokenAttributes::getBackground(encodedTokenAttributes);

		if (languageId != 0) {
			_languageId = languageId;
		}
		if (tokenType != OptionalStandardTokenType::NotSet) {
			_tokenType = fromOptionalTokenType(tokenType);
		}
		if (containsBalancedBrackets.type() == typeof(bool)) {
			_containsBalancedBracketsBit = containsBalancedBrackets ? 1 : 0;
		}
		if (fontStyle != FontStyle::_NotSet) {
			_fontStyle = fontStyle;
		}
		if (foreground != 0) {
			_foreground = foreground;
		}
		if (background != 0) {
			_background = background;
		}

		return (
			((_languageId << EncodedTokenDataConsts::LANGUAGEID_OFFSET) |
				(_tokenType << EncodedTokenDataConsts::TOKEN_TYPE_OFFSET) |
				(_containsBalancedBracketsBit <<
					EncodedTokenDataConsts::BALANCED_BRACKETS_OFFSET) |
				(_fontStyle << EncodedTokenDataConsts::FONT_STYLE_OFFSET) |
				(_foreground << EncodedTokenDataConsts::FOREGROUND_OFFSET) |
				(_background << EncodedTokenDataConsts::BACKGROUND_OFFSET)) >>
			0
		);
	}
}

/**
 * A map from scope name to a language id. Please do not use language id 0.
 */
using EmbeddedLanguagesMap = map<num>;

using TokenTypeMap = map<StandardTokenType>;

struct ILocation {
	str 			filename;
	num 			line;
	num 			chr;
};

struct ILocatable {
	ILocation 		_vscodeTextmateLocation;
};

using RuleId = num;

struct RawRule;
using RawCaptures    = map<mx>; // not wanting to use ILocatable metadata

struct RawRepository:mx {
	struct members {
		map<mx>			 props; /// of type RawRule
		void init() {
			props["$self"] = mx();
			props["$base"] = mx();
		}
		register(members)
	};
	mx_basic(RawRepository);
	operator bool();
	bool operator!();
};

struct RawRule:mx {
	struct members {
		ILocation			_vscodeTextmateLocation;
		RuleId 				id; // This is not part of the spec only used internally
		str 				include;
		utf16 				name;
		utf16 				contentName;
		utf16 				match;
		RawCaptures 		captures;
		utf16 				begin;
		RawCaptures	 		beginCaptures;
		utf16 				end;
		RawCaptures 		endCaptures;
		utf16 				_while;
		RawCaptures 		whileCaptures;
		array<RawRule>	 	patterns; /// <RawRule>
		RawRepository 		repository;
		bool 				applyEndPatternLast;
	};
	mx_basic(RawRule);
	operator  bool() { return data->name || data->include || data->id; }
	bool operator!() { return !operator bool(); }
};

struct RawGrammar:mx {
	struct members {
		ILocation 			_vscodeTextmateLocation;
	    RawRepository 		repository;
		ScopeName 			scopeName;
		array<RawRule> 		patterns;
		map<RawRule> 		injections;
		str 				injectionSelector;
		array<str> 			fileTypes;
		str 				name;
		str 				firstLineMatch;
		register(members)
	};
	mx_basic(RawGrammar);

	RawGrammar(null_t) : RawGrammar() { }
	
	operator bool() {
		return data->name;
	}
};

struct ScopeStack;

struct ThemeProvider:mx {
	lambda<StyleAttributes(ScopeStack&)> themeMatch;
	lambda<StyleAttributes()> getDefaults;
	mx_basic(ThemeProvider)
};

struct GrammarRepository:mx {
	struct members {
		lambda<RawGrammar(ScopeName)> 		lookup;
		lambda<array<ScopeName>(ScopeName)> injections;
	};
	mx_basic(GrammarRepository)
};

struct RawThemeStyles:mx {
	struct members {
		str fontStyle;
		str foreground;
		str background;
		register(members)
	};
	mx_basic(RawThemeStyles);
};

struct IRawThemeSetting {
	str 			name;
	mx 				scope; // can be ScopePattern or ScopePattern[] ! (so string or array of them)
	RawThemeStyles  styles;
	register(IRawThemeSetting)
};

struct IRawTheme {
	str name;
	array<IRawThemeSetting> settings;
};

struct RegistryOptions {
	IRawTheme theme;
	array<str> colorMap;
	future loadGrammar(ScopeName scopeName);
	lambda<array<ScopeName>(ScopeName)> getInjections;
};

struct ScopeStack:mx {
	struct members {
		mx 			parent;
		ScopeName 	scopeName;
		register(members)
	};

	mx_basic(ScopeStack);

	static ScopeStack push(ScopeStack path, array<ScopeName> scopeNames) {
		for (auto &name:scopeNames) {
			path = ScopeStack(path, name);
		}
		return path;
	}

	ScopeStack(ScopeStack parent, ScopeName scopeName) : ScopeStack() {
		data->parent    = parent.grab();
		data->scopeName = scopeName;
	}

	ScopeStack push(ScopeName scopeName) {
		return ScopeStack(*this, scopeName);
	}

	array<ScopeName> getSegments() {
		ScopeStack item = *this;
		array<ScopeName> result;
		while (item) {
			result.push(item->scopeName);
			if (!item->parent)
				break;
			item = item->parent.grab();
		}
		return result.reverse();
	}

	str toString() {
		return getSegments().join(' ');
	}

	bool extends(ScopeStack other) {
		if (mem == other.mem) {
			return true;
		}
		if (!data->parent) {
			return false;
		}
		ScopeStack p = data->parent.grab();
		return p.extends(other);
	}

	array<str> getExtensionIfDefined(ScopeStack base) {
		array<str> result;
		ScopeStack item = *this;
		while (item && item != base) {
			result.push(item->scopeName);
			if (!item->parent)
				return {};
			item = item->parent.grab();
		}
		return result.reverse();
	}
};

bool _scopePathMatchesParentScopes(ScopeStack scopePath, array<ScopeName> parentScopes) {
	if (!parentScopes) {
		return true;
	}

	num index = 0;
	str scopePattern = parentScopes[index];

	while (scopePath) {
		if (_matchesScope(scopePath->scopeName, scopePattern)) {
			index++;
			if (index == parentScopes.len()) {
				return true;
			}
			scopePattern = parentScopes[index];
		}
		scopePath = scopePath->parent.grab();
	}
	return false;
};

struct StyleAttributes:mx {
	struct members {
		states<FontStyle> fontStyle;
		num foregroundId;
		num backgroundId;
		bool is_null;

		operator  bool() { return !is_null; }
		bool operator!() { return  is_null; }
		
		register(members)
	};
	mx_basic(StyleAttributes);

	StyleAttributes(null_t) : StyleAttributes() {
		data->is_null = true;
	}

	StyleAttributes(states<FontStyle> fontStyle, num foregroundId, num backgroundId):StyleAttributes() {
		data->fontStyle = fontStyle;
		data->foregroundId = foregroundId;
		data->backgroundId = backgroundId;
	}
};

/**
 * Parse a raw theme into rules.
 */

struct ParsedThemeRule:mx {
	struct members {
		ScopeName 			scope;
		array<ScopeName> 	parentScopes;
		num 				index;
		states<FontStyle>	fontStyle;
		str 				foreground;
		str 				background;
		register(members)
	};
	mx_basic(ParsedThemeRule);
};

array<ParsedThemeRule> parseTheme(IRawTheme &source) {
	if (!source.settings)
		return {};
	array<IRawThemeSetting> &settings = source.settings;
	array<ParsedThemeRule> result;

	for (num i = 0, len = settings.len(); i < len; i++) {
		auto &entry = settings[i];

		if (!entry.styles) {
			continue;
		}

		array<str> scopes;
		if (entry.scope.type() == typeof(str)) {
			str _scope = entry.scope.grab();

			// remove leading commas
			_scope = _scope.replace(R"(^[,]+)", "");

			// remove trailing commans
			_scope = _scope.replace(R"([,]+$)", "");

			scopes = _scope.split(',');
		} else if (entry.scope.type()->traits & traits::array) {
			scopes = entry.scope.grab();
		} else {
			scopes = array<str> { "" };
		}

		states<FontStyle> fontStyle = {};
		if (entry.styles->fontStyle) {
			array<str> segments = entry.styles->fontStyle.split(" ");
			for (int j = 0, lenJ = segments.len(); j < lenJ; j++) {
				str &segment = segments[j];
				fontStyle[segment] = true;
				break;
			}
		}

		str foreground = null;
		if (isValidHexColor(entry.styles->foreground)) {
			foreground = entry.styles->foreground;
		}

		str background = null;
		if (isValidHexColor(entry.styles->background)) {
			background = entry.styles->background;
		}

		for (num j = 0, lenJ = scopes.len(); j < lenJ; j++) {
			str _scope = scopes[j].trim();

			array<str> segments = _scope.split(" ");

			str scope = segments[segments.len() - 1];
			array<str> parentScopes;
			if (segments.len() > 1) {
				parentScopes = segments.slice(0, segments.len() - 1);
				parentScopes.reverse();
			}
			ParsedThemeRule p = ParsedThemeRule::members {
				scope,
				parentScopes,
				i,
				fontStyle,
				foreground,
				background
			};
			result.push(p);

			// no overload on the pointer op, but overload on the '.' is possible
			//T operator.(SYMBOL s) { /// design-time symbol
			//}
		}
	}

	return result;
}

str fontStyleToString(states<FontStyle> fontStyle) {
	if (fontStyle[FontStyle::_NotSet]) {
		return "not set";
	}

	str style = "";
	if (fontStyle[FontStyle::Italic]) {
		style += "italic ";
	}
	if (fontStyle[FontStyle::Bold]) {
		style += "bold ";
	}
	if (fontStyle[FontStyle::Underline]) {
		style += "underline ";
	}
	if (fontStyle[FontStyle::Strikethrough]) {
		style += "strikethrough ";
	}
	if (style == "") {
		style = "none";
	}
	return style.trim();
};


struct ColorMap:mx {
	struct members {
		bool 		_isFrozen;
		num 		_lastColorId;
		array<str> 	_id2color;
		map<mx> 	_color2id;

		num getId(str color) {
			if (!color) {
				return 0;
			}
			color = color.ucase();
			mx value = _color2id[color];
			if (value) {
				return num(value);
			}
			if (_isFrozen) {
				console.fault("Missing color in color map - {0}", {color});
			}
			value = ++_lastColorId;
			_color2id[color] = value;
			_id2color[value] = color;
			return num(value);
		}

		array<str> getColorMap() {
			return _id2color.slice(0, _id2color.len());
		}
		register(members)
	};

	mx_basic(ColorMap);

	ColorMap(array<str> _colorMap):ColorMap() {
		data->_lastColorId = 0;

		if (_colorMap) {
			data->_isFrozen = true;
			for (num i = 0, len = _colorMap.len(); i < len; i++) {
				data->_color2id[_colorMap[i]] = i;
				data->_id2color.push(_colorMap[i]);
			}
		} else {
			data->_isFrozen = false;
		}
	}
};

struct ThemeTrieElementRule:mx {
	struct members {
		num 				scopeDepth;
		array<ScopeName> 	parentScopes;
		FontStyle 			fontStyle;
		num 				foreground;
		num 				background;
		bool				is_null;
		operator  bool() { return !is_null; }
		bool operator!() { return  is_null; }

		ThemeTrieElementRule clone() {
			return members {
				scopeDepth, parentScopes, fontStyle, foreground, background
			};
		}

		static array<ThemeTrieElementRule> cloneArr(array<ThemeTrieElementRule> arr) {
			array<ThemeTrieElementRule> r;
			for (num i = 0, len = arr.len(); i < len; i++) {
				r[i] = arr[i].clone();
			}
			return r;
		}

		void acceptOverwrite(num scopeDepth, FontStyle fontStyle, num foreground, num background) {
			if (scopeDepth > scopeDepth) {
				console.log("how did this happen?");
			} else {
				scopeDepth = scopeDepth;
			}
			// console.log("TODO -> my depth: " + scopeDepth + ", overwriting depth: " + scopeDepth);
			if (fontStyle != FontStyle::_NotSet) {
				fontStyle = fontStyle;
			}
			if (foreground != 0) {
				foreground = foreground;
			}
			if (background != 0) {
				background = background;
			}
		}
		
		register(members)
	};
	mx_basic(ThemeTrieElementRule);

	ThemeTrieElementRule(null_t) : ThemeTrieElementRule() {
		data->is_null = true;
	}
};


struct ThemeTrieElement:mx {
	using ITrieChildrenMap = map<ThemeTrieElement>;

	struct members {
		ThemeTrieElementRule 		_mainRule;
		array<ThemeTrieElementRule> _rulesWithParentScopes;
		ITrieChildrenMap 			_children;

		array<ThemeTrieElementRule> default_result() {
			array<ThemeTrieElementRule> a;
			a.push(_mainRule);
			for (ThemeTrieElementRule &r: _rulesWithParentScopes)
				a.push(r);
			return ThemeTrieElement::_sortBySpecificity(a);
		}

		array<ThemeTrieElementRule> match(ScopeName scope) {
			if (scope == "")
				return default_result();
			int dotIndex = scope.index_of(".");
			str head     = (dotIndex == -1) ? scope : scope.mid(0, dotIndex);
			str tail     = (dotIndex == -1) ? ""    : scope.mid(dotIndex + 1);
			if (_children(head))
				return _children[head].match(tail);
			return default_result();
		}

		void insert(num scopeDepth, ScopeName scope, array<ScopeName> parentScopes, FontStyle fontStyle, num foreground, num background) {
			if (scope == "") {
				_doInsertHere(scopeDepth, parentScopes, fontStyle, foreground, background);
				return;
			}
			int dotIndex = scope.index_of(".");
			str head = (dotIndex == -1) ? scope : scope.mid(0, dotIndex);
			str tail = (dotIndex == -1) ? ""    : scope.mid(dotIndex + 1);
			ThemeTrieElement child;
			if (_children(head)) {
				child = _children[head];
			} else {
				child = ThemeTrieElement(
					_mainRule.clone(),
					ThemeTrieElementRule::cloneArr(_rulesWithParentScopes));
				_children[head] = child;
			}

			child.insert(scopeDepth + 1, tail, parentScopes, fontStyle, foreground, background);
		}

		void _doInsertHere(num scopeDepth, array<ScopeName> parentScopes, FontStyle fontStyle, num foreground, num background) {

			if (!parentScopes) {
				// Merge into the main rule
				_mainRule.acceptOverwrite(scopeDepth, fontStyle, foreground, background);
				return;
			}

			// Try to merge into existing rule
			for (num i = 0, len = _rulesWithParentScopes.len(); i < len; i++) {
				ThemeTrieElementRule &rule = _rulesWithParentScopes[i];

				if (strArrCmp(rule->parentScopes, parentScopes) == 0) {
					rule.acceptOverwrite(scopeDepth, fontStyle, foreground, background);
					return;
				}
			}

			// Must add a new rule

			// Inherit from main rule
			if (fontStyle == FontStyle::_NotSet) {
				fontStyle = _mainRule->fontStyle;
			}
			if (foreground == 0) {
				foreground = _mainRule->foreground;
			}
			if (background == 0) {
				background = _mainRule->background;
			}

			ThemeTrieElementRule tt = ThemeTrieElementRule::members {
				scopeDepth, parentScopes, fontStyle, foreground, background
			};
			_rulesWithParentScopes.push(tt);
		}
		
		register(members)
	};

	mx_basic(ThemeTrieElement);

	ThemeTrieElement(
		ThemeTrieElementRule _mainRule,
		array<ThemeTrieElementRule> rulesWithParentScopes = {},
		ITrieChildrenMap _children = {}
	) : ThemeTrieElement() {
		data->_mainRule = _mainRule;
		data->_rulesWithParentScopes = rulesWithParentScopes;
		data->_children = _children;
	}

	static array<ThemeTrieElementRule> _sortBySpecificity(array<ThemeTrieElementRule> arr) {
		if (arr.len() == 1) {
			return arr;
		}
		arr.sort([](ThemeTrieElementRule a, ThemeTrieElementRule b) -> int {
			if (a->scopeDepth == b->scopeDepth) {
				auto &aParentScopes = a->parentScopes;
				auto &bParentScopes = b->parentScopes;
				num aParentScopesLen = !aParentScopes ? 0 : aParentScopes.len();
				num bParentScopesLen = !bParentScopes ? 0 : bParentScopes.len();
				if (aParentScopesLen == bParentScopesLen) {
					for (num i = 0; i < aParentScopesLen; i++) {
						num aLen = aParentScopes[i].len();
						num bLen = bParentScopes[i].len();
						if (aLen != bLen) {
							return bLen - aLen;
						}
					}
				}
				return bParentScopesLen - aParentScopesLen;
			}
			return b->scopeDepth - a->scopeDepth;
		});
		return arr;
	}
};


struct Theme:mx {
	struct members {
		ColorMap 			_colorMap;
		StyleAttributes	 	_defaults;
		ThemeTrieElement 	_root;
		lambda<array<ThemeTrieElementRule>(ScopeName)> _cachedMatchRoot;

		array<str> getColorMap() {
			return _colorMap.getColorMap();
		}

		StyleAttributes getDefaults() {
			return _defaults;
		}

		StyleAttributes match(ScopeStack scopePath) {
			if (!scopePath) {
				return _defaults;
			}
			auto scopeName = scopePath->scopeName;
			array<ThemeTrieElementRule> matchingTrieElements = _cachedMatchRoot(scopeName);

			/// select first should not return R but use R for its lambda; it should always return the model; t
			ThemeTrieElementRule effectiveRule = matchingTrieElements.select_first<ThemeTrieElementRule>([&](ThemeTrieElementRule &v) -> ThemeTrieElementRule {
				if (scopePath->parent && _scopePathMatchesParentScopes(scopePath->parent.grab(), v->parentScopes))
					return v;
				return null;
			});
			if (!effectiveRule) {
				return null;
			}
			return StyleAttributes(
				effectiveRule->fontStyle,
				effectiveRule->foreground,
				effectiveRule->background
			);
		}

		register(members)

		static Theme createFromRawTheme(
			IRawTheme source,
			array<str> colorMap
		)  {
			return createFromParsedTheme(parseTheme(source), colorMap);
		}

		static Theme createFromParsedTheme(
			array<ParsedThemeRule> source,
			array<str> colorMap
		) {
			return resolveParsedThemeRules(source, colorMap);
		}

		/**
		 * Resolve rules (i.e. inheritance).
		 */
		static Theme resolveParsedThemeRules(array<ParsedThemeRule> parsedThemeRules, array<str> _colorMap) {

			// Sort rules lexicographically, and then by index if necessary
			parsedThemeRules.sort([](ParsedThemeRule &a, ParsedThemeRule &b) -> int {
				num r = strcmp(a->scope.cs(), b->scope.cs());
				if (r != 0) {
					return r;
				}
				r = strArrCmp(a->parentScopes, b->parentScopes);
				if (r != 0) {
					return r;
				}
				return a->index - b->index;
			});

			// Determine defaults
			states<FontStyle> defaultFontStyle;
			//FontStyle::None;
			str defaultForeground = "#000000";
			str defaultBackground = "#ffffff";
			while (parsedThemeRules.len() >= 1 && parsedThemeRules[0]->scope == "") {
				ParsedThemeRule incomingDefaults = parsedThemeRules.shift();
				if (!incomingDefaults->fontStyle[FontStyle::_NotSet]) {
					defaultFontStyle = incomingDefaults->fontStyle;
				}
				if (incomingDefaults->foreground != null) {
					defaultForeground = incomingDefaults->foreground;
				}
				if (incomingDefaults->background != null) {
					defaultBackground = incomingDefaults->background;
				}
			}
			ColorMap 		colorMap = ColorMap(_colorMap);
			StyleAttributes defaults = StyleAttributes(defaultFontStyle, colorMap.getId(defaultForeground), colorMap.getId(defaultBackground));

			ThemeTrieElement root    = ThemeTrieElement(ThemeTrieElementRule::members { 0, null, FontStyle::_NotSet, 0, 0 }, {});
			for (num i = 0, len = parsedThemeRules.len(); i < len; i++) {
				ParsedThemeRule rule = parsedThemeRules[i];
				root.insert(0, rule->scope, rule->parentScopes,
					rule->fontStyle, colorMap.getId(rule->foreground), colorMap.getId(rule->background));
			}

			return Theme::members { colorMap, defaults, root };
		}

		/// this is so you can trivially construct and still get logic to initialize from there
		void init() {
			_cachedMatchRoot = [&](ScopeName scopeName) -> array<ThemeTrieElementRule> { return _root.match(scopeName); };
		}
	};
	mx_basic(Theme);
};


using ITrieChildrenMap = map<ThemeTrieElement>;

enums(IncludeReferenceKind, Base,
	Base,
	Self,
	RelativeReference,
	TopLevelReference,
	TopLevelRepositoryReference);

struct IncludeReference:mx {
	struct members {
		IncludeReferenceKind kind;
		str ruleName;
		str scopeName; // not needed
		register(members)
	};
	IncludeReference(IncludeReferenceKind::etype kind) : IncludeReference() {
		data->kind = kind;
	}
	mx_basic(IncludeReference);
};

struct BaseReference:IncludeReference {
	BaseReference() : IncludeReference(IncludeReferenceKind::Base) { }
};

struct SelfReference:IncludeReference {
	SelfReference() : IncludeReference(IncludeReferenceKind::Self) { }
};

struct RelativeReference:IncludeReference {
	RelativeReference(str ruleName) : 
			IncludeReference(IncludeReferenceKind::RelativeReference) {
		IncludeReference::data->ruleName = ruleName;
	}
};

struct TopLevelReference:IncludeReference {
	TopLevelReference(str scopeName) : 
			IncludeReference(IncludeReferenceKind::TopLevelReference) {
		IncludeReference::data->scopeName = scopeName;
	}
};

struct TopLevelRepositoryReference:IncludeReference {
	TopLevelRepositoryReference(str scopeName, str ruleName) : 
			IncludeReference(IncludeReferenceKind::TopLevelRepositoryReference) {
		IncludeReference::data->scopeName = scopeName;
		IncludeReference::data->ruleName = ruleName;
	}
};

IncludeReference parseInclude(utf16 include) {
	if (include == "$base") {
		return BaseReference();
	} else if (include == "$self") {
		return SelfReference();
	}

	auto indexOfSharp = str(include).index_of("#");
	if (indexOfSharp == -1) {
		return TopLevelReference(include);
	} else if (indexOfSharp == 0) {
		return RelativeReference(include.mid(1));
	} else {
		str scopeName = include.mid(0, indexOfSharp);
		str ruleName = include.mid(indexOfSharp + 1);
		return TopLevelRepositoryReference(scopeName, ruleName);
	}
}


struct TopLevelRuleReference:mx {
	struct members {
		ScopeName scopeName;
		register(members)
	};
	mx_basic(TopLevelRuleReference)
	
	TopLevelRuleReference(ScopeName scopeName) : TopLevelRuleReference() {
		data->scopeName = scopeName;
	}

	str toKey() {
		return data->scopeName;
	}
};

/**
 * References a rule of a grammar in the top level repository section with the given name.
*/
struct TopLevelRepositoryRuleReference:TopLevelRuleReference {
	struct members {
		str ruleName;
		register(members)
	};

	TopLevelRepositoryRuleReference(
		ScopeName 	scopeName,
		str 		ruleName) : TopLevelRepositoryRuleReference() {
		TopLevelRuleReference::data->scopeName = scopeName;
		data->ruleName = ruleName;
	}
	
	mx_object(TopLevelRepositoryRuleReference, TopLevelRuleReference, members)

	str toKey() {
		return fmt { "{0}#{1}", {
			TopLevelRuleReference::data->scopeName,
			TopLevelRepositoryRuleReference::data->ruleName} };
	}
};

struct ExternalReferenceCollector:mx {
	struct members {
		array<TopLevelRuleReference> references;
		array<str>					 seenReferenceKeys;
		array<RawRule> 			 	 visitedRule;
		register(members)
	};

	mx_basic(ExternalReferenceCollector)

	void add(TopLevelRuleReference reference) {
		str key = reference.toKey();
		if (data->seenReferenceKeys.index_of(key) >= 0) {
			return;
		}
		data->seenReferenceKeys.push(key);
		data->references.push(reference);
	}
};

void collectExternalReferencesInTopLevelRule(RawGrammar &baseGrammar, RawGrammar &selfGrammar, ExternalReferenceCollector result);
void collectExternalReferencesInTopLevelRepositoryRule(
	str 			ruleName,
	RawGrammar      baseGrammar,	/// omitted 'ContextWithRepository' and added the args here
	RawGrammar      selfGrammar,
	map<RawRule> 	repository,
	ExternalReferenceCollector result
);

RawRepository::operator bool() { /// todo: IRawRule* ptr allocation check
	return data->props;
}

bool RawRepository::operator!() {
	return !(operator bool());
}

void collectExternalReferencesInRules(
	array<RawRule>  rules,
	RawGrammar      baseGrammar,	/// omitted 'ContextWithRepository' and added the args here
	RawGrammar      selfGrammar,
	RawRepository 	repository,
	ExternalReferenceCollector result
) {
	for (RawRule &rule: rules) {
		if (result->visitedRule.index_of(rule) >= 0) {
			continue;
		}
		result->visitedRule.push(rule);

		map<mx> m;
		// creates mutable map and i wonder if it needs to be that way
		RawRepository patternRepository;
		patternRepository->props = rule->repository->props->count() ? 
			mergeObjects(m, repository->props, rule->repository->props) : repository->props;

		if (rule->patterns) {
			collectExternalReferencesInRules(rule->patterns, baseGrammar, selfGrammar, patternRepository, result);
		}

		str &include = rule->include;
		if (!include)
			continue;

		auto reference = parseInclude(include);
		switch (reference->kind) {
			case IncludeReferenceKind::Base:
				collectExternalReferencesInTopLevelRule(baseGrammar, baseGrammar, result); // not a bug.
				break;
			case IncludeReferenceKind::Self:
				collectExternalReferencesInTopLevelRule(baseGrammar, selfGrammar, result);
				break;
			case IncludeReferenceKind::RelativeReference:
				collectExternalReferencesInTopLevelRepositoryRule(reference->ruleName, baseGrammar, selfGrammar, patternRepository, result);
				break;
			case IncludeReferenceKind::TopLevelReference:
			case IncludeReferenceKind::TopLevelRepositoryReference:
				RawGrammar selfGrammar =
					reference->scopeName == selfGrammar->scopeName
						? selfGrammar
						: reference->scopeName == baseGrammar->scopeName
						? baseGrammar
						: null;
				if (selfGrammar) {
					if (reference->kind == IncludeReferenceKind::TopLevelRepositoryReference) {
						collectExternalReferencesInTopLevelRepositoryRule(
							reference->ruleName, baseGrammar, selfGrammar, patternRepository, result);
					} else {
						collectExternalReferencesInTopLevelRule(baseGrammar, selfGrammar, result);
					}
				} else {
					if (reference->kind == IncludeReferenceKind::TopLevelRepositoryReference) {
						result.add(TopLevelRepositoryRuleReference(reference->scopeName, reference->ruleName));
					} else {
						result.add(TopLevelRuleReference(reference->scopeName));
					}
				}
				break;
		}
	}
};

void collectExternalReferencesInTopLevelRule(RawGrammar &baseGrammar, RawGrammar &selfGrammar, ExternalReferenceCollector result) {
	if (selfGrammar->patterns) { //&& Array.isArray(context.selfGrammar.patterns)) { [redundant check]
		collectExternalReferencesInRules(
			selfGrammar->patterns,
			baseGrammar,
			selfGrammar,
			selfGrammar->repository,
			result
		);
	}
	if (selfGrammar->injections) {
		array<RawRule> values = selfGrammar->injections->values();
		collectExternalReferencesInRules(
			values,
			baseGrammar,
			selfGrammar,
			selfGrammar->repository,
			result
		);
	}
}

void collectReferencesOfReference(
	TopLevelRuleReference 		reference,
	ScopeName 					baseGrammarScopeName,
	GrammarRepository 		    grammar_repo,
	ExternalReferenceCollector 	result)
{
	RawGrammar selfGrammar = grammar_repo->lookup(reference->scopeName);
	if (!selfGrammar) {
		if (reference->scopeName == baseGrammarScopeName) {
			console.fault("No grammar provided for <{0}>", {baseGrammarScopeName});
		}
		return;
	}

	RawGrammar baseGrammar = grammar_repo->lookup(baseGrammarScopeName);

	if (reference.type() == typeof(TopLevelRuleReference)) {
		collectExternalReferencesInTopLevelRule(baseGrammar, selfGrammar, result);
	} else {
		TopLevelRepositoryRuleReference r = reference.grab();
		collectExternalReferencesInTopLevelRepositoryRule(
			r->ruleName, baseGrammar, selfGrammar, selfGrammar->repository,
			result
		);
	}

	auto injections = grammar_repo->injections(reference->scopeName);
	if (injections) {
		for (ScopeName &scopeName: injections) {
			result.add(TopLevelRuleReference(scopeName));
		}
	}
}

/// the interfaces we use multiple inheritence on the data member; thus isolating diamonds.
struct Context:mx {
	struct members {
		RawGrammar baseGrammar;
		RawGrammar selfGrammar;
		register(members)
	};
	mx_basic(Context);
};

struct ContextWithRepository {
	RawGrammar   	baseGrammar;
	RawGrammar   	selfGrammar;
	map<RawRule> 	repository;
};


void collectExternalReferencesInTopLevelRepositoryRule(
	str 			ruleName,
	RawGrammar      baseGrammar,	/// omitted 'ContextWithRepository' and added the args here
	RawGrammar      selfGrammar,
	RawRepository 	repository,
	ExternalReferenceCollector result
) {
	if (repository->props(ruleName)) {
		array<RawRule> rules { repository->props[ruleName] };
		collectExternalReferencesInRules(rules, baseGrammar, selfGrammar, repository, result);
	}
}

struct ScopeDependencyProcessor:mx {
	struct members {
		GrammarRepository grammar_repo;
		ScopeName initialScopeName;
		array<ScopeName> seenFullScopeRequests;
		array<str> seenPartialScopeRequests;
		array<TopLevelRuleReference> Q;

		void processQueue() {
			auto q = Q;
			Q = array<TopLevelRuleReference> {};

			ExternalReferenceCollector deps;
			for (TopLevelRuleReference &dep: q) {
				collectReferencesOfReference(dep, initialScopeName, grammar_repo, deps);
			}

			for (TopLevelRuleReference &dep: deps->references) {
				if (dep.type() == typeof(TopLevelRuleReference)) {
					if (seenFullScopeRequests.has(dep->scopeName)) {
						// already processed
						continue;
					}
					seenFullScopeRequests.push(dep->scopeName);
					Q.push(dep);
				} else {
					if (seenFullScopeRequests.has(dep->scopeName)) {
						// already processed in full
						continue;
					}
					if (seenPartialScopeRequests.has(dep.toKey())) {
						// already processed
						continue;
					}
					str k = dep.toKey();
					seenPartialScopeRequests.push(k);
					Q.push(dep);
				}
			}
		}
		register(members)
	};

	mx_basic(ScopeDependencyProcessor);

	ScopeDependencyProcessor(
		GrammarRepository grammar_repo,
		ScopeName initialScopeName
	) : ScopeDependencyProcessor() {
		data->grammar_repo = grammar_repo;
		data->seenFullScopeRequests.push(data->initialScopeName);
		data->Q = array<TopLevelRuleReference> { TopLevelRuleReference(data->initialScopeName) };
	}
};

using OnigString = str;

struct IOnigMatch {
	num index = -1;
	array<IOnigCaptureIndex> captureIndices;
	operator bool() {
		return index >= 0;
	}
	bool operator!() {
		return index < 0;
	}
};

/// renamed to IOnigScanner
struct OnigScanner:mx {
	struct members {
		RegEx regex;
		register(members)

		IOnigMatch findNextMatchSync(str string, num startPosition, states<FindOption> options) {
			regex.set_cursor(startPosition);
			array<indexed<utf16>> matches = regex.exec(string);
			if (matches) {
				IOnigMatch result {
					.index = regex.pattern_index(),
					.captureIndices = { matches.len() }
				};
				for (indexed<utf16> &m: matches) {
					IOnigCaptureIndex capture_index = {
						m.index, m.index + m.length, m.length
					};
					result.captureIndices.push(capture_index);
				}
				return result;
			}
			return {};
		}
	};

	mx_basic(OnigScanner)

	OnigScanner(array<utf16> scan) : OnigScanner() {
		data->regex = RegEx(scan); /// needs to set the scan mode here
	}
};

struct OnigLib:mx {
	struct members {
		lambda<OnigScanner(array<str>)> createOnigScanner;
		lambda<OnigString(str)> 		createOnigString;
	};
	mx_basic(OnigLib)
};

// This is a special constant to indicate that the end regexp matched.
static const num endRuleId = -1;

// This is a special constant to indicate that the while regexp matched.
static const num whileRuleId = -2;

static RuleId ruleIdFromNumber(num id) { // this doesnt square with the above type def
	return id;
}

static num ruleIdToNumber(RuleId id) {
	return id;
}

struct IFindNextMatchResult {
	RuleId ruleId;
	array<IOnigCaptureIndex> captureIndices;
	operator bool() {
		return captureIndices;
	}
};

struct CompiledRule:mx {
	struct members {
		OnigScanner scanner;
		array<utf16> regExps;
		array<RuleId> rules;
		operator  bool() { return regExps.len() >  0; }
		bool operator!() { return regExps.len() == 0; }

		register(members)

		utf16 toString() {
			array<utf16> r;
			for (num i = 0, len = rules.len(); i < len; i++) {
				utf16 vstr = fmt { "   - {0}: {1}", { rules[i] , str(regExps[i]) }};
				r.push(vstr);
			}
			return utf16::join(r, utf16("\n"));
		}

		IFindNextMatchResult findNextMatchSync(
			utf16 string,
			num startPosition,
			states<FindOption> options
		) {
			IOnigMatch result = scanner.findNextMatchSync(string, startPosition, options);
			if (!result) {
				return {};
			}

			return {
				.ruleId = rules[result.index],
				.captureIndices = result.captureIndices,
			};
		}
	};

	mx_basic(CompiledRule)

	CompiledRule(null_t) : CompiledRule() { }

	CompiledRule(OnigLib oni_lib, array<utf16> regExps, array<RuleId> rules) : CompiledRule() {
		data->scanner = oni_lib->createOnigScanner(regExps);
		data->regExps = regExps;
		data->rules   = rules;
	}
};

struct IRegExpSourceAnchorCache {
	utf16 A0_G0;
	utf16 A0_G1;
	utf16 A1_G0;
	utf16 A1_G1;
	bool is_null = false;
	register(IRegExpSourceAnchorCache);
	
	operator  bool() { return !is_null; }
	bool operator!() { return  is_null; }
};

struct RegExpSource:mx {
	struct members {
		utf16 		source;
		RuleId 		ruleId;
		bool 		hasAnchor;
		bool 		hasBackReferences;
		IRegExpSourceAnchorCache _anchorCache;
		register(members)

		RegExpSource clone() {
			return RegExpSource(source, ruleId);
		}

		void setSource(utf16 newSource) {
			if (source == newSource) {
				return;
			}
			source = newSource;

			if (hasAnchor) {
				_anchorCache = _buildAnchorCache();
			}
		}

		/// todo: test me
		utf16 resolveBackReferences(utf16 lineText, array<IOnigCaptureIndex> captureIndices) {
			array<utf16> capturedValues = captureIndices.map<utf16>(
				[&](IOnigCaptureIndex &capture) -> utf16 {
					return lineText.mid(capture.start, capture.length);
				});
			BACK_REFERENCING_END.reset(); // support this! (was last_index = 0)

			/// todo: needs var arg support, or an array arg for this purpose (preferred fallback case for > 2 args)
			return BACK_REFERENCING_END.replace(source,
				lambda<utf16(utf16, utf16)>([capturedValues] (utf16 match, utf16 g1) -> utf16 {
					num k = str(g1).integer_value();
					return RegEx::escape(capturedValues.len() > k ? capturedValues[k] : utf16(str("")));
				}));
		}

		IRegExpSourceAnchorCache _buildAnchorCache() {
			array<utf16> A0_G0_result;
			array<utf16> A0_G1_result;
			array<utf16> A1_G0_result;
			array<utf16> A1_G1_result;

			num pos = 0;
			num len = 0;
			utf16::char_t ch;
			utf16::char_t nextCh;

			for (pos = 0, len = source.len(); pos < len; pos++) {
				ch = source[pos];
				A0_G0_result[pos] = ch;
				A0_G1_result[pos] = ch;
				A1_G0_result[pos] = ch;
				A1_G1_result[pos] = ch;

				if (ch == '\\') {
					if (pos + 1 < len) {
						nextCh = source[pos + 1];
						if (nextCh == 'A') {
							A0_G0_result[pos + 1] = (wchar)0xFFFF;
							A0_G1_result[pos + 1] = (wchar)0xFFFF;
							A1_G0_result[pos + 1] = str("A");
							A1_G1_result[pos + 1] = str("A");
						} else if (nextCh == 'G') {
							A0_G0_result[pos + 1] = (wchar)0xFFFF;
							A0_G1_result[pos + 1] = str("G");
							A1_G0_result[pos + 1] = (wchar)0xFFFF;
							A1_G1_result[pos + 1] = str("G");
						} else {
							A0_G0_result[pos + 1] = nextCh;
							A0_G1_result[pos + 1] = nextCh;
							A1_G0_result[pos + 1] = nextCh;
							A1_G1_result[pos + 1] = nextCh;
						}
						pos++;
					}
				}
			}

			return IRegExpSourceAnchorCache {
				.A0_G0 = A0_G0_result.join(),
				.A0_G1 = A0_G1_result.join(),
				.A1_G0 = A1_G0_result.join(),
				.A1_G1 = A1_G1_result.join(),
				.is_null = false
			};
		}

		utf16 resolveAnchors(bool allowA, bool allowG) {
			if (!hasAnchor || !_anchorCache)
				return source;

			if (allowA) {
				if (allowG) {
					return _anchorCache.A1_G1;
				} else {
					return _anchorCache.A1_G0;
				}
			} else {
				if (allowG) {
					return _anchorCache.A0_G1;
				} else {
					return _anchorCache.A0_G0;
				}
			}
		}
	};

	mx_basic(RegExpSource)

	RegExpSource(utf16 regExpSource, RuleId ruleId) : RegExpSource() {
		if (regExpSource) {
			num 			len 		  = regExpSource.len();
			num 			lastPushedPos = 0;
			bool 			hasAnchor 	  = false;
			array<utf16> 	output;
			for (num pos = 0; pos < len; pos++) {
				wchar ch = regExpSource[pos];

				if (ch == '\\') {
					if (pos + 1 < len) {
						wchar nextCh = regExpSource[pos + 1];
						if (nextCh == 'z') {
							utf16 s = regExpSource.mid(lastPushedPos, pos - lastPushedPos);
							output.push(s);
							utf16 s2 = str("$(?!\\n)(?<!\\n)");
							output.push(s2);
							lastPushedPos = pos + 2;
						} else if (nextCh == 'A' || nextCh == 'G') {
							hasAnchor = true;
						}
						pos++;
					}
				}
			}

			data->hasAnchor = hasAnchor;
			if (lastPushedPos == 0) {
				// No \z hit
				data->source = regExpSource;
			} else {
				output += regExpSource.mid(lastPushedPos, len);
				data->source = utf16::join(output, str(""));
			}
		} else {
			data->hasAnchor = false;
			data->source = regExpSource;
		}

		if (data->hasAnchor) {
			data->_anchorCache = _buildAnchorCache();
		} else {
			data->_anchorCache = {};
		}

		data->ruleId = ruleId;
		data->hasBackReferences = !!HAS_BACK_REFERENCES.exec(data->source);

		// console.log('input: ' + regExpSource + ' => ' + data->source + ', ' + data->hasAnchor);
	}
};

struct IRegExpSourceListAnchorCache {
	CompiledRule A0_G0;
	CompiledRule A0_G1;
	CompiledRule A1_G0;
	CompiledRule A1_G1;
	register(IRegExpSourceListAnchorCache)
};

struct RegExpSourceList:mx {
	struct members {
		array<RegExpSource> 			_items;
		bool 							_hasAnchors;
		CompiledRule 					_cached;
		IRegExpSourceListAnchorCache 	_anchorCache;
		register(members)

		void dispose() {
			_disposeCaches();
		}

		void _disposeCaches() {
			if (_cached)
				_cached = null;
			_anchorCache.A0_G0 = null;
			_anchorCache.A0_G1 = null;
			_anchorCache.A1_G0 = null;
			_anchorCache.A1_G1 = null;
		}

		void push(RegExpSource item) {
			_items.push(item);
			_hasAnchors = _hasAnchors || item->hasAnchor;
		}

		void unshift(RegExpSource item) {
			_items = _items.unshift(item);
			_hasAnchors = _hasAnchors || item->hasAnchor;
		}

		num length() {
			return _items.len();
		}

		void setSource(num index, utf16 newSource) {
			if (_items[index]->source != newSource) {
				// bust the cache
				_disposeCaches();
				_items[index].setSource(newSource);
			}
		}

		CompiledRule _resolveAnchors(OnigLib oni_lib, bool allowA, bool allowG) {
			array<utf16> regExps = _items.map<utf16>([&](RegExpSource e) -> utf16 {
				return e.resolveAnchors(allowA, allowG);
			});
			return CompiledRule(oni_lib, regExps, _items.map<RuleId>([](RegExpSource &e) -> RuleId {
				return e->ruleId;
			}));
		}
	};

	mx_basic(RegExpSourceList)


	CompiledRule compile(OnigLib oni_lib) {
		if (!data->_cached) {
			array<utf16> regExps = data->_items.map<utf16>(
				[](RegExpSource e) -> utf16 {
					return e->source;
				}
			);
			data->_cached = CompiledRule(oni_lib, regExps, data->_items.map<RuleId>(
				[](RegExpSource e) -> RuleId {
					return e->ruleId;
				}
			));
		}
		return data->_cached;
	}

	CompiledRule compileAG(OnigLib oni_lib, bool allowA, bool allowG) {
		if (!data->_hasAnchors) {
			return compile(oni_lib);
		} else {
			if (allowA) {
				if (allowG) {
					if (!data->_anchorCache.A1_G1) {
						data->_anchorCache.A1_G1 = _resolveAnchors(oni_lib, allowA, allowG);
					}
					return data->_anchorCache.A1_G1;
				} else {
					if (!data->_anchorCache.A1_G0) {
						data->_anchorCache.A1_G0 = _resolveAnchors(oni_lib, allowA, allowG);
					}
					return _data->anchorCache.A1_G0;
				}
			} else {
				if (allowG) {
					if (!data->_anchorCache.A0_G1) {
						data->_anchorCache.A0_G1 = _resolveAnchors(oni_lib, allowA, allowG);
					}
					return data->_anchorCache.A0_G1;
				} else {
					if (!data->_anchorCache.A0_G0) {
						data->_anchorCache.A0_G0 = _resolveAnchors(oni_lib, allowA, allowG);
					}
					return data->_anchorCache.A0_G0;
				}
			}
		}
	}

};

struct RuleRegistry;

struct Rule:mx {
	struct members {
		ILocation      *_location;
		RuleId          id;
		bool 	        _nameIsCapturing;
		utf16  	        _name;
		bool 	        _contentNameIsCapturing;
		utf16  	        _contentName;
		array<RuleId>	patterns;
		bool 	  		hasMissingPatterns; /// there was duck typing in ts for this one
		register(members)


		utf16 debugName() {
			if (_location)
				return fmt {"{0}:{1}", { basename(_location->filename), _location->line }};
			return str("unknown");
		}

		utf16 getName(utf16 lineText, array<IOnigCaptureIndex> captureIndices) {
			if (!_nameIsCapturing || !_name || !lineText || !captureIndices) {
				return _name;
			}
			/// not used in cpp.json
			return RegexSource::replaceCaptures(_name, lineText, captureIndices);
		}

		utf16 getContentName(utf16 lineText, array<IOnigCaptureIndex> captureIndices) {
			if (!_contentNameIsCapturing || !_contentName) {
				return _contentName;
			}
			/// not used in cpp.json
			return RegexSource::replaceCaptures(_contentName, lineText, captureIndices);
		}

	};

	mx_basic(Rule);

	void init(ILocation &_location, RuleId id, utf16 name, utf16 contentName) {
		data->_location = &_location;
		data->id = id;
		data->_name = name;
		data->_nameIsCapturing = RegexSource::hasCaptures(data->_name);
		data->_contentName = contentName;
		data->_contentNameIsCapturing = RegexSource::hasCaptures(data->_contentName);
	}

	Rule(ILocation &_location, RuleId id, utf16 name, utf16 contentName) : Rule() {
		init(_location, id, name, contentName);
	}

	virtual void collectPatterns(RuleRegistry &grammar, RegExpSourceList &out) { };
	virtual CompiledRule compile(RuleRegistry &grammar, utf16 endRegexSource) { };
	virtual CompiledRule compileAG(RuleRegistry &grammar, utf16 endRegexSource, bool allowA, bool allowG) { };
};

struct RuleRegistry:mx {
	struct members {
		lambda<Rule(RuleId)> getRule;
		lambda<Rule(lambda<Rule(RuleId)>)> registerRule;
	};
	mx_basic(RuleRegistry)
};

struct GrammarRegistry:mx {
	struct members {
		lambda<RawGrammar(str, RawRepository)> getExternalGrammar;
	};
	mx_basic(GrammarRegistry)
};

struct RuleFactoryHelper:mx {
	struct members {
		RuleRegistry 	rule_reg;
		GrammarRegistry grammar_reg;
	};
	mx_basic(RuleFactoryHelper)
};

struct ICompilePatternsResult {
	array<RuleId> patterns;
	bool hasMissingPatterns;
};

struct CaptureRule : Rule {
	struct members {
		RuleId retokenizeCapturedWithRuleId;
		register(members)
	};
	mx_object(CaptureRule, Rule, members);

	CaptureRule(ILocation &_location, RuleId id, utf16 name,
			utf16 contentName, RuleId retokenizeCapturedWithRuleId) : CaptureRule() {
		/// use this pattern when doing poly construction with mx
		/// would be nice to define the args at macro level; enforces impl
		Rule::init(_location, id, name, contentName);
		data->retokenizeCapturedWithRuleId = retokenizeCapturedWithRuleId;
	}

	void dispose() {
		// nothing to dispose
	}

	void collectPatterns(RuleRegistry &grammar, RegExpSourceList &out) {
		console.fault("Not supported!");
	}

	CompiledRule compile(RuleRegistry &grammar, utf16 endRegexSource) {
		console.fault("Not supported!");
	}

	CompiledRule compileAG(RuleRegistry &grammar, utf16 endRegexSource, bool allowA, bool allowG) {
		console.fault("Not supported!");
	}
};

struct MatchRule:Rule {
	struct members {
		RegExpSource 		_match;
		array<CaptureRule> 	captures;
		RegExpSourceList 	_cachedCompiledPatterns;
		register(members)

		void dispose() {
			if (_cachedCompiledPatterns) {
				_cachedCompiledPatterns.dispose();
				_cachedCompiledPatterns = null;
			}
		}

		utf16 debugMatchRegExp() {
			return _match->source;
		}

		RegExpSourceList _getCachedCompiledPatterns(RuleRegistry grammar) {
			if (!_cachedCompiledPatterns) {
				_cachedCompiledPatterns = RegExpSourceList();
				collectPatterns(grammar, _cachedCompiledPatterns);
			}
			return _cachedCompiledPatterns;
		}
	};

	mx_object(MatchRule, Rule, members);

	MatchRule(ILocation &_location, RuleId id, utf16 name, utf16 match, array<CaptureRule> captures):MatchRule() {
		Rule::init(_location, id, name, utf16());
		data->_match = RegExpSource(match, id);
		data->captures = captures;
		data->_cachedCompiledPatterns = null;
	}

	void collectPatterns(RuleRegistry &grammar, RegExpSourceList out) {
		out.push(data->_match);
	}

	CompiledRule compile(OnigLib oni_lib, RuleRegistry grammar, utf16 endRegexSource) {
		return data->_getCachedCompiledPatterns(grammar).compile(oni_lib);
	}

	CompiledRule compileAG(OnigLib oni_lib, RuleRegistry grammar, utf16 endRegexSource, bool allowA, bool allowG) {
		return data->_getCachedCompiledPatterns(grammar).compileAG(oni_lib, allowA, allowG);
	}
};

struct IncludeOnlyRule:Rule {
	struct members {
		RegExpSourceList _cachedCompiledPatterns;
		register(members)
		
		void dispose() {
			if (_cachedCompiledPatterns) {
				_cachedCompiledPatterns.dispose();
				_cachedCompiledPatterns = null;
			}
		}

		RegExpSourceList _getCachedCompiledPatterns(RuleRegistry grammar) {
			if (!_cachedCompiledPatterns) {
				_cachedCompiledPatterns = RegExpSourceList();
				collectPatterns(grammar, _cachedCompiledPatterns);
			}
			return _cachedCompiledPatterns;
		}
	};

	mx_object(IncludeOnlyRule, Rule, members)

	IncludeOnlyRule(ILocation &_location, RuleId id, utf16 name, utf16 contentName, ICompilePatternsResult &patterns) : IncludeOnlyRule() {
		Rule::init(_location, id, name, contentName);
		Rule::data->patterns = patterns.patterns;
		Rule::data->hasMissingPatterns = patterns.hasMissingPatterns;
		data->_cachedCompiledPatterns = null;
	}

	void collectPatterns(RuleRegistry &grammar, RegExpSourceList &out) {
		for (RuleId &pattern: Rule::data->patterns) {
			auto rule = grammar->getRule(pattern);
			rule.collectPatterns(grammar, out);
		}
	}

	CompiledRule compile(OnigLib oni_lib, RuleRegistry grammar, utf16 endRegexSource) {
		return data->_getCachedCompiledPatterns(grammar).compile(oni_lib);
	}

	CompiledRule compileAG(OnigLib oni_lib, RuleRegistry grammar, utf16 endRegexSource, bool allowA, bool allowG) {
		return data->_getCachedCompiledPatterns(grammar).compileAG(oni_lib, allowA, allowG);
	}
};

struct BeginEndRule:Rule {
	struct members {
		RegExpSource 	_begin;
		array<CaptureRule> 	beginCaptures;
		RegExpSource 	_end;
		bool 			endHasBackReferences;
		array<CaptureRule> 	endCaptures;
		bool 			applyEndPatternLast;
		array<RuleId>	patterns;
		RegExpSourceList _cachedCompiledPatterns;
		register(members)

		void dispose() {
			if (_cachedCompiledPatterns) {
				_cachedCompiledPatterns.dispose();
				_cachedCompiledPatterns = null;
			}
		}

		utf16 debugBeginRegExp() {
			return _begin->source;
		}

		utf16 debugEndRegExp() {
			return _end->source;
		}

		utf16 getEndWithResolvedBackReferences(utf16 lineText, array<IOnigCaptureIndex> captureIndices) {
			return _end.resolveBackReferences(lineText, captureIndices);
		}

		RegExpSourceList _getCachedCompiledPatterns(RuleRegistry grammar, utf16 endRegexSource) {
			if (!_cachedCompiledPatterns) {
				_cachedCompiledPatterns = RegExpSourceList();

				for (RuleId &pattern: Rule::patterns) {
					auto rule = grammar->getRule(pattern);
					rule.collectPatterns(grammar, _cachedCompiledPatterns);
				}

				if (applyEndPatternLast) {
					_cachedCompiledPatterns.push(_end->hasBackReferences ? _end.clone() : _end);
				} else {
					_cachedCompiledPatterns.unshift(_end->hasBackReferences ? _end.clone() : _end);
				}
			}
			if (_end->hasBackReferences) {
				if (applyEndPatternLast) {
					_cachedCompiledPatterns.setSource(_cachedCompiledPatterns.length() - 1, endRegexSource);
				} else {
					_cachedCompiledPatterns.setSource(0, endRegexSource);
				}
			}
			return _cachedCompiledPatterns;
		}
	};

	mx_object(BeginEndRule, Rule, members);

	BeginEndRule(ILocation &_location, RuleId id, utf16 name, utf16 contentName,
			utf16 begin, array<CaptureRule> beginCaptures, utf16 end, array<CaptureRule> endCaptures,
			bool applyEndPatternLast, ICompilePatternsResult &patterns) {
		Rule::init(_location, id, name, contentName);
		data->_begin = RegExpSource(begin, id);
		data->beginCaptures = beginCaptures;
		data->_end = RegExpSource(end ? end : utf16(wchar(0xFFFF)), -1);
		data->endHasBackReferences = data->_end->hasBackReferences;
		data->endCaptures = endCaptures;
		data->applyEndPatternLast = applyEndPatternLast || false;
		Rule::data->patterns = patterns.patterns;
		Rule::data->hasMissingPatterns = patterns.hasMissingPatterns;
		data->_cachedCompiledPatterns = null;
	}

	void collectPatterns(RuleRegistry &grammar, RegExpSourceList out) {
		out.push(data->_begin);
	}

	CompiledRule compile(OnigLib oni_lib, RuleRegistry grammar, utf16 endRegexSource) {
		return _getCachedCompiledPatterns(grammar, endRegexSource).compile(oni_lib);
	}

	CompiledRule compileAG(OnigLib oni_lib, RuleRegistry grammar, utf16 endRegexSource, bool allowA, bool allowG) {
		return _getCachedCompiledPatterns(grammar, endRegexSource).compileAG(oni_lib, allowA, allowG);
	}
};

struct BeginWhileRule : Rule {
	struct members {
		RegExpSource 		_begin;
		array<CaptureRule> 	beginCaptures;
		array<CaptureRule> 	whileCaptures;
		RegExpSource 		_while;
		bool 				whileHasBackReferences;
		array<RuleId> 		patterns;
		RegExpSourceList 	_cachedCompiledPatterns;
		RegExpSourceList 	_cachedCompiledWhilePatterns;

		register(members)

		void dispose() {
			if (_cachedCompiledPatterns) {
				_cachedCompiledPatterns.dispose();
				_cachedCompiledPatterns = null;
			}
			if (_cachedCompiledWhilePatterns) {
				_cachedCompiledWhilePatterns.dispose();
				_cachedCompiledWhilePatterns = null;
			}
		}

		utf16 debugBeginRegExp() {
			return _begin->source;
		}

		utf16 debugWhileRegExp() {
			return _while->source;
		}

		utf16 getWhileWithResolvedBackReferences(utf16 lineText, array<IOnigCaptureIndex> captureIndices) {
			return _while.resolveBackReferences(lineText, captureIndices);
		}

		RegExpSourceList _getCachedCompiledPatterns(RuleRegistry grammar) {
			if (!_cachedCompiledPatterns) {
				_cachedCompiledPatterns = RegExpSourceList();

				for (RuleId &pattern: Rule::patterns) {
					auto rule = grammar->getRule(pattern);
					rule.collectPatterns(grammar, _cachedCompiledPatterns);
				}
			}
			return _cachedCompiledPatterns;
		}

		CompiledRule compileWhile(OnigLib oni_lib, RuleRegistry grammar, utf16 endRegexSource) {
			return _getCachedCompiledWhilePatterns(oni_lib, endRegexSource).compile(oni_lib);
		}

		CompiledRule compileWhileAG(OnigLib oni_lib, RuleRegistry grammar, utf16 endRegexSource, bool allowA, bool allowG) {
			return _getCachedCompiledWhilePatterns(oni_lib, endRegexSource).compileAG(oni_lib, allowA, allowG);
		}

		RegExpSourceList _getCachedCompiledWhilePatterns(OnigLib oni_lib, utf16 endRegexSource) {
			if (!_cachedCompiledWhilePatterns) {
				_cachedCompiledWhilePatterns = RegExpSourceList();
				_cachedCompiledWhilePatterns.push(_while->hasBackReferences ? _while.clone() : _while);
			}
			if (_while->hasBackReferences) {
				_cachedCompiledWhilePatterns.setSource(0, endRegexSource ? endRegexSource : utf16((unsigned short)0xFFFF));
			}
			return _cachedCompiledWhilePatterns;
		}
	};

	mx_object(BeginWhileRule, Rule, members);

	BeginWhileRule(
			ILocation &_location, RuleId id, utf16 name, utf16 contentName, 
			utf16 begin, array<CaptureRule> beginCaptures, utf16 _while,
			array<CaptureRule> whileCaptures, ICompilePatternsResult patterns) : BeginWhileRule() {

		Rule::init(_location, id, name, contentName);
		data->_begin = RegExpSource(begin, id);
		data->beginCaptures = beginCaptures;
		data->whileCaptures = whileCaptures;
		data->_while = RegExpSource(_while, whileRuleId);
		data->whileHasBackReferences = data->_while->hasBackReferences;
		Rule::data->patterns = patterns.patterns;
		Rule::data->hasMissingPatterns = patterns.hasMissingPatterns;
		data->_cachedCompiledPatterns = null;
		data->_cachedCompiledWhilePatterns = null;
	}

	void collectPatterns(RuleRegistry &grammar, RegExpSourceList &out) {
		out.push(data->_begin);
	}

	CompiledRule compile(OnigLib oni_lib, RuleRegistry grammar, utf16 endRegexSource) {
		return data->_getCachedCompiledPatterns(grammar).compile(oni_lib);
	}

	CompiledRule compileAG(OnigLib oni_lib, RuleRegistry grammar, utf16 endRegexSource, bool allowA, bool allowG) {
		return data->_getCachedCompiledPatterns(grammar).compileAG(oni_lib, allowA, allowG);
	}
};

struct RuleFactory {

	static CaptureRule createCaptureRule(RuleFactoryHelper helper, ILocation &_location, utf16 name, utf16 contentName, RuleId retokenizeCapturedWithRuleId) {
		return helper->rule_reg->registerRule([&](RuleId id) -> Rule {
			return CaptureRule(_location, id, name, contentName, retokenizeCapturedWithRuleId);
		});
	}

	static RuleId getCompiledRuleId(RawRule desc, RuleFactoryHelper helper, RawRepository repository) {
		if (!desc->id) {
			helper->rule_reg->registerRule([&](RuleId id) -> Rule {
				desc->id = id;

				if (desc->match) {
					return MatchRule(
						desc->_vscodeTextmateLocation,
						desc->id,
						desc->name,
						desc->match,
						RuleFactory::_compileCaptures(desc->captures, helper, repository)
					);
				}

				if (!desc->begin) {
					if (desc->repository) {
						repository = RawRepository(); /// merge objects should be 
						repository->props = mergeObjects(map<mx> {}, repository->props, desc->repository->props);
					}
					array<RawRule> patterns = desc->patterns;
					if (!patterns && desc->include) { /// this checked against undefined on patterns; empty should be the same
						RawRule r = RawRule::members {
							.include = desc->include
						};
						patterns = array<RawRule> { r };
					}

					ICompilePatternsResult _patterns = RuleFactory::_compilePatterns(patterns, helper, repository);
					return IncludeOnlyRule(
						desc->_vscodeTextmateLocation,
						desc->id,
						desc->name,
						desc->contentName,
						_patterns
					);
				}

				if (desc->_while) {
					ICompilePatternsResult _patterns = RuleFactory::_compilePatterns(desc->patterns, helper, repository);
					return BeginWhileRule(
						desc->_vscodeTextmateLocation,
						desc->id,
						desc->name,
						desc->contentName,
						desc->begin, RuleFactory::_compileCaptures(
							desc->beginCaptures ? desc->beginCaptures : desc->captures, helper, repository),
						desc->_while, RuleFactory::_compileCaptures(
							desc->whileCaptures ? desc->whileCaptures : desc->captures, helper, repository),
						_patterns
					);
				}

				ICompilePatternsResult _patterns = RuleFactory::_compilePatterns(desc->patterns, helper, repository);
				return BeginEndRule(
					desc->_vscodeTextmateLocation,
					desc->id,
					desc->name,
					desc->contentName,
					desc->begin, RuleFactory::_compileCaptures(
						desc->beginCaptures ? desc->beginCaptures : desc->captures, helper, repository),
					desc->end, RuleFactory::_compileCaptures(
						desc->endCaptures ? desc->endCaptures : desc->captures, helper, repository),
					desc->applyEndPatternLast,
					_patterns
				);
			});
		}

		return desc->id;
	}

	static array<CaptureRule> _compileCaptures(RawCaptures captures, RuleFactoryHelper helper, RawRepository repository) {
		array<CaptureRule> r;

		if (captures) {
			// Find the maximum capture id
			num maximumCaptureId = 0;
			for (field<mx> &f: captures) {
				str    captureId = f.key.grab();
				if (captureId == "_vscodeTextmateLocation") {
					continue;
				}
				auto numericCaptureId = captureId.integer_value();
				if (numericCaptureId > maximumCaptureId) {
					maximumCaptureId = numericCaptureId;
				}
			}

			// Initialize result
			for (num i = 0; i <= maximumCaptureId; i++) {
				CaptureRule default_rule;
				r.push(default_rule);
			}

			// Fill out result
			for (field<mx> &f: captures) {
				r.set_size(maximumCaptureId + 1);
				str    captureId = f.key.grab();
				RawRule raw_rule = f.value.grab();
				if (captureId == "_vscodeTextmateLocation")
					continue;
				auto numericCaptureId = captureId.integer_value();
				RuleId retokenizeCapturedWithRuleId = 0;
				if (raw_rule->patterns) {
					retokenizeCapturedWithRuleId = RuleFactory::getCompiledRuleId(raw_rule, helper, repository);
				}
				r[numericCaptureId] = RuleFactory::createCaptureRule(helper, raw_rule->_vscodeTextmateLocation,
					raw_rule->name, raw_rule->contentName, retokenizeCapturedWithRuleId);
				
			}
		}
		assert(captures.count() == r.len());
		return r;
	}

	static ICompilePatternsResult _compilePatterns(array<RawRule> patterns, RuleFactoryHelper helper, RawRepository repository) {
		array<RuleId> r;

		if (patterns) {
			for (num i = 0, len = patterns.len(); i < len; i++) {
				RawRule pattern = patterns[i];
				RuleId  ruleId  = -1;

				if (pattern->include) {

					auto reference = parseInclude(pattern->include);

					switch (reference->kind) {
						case IncludeReferenceKind::Base:
						case IncludeReferenceKind::Self:
							ruleId = RuleFactory::getCompiledRuleId(repository->props[pattern->include], helper, repository);
							break;

						case IncludeReferenceKind::RelativeReference: {
							// Local include found in `repository`
							RawRule localIncludedRule = repository->props[reference->ruleName];
							if (localIncludedRule) {
								ruleId = RuleFactory::getCompiledRuleId(localIncludedRule, helper, repository);
							} else {
								// console.warn('CANNOT find rule for scopeName: ' + pattern->include + ', I am: ', repository->props['$base'].name);
							}
							break;
						}
						case IncludeReferenceKind::TopLevelReference:
						case IncludeReferenceKind::TopLevelRepositoryReference: {

							auto externalGrammarName = reference->scopeName;
							auto externalGrammarInclude =
								reference->kind == IncludeReferenceKind::TopLevelRepositoryReference
									? reference->ruleName
									: null;

							// External include
							auto externalGrammar = helper->grammar_reg->getExternalGrammar(externalGrammarName, repository);

							if (externalGrammar) {
								if (externalGrammarInclude) {
									auto &externalIncludedRule = externalGrammar->repository->props[externalGrammarInclude];
									if (externalIncludedRule) {
										ruleId = RuleFactory::getCompiledRuleId(externalIncludedRule, helper, externalGrammar->repository);
									} else {
										// console.warn('CANNOT find rule for scopeName: ' + pattern->include + ', I am: ', repository->props['$base'].name);
									}
								} else {
									ruleId = RuleFactory::getCompiledRuleId(externalGrammar->repository->props["$self"], helper, externalGrammar->repository);
								}
							} else {
								// console.warn('CANNOT find grammar for scopeName: ' + pattern->include + ', I am: ', repository->props['$base'].name);
							}
							break;
						}
					}
				} else {
					ruleId = RuleFactory::getCompiledRuleId(pattern, helper, repository);
				}

				if (ruleId != -1) {
					auto rule = helper->rule_reg->getRule(ruleId);

					bool skipRule = false;

					type_t t = rule.type();
					if (t == typeof(IncludeOnlyRule) || t == typeof(BeginEndRule) || t == typeof(BeginWhileRule)) {
						if (rule->hasMissingPatterns && rule->patterns.len() == 0) {
							skipRule = true;
						}
					}

					if (skipRule) {
						// console.log('REMOVING RULE ENTIRELY DUE TO EMPTY PATTERNS THAT ARE MISSING');
						continue;
					}

					r.push(ruleId);
				}
			}
		}

		return {
			.patterns = r,
			.hasMissingPatterns = ((patterns ? patterns.len() : 0) != r.len())
		};
	}
};

struct AttributedScopeStackFrame {
	num encodedTokenAttributes;
	array<str> scopeNames;
};

struct StateStackFrame:mx {
	struct members {
		num 	ruleId;
		num 	enterPos;
		num 	anchorPos;
		bool 	beginRuleCapturedEOL;
		utf16 	endRule;
		array<AttributedScopeStackFrame> nameScopesList;
		array<AttributedScopeStackFrame> contentNameScopesList;
	};
	mx_basic(StateStackFrame);
};

struct Grammar;

using Matcher = lambda<bool(mx)>;

struct TokenTypeMatcher {
	Matcher matcher;
	StandardTokenType type;
};

struct IToken {
	num startIndex;
	num endIndex;
	array<utf16> scopes;
};

struct BasicScopeAttributes:mx {
	struct members {
		num languageId;
		OptionalStandardTokenType tokenType;
	};
	mx_basic(BasicScopeAttributes)
};


struct ScopeMatcher:mx {
	struct members {
		EmbeddedLanguagesMap values;
		RegEx scopesRegExp;

		register(members)

		mx match(ScopeName scope) {
			if (!scopesRegExp) {
				return {};
			}
			array<str> m = scopesRegExp.exec(scope);
			if (!m) {
				// no scopes matched
				return {};
			}
			return values[m[1]];
		}
	};
	mx_basic(ScopeMatcher);

	ScopeMatcher(EmbeddedLanguagesMap &values) : ScopeMatcher() { /// todo: this was designed to be an array of arrays; convert its input from the users of it
		if (!values) {
			data->values = null;
			data->scopesRegExp = null;
		} else {
			data->values = values;

			// create the regex todo -- verify order with js
			array<str> escapedScopes = values->map<str>(
				[](field<num> &f) -> str {
					return RegEx::escape(f.key);
				}
			);

			escapedScopes = escapedScopes.sort([](str &a, str &b) -> int {
				if (a < b)
					return -1;
				else if (b > a)
					return 1;
				return 0;
			});

			//escapedScopes = escapedScopes.reverse(); // Longest scope first

			str j = escapedScopes.join(")|(");
			data->scopesRegExp = RegEx(
				fmt {"^((${0}))($|\\.)", { j }}
			);
		}
	}
};

struct BasicScopeAttributesProvider:mx {
	struct members {
		BasicScopeAttributes _defaultAttributes;
		ScopeMatcher _embeddedLanguagesMatcher;

		static BasicScopeAttributes _getBasicScopeAttributes(ScopeMatcher &scope_matcher, ScopeName scopeName) {
			num languageId = _scopeToLanguage(scope_matcher, scopeName);
			OptionalStandardTokenType standardTokenType = _toStandardTokenType(scopeName);
			return BasicScopeAttributes(BasicScopeAttributes::members { languageId, standardTokenType });
		};

		inline static BasicScopeAttributes _NULL_SCOPE_METADATA = BasicScopeAttributes();
		inline static RegEx STANDARD_TOKEN_TYPE_REGEXP = R"(\b(comment|string|regex|meta\.embedded)\b)";

		BasicScopeAttributes getDefaultAttributes() {
			return _defaultAttributes;
		}

		BasicScopeAttributes getBasicScopeAttributes(ScopeName scopeName) {
			if (!scopeName) {
				return BasicScopeAttributesProvider::_NULL_SCOPE_METADATA;
			}
			return _getBasicScopeAttributes(_embeddedLanguagesMatcher, scopeName);
		}

		/**
		 * Given a produced TM scope, return the language that token describes or null if unknown.
		 * e.g. source.html => html, source.css.embedded.html => css, punctuation.definition.tag.html => null
		 */
		static num _scopeToLanguage(ScopeMatcher &scope_matcher, ScopeName scope) {
			return scope_matcher.match(scope) || 0;
		}

		static OptionalStandardTokenType _toStandardTokenType(ScopeName scopeName) { /// shouldnt need utf16 here
			array<str> m = BasicScopeAttributesProvider::STANDARD_TOKEN_TYPE_REGEXP.exec(scopeName);
			if (!m)
				return OptionalStandardTokenType::NotSet;
			
			str m1 = m[1];
			if (m1 == "comment") return OptionalStandardTokenType::_Comment;
			if (m1 == "string")  return OptionalStandardTokenType::_String;
			if (m1 == "regex")   return OptionalStandardTokenType::__RegEx;
			///
			if (m1 == "meta.embedded") return OptionalStandardTokenType::_Other;
			throw new str("Unexpected match for standard token type!");
		}
	};

	BasicScopeAttributesProvider(num initialLanguageId, EmbeddedLanguagesMap embeddedLanguages) : BasicScopeAttributesProvider() {
		data->_defaultAttributes = BasicScopeAttributes(
			BasicScopeAttributes::members {
				initialLanguageId, OptionalStandardTokenType::NotSet
			}
		);
		data->_embeddedLanguagesMatcher = ScopeMatcher(embeddedLanguages);
	}

	mx_basic(BasicScopeAttributesProvider)
};

struct Grammar;

struct AttributedScopeStack:mx {
	struct members {
		mx parent; /// use mx instead of pointer; use this for management of typed access; dependency order quirks as well.
		ScopeStack scopePath;
		EncodedTokenAttr tokenAttributes;
		register(members)

		static AttributedScopeStack fromExtension(AttributedScopeStack namesScopeList, array<AttributedScopeStackFrame> contentNameScopesList) {
			AttributedScopeStack current = namesScopeList;
			ScopeStack scopeNames = namesScopeList ?
				namesScopeList->scopePath : ScopeStack {};
			for (AttributedScopeStackFrame &frame: contentNameScopesList) {
				scopeNames = ScopeStack::push(scopeNames, frame.scopeNames);
				current = AttributedScopeStack(AttributedScopeStack::members {
					current, scopeNames, frame.encodedTokenAttributes
				});
			}
			return current;
		}

		static AttributedScopeStack createRoot(ScopeName scopeName, EncodedTokenAttr tokenAttributes) {
			return AttributedScopeStack(
				AttributedScopeStack::members {
					null, ScopeStack({}, scopeName), tokenAttributes
				});
		}

		static AttributedScopeStack createRootAndLookUpScopeName(ScopeName scopeName, EncodedTokenAttr tokenAttributes, Grammar &grammar);

		ScopeName scopeName() { return scopePath->scopeName; }

		str toString() {
			return getScopeNames().join(" ");
		}

		bool equals(AttributedScopeStack other) {
			return AttributedScopeStack::memebers::equals(*this, other);
		}

		static bool equals(
			AttributedScopeStack a,
			AttributedScopeStack b
		) {
			do {
				if (a == b) {
					return true;
				}

				if (!a && !b) {
					// End of list reached for both
					return true;
				}

				if (!a || !b) {
					// End of list reached only for one
					return false;
				}

				if (a->scopePath->scopeName != b->scopePath->scopeName || 
					a->tokenAttributes != b->tokenAttributes) {
					return false;
				}

				// Go to previous pair
				a = a->parent.grab();
				b = b->parent.grab();
			} while (true);
		}

		static EncodedTokenAttr mergeAttributes(
			EncodedTokenAttr 		existingTokenAttributes,
			BasicScopeAttributes 	basicScopeAttributes,
			StyleAttributes 		styleAttributes
		) {
			FontStyle fontStyle  = FontStyle::_NotSet;
			num 	  foreground = 0;
			num 	  background = 0;

			if (styleAttributes) {
				fontStyle  = styleAttributes->fontStyle;
				foreground = styleAttributes->foregroundId;
				background = styleAttributes->backgroundId;
			}

			return EncodedTokenAttributes::set(
				existingTokenAttributes,
				basicScopeAttributes->languageId,
				basicScopeAttributes->tokenType,
				null,
				fontStyle,
				foreground,
				background
			);
		}

		AttributedScopeStack pushAttributed(ScopePath scopePath, Grammar &grammar);

		static AttributedScopeStack _pushAttributed(
			AttributedScopeStack target,
			ScopeName scopeName,
			Grammar &grammar
		);

		array<str> getScopeNames() {
			return scopePath.getSegments();
		}

		array<AttributedScopeStackFrame> getExtensionIfDefined(AttributedScopeStack base) {
			array<AttributedScopeStackFrame> result;
			AttributedScopeStack self = *this;

			while (self && self != base) {
				if (self->parent) {
					AttributedScopeStack parent = self->parent.grab();
					AttributedScopeStackFrame f {
						.encodedTokenAttributes = self->tokenAttributes,
						.scopeNames = self->scopePath.getExtensionIfDefined(
							parent->scopePath)
					};
					result.push(f);
					self = parent;
				} else {
					AttributedScopeStackFrame f {
						.encodedTokenAttributes = self->tokenAttributes,
						.scopeNames = self->scopePath.getExtensionIfDefined(
							AttributedScopeStack {}) /// this is a null item
					};
					result.push(f);
					break;
				}
			}
			return self == base ? result.reverse() : array<AttributedScopeStackFrame> {};
		}
	};
	mx_basic(AttributedScopeStack);

	AttributedScopeStack(null_t) : AttributedScopeStack() { }
};

struct StateStackImpl:mx {
	struct members {
		mx   parent;
		num _enterPos;
		num _anchorPos;
		num  depth;
		RuleId ruleId;
		bool beginRuleCapturedEOL;
		AttributedScopeStack nameScopesList;
		AttributedScopeStack contentNameScopesList;
		utf16 endRule;
		bool filled;
		register(members);

		operator  bool() { return  filled; }
		bool operator!() { return !filled; }
	};

	mx_object(StateStackImpl, mx, members);

	StateStackImpl(nullptr_t) : StateStackImpl() { }

	StateStackImpl(
		mx		   parent, // typeof StateStackImpl
		RuleId 	   ruleId,
		num 	   enterPos,
		num 	   anchorPos,
		bool 	   beginRuleCapturedEOL,
		utf16 	   endRule,
		AttributedScopeStack nameScopesList,
		AttributedScopeStack contentNameScopesList) : StateStackImpl() {
		assert(parent.type() == typeof(StateStackImpl));

		data->parent = parent.grab();
		if (data->parent) {
			StateStackImpl parent = parent.grab();
			data->depth = parent->depth + 1;
		} else
			data->depth = 1;
		
		data->_enterPos = enterPos;
		data->_anchorPos = anchorPos;
		data->ruleId = ruleId;
		data->beginRuleCapturedEOL = beginRuleCapturedEOL;
		data->endRule = endRule;
		data->nameScopesList = nameScopesList;
		data->contentNameScopesList = contentNameScopesList;
		data->filled = true;
	}

	bool equals(StateStackImpl &other) {
		if (!other) {
			return false;
		}
		return StateStackImpl::_equals(*this, other);
	}

	static bool _equals(StateStackImpl a, StateStackImpl b) {
		if (a == b) {
			return true;
		}
		if (!_structuralEquals(a, b)) {
			return false;
		}
		return AttributedScopeStack::members::equals(a->contentNameScopesList, b->contentNameScopesList);
	}

	operator bool() {
		return data->filled;
	}

	static bool _structuralEquals(StateStackImpl a, StateStackImpl b) {
		do {
			if (a == b)
				return true;

			// End of list reached for both
			if (!a && !b)
				return true;

			// End of list reached only for one
			if (!a || !b)
				return false;

			if (a->depth   != b->depth  ||
				a->ruleId  != b->ruleId ||
				a->endRule != b->endRule)
				return false;

			// Go to previous pair
			a = a->parent ? a->parent.grab() : StateStackImpl {};
			b = b->parent ? b->parent.grab() : StateStackImpl {};
		} while (true);
	}

	bool clone() {
		return this;
	}

	static void _reset(StateStackImpl &el) {
		while (el) {
			el->_enterPos = -1;
			el->_anchorPos = -1;
			el = el->parent.grab();
		}
	}

	void reset() {
		StateStackImpl::_reset(*this);
	}

	StateStackImpl pop() {
		return data->parent.grab();
	}

	StateStackImpl safePop() {
		if (data->parent) {
			return data->parent.grab();
		}
		return *this;
	}

	StateStackImpl push(
		RuleId 	ruleId,
		num 	enterPos,
		num 	anchorPos,
		bool 	beginRuleCapturedEOL,
		utf16 	endRule,
		AttributedScopeStack nameScopesList,
		AttributedScopeStack contentNameScopesList
	) {
		return StateStackImpl(
			*this,
			ruleId,
			enterPos,
			anchorPos,
			beginRuleCapturedEOL,
			endRule,
			nameScopesList,
			contentNameScopesList
		);
	}

	num getEnterPos() {
		return data->_enterPos;
	}

	num getAnchorPos() {
		return data->_anchorPos;
	}

	Rule getRule(RuleRegistry grammar) {
		return grammar->getRule(data->ruleId);
	}

	utf16 toString() { /// 'toString' should probably always be to String.  not another form of it.  UTF8 out
		array<utf16> r;
		_writeString(r, 0);
		return fmt {"[{0}]", { str(r.join(str(","))) }};
	}

	num _writeString(array<utf16> res, num outIndex) {
		if (data->parent) {
			StateStackImpl s = data->parent.grab();
			outIndex = s._writeString(res, outIndex);
		}

		utf16 f = fmt {"({0}, {1}, {2})", {
			data->ruleId,
			data->nameScopesList ? data->nameScopesList.toString() : null,
			data->contentNameScopesList ? data->contentNameScopesList.toString() : null}};
		
		res.push(f);
		return res.len();
	}

	StateStackImpl withContentNameScopesList(AttributedScopeStack &contentNameScopeStack) {
		if (data->contentNameScopesList == contentNameScopeStack)
			return *this;
		StateStackImpl p = data->parent.grab();
		return p.push(
			data->ruleId,
			data->_enterPos,
			data->_anchorPos,
			data->beginRuleCapturedEOL,
			data->endRule,
			data->nameScopesList,
			contentNameScopeStack
		);
	}

	StateStackImpl withEndRule(utf16 endRule) {
		if (data->endRule == endRule)
			return *this;
		
		return StateStackImpl(
			data->parent,
			data->ruleId,
			data->_enterPos,
			data->_anchorPos,
			data->beginRuleCapturedEOL,
			endRule,
			data->nameScopesList,
			data->contentNameScopesList
		);
	}

	// Used to warn of endless loops
	bool hasSameRuleAs(StateStackImpl other) {
		StateStackImpl el = *this;
		while (el && el->_enterPos == other->_enterPos) {
			if (el->ruleId == other->ruleId) {
				return true;
			}
			el = el->parent.grab();
		}
		return false;
	}

	StateStackFrame toStateStackFrame() {
		StateStackImpl parent = data->parent.grab();
		AttributedScopeStack scope_list = data->nameScopesList ? 
				data->nameScopesList.getExtensionIfDefined(
					bool(parent) ? parent->nameScopesList : null) : null;
		
		return StateStackFrame::members {
			.ruleId = ruleIdToNumber(data->ruleId),
			.beginRuleCapturedEOL 	= data->beginRuleCapturedEOL,
			.endRule 				= data->endRule,
			.nameScopesList 		= scope_list,
			.contentNameScopesList 	= data->contentNameScopesList ? 
				data->contentNameScopesList.getExtensionIfDefined(data->nameScopesList) : null,
		};
	}

	static StateStackImpl pushFrame(StateStackImpl self, StateStackFrame &frame) {
		auto namesScopeList = AttributedScopeStack::members::fromExtension(self ? self->nameScopesList : null, frame->nameScopesList);
		return StateStackImpl(
			self,
			
			frame->enterPos 	? frame->enterPos : -1,
			frame->anchorPos ? frame->anchorPos : -1,
			frame->beginRuleCapturedEOL,

			ruleIdFromNumber(frame->ruleId),
			
			frame->endRule,
			namesScopeList,
			AttributedScopeStack::members::fromExtension(namesScopeList, frame->contentNameScopesList)
		);
	}
};


using NameMatcher = lambda<bool(array<str>, array<str>)>;

struct MatcherWithPriority:mx {
	struct members {
		Matcher matcher;
		num 	priority; // -1, 0, 1
		register(members);
	};
	mx_basic(MatcherWithPriority);
	MatcherWithPriority(Matcher &matcher, num priority) : MatcherWithPriority() {
		data->matcher  = matcher;
		data->priority = priority;
	}
};

struct Tokenizer:mx {
    struct members {
		utf16			input;
		RegEx		  	regex;
		array<utf16>    match;
		utf16 next() {
			if (!match)
				return {};
			utf16 prev = match[0];
			match = regex.exec(input); /// regex has a marker for its last match stored, so one can call it again if you give it the same input.  if its a different input it will reset
			return prev;
		}
    };

    mx_basic(Tokenizer);

    Tokenizer(utf16 input) : Tokenizer() {
        data->regex = utf16("([LR]:|[\\w\\.:][\\w\\.:\\-]*|[\\,\\|\\-\\(\\)])");
	    data->match = data->regex.exec(input);
    }
};

bool isIdentifier(utf16 token) {
	RegEx regex("[\\w\\.:]+/");
	return token && regex.exec(token);
}

array<MatcherWithPriority> createMatchers(utf16 selector, NameMatcher matchesName) {
	array<MatcherWithPriority> results;
	Tokenizer tokenizer { selector };
	utf16 token = tokenizer->next();

	/// accesses tokenizer
	lambda<Matcher()> parseInnerExpression;
	lambda<Matcher()> &parseInnerExpressionRef = parseInnerExpression;
	lambda<Matcher()> parseOperand;
	lambda<Matcher()> &parseOperandRef = parseOperand; /// dont believe we can merely set after we copy the other value in; the data will change
	parseOperand = [&token, _tokenizer=tokenizer, matchesName, parseInnerExpressionRef, parseOperandRef]() -> Matcher {
		Tokenizer &tokenizer = (Tokenizer &)_tokenizer;
		if (token == "-") {
			token = tokenizer->next();
			Matcher expressionToNegate = parseOperandRef();
			return Matcher([expressionToNegate](mx matcherInput) -> bool {
				return bool(expressionToNegate) && !expressionToNegate(matcherInput);
			});
		}
		if (token == "(") {
			token = tokenizer->next();
			Matcher expressionInParents = parseInnerExpressionRef();
			if (token == ")") {
				token = tokenizer->next();
			}
			return expressionInParents;
		}
		if (isIdentifier(token)) {
			array<utf16> identifiers;
			do {
				identifiers.push(token);
				token = tokenizer->next();
			} while (isIdentifier(token));
			///
			return Matcher([matchesName, identifiers] (mx matcherInput) -> bool {
				return matchesName((array<utf16>&)identifiers, matcherInput);
			});
		}
		return null;
	};

	/// accesses tokenizer
	auto parseConjunction = [_tokenizer=tokenizer, parseOperand]() -> Matcher {
		Tokenizer &tokenizer = (Tokenizer &)_tokenizer;
		array<Matcher> matchers;
		Matcher matcher = parseOperand();
		while (matcher) {
			matchers.push(matcher);
			matcher = parseOperand();
		}
		return [matchers](mx matcherInput) -> bool {
			return matchers.every([matcherInput](Matcher &matcher) -> bool {
				return matcher(matcherInput);
			});
		};
	};

	/// accesses tokenizer
	parseInnerExpression = [&token, _tokenizer=tokenizer, parseConjunction]() -> Matcher {
		Tokenizer &tokenizer = (Tokenizer &)_tokenizer;
		array<Matcher> matchers;
		Matcher matcher = parseConjunction();
		while (matcher) {
			matchers.push(matcher);
			if (token == "|" || token == ",") {
				do {
					token = tokenizer->next();
				} while (token == "|" || token == ","); // ignore subsequent commas
			} else {
				break;
			}
			matcher = parseConjunction();
		}
		return Matcher([matchers](mx matcherInput) -> bool {
			return matchers.some([matcherInput](Matcher &matcher) {
				return matcher(matcherInput);
			});
		});
	};

	/// top level loop to iterate through tokens
	while (token) {
		num priority = 0;
		if (token.len() == 2 && token[1] == ':') {
			switch (token[0]) {
				case 'R': priority =  1; break;
				case 'L': priority = -1; break;
				default:
					console.log("Unknown priority {0} in scope selector", {token});
					return {};
			}
			token = tokenizer->next();
		}
		Matcher matcher = parseConjunction();
		MatcherWithPriority mp { matcher, priority };
		results.push(mp);
		if (token != ",") {
			break;
		}
		token = tokenizer->next();
	}
	return results;
}

struct ITokenizeLineResult {
	array<IToken> tokens;
	StateStackImpl ruleStack;
	bool stoppedEarly;
};

struct ITokenizeLineResult2 {
	Uint32Array tokens;
	StateStackImpl ruleStack;
	bool stoppedEarly;
};

bool scopesAreMatching(str thisScopeName, str scopeName) {
	if (!thisScopeName) {
		return false;
	}
	if (thisScopeName == scopeName) {
		return true;
	}
	auto len = scopeName.len();
	return thisScopeName.len() > len && thisScopeName.mid(0, len) == scopeName && thisScopeName[len] == '.';
}

struct BalancedBracketSelectors:mx {
	struct members {
		array<Matcher> balancedBracketScopes;
		array<Matcher> unbalancedBracketScopes;
		bool allowAny;
	};
	mx_basic(BalancedBracketSelectors)
	
	bool nameMatcher(array<ScopeName> identifers, array<ScopeName> scopes) {
		if (scopes.len() < identifers.len()) {
			return false;
		}
		size_t lastIndex = 0;
		return identifers.every([&](ScopeName &identifier) -> bool {
			for (size_t i = lastIndex; i < scopes.len(); i++) {
				if (scopesAreMatching(scopes[i], identifier)) {
					lastIndex = i + 1;
					return true;
				}
			}
			return false;
		});
	}

	bool scopesAreMatching(str thisScopeName, str scopeName) {
		if (!thisScopeName) {
			return false;
		}
		if (thisScopeName == scopeName) {
			return true;
		}
		size_t len = scopeName.len();
		return thisScopeName.len() > len && thisScopeName.mid(0, len) == scopeName && thisScopeName[len] == '.';
	}

	BalancedBracketSelectors(
		array<str> balancedBracketScopes,
		array<str> unbalancedBracketScopes) : BalancedBracketSelectors() {
		
		
		data->balancedBracketScopes = balancedBracketScopes.flat_map<Matcher>(
			[&](str &selector) -> array<Matcher> {
				if (selector == "*") {
					data->allowAny = true;
					return array<Matcher> {};
				}
				//createMatchers(selector, NameMatcher(this, nameMatcher))
				array<MatcherWithPriority> p = createMatchers(utf16(selector), NameMatcher(this, &BalancedBracketSelectors::nameMatcher));
				return p.map<Matcher>(
					[](MatcherWithPriority &m) -> Matcher { 
						return m->matcher;
					});
			}
		);


		data->unbalancedBracketScopes = unbalancedBracketScopes.flat_map<Matcher>(
			[&](str &selector) -> array<Matcher> {
				array<MatcherWithPriority> p = createMatchers(utf16(selector), NameMatcher(this, &BalancedBracketSelectors::nameMatcher));
				return p.map<Matcher>(
					[](MatcherWithPriority &m) -> Matcher {
						return m->matcher;
					});
			}
		);
	}

	bool matchesAlways() {
		return data->allowAny && data->unbalancedBracketScopes.len() == 0;
	}

	bool matchesNever() {
		return data->balancedBracketScopes.len() == 0 && !data->allowAny;
	}

	bool match(array<str> scopes) {
		for (auto excluder: data->unbalancedBracketScopes) {
			if (excluder(scopes)) {
				return false;
			}
		}
		for (auto includer: data->balancedBracketScopes) {
			if (includer(scopes)) {
				return true;
			}
		}
		return data->allowAny;
	}
};

struct LineTokens:mx {
	struct members {
		bool 			_emitBinaryTokens;
		utf16 			_lineText;
		array<IToken> 	_tokens;
		array<num> 		_binaryTokens;
		num 			_lastTokenEndIndex;
		array<TokenTypeMatcher> _tokenTypeOverrides;
		BalancedBracketSelectors balancedBracketSelectors;
		register(members);
	};
	
	mx_basic(LineTokens);

	LineTokens(
		bool emitBinaryTokens, utf16 lineText,
		array<TokenTypeMatcher> tokenTypeOverrides,
		BalancedBracketSelectors balancedBracketSelectors) : LineTokens()
	{
		data->_emitBinaryTokens = emitBinaryTokens;
		data->_tokenTypeOverrides = tokenTypeOverrides;
		data->balancedBracketSelectors = balancedBracketSelectors;
		if (is_debug()) {
			data->_lineText = lineText;
		} else {
			data->_lineText = null;
		}
	}

	void produce(StateStackImpl stack, num endIndex) {
		produceFromScopes(stack->contentNameScopesList, endIndex);
	}

	void produceFromScopes(
		AttributedScopeStack scopesList,
		num endIndex
	) {
		if (data->_lastTokenEndIndex >= endIndex) {
			return;
		}

		if (data->_emitBinaryTokens) {
			auto metadata = scopesList ? scopesList->tokenAttributes : 0;
			bool containsBalancedBrackets = (data->balancedBracketSelectors && data->balancedBracketSelectors.matchesAlways());

			if (data->_tokenTypeOverrides.len() > 0 ||
			   (data->balancedBracketSelectors && 
			   !data->balancedBracketSelectors.matchesAlways() && 
			   !data->balancedBracketSelectors.matchesNever())) {
				// Only generate scope array when required to improve performance
				array<ScopeName> scopes = scopesList ? scopesList.getScopeNames() : array<ScopeName>();
				for (auto &tokenType: data->_tokenTypeOverrides) {
					if (tokenType.matcher(scopes)) {
						metadata = EncodedTokenAttributes::set(
							metadata,
							0,
							toOptionalTokenType(tokenType.type),
							null,
							FontStyle::_NotSet,
							0,
							0
						);
					}
				}
				if (data->balancedBracketSelectors) {
					containsBalancedBrackets = data->balancedBracketSelectors.match(scopes);
				}
			}

			if (containsBalancedBrackets) {
				metadata = EncodedTokenAttributes::set(
					metadata,
					0,
					OptionalStandardTokenType::NotSet,
					containsBalancedBrackets,
					FontStyle::_NotSet,
					0,
					0
				);
			}

			if (data->_binaryTokens.len() > 0 && data->_binaryTokens[data->_binaryTokens.len() - 1] == metadata) {
				// no need to push a token with the same metadata
				data->_lastTokenEndIndex = endIndex;
				return;
			}

			if (is_debug()) {
				array<str> scopes = scopesList ? scopesList.getScopeNames() : array<str>();
				RegEx      regex  = RegEx(R"(\n$)");
				utf16      txt    = data->_lineText.mid(data->_lastTokenEndIndex, endIndex - data->_lastTokenEndIndex);
				console.log("  token: |{0}|", { regex.replace(txt, "\\n") });
				for (size_t k = 0; k < scopes.len(); k++) {
					console.log("      * {0}", { scopes[k] });
				}
			}

			data->_binaryTokens.push(data->_lastTokenEndIndex);
			data->_binaryTokens.push(metadata);

			data->_lastTokenEndIndex = endIndex;
			return;
		}

		array<str> scopes = scopesList ? scopesList.getScopeNames() : array<str>();

		if (is_debug()) {
			RegEx regex = RegEx(R"(\n$)");
			utf16 txt = data->_lineText.mid(data->_lastTokenEndIndex, endIndex - data->_lastTokenEndIndex);
			console.log("  token: |{0}|", { regex.replace(txt, "\\n") });
			for (size_t k = 0; k < scopes.len(); k++) {
				console.log("      * {0}", { scopes[k] });
			}
		}
		IToken p {
			.startIndex = data->_lastTokenEndIndex,
			.endIndex   = endIndex,
			.scopes     = scopes
		};
		data->_tokens.push(p);
		data->_lastTokenEndIndex = endIndex;
	}

	array<IToken> getResult(StateStackImpl stack, num lineLength) {
		if (data->_tokens.len() > 0 && data->_tokens[data->_tokens.len() - 1].startIndex == lineLength - 1) {
			// pop produced token for newline
			data->_tokens.pop();
		}

		if (data->_tokens.len() == 0) {
			data->_lastTokenEndIndex = -1;
			produce(stack, lineLength);
			data->_tokens[data->_tokens.len() - 1].startIndex = 0;
		}

		return data->_tokens;
	}

	Uint32Array getBinaryResult(StateStackImpl stack, num lineLength) {
		if (data->_binaryTokens.len() > 0 && data->_binaryTokens[data->_binaryTokens.len() - 2] == lineLength - 1) {
			// pop produced token for newline
			data->_binaryTokens.pop();
			data->_binaryTokens.pop();
		}

		if (data->_binaryTokens.len() == 0) {
			data->_lastTokenEndIndex = -1;
			produce(stack, lineLength);
			data->_binaryTokens[data->_binaryTokens.len() - 2] = 0;
		}

		auto result = Uint32Array(data->_binaryTokens.len());
		for (size_t i = 0, len = data->_binaryTokens.len(); i < len; i++) {
			u32 t = (u32)data->_binaryTokens[i];
			result.push(t);
		}

		return result;
	}
};

struct ILineTokensResult {
	num lineLength;
	LineTokens lineTokens;
	StateStackImpl ruleStack;
	bool stoppedEarly;
};

struct TokenizeStringResult {
	StateStackImpl stack;
	bool stoppedEarly;
};

TokenizeStringResult _tokenizeString(
	Grammar &grammar, utf16 lineText, bool isFirstLine,
	num linePos, StateStackImpl stack, LineTokens lineTokens,
	bool checkWhileConditions, num timeLimit 
);

struct Injection:mx {
	struct members {
		str debugSelector;
		Matcher matcher;
		num priority; // 0 is the default. -1 for 'L' and 1 for 'R'
		RuleId ruleId;
		RawGrammar grammar;
	};
	mx_basic(Injection);
};

/// Grammar should use 'Registry' there are too many data relationships
RawGrammar initGrammar(RawGrammar grammar, RawRule base) {
	grammar = grammar.copy();
	//grammar->repository = grammar->repository ? grammar->repository : RawRepository {};
	grammar->repository->props["$self"] = RawRule(RawRule::members {
		._vscodeTextmateLocation = grammar->_vscodeTextmateLocation,
		.name 					 = grammar->scopeName,
		.patterns 				 = grammar->patterns
	});
	grammar->repository->props["$base"] = base ? base : grammar->repository->props["$self"];
	return grammar;
}

struct Grammar:mx { // implements IGrammar, IRuleFactoryHelper, IOnigLib
	struct members {
		str						_rootScopeName;
		RuleId 					_rootId = -1;
		num 					_lastRuleId;
		array<Rule> 			_ruleId2desc;
		map<RawRepository>		_includedGrammars;
		num						initialLanguage;
		BalancedBracketSelectors balancedBracketSelectors;
		RuleFactoryHelper		 helper;
		GrammarRepository	     grammar_repo; // IGrammarRepository IThemeProvider
		ThemeProvider 			 theme_provider;
		var 					_grammar;
		array<Injection>		_injections;
		OnigLib					oni_lib;
		
		BasicScopeAttributesProvider _basicScopeAttributesProvider;
		array<TokenTypeMatcher> 	 _tokenTypeMatchers;


		BasicScopeAttributes getMetadataForScope(str scope) {
			return _basicScopeAttributesProvider.getBasicScopeAttributes(scope);
		}

		bool nameMatcher(array<ScopeName> identifers, array<ScopeName> scopes) {
			if (scopes.len() < identifers.len()) {
				return false;
			}
			size_t lastIndex = 0;
			return identifers.every([&](ScopeName &identifier) -> bool {
				for (size_t i = lastIndex; i < scopes.len(); i++) {
					if (scopesAreMatching(scopes[i], identifier)) {
						lastIndex = i + 1;
						return true;
					}
				}
				return false;
			});
		}

		void collectInjections(array<Injection> result, str selector, RawRule rule, RuleFactoryHelper ruleFactoryHelper, RawGrammar grammar) {
			array<MatcherWithPriority> matchers = createMatchers(selector, NameMatcher(this, &members::nameMatcher));
			RuleId ruleId = RuleFactory::getCompiledRuleId(rule, ruleFactoryHelper, grammar->repository);
			for (MatcherWithPriority &matcher: matchers) {
				result.push({
					.debugSelector = selector,
					.matcher = matcher->matcher,
					.ruleId = ruleId,
					.grammar = grammar,
					.priority = matcher->priority
				});
			}
		}

		array<Injection> _collectInjections() {
			GrammarRepository grammar_repo = GrammarRepository::members {
				.lookup = [&](str scopeName) -> RawGrammar {
					if (scopeName == _rootScopeName) {
						return _grammar;
					}
					return helper->grammar_reg->getExternalGrammar(scopeName, RawRepository {});
				},
				.injections = [&](str scopeName) -> array<str> {
					return grammar_repo->injections(scopeName);
				}
			};

			array<Injection> result;
			str scopeName = _rootScopeName;

			RawGrammar grammar = grammar_repo->lookup(scopeName);
			if (grammar) {
				// add injections from the current grammar
				map<RawRule> rawInjections = grammar->injections;
				if (rawInjections) {
					for (field<RawRule> &f: rawInjections) {
						str expression    = f.key.grab();
						RawRule raw_rule  = f.value.grab();
						collectInjections(
							result,
							expression,
							raw_rule,
							*this,
							grammar
						);
					}
				}

				// add injection grammars contributed for the current scope

				array<ScopeName> injectionScopeNames = grammar_repo->injections(scopeName);
				if (injectionScopeNames) {
					for (ScopeName &injectionScopeName: injectionScopeNames) {
						auto injectionGrammar =
							helper->grammar_reg->getExternalGrammar(injectionScopeName, RawRepository {});
						if (injectionGrammar) {
							auto selector = injectionGrammar.injectionSelector;
							if (selector) {
								collectInjections(
									result,
									selector,
									injectionGrammar,
									this,
									injectionGrammar
								);
							}
						}
					}
				}
			}

			result.sort([](Injection &i1, Injection &i2) -> int {
				return i1.priority - i2.priority;
			}); // sort by priority

			return result;
		}

		array<Injection> getInjections() {
			if (!_injections) {
				_injections = _collectInjections();

				if (DebugFlags.InDebugMode && _injections.length > 0) {
					console.log(
						"Grammar {0} contains the following injections:"
					, { _rootScopeName });
					for (Injection &injection: _injections) {
						console.log("  - {0}", { injection.debugSelector });
					}
				}
			}
			return _injections;
		}


		void dispose() {
			for (auto &rule: _ruleId2desc) {
				if (rule) {
					rule.dispose();
				}
			}
		}

		ITokenizeLineResult tokenizeLine(
				utf16 lineText,
				StateStackImpl prevState,
				num timeLimit = 0) {
			auto r = _tokenize(lineText, prevState, false, timeLimit);
			return {
				tokens = 		r.lineTokens.getResult(r.ruleStack, r.lineLength),
				ruleStack = 	r.ruleStack,
				stoppedEarly = 	r.stoppedEarly,
			};
		}

		ITokenizeLineResult2 tokenizeLine2(
				utf16 			lineText,
				StateStackImpl 	prevState,
				num 			timeLimit = 0) {
			auto r = _tokenize(lineText, prevState, true, timeLimit);
			return {
				tokens = 		r.lineTokens.getBinaryResult(r.ruleStack, r.lineLength),
				ruleStack = 	r.ruleStack,
				stoppedEarly = 	r.stoppedEarly,
			};
		}

		ILineTokensResult _tokenize(
			utf16 lineText,
			StateStackImpl prevState,
			bool emitBinaryTokens,
			num timeLimit
		) {
			if (_rootId == -1) {
				_rootId = RuleFactory::getCompiledRuleId(
					_grammar.repository.$self,
					this,
					_grammar.repository
				);
				// This ensures ids are deterministic, and thus equal in renderer and webworker.
				getInjections();
			}

			bool isFirstLine;
			if (!prevState || prevState == StateStackImpl.NULL) {
				isFirstLine = true;
				//const rawDefaultMetadata =
				//	_basicScopeAttributesProvider.getDefaultAttributes();
				auto defaultStyle = theme_provider->getDefaults();
				auto defaultMetadata = EncodedTokenAttributes::set(
					0,
					0, // null rawDefaultMetadata.languageId,
					null, // rawDefaultMetadata.tokenType
					null,
					defaultStyle.fontStyle,
					defaultStyle.foregroundId,
					defaultStyle.backgroundId
				);

				auto rootScopeName = getRule(_rootId).getName(
					null,
					null
				);

				AttributedScopeStack scopeList;
				if (rootScopeName) {
					scopeList = AttributedScopeStack::members::createRootAndLookUpScopeName(
						rootScopeName,
						defaultMetadata,
						this
					);
				} else {
					scopeList = AttributedScopeStack::members::createRoot(
						"unknown",
						defaultMetadata
					);
				}

				prevState = StateStackImpl(
					null,
					_rootId,
					-1,
					-1,
					false,
					null,
					scopeList,
					scopeList
				);
			} else {
				isFirstLine = false;
				prevState.reset();
			}

			lineText += "\n";
			num   lineLength = lineText.len()
			auto lineTokens = LineTokens(
				emitBinaryTokens,
				lineText,
				_tokenTypeMatchers,
				balancedBracketSelectors
			);
			auto r = _tokenizeString(
				this,
				onigLineText,
				isFirstLine,
				0,
				prevState,
				lineTokens,
				true,
				timeLimit
			);
			return ILineTokensResult {
				lineLength	 = lineLength,
				lineTokens	 = lineTokens,
				ruleStack	 = r.stack,
				stoppedEarly = r.stoppedEarly,
			};
		}

		void init() {
			oni_lib->createOnigScanner = [](array<str> sources) -> OnigScanner {
				return OnigScanner(sources);
			};
			oni_lib->createOnigString = [](str string) -> OnigString {
				return string;
			};

			helper->grammar_reg->getExternalGrammar = [&](str scopeName, RawRepository repository) -> RawGrammar {
				if (_includedGrammars[scopeName]) {
					return _includedGrammars[scopeName];
				} else if (grammar_repo) {
					RawGrammar rawIncludedGrammar =
						grammar_repo->lookup(scopeName);
					if (rawIncludedGrammar) {
						// console.log('LOADED GRAMMAR ' + pattern.include);
						_includedGrammars[scopeName] = initGrammar(
							rawIncludedGrammar,
							(repository->props->count("$base")) ? repository->props["$base"] : RawRule());
						return _includedGrammars[scopeName];
					}
				}
				return null;
			};

			helper->rule_reg->registerRule = [&](lambda<Rule(RuleId)> factory) -> Rule {
				auto id = ++_lastRuleId;
				auto result = factory(ruleIdFromNumber(id));
				_ruleId2desc[id] = result; /// this cant be set this way
				return result;
			};

			helper->rule_reg->getRule = [&](RuleId ruleId) -> Rule {
				return _ruleId2desc[int(ruleId)];
			};
		}

		register(members)

		//BasicScopeAttributesProvider _basicScopeAttributesProvider;
	};
	mx_basic(Grammar);

	Grammar(
		ScopeName 				 _rootScopeName,
		var 					 grammar,
		num 					 initialLanguage,
		EmbeddedLanguagesMap     embeddedLanguages,
		TokenTypeMap 			 tokenTypes,
		BalancedBracketSelectors balancedBracketSelectors,
		GrammarRepository 		 grammar_repo,
		OnigLib 			     oni_lib
		)
	{
		data->_basicScopeAttributesProvider = BasicScopeAttributesProvider(
			initialLanguage,
			embeddedLanguages
		);
		data->_rootScopeName 			= _rootScopeName;
		data->_rootId 					= -1;
		data->_lastRuleId 				= 0;
		data->initialLanguage 			= initialLanguage;
		data->_ruleId2desc 				= array<Rule> { Rule() };
		data->_includedGrammars 		= {};
		data->_grammar 					= grammar;
		data->balancedBracketSelectors 	= balancedBracketSelectors;
		data->grammar_repo 				= grammar_repo; /// this needs to be mx with SyncRegistry
		data->oni_lib 					= oni_lib;

		if (tokenTypes) {
			array<str> keys = tokenTypes.keys<str>();
			for (str &selector: keys) {
				array<Matcher> matchers = createMatchers(selector, NameMatcher(this, &Grammar::nameMatcher));
				for (Matcher &matcher: matchers) {
					TokenTypeMatcher t {
						.matcher = matcher,
						.type    = tokenTypes[selector],
					};
					data->_tokenTypeMatchers.push(t);
				}
			}
		}
	}
};

AttributedScopeStack AttributedScopeStack::members::pushAttributed(ScopePath scopePath, Grammar &grammar) {
	if (!scopePath)
		return *this;

	if (scopePath.index_of(' ') == -1)
		return AttributedScopeStack::members::_pushAttributed(*this, scopePath, grammar);

	auto scopes = scopePath.split(" ");
	AttributedScopeStack result = *this;
	for (str &scope: scopes) {
		result = AttributedScopeStack::members::_pushAttributed(result, scope, grammar);
	}
	return result;

}

static AttributedScopeStack AttributedScopeStack::members::createRootAndLookUpScopeName(ScopeName scopeName, EncodedTokenAttr tokenAttributes, Grammar &grammar) {
	auto rawRootMetadata = grammar->getMetadataForScope(scopeName);
	auto scopePath = ScopeStack(null, scopeName);
	auto rootStyle = grammar->theme_provider->themeMatch(scopePath);
	auto resolvedTokenAttributes = AttributedScopeStack::members::mergeAttributes(
		tokenAttributes,
		rawRootMetadata,
		rootStyle
	);
	return AttributedScopeStack(
		AttributedScopeStack::members {
			null, scopePath, resolvedTokenAttributes
		}
	);
}

AttributedScopeStack AttributedScopeStack::members::_pushAttributed(
	AttributedScopeStack target,
	ScopeName scopeName,
	Grammar &grammar,
) {
	auto rawMetadata = grammar->getMetadataForScope(scopeName);

	auto newPath = target.scopePath.push(scopeName);
	auto scopeThemeMatchResult =
		grammar->theme_provider->themeMatch(newPath);
	auto metadata = AttributedScopeStack::members::mergeAttributes(
		target.tokenAttributes,
		rawMetadata,
		scopeThemeMatchResult
	);
	return AttributedScopeStack(target, newPath, metadata);
}

struct StackDiff {
	num pops;
	array<StateStackFrame> newFrames;
};

StackDiff diffStateStacksRefEq(StateStackImpl first, StateStackImpl second) {
	num pops = 0;
	array<StateStackFrame> newFrames;

	StateStackImpl curFirst  = first;
	StateStackImpl curSecond = second;

	while (curFirst != curSecond) {
		if (curFirst && (!curSecond || curFirst.depth >= curSecond.depth)) {
			// curFirst is certainly not contained in curSecond
			pops++;
			curFirst = curFirst.parent;
		} else {
			// curSecond is certainly not contained in curFirst.
			// Also, curSecond must be defined, as otherwise a previous case would match
			newFrames.push(curSecond.toStateStackFrame());
			curSecond = curSecond->parent;
		}
	}
	return {
		pops,
		newFrames.reverse(),
	};
}

StateStackImpl applyStateStackDiff(StateStackImpl stack, StateStackImpl diff) {
	StateStackImpl curStack = stack;
	for (num i = 0; i < diff.pops; i++) {
		curStack = curStack->parent;
	}
	for (auto &frame: diff.newFrames) {
		curStack = StateStackImpl.pushFrame(curStack, frame);
	}
	return curStack;
}

Grammar createGrammar(
	ScopeName scopeName,
	RawGrammar grammar,
	num initialLanguage,
	EmbeddedLanguagesMap embeddedLanguages,
	TokenTypeMap tokenTypes,
	BalancedBracketSelectors balancedBracketSelectors,
	GrammarRepository grammar_repo,
	OnigLib oni_lib
) {
	return Grammar(
		scopeName,
		grammar,
		initialLanguage,
		embeddedLanguages,
		tokenTypes,
		balancedBracketSelectors,
		grammar_repo,
		oni_lib
	); //TODO
}

struct SyncRegistry : mx {
	struct members {
		GrammarRepository grammar_repo;
		ion::map<Grammar> _grammars;
		ion::map<RawGrammar> _rawGrammars; /// must be allocated and stored unless this is primary store
		ion::map<array<ScopeName>> _injectionGrammars;
		Theme _theme;
		ThemeProvider theme_provider;
		register(members)

		void setTheme(Theme theme) {
			_theme = theme;
		}

		array<str> getColorMap() {
			return _theme.getColorMap();
		}

		/**
		 * Add `grammar` to registry and return a list of referenced scope names
		 */
		void addGrammar(RawGrammar grammar, array<ScopeName> injectionScopeNames) {
			_rawGrammars[grammar->scopeName] = grammar;
			if (injectionScopeNames) {
				_injectionGrammars[grammar->scopeName] = injectionScopeNames;
			}
		}

		/**
		 * Lookup a raw grammar.
		 */
		RawGrammar lookup(ScopeName scopeName) {
			field<RawGrammar> *f = _rawGrammars.lookup(scopeName);
			return f ? f->value : null;
		}

		/**
		 * Returns the injections for the given grammar
		 */
		array<ScopeName> injections(ScopeName targetScope) {
			return _injectionGrammars[targetScope];
		}

		/**
		 * Get the default theme settings
		 */
		StyleAttributes getDefaults() {
			return _theme.getDefaults();
		}

		/**
		 * Match a scope in the theme.
		 */
		StyleAttributes themeMatch(ScopeStack scopePath) {
			return _theme.match(scopePath);
		}

		/**
		 * Lookup a grammar.
		 */
		Grammar &grammarForScopeName(
			ScopeName 					scopeName,
			num 						initialLanguage,
			EmbeddedLanguagesMap 		embeddedLanguages,
			TokenTypeMap 				tokenTypes,
			BalancedBracketSelectors 	balancedBracketSelectors
		) {
			if (!_grammars.has(scopeName)) {
				auto rawGrammar = _rawGrammars.get(scopeName);
				if (!rawGrammar) {
					return null;
				}
				_grammars.set(scopeName, createGrammar(
					scopeName,
					rawGrammar,
					initialLanguage,
					embeddedLanguages,
					tokenTypes,
					balancedBracketSelectors,
					this
				));
			}
			return _grammars[scopeName];
		}
	};

	mx_basic(SyncRegistry);

	SyncRegistry(Theme theme):SyncRegistry() {
		data->_theme = theme;
		data->grammar_repo->lookup     = lambda<RawGrammar(ScopeName)>(this, SyncRegistry::lookup);
		data->grammar_repo->injections = lambda<array<ScopeName>(ScopeName)>(this, SyncRegistry::injections);
	}
}

struct IWhileStack:mx {
	struct members {
		StateStackImpl stack;
		BeginWhileRule rule;
		register(members);
	};
	mx_basic(IWhileStack);
};

IWhileCheckResult _checkWhileConditions(
		Grammar &grammar, utf16 &lineText, bool isFirstLine,
		num linePos, StateStackImpl stack, LineTokens &lineTokens) {
	num anchorPosition = (stack.beginRuleCapturedEOL ? 0 : -1);

	array<IWhileStack> whileRules;

	for (StateStackImpl node = stack; node; node = node.pop()) {
		mx nodeRule = node.getRule(grammar);
		if (nodeRule.type() == typeof(BeginWhileRule)) {
			whileRules.push(IWhileStack {
				rule  = nodeRule,
				stack = node
			});
		}
	}

	for (auto whileRule = whileRules.pop(); whileRules.len(); whileRule = whileRules.pop()) {
		const { ruleScanner, findOptions } = prepareRuleWhileSearch(whileRule.rule, grammar, whileRule.stack.endRule, isFirstLine, linePos == anchorPosition);
		const r = ruleScanner.findNextMatchSync(lineText, linePos, findOptions);
		if (DebugFlags.InDebugMode) {
			console.log('  scanning for while rule');
			console.log(ruleScanner.toString());
		}

		if (r) {
			const matchedRuleId = r.ruleId;
			if (matchedRuleId != whileRuleId) {
				// we shouldn't end up here
				stack = whileRule.stack.pop();
				break;
			}
			if (r.captureIndices && r.captureIndices.len()) {
				lineTokens.produce(whileRule.stack, r.captureIndices[0].start);
				handleCaptures(grammar, lineText, isFirstLine, whileRule.stack, lineTokens, whileRule.rule.whileCaptures, r.captureIndices);
				lineTokens.produce(whileRule.stack, r.captureIndices[0].end);
				anchorPosition = r.captureIndices[0].end;
				if (r.captureIndices[0].end > linePos) {
					linePos = r.captureIndices[0].end;
					isFirstLine = false;
				}
			}
		} else {
			if (DebugFlags.InDebugMode) {
				console.log('  popping ' + whileRule.rule.debugName() + ' - ' + whileRule.rule.debugWhileRegExp);
			}

			stack = whileRule.stack.pop();
			break;
		}
	}

	return { .stack = stack, .linePos = linePos, .anchorPosition = anchorPosition, .isFirstLine = isFirstLine };
}

TokenizeStringResult _tokenizeString(
	Grammar grammar, utf16 lineText, bool isFirstLine,
	num linePos, StateStackImpl stack, LineTokens lineTokens,
	bool checkWhileConditions, num timeLimit 
) {
	num lineLength = lineText.len();

	bool STOP = false;
	num anchorPosition = -1;

	if (checkWhileConditions) {
		const whileCheckResult = _checkWhileConditions(
			grammar,
			lineText,
			isFirstLine,
			linePos,
			stack,
			lineTokens
		);
		stack = whileCheckResult.stack;
		linePos = whileCheckResult.linePos;
		isFirstLine = whileCheckResult.isFirstLine;
		anchorPosition = whileCheckResult.anchorPosition;
	}

	const startTime = Date.now();
	auto scanNext = [&] () -> void {
		if (is_debug()) {
			console.log("");
			//console.log(
			//	`@@scanNext ${linePos}: |${lineText.content
			//		.substr(linePos)
			//		.replace(/\n$/, "\\n")}|`
			//);
		}
		const r = matchRuleOrInjections(
			grammar,
			lineText,
			isFirstLine,
			linePos,
			stack,
			anchorPosition
		);

		if (!r) {
			if (is_debug()) {
				console.log("  no more matches.");
			}
			// No match
			lineTokens.produce(stack, lineLength);
			STOP = true;
			return;
		}

		const IOnigCaptureIndex captureIndices[] = r.captureIndices;
		const matchedRuleId = r.matchedRuleId;

		const hasAdvanced =
			captureIndices && captureIndices.len() > 0
				? captureIndices[0].end > linePos
				: false;

		if (matchedRuleId == endRuleId) {
			// We matched the `end` for this rule => pop it
			BeginEndRule poppedRule = stack.getRule(grammar);

			if (DebugFlags.InDebugMode) {
				console.log(
					"  popping " +
						poppedRule.debugName +
						" - " +
						poppedRule.debugEndRegExp
				);
			}

			lineTokens.produce(stack, captureIndices[0].start);
			stack = stack.withContentNameScopesList(stack.nameScopesList!);
			handleCaptures(
				grammar,
				lineText,
				isFirstLine,
				stack,
				lineTokens,
				poppedRule.endCaptures,
				captureIndices
			);
			lineTokens.produce(stack, captureIndices[0].end);

			// pop
			const popped = stack;
			stack = stack->parent.grab();
			anchorPosition = popped.getAnchorPos();

			if (!hasAdvanced && popped.getEnterPos() == linePos) {
				// Grammar pushed & popped a rule without advancing
				if (DebugFlags.InDebugMode) {
					console.error(
						"[1] - Grammar is in an endless loop - Grammar pushed & popped a rule without advancing"
					);
				}

				// See https://github.com/Microsoft/vscode-textmate/issues/12
				// Let's assume this was a mistake by the grammar author and the intent was to continue in this state
				stack = popped;

				lineTokens.produce(stack, lineLength);
				STOP = true;
				return;
			}
		} else {
			// We matched a rule!
			const _rule = grammar->getRule(matchedRuleId);

			lineTokens.produce(stack, captureIndices[0].start);

			const beforePush = stack;
			// push it on the stack rule
			const scopeName = _rule.getName(lineText.content, captureIndices);
			const nameScopesList = stack.contentNameScopesList.pushAttributed(
				scopeName,
				grammar
			);
			stack = stack.push(
				matchedRuleId,
				linePos,
				anchorPosition,
				captureIndices[0].end == lineLength,
				null,
				nameScopesList,
				nameScopesList
			);

			if (_rule instanceof BeginEndRule) {
				const pushedRule = _rule;
				if (DebugFlags.InDebugMode) {
					console.log(
						"  pushing " +
							pushedRule.debugName +
							" - " +
							pushedRule.debugBeginRegExp
					);
				}

				handleCaptures(
					grammar,
					lineText,
					isFirstLine,
					stack,
					lineTokens,
					pushedRule.beginCaptures,
					captureIndices
				);
				lineTokens.produce(stack, captureIndices[0].end);
				anchorPosition = captureIndices[0].end;

				const contentName = pushedRule.getContentName(
					lineText.content,
					captureIndices
				);
				const contentNameScopesList = nameScopesList.pushAttributed(
					contentName,
					grammar
				);
				stack = stack.withContentNameScopesList(contentNameScopesList);

				if (pushedRule.endHasBackReferences) {
					stack = stack.withEndRule(
						pushedRule.getEndWithResolvedBackReferences(
							lineText.content,
							captureIndices
						)
					);
				}

				if (!hasAdvanced && beforePush.hasSameRuleAs(stack)) {
					// Grammar pushed the same rule without advancing
					if (DebugFlags.InDebugMode) {
						console.error(
							"[2] - Grammar is in an endless loop - Grammar pushed the same rule without advancing"
						);
					}
					stack = stack.pop();
					lineTokens.produce(stack, lineLength);
					STOP = true;
					return;
				}
			} else if (_rule instanceof BeginWhileRule) {
				const pushedRule = <BeginWhileRule>_rule;
				if (DebugFlags.InDebugMode) {
					console.log("  pushing " + pushedRule.debugName());
				}

				handleCaptures(
					grammar,
					lineText,
					isFirstLine,
					stack,
					lineTokens,
					pushedRule.beginCaptures,
					captureIndices
				);
				lineTokens.produce(stack, captureIndices[0].end);
				anchorPosition = captureIndices[0].end;
				const contentName = pushedRule.getContentName(
					lineText.content,
					captureIndices
				);
				const contentNameScopesList = nameScopesList.pushAttributed(
					contentName,
					grammar
				);
				stack = stack.withContentNameScopesList(contentNameScopesList);

				if (pushedRule.whileHasBackReferences) {
					stack = stack.withEndRule(
						pushedRule.getWhileWithResolvedBackReferences(
							lineText.content,
							captureIndices
						)
					);
				}

				if (!hasAdvanced && beforePush.hasSameRuleAs(stack)) {
					// Grammar pushed the same rule without advancing
					if (DebugFlags.InDebugMode) {
						console.error(
							"[3] - Grammar is in an endless loop - Grammar pushed the same rule without advancing"
						);
					}
					stack = stack.pop();
					lineTokens.produce(stack, lineLength);
					STOP = true;
					return;
				}
			} else {
				const matchingRule = <MatchRule>_rule;
				if (DebugFlags.InDebugMode) {
					console.log(
						"  matched " +
							matchingRule.debugName() +
							" - " +
							matchingRule.debugMatchRegExp
					);
				}

				handleCaptures(
					grammar,
					lineText,
					isFirstLine,
					stack,
					lineTokens,
					matchingRule.captures,
					captureIndices
				);
				lineTokens.produce(stack, captureIndices[0].end);

				// pop rule immediately since it is a MatchRule
				stack = stack.pop();

				if (!hasAdvanced) {
					// Grammar is not advancing, nor is it pushing/popping
					if (DebugFlags.InDebugMode) {
						console.error(
							"[4] - Grammar is in an endless loop - Grammar is not advancing, nor is it pushing/popping"
						);
					}
					stack = stack.safePop();
					lineTokens.produce(stack, lineLength);
					STOP = true;
					return;
				}
			}
		}

		if (captureIndices[0].end > linePos) {
			// Advance stream
			linePos = captureIndices[0].end;
			isFirstLine = false;
		}
	};

	while (!STOP) {
		if (timeLimit != 0) {
			const elapsedTime = Date.now() - startTime;
			if (elapsedTime > timeLimit) {
				return TokenizeStringResult(stack, true);
			}
		}
		scanNext(); // potentially modifies linePos && anchorPosition
	}

	return TokenizeStringResult(stack, false);
}

struct Registry:mx {
	struct members {
		RegistryOptions _options;
		SyncRegistry _syncRegistry;
		register(members)

		void dispose() {
			_syncRegistry.dispose();
		}

		/**
		 * Change the theme. Once called, no previous `ruleStack` should be used anymore.
		 */
		void setTheme(IRawTheme &theme, array<str> colorMap = {}) {
			_syncRegistry.setTheme(Theme.createFromRawTheme(theme, colorMap));
		}

		/**
		 * Returns a lookup array for color ids.
		 */
		array<str> getColorMap() {
			return _syncRegistry.getColorMap();
		}

		/**
		 * Load the grammar for `scopeName` and all referenced included grammars asynchronously.
		 * Please do not use language id 0.
		 */
		Grammar loadGrammarWithEmbeddedLanguages(
			ScopeName initialScopeName,
			num initialLanguage,
			EmbeddedLanguagesMap &embeddedLanguages // never pass the structs
		) {
			return loadGrammarWithConfiguration(initialScopeName, initialLanguage, { embeddedLanguages });
		}

		/**
		 * Load the grammar for `scopeName` and all referenced included grammars asynchronously.
		 * Please do not use language id 0.
		 */
		Grammar loadGrammarWithConfiguration(
			ScopeName initialScopeName,
			num initialLanguage,
			IGrammarConfiguration &configuration
		) {
			return _loadGrammar(
				initialScopeName,
				initialLanguage,
				configuration.embeddedLanguages,
				configuration.tokenTypes,
				BalancedBracketSelectors(
					configuration.balancedBracketSelectors,
					configuration.unbalancedBracketSelectors
				)
			);
		}

		/**
		 * Load the grammar for `scopeName` and all referenced included grammars asynchronously.
		 */
		Grammar loadGrammar(ScopeName initialScopeName) {
			return _loadGrammar(initialScopeName, 0, null, null, null);
		}

		Grammar _loadGrammar(
			ScopeName 					initialScopeName,
			num 						initialLanguage,
			EmbeddedLanguagesMap 	   &embeddedLanguages,
			TokenTypeMap 			   &tokenTypes,
			BalancedBracketSelectors 	balancedBracketSelectors
		) {
			const dependencyProcessor = ScopeDependencyProcessor(_syncRegistry, initialScopeName);
			while (dependencyProcessor.Q.length > 0) {
				await Promise.all(dependencyProcessor.Q.map((request) => _loadSingleGrammar(request.scopeName)));
				dependencyProcessor.processQueue();
			}

			return _grammarForScopeName(
				initialScopeName,
				initialLanguage,
				embeddedLanguages,
				tokenTypes,
				balancedBracketSelectors
			);
		}

		/// convert all async functions to sync; not a problem

		Grammar _loadSingleGrammar(ScopeName scopeName) {
			return _doLoadSingleGrammar(scopeName);
		}

		Grammar _doLoadSingleGrammar(ScopeName scopeName) {
			Grammar grammar = _options.loadGrammar(scopeName);
			lambda<array<ScopeName>(ScopeName)> injections =
				_options.getInjections.type() == typeof(lambda<array<ScopeName>(ScopeName)>) ?
					_options.getInjections(scopeName) : {};
			_syncRegistry.addGrammar(grammar, injections);
			return grammar;
		}

		/**
		 * Adds a rawGrammar.
		 */
		void addGrammar(
			RawGrammar rawGrammar,
			array<str> injections = {},
			num initialLanguage = 0,
			EmbeddedLanguagesMap embeddedLanguages = null)
		{
			_syncRegistry.addGrammar(rawGrammar, injections);
			return _grammarForScopeName(rawGrammar.scopeName, initialLanguage, embeddedLanguages);
		}

		/**
		 * Get the grammar for `scopeName`. The grammar must first be created via `loadGrammar` or `addGrammar`.
		 */
		Grammar _grammarForScopeName(
			str scopeName,
			num initialLanguage = 0,
			EmbeddedLanguagesMap embeddedLanguages = null,
			TokenTypeMap tokenTypes = null,
			BalancedBracketSelectors balancedBracketSelectors = null
		) {
			return _syncRegistry.grammarForScopeName(
				scopeName,
				initialLanguage,
				embeddedLanguages,
				tokenTypes,
				balancedBracketSelectors
			);
		}
	};

	mx_basic(Registry);

	Registry(RegistryOptions options) {
		data->_options = options;
		data->_syncRegistry = SyncRegistry(Theme::members::createFromRawTheme(options.theme, options.colorMap));
	}
}

#endif



struct View:Element {
    struct props {
        int         sample;
        int         sample2;
        int         test_set;
        callback    clicked;
        ///
        doubly<prop> meta() {
            return {
                prop { "sample",   sample   },
                prop { "sample2",  sample2  },
                prop { "test_set", test_set },
                prop { "clicked",  clicked  }
            };
        }
        type_register(props);
    };

    component(View, Element, props);

    node update() {
        return array<node> {
            Edit {
                { "content", "Multiline edit test" }
            }
        };
    }
};

#if 0

template <typename... Ts>
struct com;

template <typename T>
using rr = std::reference_wrapper<T>;

/// the memory should all be reference counted the same
/// so the idea of pushing lots of these refs per class is not ideal.  they are used at the same time
/// there must be 1 memory!
/// its a bit too much to change atm.

template <typename T, typename... Ts>
struct com<T, Ts...>: com<Ts...> {
	static T default_v;

	std::tuple<T*, Ts*...> ptr;

	template <typename TT, typename... TTs>
	memory *tcount() {
		if (sizeof(TTs))
			return 1 + tcount<TTs...>();
		else
			return 1;
	}

	template <typename TT, typename... TTs>
	memory *tsize() {
		if (sizeof(TTs))
			return 1 + tcount<TTs...>();
		else
			return sizeof(TTs);
	}

	/// we want this method to do all allocation for the types
	template <typename TT, typename... TTs>
	TT *alloc_next(TT *src, TTs *src_next...) {
		if (sizeof(TTs))
			return 1 + tcount<TTs...>();
		else
			return 1;
	}

	/// we want this method to do all allocation for the types
	template <typename TT, typename... TTs>
	TT *alloc(TT *src, TTs *src_next...) {
		static std::mutex mtx;
		mtx.lock();

		static const int bsize = 32;
		static int cursor = bsize;
		static TT* block = null;
		if (cursor == bsize) {
			cursor = 0;
			block  = (TT*)calloc(bsize, sizeof(TT));
		}
		if (src) {
			return new (&block[cursor++]) TT(src);
		} else {
			return new (&block[cursor++]) TT();
		}

		if (sizeof(TTs))
			return 1 + tcount<TTs...>();
		else
			return 1;

		mtx.unlock();
	}


	/// perhaps use 
	template <typename TT, typename... TTs>
	memory *ralloc(TT* src) {
		mem = mx::alloc<T>(src, typeof(TT)); /// we should alloc-cache about 32 types at a time inside memory, pull from that
		if (sizeof(TTs)) {
			ralloc<TTs...>(src);
		}
	}

	template <typename TT, typename... TTs>
	memory *base(TT* src) {
		mem = mx::alloc<T>(src, typeof(TT));
		if (sizeof(TTs...))
			ralloc<TTs...>()
	}

	com(memory* mem) : mem(mem->grab()) { }

    com() : com<Ts...>(), ptr(new T(), new Ts()...), mem(base<T, Ts...>((T*)null)), ptr() {

		// new T(*std::get<T*>(other.pointers)), new Ts(*std::get<Ts*>(other.pointers))...
	}

	com(const com& b) : com<Ts...>(), ptr(
		new T(*std::get<T*>(b.ptr)), new Ts(*std::get<Ts*>(b.ptr))...), mem(base<T, Ts...>((T*)null)), ptr() {
	}

	com(const T& b, const Ts... bs) : com<Ts...>(), ptr(
		alloc(&b), alloc(&bs)...), mem(base<T, Ts...>((T*)null)), ptr() {
	}
 
	template <typename CTX>
	com(initial<arg> args) {
		/// props can be shared throughout, perhaps
		/// the only thing thing this class needs is references accessible at design time by the users of these classes
	}
};

template <> struct com <> : mx { };

}
#endif