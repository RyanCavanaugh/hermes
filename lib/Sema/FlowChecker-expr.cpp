/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include "FlowChecker.h"

#define DEBUG_TYPE "FlowChecker"

namespace hermes {
namespace flow {

class FlowChecker::ExprVisitor {
  FlowChecker &outer_;

 public:
  explicit ExprVisitor(FlowChecker &outer) : outer_(outer) {}

  bool incRecursionDepth(ESTree::Node *n) {
    return outer_.incRecursionDepth(n);
  }
  void decRecursionDepth() {
    return outer_.decRecursionDepth();
  }

  /// Default case for all ignored nodes, we still want to visit their children.
  void visit(ESTree::Node *node) {
    if (0) {
      LLVM_DEBUG(
          llvh::dbgs() << "Unsupported node " << node->getNodeName()
                       << " in expr context\n");
      llvm_unreachable("invalid node in expression context");
    } else {
      visitESTreeChildren(*this, node);
    }
  }

  void visit(ESTree::FunctionExpressionNode *node) {
    return outer_.visit(node);
  }
  void visit(ESTree::ArrowFunctionExpressionNode *node) {
    return outer_.visit(node);
  }
  void visit(ESTree::ClassExpressionNode *node) {
    return outer_.visit(node);
  }

  void visit(ESTree::IdentifierNode *node, ESTree::Node *parent) {
    // Skip cases where the identifier isn't a variable.
    // TODO: these should be dealt with by the parent node.
    if (auto *prop = llvh::dyn_cast<ESTree::PropertyNode>(parent)) {
      if (!prop->_computed && prop->_key == node) {
        // { identifier: ... }
        return;
      }
    }

    if (auto *mem = llvh::dyn_cast<ESTree::MemberExpressionNode>(parent)) {
      if (!mem->_computed && mem->_property == node) {
        // expr.identifier
        return;
      }
    }

    // Identifiers that aren't variables.
    if (llvh::isa<ESTree::MetaPropertyNode>(parent) ||
        llvh::isa<ESTree::BreakStatementNode>(parent) ||
        llvh::isa<ESTree::ContinueStatementNode>(parent) ||
        llvh::isa<ESTree::LabeledStatementNode>(parent)) {
      return;
    }

    // typeof
    if (auto *unary = llvh::dyn_cast<ESTree::UnaryExpressionNode>(parent)) {
      if (unary->_operator == outer_.kw_.identTypeof) {
        // FIXME: handle typeof identifier
        return;
      }
    }

    auto *decl = outer_.getDecl(node);
    assert(decl && "unresolved identifier in expression context");

    if (sema::Decl::isKindGlobal(decl->kind) &&
        decl->name.getUnderlyingPointer() == outer_.kw_.identUndefined) {
      outer_.setNodeType(node, outer_.flowContext_.getVoid());
      return;
    }

    if (decl->generic) {
      bool isValid = false;
      if (auto *call = llvh::dyn_cast<ESTree::CallExpressionLikeNode>(parent)) {
        if (ESTree::getCallee(call) == node)
          isValid = true;
      }
      if (auto *newExpr = llvh::dyn_cast<ESTree::NewExpressionNode>(parent)) {
        if (newExpr->_callee == node)
          isValid = true;
      }
      if (auto *classDecl =
              llvh::dyn_cast<ESTree::ClassDeclarationNode>(parent)) {
        if (classDecl->_id == node)
          isValid = true;
        if (classDecl->_superClass == node)
          isValid = true;
      }
      if (!isValid) {
        // Unspecialized generic functions are only allowed in calls.
        // They can't be stored directly because they need type parameters.
        outer_.sm_.error(
            node->getSourceRange(),
            "ft: invalid use of generic function outside of call");
        return;
      }
    }

    // The type is either the type of the identifier or "any".
    Type *type = outer_.flowContext_.findDeclType(decl);

    // Generic decls don't have types set because they aren't real values.
    if (!type && !sema::Decl::isKindGlobal(decl->kind) && !decl->generic) {
      // Assume "any" during the call to setNodeType below.
      // If we're in the same function as decl was declared,
      // then IRGen can report TDZ violations early when applicable.
      // See FlowChecker::AnnotateScopeDecls doc-comment.

      // Report a warning because this is likely unintended.
      // The following code errors in Flow but not in untyped JS:
      //   x = 10;
      //   var x = x + 1;
      // So we don't error to maintain compatibility when there's no
      // annotations.
      outer_.sm_.warning(
          node->getSourceRange(),
          "local variable may be used prior to declaration, assuming 'any'");
    }

    outer_.setNodeType(node, type ? type : outer_.flowContext_.getAny());
  }

  void visit(ESTree::ThisExpressionNode *node) {
    outer_.setNodeType(
        node,
        outer_.curFunctionContext_->thisParamType
            ? outer_.curFunctionContext_->thisParamType
            : outer_.flowContext_.getAny());
  }

  void visit(ESTree::MemberExpressionNode *node) {
    // TODO: types
    visitESTreeNode(*this, node->_object, node);
    if (node->_computed)
      visitESTreeNode(*this, node->_property, node);

    Type *objType = outer_.getNodeTypeOrAny(node->_object);
    Type *resType = outer_.flowContext_.getAny();

    // Attempt to narrow object type if it doesn't currently support member
    // access.
    if (Type *narrowedObjType = outer_.getNonOptionalSingleType(objType)) {
      objType = narrowedObjType;
      node->_object = outer_.implicitCheckedCast(
          node->_object,
          narrowedObjType,
          {.canFlow = true, .needCheckedCast = true});
    }

    if (auto *classType = llvh::dyn_cast<ClassType>(objType->info)) {
      if (node->_computed) {
        outer_.sm_.error(
            node->_property->getSourceRange(),
            "ft: computed access to class instances not supported");
      } else {
        bool found = false;
        auto id = llvh::cast<ESTree::IdentifierNode>(node->_property);
        auto optField =
            classType->findField(Identifier::getFromPointer(id->_name));
        if (optField) {
          resType = optField->getField()->type;
          found = true;
        } else {
          auto optMethod = classType->getHomeObjectTypeInfo()->findField(
              Identifier::getFromPointer(id->_name));
          if (optMethod) {
            resType = optMethod->getField()->type;
            found = true;
            assert(
                llvh::isa<BaseFunctionType>(resType->info) &&
                "methods must be functions");
          }
        }
        if (!found) {
          // TODO: class declaration location.
          outer_.sm_.error(
              node->_property->getSourceRange(),
              "ft: property " + id->_name->str() + " not defined in class " +
                  classType->getClassNameOrDefault());
        }
      }
    } else if (auto *arrayType = llvh::dyn_cast<ArrayType>(objType->info)) {
      if (node->_computed) {
        resType = arrayType->getElement();
        Type *indexType = outer_.getNodeTypeOrAny(node->_property);
        if (!llvh::isa<NumberType>(indexType->info) &&
            !llvh::isa<AnyType>(indexType->info)) {
          outer_.sm_.error(
              node->_property->getSourceRange(),
              "ft: array index must be a number");
        }
      } else {
        auto *id = llvh::cast<ESTree::IdentifierNode>(node->_property);
        if (id->_name == outer_.kw_.identLength) {
          resType = outer_.flowContext_.getNumber();
        } else if (id->_name == outer_.kw_.identPush) {
          // TODO: Represent .push as a real function.
          resType = outer_.flowContext_.getAny();
        } else {
          outer_.sm_.error(
              node->_property->getSourceRange(), "ft: unknown array property");
        }
      }
    } else if (auto *tupleType = llvh::dyn_cast<TupleType>(objType->info)) {
      if (node->_computed) {
        if (auto *idx =
                llvh::dyn_cast<ESTree::NumericLiteralNode>(node->_property)) {
          double d = idx->_value;
          if (0 <= d && d < tupleType->getTypes().size()) {
            // d is in bounds of the valid integer indices so the cast is safe.
            if ((uint32_t)d == d) {
              // ulen can only compare equal to d when d is a valid uint32
              // integer.
              resType = tupleType->getTypes()[(uint32_t)d];
            } else {
              outer_.sm_.error(
                  node->_property->getSourceRange(),
                  "ft: tuple index must be a non-negative integer");
            }
          } else {
            outer_.sm_.error(
                node->_property->getSourceRange(),
                "ft: tuple index out of bounds");
          }
        } else {
          outer_.sm_.error(
              node->_property->getSourceRange(),
              "ft: tuple property access requires an number literal index");
        }
      }
    } else if (!llvh::isa<AnyType>(objType->info)) {
      if (node->_computed) {
        outer_.sm_.error(
            node->_property->getSourceRange(),
            llvh::Twine(
                "ft: indexed access only allowed on array and tuple, found ") +
                objType->info->getKindName());
      } else {
        outer_.sm_.error(
            node->_property->getSourceRange(),
            llvh::Twine(
                "ft: named property access only allowed on objects, found ") +
                objType->info->getKindName());
      }
    }

    outer_.setNodeType(node, resType);
  }
  void visit(ESTree::OptionalMemberExpressionNode *node) {
    // TODO: types
    outer_.sm_.warning(
        node->getSourceRange(),
        "ft: optional member expression not implemented");
    visitESTreeNode(*this, node->_object, node);
    if (node->_computed)
      visitESTreeNode(*this, node->_property, node);
  }

