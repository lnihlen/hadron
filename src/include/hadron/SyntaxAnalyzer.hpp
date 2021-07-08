#ifndef SRC_SYNTAX_ANALYZER_HPP_
#define SRC_SYNTAX_ANALYZER_HPP_

#include "hadron/Hash.hpp"
#include "hadron/Literal.hpp"
#include "hadron/Type.hpp"

#include <list>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace hadron {

class ErrorReporter;
class Parser;
namespace parse {
struct BinopCallNode;
struct BlockNode;
struct CallNode;
struct ClassNode;
struct Node;
}

namespace ast {
    enum ASTType {
        kAssign,      // Assign a value to a variable
        kInlineBlock, // Like a block but hoists its variable defenitions to the parent Block
        kBlock,       // Scoped block of code
        kCalculate,   // Arithmetic or comparisons of two numbers, either float or int
        kConstant,
        kDispatch,    // method call
        kValue,
        kResult,
        kWhile,       // while loop
        kClass        // class definition
    };

    struct AST {
        AST(ASTType t): astType(t) {}
        AST() = delete;
        virtual ~AST() = default;

        ASTType astType;
        Type valueType = Type::kSlot;
    };

    struct CalculateAST : public AST {
        CalculateAST(Hash hash): AST(kCalculate), selector(hash) {}
        CalculateAST() = delete;
        virtual ~CalculateAST() = default;

        Hash selector;
        std::unique_ptr<AST> left;
        std::unique_ptr<AST> right;
    };

    struct ValueAST;

    struct Value {
        Value(std::string n): name(n) {}
        std::string name;
        std::list<ValueAST*> references;
    };

    struct BlockAST : public AST {
        BlockAST(BlockAST* p): AST(kBlock), parent(p) {}
        BlockAST() = delete;
        virtual ~BlockAST() = default;

        BlockAST* parent;
        std::unordered_map<Hash, Value> arguments;
        std::unordered_map<Hash, Value> variables;
        std::vector<std::unique_ptr<AST>> statements;
    };

    struct InlineBlockAST : public AST {
        InlineBlockAST(): AST(kInlineBlock) {}
        virtual ~InlineBlockAST() = default;

        std::vector<std::unique_ptr<AST>> statements;
    };

    struct ValueAST : public AST {
        ValueAST(Hash hash, BlockAST* block): AST(kValue), nameHash(hash), owningBlock(block) {}
        ValueAST() = delete;
        virtual ~ValueAST() = default;

        Hash nameHash = 0;
        BlockAST* owningBlock = nullptr;
        size_t revision = 0;
        std::list<ValueAST*>::iterator reference;
        bool isWrite = false;
    };

    struct ResultAST : public AST {
        ResultAST(): AST(kResult) {}
        virtual ~ResultAST() = default;

        std::unique_ptr<AST> value;
    };

    struct AssignAST : public AST {
        AssignAST(): AST(kAssign) {}
        virtual ~AssignAST() = default;

        std::unique_ptr<ValueAST> target;
        std::unique_ptr<AST> value;
    };

    struct ConstantAST : public AST {
        ConstantAST(const Literal& literal): AST(kConstant), value(literal) {
            valueType = value.type();
        }
        ConstantAST() = delete;
        virtual ~ConstantAST() = default;
        Literal value;
    };

    struct WhileAST : public AST {
        WhileAST(): AST(kWhile) {
            valueType = Type::kNil;
        }
        virtual ~WhileAST() = default;

        // while { condition } { action }
        std::unique_ptr<AST> condition;
        std::unique_ptr<AST> action;
    };

    struct DispatchAST : public AST {
        DispatchAST(): AST(kDispatch) {}
        virtual ~DispatchAST() = default;

        Hash selectorHash;
        std::string selector;
        std::vector<std::unique_ptr<AST>> arguments;
    };

    struct ClassAST : public AST {
        ClassAST(): AST(kClass) {}
        virtual ~ClassAST() = default;

        Hash nameHash;
        std::string name;
        Hash superClassHash;

        std::unordered_map<Hash, Value> variables;
        std::unordered_map<Hash, Value> classVariables;
        std::unordered_map<Hash, Literal> constants;
        std::unordered_map<Hash, std::unique_ptr<BlockAST>> methods;
        std::unordered_map<Hash, std::unique_ptr<BlockAST>> classMethods;
        // Values store their names in the struct. For the constants and methods we store the name here.
        std::unordered_map<Hash, std::string> names;
    };
} // namespace ast

// Produces an AST from a Parse Tree
class SyntaxAnalyzer {
public:
    SyntaxAnalyzer(std::shared_ptr<ErrorReporter> errorReporter);
    ~SyntaxAnalyzer() = default;

    bool buildAST(const Parser* parser);

    // lower tree to three-address form with virtual registers
    bool toThreeAddressForm();

    const ast::AST* ast() const { return m_ast.get(); }

private:
    // Create a new BlockAST from a parse tree BlockNode.
    std::unique_ptr<ast::BlockAST> buildBlock(const Parser* parser, const parse::BlockNode* blockNode,
        ast::BlockAST* parent);
    std::unique_ptr<ast::InlineBlockAST> buildInlineBlock(const Parser* parser, const parse::BlockNode* blockNode,
        ast::BlockAST* parent);
    std::unique_ptr<ast::ClassAST> buildClass(const Parser* parser, const parse::ClassNode* classNode);

    // Fill an existing AST with nodes from the parse tree, searching within |block| for variable names
    void fillAST(const Parser* parser, const parse::Node* parseNode, ast::BlockAST* block,
        std::vector<std::unique_ptr<ast::AST>>* ast);
    // Build an expression tree without appending to the block, although variables may be appended if they are defined.
    std::unique_ptr<ast::AST> buildExprTree(const Parser* parser, const parse::Node* parseNode, ast::BlockAST* block);

    // Calls can be control flow or method dispatches. Differentiate, assemble, and return.
    std::unique_ptr<ast::AST> buildCall(const Parser* parser, const parse::CallNode* callNode, ast::BlockAST* block);
    // Binops can be arithmetic functions or method dispatches. Differentiate, assemble, and return.
    std::unique_ptr<ast::AST> buildBinop(const Parser* parser, const parse::BinopCallNode* binopNode,
        ast::BlockAST* block);

    // Find a value within the Block tree, or return nullptr if not found. |isWrite| should be true if this is
    // a write to this value.
    std::unique_ptr<ast::ValueAST> findValue(Hash nameHash, ast::BlockAST* block, bool isWrite);

    std::shared_ptr<ErrorReporter> m_errorReporter;

    std::unique_ptr<ast::AST> m_ast;
};

} // namespace hadron

#endif // SRC_SYNTAX_ANALYZER_HPP_