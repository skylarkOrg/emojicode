//
//  Scope.hpp
//  Emojicode
//
//  Created by Theo Weidmann on 02/05/16.
//  Copyright © 2016 Theo Weidmann. All rights reserved.
//

#ifndef Scope_hpp
#define Scope_hpp

#include <map>
#include <memory>
#include "Token.hpp"
#include "Variable.hpp"

class Scoper;

class Scope {
public:
    Scope() = delete;
    Scope(Scoper *scoper) : scoper_(scoper) {};
    
    void setVariableInitialization(bool initd);
    void pushInitializationLevel();
    void popInitializationLevel();
    
    /** Sets a variable in this scope. */
    Variable& setLocalVariable(const EmojicodeString &variable, Type type, bool frozen, SourcePosition pos,
                               bool initialized = false);
    /** Allocates a variable only for internal use and returns its ID. */
    int allocateInternalVariable(Type type);
    
    /**
     * Retrieves a variable form the scope or returns @c nullptr.
     */
    Variable& getLocalVariable(const EmojicodeString &variable);

    bool hasLocalVariable(const EmojicodeString &variable) const;
    
    /**
     * Emits @c errorMessage if not all instance variable were initialized.
     * @params errorMessage The error message that will probably be issued. It should include @c %s for the name
                            of the variable.
     */
    void initializerUnintializedVariablesCheck(const Token &errorToken, const char *errorMessage);
    
    /**
     * Emits a warning for each non-frozen variable that has not been mutated.
     */
    void recommendFrozenVariables() const;
    
    size_t size() const { return size_; }

    const std::map<EmojicodeString, Variable>& map() const { return map_; }
private:
    std::map<EmojicodeString, Variable> map_;
    int size_ = 0;
    Scoper *scoper_;
};

#endif /* Scope_hpp */