  void visitExplicitCast(
      ESTree::Node *node,
      ESTree::Node *expression,
      ESTree::Node *typeAnnotation) {
    auto *resTy = outer_.parseTypeAnnotation(typeAnnotation);
    // Populate the type of this node before visiting the expression, since it
    // is already known. This also allows the result type to be used as context
    // while we are visiting the expression being cast. For instance, if we are
    // casting an empty array literal, the resulting type of the cast can be
    // used to set the element type of the array.
    outer_.setNodeType(node, resTy);
    visitESTreeNode(*this, expression, node);

    auto *expTy = outer_.getNodeTypeOrAny(expression);
    auto cf = canAFlowIntoB(expTy->info, resTy->info);
    if (!cf.canFlow) {
      outer_.sm_.error(
          node->getSourceRange(), "ft: cast from incompatible type");
    }
  }

  void visit(ESTree::TypeCastExpressionNode *node) {
    visitExplicitCast(
        node,
        node->_expression,
        llvh::cast<ESTree::TypeAnnotationNode>(node->_typeAnnotation)
            ->_typeAnnotation);
  }

  void visit(ESTree::AsExpressionNode *node) {
    visitExplicitCast(node, node->_expression, node->_typeAnnotation);
  }

  void visit(ESTree::ArrayExpressionNode *node, ESTree::Node *parent) {
    visitESTreeChildren(*this, node);

    /// Given an element in the array expression, determine its type.
    auto getElementType = [this](const ESTree::Node *elem) -> Type * {
      if (auto *spread = llvh::dyn_cast<ESTree::SpreadElementNode>(elem)) {
        // The type of a spread element depends on its argument.
        auto *spreadTy = outer_.getNodeTypeOrAny(spread->_argument);
        // If we are spreading an array, use the type of the array.
        if (auto *spreadArrTy = llvh::dyn_cast<ArrayType>(spreadTy->info))
          return spreadArrTy->getElement();
        // TODO: Handle spread of non-arrays.
        outer_.sm_.error(
            spread->_argument->getSourceRange(),
            "ft: spread argument must be an array");
        return outer_.flowContext_.getAny();
      }
      return outer_.flowContext_.getNodeTypeOrAny(elem);
    };

    /// Try using the given element type \p elTy for this array. If any elements
    /// are incompatible with the given type, report an error, otherwise, set
    /// the type of the array.
    auto tryArrayElementType = [node, this, getElementType](Type *elTy) {
      auto &elements = node->_elements;
      size_t i = 0;
      for (auto it = elements.begin(); it != elements.end(); ++it) {
        ESTree::Node *arg = &*it;
        Type *argTy = getElementType(arg);
        auto cf = canAFlowIntoB(argTy->info, elTy->info);
        if (!cf.canFlow) {
          outer_.sm_.error(
              arg->getSourceRange(),
              llvh::Twine("ft: incompatible array element type at index: ") +
                  llvh::Twine(i));
          return;
        }
        // Add a checked cast if needed. Skip spread elements, since we need to
        // cast each element they produce, rather than the spread itself.
        if (cf.needCheckedCast && !llvh::isa<ESTree::SpreadElementNode>(arg)) {
          auto newIt =
              elements.insert(it, *outer_.implicitCheckedCast(arg, elTy, cf));
          elements.erase(it);
          it = newIt;
        }

        ++i;
      }
      auto *arrTy = outer_.flowContext_.createArray(elTy);
      outer_.setNodeType(node, outer_.flowContext_.createType(arrTy));
    };

    /// Make the tuple type of the array expression based on its elements.
    auto tryTupleType = [node, this, getElementType](
                            Type *target, TupleType *targetTuple) -> void {
      auto &elements = node->_elements;
      size_t i = 0;
      // Whether we had to stop early due to not enough tuple elements.
      bool fail = false;
      for (ESTree::Node &arg : elements) {
        if (i >= targetTuple->getTypes().size()) {
          fail = true;
          break;
        }

        Type *targetTy = targetTuple->getTypes()[i];
        Type *argTy = getElementType(&arg);
        auto cf = canAFlowIntoB(argTy->info, targetTy->info);
        if (!cf.canFlow) {
          outer_.sm_.error(
              arg.getSourceRange(),
              llvh::Twine("ft: incompatible tuple element type at index: ") +
                  llvh::Twine(i));
          return;
        }
        ++i;
      }
      if (fail || i != targetTuple->getTypes().size()) {
        outer_.sm_.error(
            node->getSourceRange(),
            llvh::Twine("ft: incompatible tuple type, expected ") +
                llvh::Twine(targetTuple->getTypes().size()) +
                " elements, found " + llvh::Twine(elements.size()));
        outer_.setNodeType(node, outer_.flowContext_.getAny());
        return;
      }
      outer_.setNodeType(node, target);
    };

    // First, try to determine the desired element type from surrounding
    // context. For instance, this lets us determine the type of empty array
    // literals. If the type of an element is incompatible, it is okay to fail
    // here, since we can point to the exact element that is incompatible.

    // If this array expression initializes a variable, try the type of that
    // variable.
    if (auto *declarator =
            llvh::dyn_cast<ESTree::VariableDeclaratorNode>(parent)) {
      if (auto *id = llvh::dyn_cast<ESTree::IdentifierNode>(declarator->_id)) {
        sema::Decl *decl = outer_.getDecl(id);
        // It's possible we're just trying to infer the type of the declarator
        // right now, so it's possible `findDeclType` returns nullptr.
        if (Type *declType = outer_.flowContext_.findDeclType(decl)) {
          if (auto *arrTy = llvh::dyn_cast<ArrayType>(declType->info)) {
            tryArrayElementType(arrTy->getElement());
            return;
          }
          if (auto *tupleTy = llvh::dyn_cast<TupleType>(declType->info)) {
            tryTupleType(declType, tupleTy);
            return;
          }
        }
      }
    }

    // If this array expression is immediately cast to something else, try using
    // the type we are casting to.
    if (llvh::isa<ESTree::TypeCastExpressionNode>(parent) ||
        llvh::isa<ESTree::AsExpressionNode>(parent)) {
      auto *resTy = outer_.getNodeTypeOrAny(parent);
      if (auto *arrTy = llvh::dyn_cast<ArrayType>(resTy->info)) {
        tryArrayElementType(arrTy->getElement());
        return;
      }
      if (auto *tupleTy = llvh::dyn_cast<TupleType>(resTy->info)) {
        tryTupleType(resTy, tupleTy);
        return;
      }
    }

    // If this is a returned literal, try the type of the function.
    if (llvh::isa<ESTree::ReturnStatementNode>(parent)) {
      if (auto *ftype = llvh::dyn_cast<TypedFunctionType>(
              outer_.curFunctionContext_->functionType->info)) {
        if (auto *arrTy = llvh::dyn_cast<ArrayType>(ftype)) {
          tryArrayElementType(arrTy->getElement());
          return;
        }
        if (auto *tupleTy =
                llvh::dyn_cast<TupleType>(ftype->getReturnType()->info)) {
          tryTupleType(ftype->getReturnType(), tupleTy);
          return;
        }
      }
    }

    // In principle, we could use the type of an enclosing array literal where
    // this literal is being spread or nested. However, we leave that
    // unsupported for now, as the enclosing literal would not have its type set
    // at this point.

    // We could not determine the type from the context, infer it from the
    // elements.
    Type *elTy;
    if (node->_elements.empty()) {
      // If there are no elements, we can't infer the type, so use 'any'.
      elTy = outer_.flowContext_.getAny();
    } else {
      // Construct a union of all the element types.
      llvh::SmallSetVector<Type *, 4> elTypes;
      for (const auto &arg : node->_elements)
        elTypes.insert(getElementType(&arg));

      elTy = outer_.flowContext_.createType(
          outer_.flowContext_.maybeCreateUnion(elTypes.getArrayRef()), node);
    }
    auto *arrTy = outer_.flowContext_.createArray(elTy);
    outer_.setNodeType(node, outer_.flowContext_.createType(arrTy));
  }

