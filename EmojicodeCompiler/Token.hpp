//
//  Token.hpp
//  Emojicode
//
//  Created by Theo Weidmann on 26/04/16.
//  Copyright © 2016 Theo Weidmann. All rights reserved.
//

#ifndef Token_hpp
#define Token_hpp

#include "EmojicodeCompiler.hpp"

class TokenStream;

enum class TokenType {
    NoType,
    String,
    Comment,
    DocumentationComment,
    Integer,
    Double,
    BooleanTrue,
    BooleanFalse,
    Identifier,
    Variable,
    Symbol
};

class Token;

struct SourcePosition {
    SourcePosition(const Token &token);
    SourcePosition(size_t line, size_t character, std::string file) : line(line), character(character), file(file) {};
    size_t line;
    size_t character;
    std::string file;
};

class Token {
    friend TokenStream lex(std::string);
    friend class TokenStream;
public:
    Token(const Token&) = delete;
    Token(SourcePosition p) : position_(p) {}
    Token(SourcePosition p, Token *prevToken) : position_(p) {
        if(prevToken)
            prevToken->nextToken_ = this;
    }
    
    /** Returns the position at which this token was defined. */
    const SourcePosition& position() const { return position_; }
    /** Returns the type of this token. */
    const TokenType& type() const { return type_; }
    /** Represents the value of this token. */
    const EmojicodeString& value() const { return value_; }

    bool isIdentifier(EmojicodeChar ch) const;
    
    /** Returns a string describing the token */
    const char* stringName() const;
    /** Returns a string describing the token */
    static const char* stringNameForType(TokenType type);
    
    void validate() const;
private:
    SourcePosition position_;
    TokenType type_ = TokenType::NoType;
    Token *nextToken_ = nullptr;
    EmojicodeString value_;
};

#endif /* Token_hpp */
