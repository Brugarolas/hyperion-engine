#include <script/compiler/ast/AstFloat.hpp>
#include <script/compiler/ast/AstInteger.hpp>
#include <script/compiler/ast/AstTrue.hpp>
#include <script/compiler/ast/AstFalse.hpp>
#include <script/compiler/ast/AstNil.hpp>
#include <script/compiler/AstVisitor.hpp>

#include <script/compiler/type-system/BuiltinTypes.hpp>

#include <script/compiler/emit/BytecodeUtil.hpp>

#include <limits>
#include <cmath>

namespace hyperion::compiler {

AstFloat::AstFloat(hyperion::afloat32 value, const SourceLocation &location)
    : AstConstant(location),
      m_value(value)
{
}

std::unique_ptr<Buildable> AstFloat::Build(AstVisitor *visitor, Module *mod)
{
    // get active register
    uint8_t rp = visitor->GetCompilationUnit()->GetInstructionStream().GetCurrentRegister();
    return BytecodeUtil::Make<ConstF32>(rp, m_value);
}

Pointer<AstStatement> AstFloat::Clone() const
{
    return CloneImpl();
}

Tribool AstFloat::IsTrue() const
{
    // any non-zero value is considered true
    return Tribool(m_value != 0.0f);
}

bool AstFloat::IsNumber() const
{
    return true;
}

hyperion::aint32 AstFloat::IntValue() const
{
    return (hyperion::aint32)m_value;
}

hyperion::auint32 AstFloat::UnsignedValue() const
{
    return (hyperion::auint32)m_value;
}

hyperion::afloat32 AstFloat::FloatValue() const
{
    return m_value;
}

SymbolTypePtr_t AstFloat::GetExprType() const
{
    return BuiltinTypes::FLOAT;
}

std::shared_ptr<AstConstant> AstFloat::HandleOperator(Operators op_type, const AstConstant *right) const
{
    switch (op_type) {
        case OP_add:
            if (!right->IsNumber()) {
                return nullptr;
            }
            return std::shared_ptr<AstFloat>(
                new AstFloat(FloatValue() + right->FloatValue(), m_location));

        case OP_subtract:
            if (!right->IsNumber()) {
                return nullptr;
            }
            return std::shared_ptr<AstFloat>(
                new AstFloat(FloatValue() - right->FloatValue(), m_location));

        case OP_multiply:
            if (!right->IsNumber()) {
                return nullptr;
            }
            return std::shared_ptr<AstFloat>(
                new AstFloat(FloatValue() * right->FloatValue(), m_location));

        case OP_divide: {
            if (!right->IsNumber()) {
                return nullptr;
            }

            auto right_float = right->FloatValue();
            if (right_float == 0.0) {
                // division by zero
                return nullptr;
            }
            return std::shared_ptr<AstFloat>(new AstFloat(FloatValue() / right_float, m_location));
        }

        case OP_modulus: {
            if (!right->IsNumber()) {
                return nullptr;
            }

            auto right_float = right->FloatValue();
            if (right_float == 0.0) {
                // division by zero
                return nullptr;
            }

            return std::shared_ptr<AstFloat>(new AstFloat(std::fmod(FloatValue(), right_float), m_location));
        }

        case OP_logical_and: {
            int this_true = IsTrue();
            int right_true = right->IsTrue();

            if (!right->IsNumber()) {
                // this operator is valid to compare against null
                if (dynamic_cast<const AstNil*>(right)) {
                    // rhs is null, return false
                    return std::shared_ptr<AstFalse>(new AstFalse(m_location));
                }
                return nullptr;
            }

            if (this_true == 1 && right_true == 1) {
                return std::shared_ptr<AstTrue>(new AstTrue(m_location));
            } else if (this_true == 0 && right_true == 0) {
                return std::shared_ptr<AstFalse>(new AstFalse(m_location));
            } else {
                // indeterminate
                return nullptr;
            }
        }

        case OP_logical_or: {
            int this_true = IsTrue();
            int right_true = right->IsTrue();

            if (!right->IsNumber()) {
                // this operator is valid to compare against null
                if (dynamic_cast<const AstNil*>(right)) {
                    if (this_true == 1) {
                        return std::shared_ptr<AstTrue>(new AstTrue(m_location));
                    } else if (this_true == 0) {
                        return std::shared_ptr<AstFalse>(new AstFalse(m_location));
                    }
                }
                return nullptr;
            }

            if (this_true == 1 || right_true == 1) {
                return std::shared_ptr<AstTrue>(new AstTrue(m_location));
            } else if (this_true == 0 || right_true == 0) {
                return std::shared_ptr<AstFalse>(new AstFalse(m_location));
            } else {
                // indeterminate
                return nullptr;
            }
        }

        case OP_less:
            if (!right->IsNumber()) {
                return nullptr;
            }
            if (FloatValue() < right->FloatValue()) {
                return std::shared_ptr<AstTrue>(new AstTrue(m_location));
            } else {
                return std::shared_ptr<AstFalse>(new AstFalse(m_location));
            }

        case OP_greater:
            if (!right->IsNumber()) {
                return nullptr;
            }
            if (FloatValue() > right->FloatValue()) {
                return std::shared_ptr<AstTrue>(new AstTrue(m_location));
            } else {
                return std::shared_ptr<AstFalse>(new AstFalse(m_location));
            }

        case OP_less_eql:
            if (!right->IsNumber()) {
                return nullptr;
            }
            if (FloatValue() <= right->FloatValue()) {
                return std::shared_ptr<AstTrue>(new AstTrue(m_location));
            } else {
                return std::shared_ptr<AstFalse>(new AstFalse(m_location));
            }

        case OP_greater_eql:
            if (!right->IsNumber()) {
                return nullptr;
            }
            if (FloatValue() >= right->FloatValue()) {
                return std::shared_ptr<AstTrue>(new AstTrue(m_location));
            } else {
                return std::shared_ptr<AstFalse>(new AstFalse(m_location));
            }

        case OP_equals:
            if (!right->IsNumber()) {
                return nullptr;
            }
            if (FloatValue() == right->FloatValue()) {
                return std::shared_ptr<AstTrue>(new AstTrue(m_location));
            } else {
                return std::shared_ptr<AstFalse>(new AstFalse(m_location));
            }

        case OP_negative:
            return std::shared_ptr<AstFloat>(new AstFloat(-FloatValue(), m_location));

        case OP_logical_not:
            if (FloatValue() == 0.0) {
                return std::shared_ptr<AstTrue>(new AstTrue(m_location));
            } else {
                return std::shared_ptr<AstFalse>(new AstFalse(m_location));
            }

        default:
            return nullptr;
    }
}

} // namespace hyperion::compiler