  void visit(ESTree::SpreadElementNode *node) {
    // Do nothing for the spread element itself, handled by the parent.
    visitESTreeChildren(*this, node);
  }

  void visit(ESTree::NullLiteralNode *node) {
    outer_.setNodeType(node, outer_.flowContext_.getNull());
  }
  void visit(ESTree::BooleanLiteralNode *node) {
    outer_.setNodeType(node, outer_.flowContext_.getBoolean());
  }
  void visit(ESTree::StringLiteralNode *node) {
    outer_.setNodeType(node, outer_.flowContext_.getString());
  }
  void visit(ESTree::TemplateLiteralNode *node) {
    for (ESTree::Node &quasi : node->_quasis)
      outer_.setNodeType(&quasi, outer_.flowContext_.getString());
    visitESTreeNodeList(*this, node->_expressions, node);
    outer_.setNodeType(node, outer_.flowContext_.getString());
  }
  void visit(ESTree::NumericLiteralNode *node) {
    outer_.setNodeType(node, outer_.flowContext_.getNumber());
  }
  void visit(ESTree::RegExpLiteralNode *node) {
    outer_.setNodeType(node, outer_.flowContext_.getAny());
  }
  void visit(ESTree::BigIntLiteralNode *node) {
    outer_.setNodeType(node, outer_.flowContext_.getBigInt());
  }
  void visit(ESTree::SHBuiltinNode *node) {
    // SHBuiltin handled at the call expression level.
    visitESTreeChildren(*this, node);
  }

  void visit(ESTree::UpdateExpressionNode *node) {
    visitESTreeNode(*this, node->_argument, node);
    Type *type = outer_.getNodeTypeOrAny(node->_argument);
    if (llvh::isa<NumberType>(type->info) ||
        llvh::isa<BigIntType>(type->info)) {
      // number and bigint don't change.
      outer_.setNodeType(node, type);
      return;
    }
    if (auto *unionType = llvh::dyn_cast<UnionType>(type->info)) {
      if (llvh::all_of(unionType->getTypes(), [](Type *t) -> bool {
            return llvh::isa<NumberType>(t->info) ||
                llvh::isa<BigIntType>(t->info);
          })) {
        // Unions of number/bigint don't change.
        outer_.setNodeType(node, type);
        return;
      }
    }
    if (llvh::isa<AnyType>(type->info)) {
      // any remains any.
      outer_.setNodeType(node, outer_.flowContext_.getAny());
      return;
    }
    outer_.sm_.error(
        node->getSourceRange(),
        "ft: update expression must be number or bigint");
  }

  enum class BinopKind : uint8_t {
    // clang-format off
    eq, ne, strictEq, strictNe, lt, le, gt, ge, shl, sshr, ushr,
    plus, minus, mul, div, rem, binOr, binXor, binAnd, exp, in, instanceOf
    // clang-format on
  };

  static BinopKind binopKind(llvh::StringRef str) {
    return llvh::StringSwitch<BinopKind>(str)
        .Case("==", BinopKind::eq)
        .Case("!=", BinopKind::ne)
        .Case("===", BinopKind::strictEq)
        .Case("!==", BinopKind::strictNe)
        .Case("<", BinopKind::lt)
        .Case("<=", BinopKind::le)
        .Case(">", BinopKind::gt)
        .Case(">=", BinopKind::ge)
        .Case("<<", BinopKind::shl)
        .Case(">>", BinopKind::sshr)
        .Case(">>>", BinopKind::ushr)
        .Case("+", BinopKind::plus)
        .Case("-", BinopKind::minus)
        .Case("*", BinopKind::mul)
        .Case("/", BinopKind::div)
        .Case("%", BinopKind::rem)
        .Case("|", BinopKind::binOr)
        .Case("^", BinopKind::binXor)
        .Case("&", BinopKind::binAnd)
        .Case("**", BinopKind::exp)
        .Case("in", BinopKind::in)
        .Case("instanceof", BinopKind::instanceOf);
  }

  enum class UnopKind : uint8_t {
    // clang-format off
    del, voidOp, typeof, plus, minus, tilde, bang, inc, dec
    // clang-format on
  };

  static UnopKind unopKind(llvh::StringRef str) {
    return llvh::StringSwitch<UnopKind>(str)
        .Case("delete", UnopKind::del)
        .Case("void", UnopKind::voidOp)
        .Case("typeof", UnopKind::typeof)
        .Case("+", UnopKind::plus)
        .Case("-", UnopKind::minus)
        .Case("~", UnopKind::tilde)
        .Case("!", UnopKind::bang)
        .Case("++", UnopKind::inc)
        .Case("--", UnopKind::dec);
  }

  static BinopKind assignKind(llvh::StringRef str) {
    return llvh::StringSwitch<BinopKind>(str)
        .Case("<<=", BinopKind::shl)
        .Case(">>=", BinopKind::sshr)
        .Case(">>>=", BinopKind::ushr)
        .Case("+=", BinopKind::plus)
        .Case("-=", BinopKind::minus)
        .Case("*=", BinopKind::mul)
        .Case("/=", BinopKind::div)
        .Case("%=", BinopKind::rem)
        .Case("|=", BinopKind::binOr)
        .Case("^=", BinopKind::binXor)
        .Case("&=", BinopKind::binAnd)
        .Case("**=", BinopKind::exp);
  }

  /// \return nullptr if the operation is not supported.
  Type *determineBinopType(BinopKind op, TypeKind lk, TypeKind rk) {
    struct BinTypes {
      BinopKind op;
      TypeKind res;
      // None indicates a wildcard, Any indicates the actual 'any' type.
      OptValue<TypeKind> left;
      OptValue<TypeKind> right;
    };

    static const BinTypes s_types[] = {
        // clang-format off
        {BinopKind::eq, TypeKind::Boolean, llvh::None, llvh::None},
        {BinopKind::ne, TypeKind::Boolean, llvh::None, llvh::None},
        {BinopKind::strictEq, TypeKind::Boolean, llvh::None, llvh::None},
        {BinopKind::strictNe, TypeKind::Boolean, llvh::None, llvh::None},

        {BinopKind::lt, TypeKind::Boolean, TypeKind::Number, TypeKind::Number},
        {BinopKind::lt, TypeKind::Boolean, TypeKind::BigInt, TypeKind::BigInt},
        {BinopKind::lt, TypeKind::Boolean, TypeKind::String, TypeKind::String},
        {BinopKind::lt, TypeKind::Boolean, TypeKind::Any, llvh::None},
        {BinopKind::lt, TypeKind::Boolean, llvh::None, TypeKind::Any},

        {BinopKind::le, TypeKind::Boolean, TypeKind::Number, TypeKind::Number},
        {BinopKind::le, TypeKind::Boolean, TypeKind::BigInt, TypeKind::BigInt},
        {BinopKind::le, TypeKind::Boolean, TypeKind::String, TypeKind::String},
        {BinopKind::le, TypeKind::Boolean, TypeKind::Any, llvh::None},
        {BinopKind::le, TypeKind::Boolean, llvh::None, TypeKind::Any},

        {BinopKind::gt, TypeKind::Boolean, TypeKind::Number, TypeKind::Number},
        {BinopKind::gt, TypeKind::Boolean, TypeKind::BigInt, TypeKind::BigInt},
        {BinopKind::gt, TypeKind::Boolean, TypeKind::String, TypeKind::String},
        {BinopKind::gt, TypeKind::Boolean, TypeKind::Any, llvh::None},
        {BinopKind::gt, TypeKind::Boolean, llvh::None, TypeKind::Any},

        {BinopKind::ge, TypeKind::Boolean, TypeKind::Number, TypeKind::Number},
        {BinopKind::ge, TypeKind::Boolean, TypeKind::BigInt, TypeKind::BigInt},
        {BinopKind::ge, TypeKind::Boolean, TypeKind::String, TypeKind::String},
        {BinopKind::ge, TypeKind::Boolean, TypeKind::Any, llvh::None},
        {BinopKind::ge, TypeKind::Boolean, llvh::None, TypeKind::Any},

        {BinopKind::shl, TypeKind::Number, TypeKind::Number, TypeKind::Number},
        {BinopKind::shl, TypeKind::BigInt, TypeKind::BigInt, TypeKind::BigInt},
        {BinopKind::shl, TypeKind::Any, TypeKind::Any, llvh::None},
        {BinopKind::shl, TypeKind::Any, llvh::None, TypeKind::Any},
        {BinopKind::sshr, TypeKind::Number, TypeKind::Number, TypeKind::Number},
        {BinopKind::sshr, TypeKind::BigInt, TypeKind::BigInt, TypeKind::BigInt},
        {BinopKind::sshr, TypeKind::Any, TypeKind::Any, llvh::None},
        {BinopKind::sshr, TypeKind::Any, llvh::None, TypeKind::Any},
        {BinopKind::ushr, TypeKind::Number, TypeKind::Number, TypeKind::Number},
        {BinopKind::ushr, TypeKind::BigInt, TypeKind::BigInt, TypeKind::BigInt},
        {BinopKind::ushr, TypeKind::Any, TypeKind::Any, llvh::None},
        {BinopKind::ushr, TypeKind::Any, llvh::None, TypeKind::Any},

        {BinopKind::plus, TypeKind::String, TypeKind::String, TypeKind::String},
        {BinopKind::plus, TypeKind::Number, TypeKind::Number, TypeKind::Number},
        {BinopKind::plus, TypeKind::BigInt, TypeKind::BigInt, TypeKind::BigInt},
        {BinopKind::plus, TypeKind::String, TypeKind::Any, TypeKind::String},
        {BinopKind::plus, TypeKind::String, TypeKind::String, TypeKind::Any},
        {BinopKind::plus, TypeKind::Any, TypeKind::Any, llvh::None},
        {BinopKind::plus, TypeKind::Any, llvh::None, TypeKind::Any},

        {BinopKind::minus, TypeKind::Number, TypeKind::Number, TypeKind::Number},
        {BinopKind::minus, TypeKind::BigInt, TypeKind::BigInt, TypeKind::BigInt},
        {BinopKind::minus, TypeKind::Any, llvh::None, TypeKind::Any},
        {BinopKind::minus, TypeKind::Any, TypeKind::Any, llvh::None},
        {BinopKind::mul, TypeKind::Number, TypeKind::Number, TypeKind::Number},
        {BinopKind::mul, TypeKind::BigInt, TypeKind::BigInt, TypeKind::BigInt},
        {BinopKind::mul, TypeKind::Any, llvh::None, TypeKind::Any},
        {BinopKind::mul, TypeKind::Any, TypeKind::Any, llvh::None},
        {BinopKind::div, TypeKind::Number, TypeKind::Number, TypeKind::Number},
        {BinopKind::div, TypeKind::BigInt, TypeKind::BigInt, TypeKind::BigInt},
        {BinopKind::div, TypeKind::Any, llvh::None, TypeKind::Any},
        {BinopKind::div, TypeKind::Any, TypeKind::Any, llvh::None},
        {BinopKind::rem, TypeKind::Number, TypeKind::Number, TypeKind::Number},
        {BinopKind::rem, TypeKind::BigInt, TypeKind::BigInt, TypeKind::BigInt},
        {BinopKind::rem, TypeKind::Any, llvh::None, TypeKind::Any},
        {BinopKind::rem, TypeKind::Any, TypeKind::Any, llvh::None},
        {BinopKind::binOr, TypeKind::Number, TypeKind::Number, TypeKind::Number},
        {BinopKind::binOr, TypeKind::Number, TypeKind::Any, TypeKind::Number},
        {BinopKind::binOr, TypeKind::Number, TypeKind::Number, TypeKind::Any},
        {BinopKind::binOr, TypeKind::BigInt, TypeKind::BigInt, TypeKind::BigInt},
        {BinopKind::binOr, TypeKind::BigInt, TypeKind::Any, TypeKind::BigInt},
        {BinopKind::binOr, TypeKind::BigInt, TypeKind::BigInt, TypeKind::Any},
        {BinopKind::binOr, TypeKind::Any, llvh::None, TypeKind::Any},
        {BinopKind::binOr, TypeKind::Any, TypeKind::Any, llvh::None},
        {BinopKind::binXor, TypeKind::Number, TypeKind::Number, TypeKind::Number},
        {BinopKind::binXor, TypeKind::Number, TypeKind::Any, TypeKind::Number},
        {BinopKind::binXor, TypeKind::Number, TypeKind::Number, TypeKind::Any},
        {BinopKind::binXor, TypeKind::BigInt, TypeKind::BigInt, TypeKind::BigInt},
        {BinopKind::binXor, TypeKind::BigInt, TypeKind::Any, TypeKind::BigInt},
        {BinopKind::binXor, TypeKind::BigInt, TypeKind::BigInt, TypeKind::Any},
        {BinopKind::binXor, TypeKind::Any, llvh::None, TypeKind::Any},
        {BinopKind::binXor, TypeKind::Any, TypeKind::Any, llvh::None},
        {BinopKind::binAnd, TypeKind::Number, TypeKind::Number, TypeKind::Number},
        {BinopKind::binAnd, TypeKind::Number, TypeKind::Any, TypeKind::Number},
        {BinopKind::binAnd, TypeKind::Number, TypeKind::Number, TypeKind::Any},
        {BinopKind::binAnd, TypeKind::BigInt, TypeKind::BigInt, TypeKind::BigInt},
        {BinopKind::binAnd, TypeKind::BigInt, TypeKind::Any, TypeKind::BigInt},
        {BinopKind::binAnd, TypeKind::BigInt, TypeKind::BigInt, TypeKind::Any},
        {BinopKind::binAnd, TypeKind::Any, llvh::None, TypeKind::Any},
        {BinopKind::binAnd, TypeKind::Any, TypeKind::Any, llvh::None},
        {BinopKind::exp, TypeKind::Number, TypeKind::Number, TypeKind::Number},
        {BinopKind::exp, TypeKind::BigInt, TypeKind::BigInt, TypeKind::BigInt},
        {BinopKind::exp, TypeKind::Any, llvh::None, TypeKind::Any},
        {BinopKind::exp, TypeKind::Any, TypeKind::Any, llvh::None},

        {BinopKind::in, TypeKind::Boolean, llvh::None, TypeKind::Any},
        {BinopKind::instanceOf, TypeKind::Boolean, llvh::None, TypeKind::ClassConstructor},
        {BinopKind::instanceOf, TypeKind::Boolean, llvh::None, TypeKind::UntypedFunction},
        {BinopKind::instanceOf, TypeKind::Boolean, llvh::None, TypeKind::Any},
        // clang-format on
    };
    static const BinTypes *const s_types_end =
        s_types + sizeof(s_types) / sizeof(s_types[0]);

    // Find the start of the section for this operator.
    auto it = std::lower_bound(
        s_types, s_types_end, op, [](const BinTypes &bt, BinopKind op) {
          return bt.op < op;
        });

    // Search for a match.
    for (; it != s_types_end && it->op == op; ++it) {
      if ((!it->left || *it->left == lk) && (!it->right || *it->right == rk)) {
        return outer_.flowContext_.getSingletonType(it->res);
      }
    }

    return nullptr;
  }

  void visit(ESTree::BinaryExpressionNode *node) {
    visitESTreeNode(*this, node->_left, node);
    visitESTreeNode(*this, node->_right, node);
    Type *lt = outer_.getNodeTypeOrAny(node->_left);
    Type *rt = outer_.getNodeTypeOrAny(node->_right);

    Type *res;
    if (Type *t = determineBinopType(
            binopKind(node->_operator->str()),
            lt->info->getKind(),
            rt->info->getKind())) {
      res = t;
    } else {
      outer_.sm_.error(
          node->getSourceRange(),
          llvh::Twine("ft: incompatible binary operation: ") +
              node->_operator->str() + " cannot be applied to " +
              lt->info->getKindName() + " and " + rt->info->getKindName());
      res = outer_.flowContext_.getAny();
    }

    outer_.setNodeType(node, res);
  }

  Type *determineUnopType(
      ESTree::UnaryExpressionNode *node,
      UnopKind op,
      TypeKind argKind) {
    struct UnTypes {
      UnopKind op;
      TypeKind res;
      // None indicates a wildcard, Any indicates the actual 'any' type.
      OptValue<TypeKind> arg;
    };

    static const UnTypes s_types[] = {
        // clang-format off
        {UnopKind::del, TypeKind::Boolean, llvh::None},
        {UnopKind::voidOp, TypeKind::Void, llvh::None},
        {UnopKind::typeof, TypeKind::String, llvh::None},
        {UnopKind::plus, TypeKind::Number, TypeKind::Number},
        {UnopKind::plus, TypeKind::Number, TypeKind::Any},
        {UnopKind::minus, TypeKind::BigInt, TypeKind::BigInt},
        {UnopKind::minus, TypeKind::Number, TypeKind::Number},
        {UnopKind::minus, TypeKind::Any, TypeKind::Any},
        {UnopKind::tilde, TypeKind::Number, TypeKind::Number},
        {UnopKind::tilde, TypeKind::BigInt, TypeKind::BigInt},
        {UnopKind::tilde, TypeKind::Any, TypeKind::Any},
        {UnopKind::bang, TypeKind::Boolean, llvh::None},
        {UnopKind::inc, TypeKind::Number, TypeKind::Number},
        {UnopKind::inc, TypeKind::BigInt, TypeKind::BigInt},
        {UnopKind::inc, TypeKind::Any, TypeKind::Any},
        {UnopKind::dec, TypeKind::Number, TypeKind::Number},
        {UnopKind::dec, TypeKind::BigInt, TypeKind::BigInt},
        {UnopKind::dec, TypeKind::Any, TypeKind::Any},
        // clang-format on
    };
    static const UnTypes *const s_types_end =
        s_types + sizeof(s_types) / sizeof(s_types[0]);

    // Find the start of the section for this operator.
    auto it = std::lower_bound(
        s_types, s_types_end, op, [](const UnTypes &bt, UnopKind op) {
          return bt.op < op;
        });

    // Search for a match.
    for (; it != s_types_end && it->op == op; ++it) {
      if (!it->arg || *it->arg == argKind) {
        return outer_.flowContext_.getSingletonType(it->res);
      }
    }

    return nullptr;
  }

  void visit(ESTree::UnaryExpressionNode *node) {
    visitESTreeNode(*this, node->_argument, node);
    Type *argType = outer_.getNodeTypeOrAny(node->_argument);

    Type *res;
    if (Type *t = determineUnopType(
            node, unopKind(node->_operator->str()), argType->info->getKind())) {
      res = t;
    } else {
      outer_.sm_.error(
          node->getSourceRange(),
          llvh::Twine("ft: incompatible unary operation: ") +
              node->_operator->str() + " cannot be applied to " +
              argType->info->getKindName());
      res = outer_.flowContext_.getAny();
    }

    outer_.setNodeType(node, res);
  }

  void visit(ESTree::LogicalExpressionNode *node) {
    visitESTreeNode(*this, node->_left, node);
    visitESTreeNode(*this, node->_right, node);
    Type *left = outer_.flowContext_.getNodeTypeOrAny(node->_left);
    Type *right = outer_.flowContext_.getNodeTypeOrAny(node->_right);

    auto hasNull = [](TypeInfo *t) -> bool {
      if (llvh::isa<NullType>(t))
        return true;
      if (auto *u = llvh::dyn_cast<UnionType>(t))
        return u->hasNull();
      return false;
    };

    auto hasVoid = [](TypeInfo *t) -> bool {
      if (llvh::isa<VoidType>(t))
        return true;
      if (auto *u = llvh::dyn_cast<UnionType>(t))
        return u->hasVoid();
      return false;
    };

    // The result of a logical expression is the union of both sides,
    // however if the operator is ?? or ||, then we can discard any null/void
    // that only appears on the left, because those can never be returned via
    // the left.
    FlowContext::UnionExcludes excludes{};
    if (node->_operator == outer_.kw_.identLogicalOr ||
        node->_operator == outer_.kw_.identNullishCoalesce) {
      if (hasVoid(left->info) && !hasVoid(right->info))
        excludes.excludeVoid = true;
      if (hasNull(left->info) && !hasNull(right->info))
        excludes.excludeNull = true;
    }

    Type *types[2]{
        outer_.flowContext_.getNodeTypeOrAny(node->_left),
        outer_.flowContext_.getNodeTypeOrAny(node->_right)};

    Type *unionType = outer_.flowContext_.createType(
        outer_.flowContext_.maybeCreateUnion(types, excludes));

    outer_.setNodeType(node, unionType);
  }

  enum class LogicalAssignmentOp : uint8_t {
    ShortCircuitOrKind, // ||=
    ShortCircuitAndKind, // &&=
    NullishCoalesceKind, // ??=
  };

  void visit(ESTree::AssignmentExpressionNode *node) {
    visitESTreeNode(*this, node->_left, node);
    visitESTreeNode(*this, node->_right, node);

    auto logicalAssign =
        llvh::StringSwitch<OptValue<LogicalAssignmentOp>>(
            node->_operator->str())
            .Case("||=", LogicalAssignmentOp::ShortCircuitOrKind)
            .Case("&&=", LogicalAssignmentOp::ShortCircuitAndKind)
            .Case("\?\?=", LogicalAssignmentOp::NullishCoalesceKind)
            .Default(llvh::None);
    if (logicalAssign) {
      outer_.sm_.error(node->getSourceRange(), "ft: unsupported");
      return;
    }

    Type *lt = outer_.getNodeTypeOrAny(node->_left);
    Type *rt = outer_.getNodeTypeOrAny(node->_right);
    Type *res;

    if (node->_operator->str() == "=") {
      auto [rtNarrow, cf] =
          tryNarrowType(outer_.getNodeTypeOrAny(node->_right), lt);
      if (!cf.canFlow) {
        outer_.sm_.error(
            node->getSourceRange(), "ft: incompatible assignment types");
        res = lt;
      } else {
        node->_right = outer_.implicitCheckedCast(node->_right, rtNarrow, cf);

        // If we don't need a checked cast, rt is possibly narrower than lt, but
        // never wider, so we want to use it as result.
        // This helps with cases like:
        //  let a: number|string, n: number; n = a = 5;
        res = cf.needCheckedCast ? lt : rt;
      }
    } else {
      Type *opResType = determineBinopType(
          assignKind(node->_operator->str()),
          lt->info->getKind(),
          rt->info->getKind());

      if (!opResType) {
        outer_.sm_.error(
            node->getSourceRange(),
            llvh::Twine("ft: incompatible binary operation: ") +
                node->_operator->str() + " cannot be applied to " +
                lt->info->getKindName() + " and " + rt->info->getKindName());
        opResType = outer_.flowContext_.getAny();
      }

      if (llvh::isa<AnyType>(lt->info)) {
        // If the target we are assigning to is untyped, there are no checks
        // needed.
        res = opResType;
      } else {
        // We are modifying a typed target. The type has to be compatible.
        CanFlowResult cf = canAFlowIntoB(opResType, lt);
        if (!cf.canFlow) {
          outer_.sm_.error(
              node->getSourceRange(), "ft: incompatible assignment types");
          res = lt;
        } else if (cf.needCheckedCast) {
          // Insert an ImplicitCheckedCast around the LHS in the
          // AssignmentExpressionNode, because there's no other place in the AST
          // that indicates that we want to cast the result of the binary
          // expression.
          // IRGen is aware of this and handles it specially.
          node->_left = outer_.implicitCheckedCast(node->_left, lt, cf);
          res = lt;
        } else {
          // If we don't need a checked cast, opResType is possibly narrower
          // than lt, but never wider, so we want to use it as result.
          res = opResType;
        }
      }
    }
    outer_.setNodeType(node, res);
  }

  void visit(ESTree::ArrayPatternNode *node) {
    // For now, this just marks the array pattern (used on the LHS of assignment
    // expressions) as a tuple, so that it can be used with destructuring.
    // This isn't called from variable declaration nodes here, because
    // AnnotateScopeDecls handles variable declarations directly.
    // The tuple type is then read by, e.g., visit(AssignmentExpressionNode *),
    // which will use the tuple type to typecheck the assignment itself.
    // TODO: Determine how to destructure from arrays.

    // Annotate the children of the array pattern.
    visitESTreeChildren(*this, node);

    llvh::SmallVector<Type *, 4> types;
    for (ESTree::Node &elem : node->_elements) {
      types.push_back(outer_.getNodeTypeOrAny(&elem));
    }
    outer_.setNodeType(
        node,
        outer_.flowContext_.createType(
            outer_.flowContext_.createTuple(types), node));
  }

  void visit(ESTree::ConditionalExpressionNode *node) {
    visitESTreeChildren(*this, node);

    Type *types[2]{
        outer_.getNodeTypeOrAny(node->_consequent),
        outer_.getNodeTypeOrAny(node->_alternate)};

    // The result of a conditional is the union of the two types.
    outer_.setNodeType(
        node,
        outer_.flowContext_.createType(
            outer_.flowContext_.maybeCreateUnion(types)));
  }

  void visit(ESTree::TypeParameterInstantiationNode *node) {
    // Do nothing.
    // These are handled in the parent node.
  }

  void visit(ESTree::CallExpressionNode *node) {
    visitESTreeChildren(*this, node);

    // Check for $SHBuiltin.
    if (auto *methodCallee =
            llvh::dyn_cast<ESTree::MemberExpressionNode>(node->_callee)) {
      if (llvh::isa<ESTree::SHBuiltinNode>(methodCallee->_object)) {
        checkSHBuiltin(
            node, llvh::cast<ESTree::IdentifierNode>(methodCallee->_property));
        return;
      }
    }

    if (auto *identCallee =
            llvh::dyn_cast<ESTree::IdentifierNode>(node->_callee)) {
      sema::Decl *decl = outer_.getDecl(identCallee);
      if (decl->generic) {
        if (!node->_typeArguments) {
          outer_.sm_.error(
              node->_callee->getSourceRange(), "ft: type arguments required");
          return;
        }

        outer_.resolveCallToGenericFunctionSpecialization(
            node, identCallee, decl);
      }
    } else if (node->_typeArguments) {
      // Generics handled above.
      outer_.sm_.error(
          node->_callee->getSourceRange(),
          "ft: generic call only works on identifiers");
      return;
    }

    Type *calleeType = outer_.getNodeTypeOrAny(node->_callee);
    // If the callee has no type, we have nothing to do/check.
    if (llvh::isa<AnyType>(calleeType->info))
      return;

    if (!llvh::isa<BaseFunctionType>(calleeType->info)) {
      outer_.sm_.error(
          node->_callee->getSourceRange(), "ft: callee is not a function");
      return;
    }

    // If the callee is an untyped function, we have nothing to check.
    if (llvh::isa<UntypedFunctionType>(calleeType->info)) {
      outer_.setNodeType(node, outer_.flowContext_.getAny());
      return;
    }

    Type *returnType;
    llvh::ArrayRef<TypedFunctionType::Param> params{};

    if (auto *ftype = llvh::dyn_cast<TypedFunctionType>(calleeType->info)) {
      returnType = ftype->getReturnType();
      params = ftype->getParams();

      Type *expectedThisType = ftype->getThisParam()
          ? ftype->getThisParam()
          : outer_.flowContext_.getAny();

      // Check the type of "this".
      if (auto *methodCallee =
              llvh::dyn_cast<ESTree::MemberExpressionNode>(node->_callee)) {
        Type *thisArgType = nullptr;
        if (auto *superNode =
                llvh::dyn_cast<ESTree::SuperNode>(methodCallee->_object)) {
          // 'super' calls implicitly pass the current class as 'this'.
          if (!outer_.curClassContext_->classType) {
            outer_.sm_.error(
                node->_callee->getSourceRange(),
                "ft: 'super' call outside class");
            return;
          }
          thisArgType = outer_.curClassContext_->classType;
        } else {
          thisArgType = outer_.getNodeTypeOrAny(methodCallee->_object);
        }

        if (!canAFlowIntoB(thisArgType->info, expectedThisType->info).canFlow) {
          outer_.sm_.error(
              methodCallee->getSourceRange(), "ft: 'this' type mismatch");
          return;
        }
      } else if (
          auto *superNode = llvh::dyn_cast<ESTree::SuperNode>(node->_callee)) {
        // 'super' calls implicitly pass the current class as 'this'.
        if (!outer_.curClassContext_->classType) {
          outer_.sm_.error(
              node->_callee->getSourceRange(),
              "ft: 'super' call outside class");
          return;
        }
        if (!canAFlowIntoB(
                 outer_.curClassContext_->classType->info,
                 expectedThisType->info)
                 .canFlow) {
          outer_.sm_.error(
              node->_callee->getSourceRange(), "ft: 'this' type mismatch");
          return;
        }
      } else {
        if (!canAFlowIntoB(
                 outer_.flowContext_.getVoid()->info, expectedThisType->info)
                 .canFlow) {
          outer_.sm_.error(
              node->_callee->getSourceRange(), "ft: 'this' type mismatch");
          return;
        }
      }
    } else {
      auto *nftype = llvh::cast<NativeFunctionType>(calleeType->info);
      returnType = nftype->getReturnType();
      params = nftype->getParams();
    }

    outer_.setNodeType(node, returnType);
    checkArgumentTypes(params, node, node->_arguments, "function");
  }

  void checkSHBuiltin(
      ESTree::CallExpressionNode *call,
      ESTree::IdentifierNode *builtin) {
    if (builtin->_name == outer_.kw_.identCall) {
      checkSHBuiltinCall(call);
      return;
    }
    if (builtin->_name == outer_.kw_.identCNull) {
      checkSHBuiltinCNull(call);
      return;
    }
    if (builtin->_name == outer_.kw_.identCNativeRuntime) {
      checkSHBuiltinCNativeRuntime(call);
      return;
    }
    if (builtin->_name == outer_.kw_.identExternC) {
      checkSHBuiltinExternC(call);
      return;
    }

    outer_.sm_.error(call->getSourceRange(), "unknown SH builtin call");
  }

  /// $SHBuiltin.call(fn, this, arg1, ...)
  /// must typecheck as an actual function call.
  void checkSHBuiltinCall(ESTree::CallExpressionNode *call) {
    auto it = call->_arguments.begin();
    if (it == call->_arguments.end()) {
      outer_.sm_.error(
          call->getSourceRange(), "ft: call requires at least two arguments");
      return;
    }
    ESTree::Node *callee = &*it;
    Type *calleeType = outer_.getNodeTypeOrAny(callee);
    // If the callee has no type, we have nothing to do/check.
    if (llvh::isa<AnyType>(calleeType->info))
      return;
    if (!llvh::isa<BaseFunctionType>(calleeType->info)) {
      outer_.sm_.error(
          callee->getSourceRange(), "ft: callee is not a function");
      return;
    }
    if (llvh::isa<NativeFunctionType>(calleeType->info)) {
      outer_.sm_.error(
          callee->getSourceRange(),
          "ft: callee is a native function, cannot use $SHBuiltin.call");
      return;
    }
    auto *ftype = llvh::dyn_cast<TypedFunctionType>(calleeType->info);

    // If the callee is an untyped function, we have nothing to check.
    if (!ftype) {
      outer_.setNodeType(call, outer_.flowContext_.getAny());
      return;
    }

    outer_.setNodeType(call, ftype->getReturnType());

    ++it;
    if (it == call->_arguments.end()) {
      outer_.sm_.error(
          call->getSourceRange(), "ft: call requires at least two arguments");
      return;
    }
    Type *expectedThisType = ftype->getThisParam()
        ? ftype->getThisParam()
        : outer_.flowContext_.getAny();
    ESTree::Node *thisArg = &*it;
    Type *thisArgType = outer_.getNodeTypeOrAny(thisArg);
    if (!canAFlowIntoB(thisArgType->info, expectedThisType->info).canFlow) {
      outer_.sm_.error(thisArg->getSourceRange(), "ft: 'this' type mismatch");
      return;
    }

    checkArgumentTypes(
        ftype->getParams(), call, call->_arguments, "function", 2);
    return;
  }

  /// SHBuiltin.c_null().
  void checkSHBuiltinCNull(ESTree::CallExpressionNode *call) {
    // Check the number and types of arguments.
    if (call->_arguments.size() != 0) {
      outer_.sm_.error(call->getSourceRange(), "ft: c_null takes no arguments");
      return;
    }
    outer_.setNodeType(call, outer_.flowContext_.getCPtr());
  }

  /// SHBuiltin.c_native_runtime().
  void checkSHBuiltinCNativeRuntime(ESTree::CallExpressionNode *call) {
    // Check the number and types of arguments.
    if (call->_arguments.size() != 0) {
      outer_.sm_.error(
          call->getSourceRange(), "ft: c_native_runtime takes no arguments");
      return;
    }
    outer_.setNodeType(call, outer_.flowContext_.getCPtr());
  }

  /// $SHBuiltin.extern_c({options}, function name():result {...})
  void checkSHBuiltinExternC(ESTree::CallExpressionNode *call) {
    // Check the number and types of arguments.
    if (call->_arguments.size() != 2) {
      outer_.sm_.error(
          call->getSourceRange(),
          "ft: extern_c requires exactly two arguments");
      return;
    }

    // Check arg 1.
    auto arg = call->_arguments.begin();
    auto *options = llvh::dyn_cast<ESTree::ObjectExpressionNode>(&*arg);
    if (!options) {
      outer_.sm_.error(
          arg->getSourceRange(),
          "ft: extern_c requires an object literal as the first argument");
      return;
    }
    // Parse the options.
    bool declaredOption = false;
    UniqueString *includeOption = nullptr;
    bool allowHVOption = false;
    if (!parseExternCOptions(
            options, &declaredOption, &includeOption, &allowHVOption))
      return;

    // Check arg 2.
    UniqueString *name;
    Type *funcType;
    TypedFunctionType *funcInfo;

    ++arg;
    auto *func = llvh::dyn_cast<ESTree::FunctionExpressionNode>(&*arg);
    if (!func) {
      outer_.sm_.error(
          arg->getSourceRange(),
          "ft: extern_c requires a function as the second argument");
      return;
    }
    if (!func->_id) {
      outer_.sm_.error(
          arg->getSourceRange(),
          "ft: extern_c requires a named function as the second argument");
      return;
    }
    name = llvh::cast<ESTree::IdentifierNode>(func->_id)->_name;
    funcType = outer_.flowContext_.findNodeType(func);
    assert(funcType && "function expression type must be set");
    if (!llvh::isa<TypedFunctionType>(funcType->info)) {
      outer_.sm_.error(
          arg->getSourceRange(),
          "ft: extern_c requires a typed function as the second argument");
      return;
    }
    funcInfo = llvh::cast<TypedFunctionType>(funcType->info);
    if (funcInfo->isAsync() || funcInfo->isGenerator()) {
      outer_.sm_.error(
          arg->getSourceRange(),
          "ft: extern_c does not support async or generator functions");
      return;
    }
    if (funcInfo->getThisParam()) {
      outer_.sm_.error(
          arg->getSourceRange(),
          "ft: extern_c does not support 'this' parameters");
      return;
    }

    // Extract the function signature.
    NativeSignature *signature;

    if (!func->_returnType) {
      outer_.sm_.error(
          func->getSourceRange(),
          "ft: extern_c requires a return type annotation");
      return;
    }
    llvh::SmallVector<NativeCType, 4> natParamTypes{};
    NativeCType natReturnType;

    // The return type, where we allow void.
    if (llvh::isa<VoidType>(funcInfo->getReturnType()->info))
      natReturnType = NativeCType::c_void;
    else if (!parseNativeAnnotation(
                 llvh::cast<ESTree::TypeAnnotationNode>(func->_returnType),
                 allowHVOption,
                 &natReturnType))
      return;

    for (auto &node : func->_params) {
      auto *param = llvh::cast<ESTree::IdentifierNode>(&node);
      if (!param->_typeAnnotation) {
        outer_.sm_.error(
            param->getSourceRange(),
            "ft: extern_c requires type annotations for all parameters");
        return;
      }
      NativeCType natParamType;
      if (!parseNativeAnnotation(
              llvh::cast<ESTree::TypeAnnotationNode>(param->_typeAnnotation),
              false,
              &natParamType))
        return;
      natParamTypes.push_back(natParamType);
    }

    signature = outer_.astContext_.getNativeContext().getSignature(
        natReturnType, natParamTypes);

    // Now that we have the signature, declare the extern and check for invalid
    // redeclaration.
    NativeExtern *ne = outer_.astContext_.getNativeContext().getExtern(
        name, signature, call->getStartLoc(), declaredOption, includeOption);
    if (ne->signature() != signature) {
      outer_.sm_.error(
          call->getSourceRange(),
          "ft: invalid redeclaration of native extern '" + name->str() + "'");
      if (ne->loc().isValid()) {
        outer_.sm_.note(ne->loc(), "ft: original declaration here");
      }
      return;
    }

    TypeInfo *nativeFuncInfo = outer_.flowContext_.createNativeFunction(
        funcInfo->getReturnType(), funcInfo->getParams(), signature);

    outer_.setNodeType(call, outer_.flowContext_.createType(nativeFuncInfo));
  }

  /// Extract the options from the options object literal. On error print an
  /// an error message and return false.
  bool parseExternCOptions(
      ESTree::ObjectExpressionNode *options,
      bool *declaredOption,
      UniqueString **includeOption,
      bool *allowHVOption) {
    *declaredOption = false;
    *includeOption = nullptr;

    auto parseObjRes = parseExternCObjectLiteral(options);
    if (!parseObjRes)
      return false;
    auto &map = *parseObjRes;
    bool success = true;

    // NOTE: Whenever we find a supported option, we erase it.

    // Parse an option of a specified literal type. On error print an error
    // message and clear the success flag.
    // \param lit The literal type to parse. The passed value is ignored, only
    //    the type matters.
    // \param typeName The name of the type to print in the error message.
    // \param optionName The name of the option to parse.
    // \param res Output parameter for the parsed value.
    auto parseOption = [&map, &success, this](
                           auto *lit,
                           llvh::StringLiteral typeName,
                           llvh::StringLiteral optionName,
                           auto *res) {
      using LitType = std::remove_pointer_t<decltype(lit)>;
      auto it = map.find(
          outer_.astContext_.getIdentifier(optionName).getUnderlyingPointer());
      if (it != map.end()) {
        lit = llvh::dyn_cast<LitType>(it->second->_value);
        if (lit) {
          *res = lit->_value;
        } else {
          outer_.sm_.error(
              it->second->getSourceRange(),
              "ft: extern_c option '" + optionName + "' must be a " + typeName +
                  " literal");
          success = false;
        }
        map.erase(it);
      }
    };

    auto parseString = [&parseOption](
                           llvh::StringLiteral optionName, UniqueString **res) {
      parseOption(
          (ESTree::StringLiteralNode *)nullptr, "string", optionName, res);
    };
    auto parseBool = [&parseOption](llvh::StringLiteral optionName, bool *res) {
      parseOption(
          (ESTree::BooleanLiteralNode *)nullptr, "boolean", optionName, res);
    };

    parseBool("declared", declaredOption);
    parseBool("hv", allowHVOption);
    parseString("include", includeOption);

    // Check for unsupported properties.
    for (auto &prop : map) {
      outer_.sm_.error(
          prop.second->getSourceRange(),
          "ft: extern_c does not support option '" + prop.first->str() + "'");
      success = false;
    }

    return success;
  }

  /// Helper to parse the options object literal used in extern_c. Ensures that
  /// only "normal" properties are present, and that the values are literals or
  /// object literals. On error prints an error message and returns None.
  /// On success returns a map of property names to PropertyNodes. The caller
  /// can quickly scan the names. It can also use the same function to scan
  /// nested object literals.
  ///
  /// \return None on error, otherwise a map of property names to
  ///     EStree::PropertyNode.
  llvh::Optional<
      llvh::SmallMapVector<UniqueString *, ESTree::PropertyNode *, 4>>
  parseExternCObjectLiteral(ESTree::ObjectExpressionNode *objLitNode) {
    llvh::SmallMapVector<UniqueString *, ESTree::PropertyNode *, 4> res{};

    for (ESTree::Node &n : objLitNode->_properties) {
      // The dyn_cast could perhaps be a cast, but just to be safe.
      auto *prop = llvh::dyn_cast<ESTree::PropertyNode>(&n);
      if (!prop || prop->_kind != outer_.kw_.identInit || prop->_computed ||
          prop->_method || prop->_shorthand) {
        outer_.sm_.error(
            n.getSourceRange(), "ft: extern_c: unsupported property format");
        return llvh::None;
      }

      // Check that the value is a literal, another object, or an array.
      auto *value = prop->_value;
      if (!(llvh::isa<ESTree::NullLiteralNode>(value) ||
            llvh::isa<ESTree::BooleanLiteralNode>(value) ||
            llvh::isa<ESTree::StringLiteralNode>(value) ||
            llvh::isa<ESTree::NumericLiteralNode>(value) ||
            llvh::isa<ESTree::BigIntLiteralNode>(value) ||
            llvh::isa<ESTree::ObjectExpressionNode>(value))) {
        outer_.sm_.error(
            value->getSourceRange(), "ft: extern_c: unsupported property type");
        return llvh::None;
      }

      // Note that we don't care about duplicates, we just want to use the last
      // one.
      res[llvh::cast<ESTree::IdentifierNode>(prop->_key)->_name] = prop;
    }

    // Note that we have to use std::move() since we are returning an optional.
    return std::move(res);
  }

  /// Parse a native type annotation. Return true on success and store the
  /// value in \p res. On error print an error message and return false.
  ///
  /// \param node The node to parse.
  /// \param allowRegular If true, allow type annotations that are not native
  ///     types.
  /// \param res Output parameter for the parsed type.
  bool parseNativeAnnotation(
      ESTree::TypeAnnotationNode *node,
      bool allowRegular,
      NativeCType *res) {
    *res = NativeCType::c_hermes_value;
    auto *ann = llvh::dyn_cast<ESTree::GenericTypeAnnotationNode>(
        node->_typeAnnotation);
    if (!ann || ann->_typeParameters) {
      if (allowRegular)
        return true;
      outer_.sm_.error(
          node->getSourceRange(), "ft: unsupported native type annotation");
      return false;
    }
    auto *id = llvh::dyn_cast<ESTree::IdentifierNode>(ann->_id);
    if (!id) {
      if (allowRegular)
        return true;
      outer_.sm_.error(
          node->getSourceRange(), "ft: unsupported native type annotation");
      return false;
    }
    UniqueString *name = id->_name;
    auto it = outer_.nativeTypes_.find(name);
    if (it == outer_.nativeTypes_.end()) {
      if (allowRegular)
        return true;
      outer_.sm_.error(
          ann->_id->getSourceRange(),
          "ft: '" + name->str() + "' is not a native type");
      return false;
    }
    *res = it->second;
    return true;
  };

  void visit(ESTree::OptionalCallExpressionNode *node) {
    outer_.sm_.error(
        node->getSourceRange(), "ft: optional call expression not supported");
  }

  void visit(ESTree::NewExpressionNode *node) {
    visitESTreeChildren(*this, node);

    // Resolve generics using type arguments if necessary.
    if (auto *identCallee =
            llvh::dyn_cast<ESTree::IdentifierNode>(node->_callee)) {
      sema::Decl *decl = outer_.getDecl(identCallee);
      if (decl->generic) {
        if (!node->_typeArguments) {
          outer_.sm_.error(
              node->_callee->getSourceRange(), "ft: type arguments required");
          return;
        }

        outer_.resolveGenericClassSpecialization(
            identCallee,
            llvh::cast<ESTree::TypeParameterInstantiationNode>(
                node->_typeArguments),
            decl);
      }
    } else if (node->_typeArguments) {
      // Generics handled above.
      outer_.sm_.error(
          node->_callee->getSourceRange(),
          "ft: generic call only works on identifiers");
      return;
    }

    Type *calleeType = outer_.getNodeTypeOrAny(node->_callee);
    // If the callee has no type, we have nothing to do/check.
    if (llvh::isa<AnyType>(calleeType->info))
      return;

    if (!llvh::isa<ClassConstructorType>(calleeType->info)) {
      outer_.sm_.error(
          node->_callee->getSourceRange(),
          "ft: callee is not a class constructor");
      return;
    }
    auto *classConsType = llvh::cast<ClassConstructorType>(calleeType->info);
    Type *classType = classConsType->getClassType();
    ClassType *classTypeInfo = classConsType->getClassTypeInfo();

    outer_.setNodeType(node, classType);

    // Does the class have an explicit constructor?
    if (Type *consFType = classTypeInfo->getConstructorType()) {
      checkArgumentTypes(
          llvh::cast<TypedFunctionType>(consFType->info)->getParams(),
          node,
          node->_arguments,
          "class " + classTypeInfo->getClassNameOrDefault() + " constructor");
    } else {
      if (!node->_arguments.empty()) {
        outer_.sm_.error(
            node->getSourceRange(),
            "ft: class " + classTypeInfo->getClassNameOrDefault() +
                " does not have an explicit constructor");
        return;
      }
    }
  }

  void visit(ESTree::SuperNode *node, ESTree::Node *parent) {
    if (!outer_.curClassContext_) {
      outer_.sm_.error(
          node->getSourceRange(), "ft: super only supported in class");
      return;
    }

    // Check that the super call is valid.
    ClassType *curClassType = outer_.curClassContext_->getClassTypeInfo();
    ClassType *superClassType = curClassType->getSuperClassInfo();
    if (!superClassType) {
      outer_.sm_.error(
          node->getSourceRange(), "ft: super requires a base class");
      return;
    }

    if (llvh::isa<ESTree::CallExpressionNode>(parent)) {
      // super() call calls the constructor of the super class.
      outer_.setNodeType(node, superClassType->getConstructorType());
    } else if (llvh::isa<ESTree::MemberExpressionNode>(parent)) {
      // super.property lookup is on the super class.
      outer_.setNodeType(node, curClassType->getSuperClass());
    } else {
      outer_.sm_.error(node->getSourceRange(), "ft: invalid usage of super");
      outer_.flowContext_.setNodeType(node, outer_.flowContext_.getAny());
      return;
    }
  }

  /// Check the types of the supplies arguments, adding checked casts if needed.
  /// \param offset the number of arguments to ignore at the front of \p
  ///   arguments. Used for $SHBuiltin.call, which has extra args at the front.
  bool checkArgumentTypes(
      llvh::ArrayRef<TypedFunctionType::Param> params,
      ESTree::Node *callNode,
      ESTree::NodeList &arguments,
      const llvh::Twine &calleeName,
      uint32_t offset = 0) {
    size_t numArgs = arguments.size() - offset;
    // FIXME: default arguments.
    if (params.size() != numArgs) {
      outer_.sm_.error(
          callNode->getSourceRange(),
          "ft: " + calleeName + " expects " + llvh::Twine(params.size()) +
              " arguments, but " + llvh::Twine(numArgs) + " supplied");
      return false;
    }

    auto begin = arguments.begin();
    std::advance(begin, offset);

    // Check the type of each argument.
    size_t argIndex = 0;
    for (auto it = begin, e = arguments.end(); it != e; ++argIndex, ++it) {
      ESTree::Node *arg = &*it;

      if (llvh::isa<ESTree::SpreadElementNode>(arg)) {
        outer_.sm_.error(
            arg->getSourceRange(), "ft: argument spread is not supported");
        return false;
      }

      const TypedFunctionType::Param &param = params[argIndex];
      Type *expectedType = param.second;
      Type *argType = outer_.getNodeTypeOrAny(arg);
      auto [argTypeNarrow, cf] = tryNarrowType(argType, expectedType);

      if (!cf.canFlow) {
        outer_.sm_.error(
            arg->getSourceRange(),
            "ft: " + calleeName + " parameter '" + param.first.str() +
                "' type mismatch");
        return false;
      }
      // If a cast is needed, replace the argument with the cast.
      if (cf.needCheckedCast && outer_.compile_) {
        // Insert the new node before the current node and erase the current
        // one.
        auto newIt = arguments.insert(
            it, *outer_.implicitCheckedCast(arg, argTypeNarrow, cf));
        arguments.erase(it);
        it = newIt;
      }
    }

    return true;
  }
};

void FlowChecker::visitExpression(ESTree::Node *node, ESTree::Node *parent) {
  ExprVisitor v(*this);
  visitESTreeNode(v, node, parent);
}

} // namespace flow
} // namespace hermes