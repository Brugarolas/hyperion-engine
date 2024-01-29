#include <script/compiler/ast/AstArrayExpression.hpp>
#include <script/compiler/ast/AstTypeObject.hpp>
#include <script/compiler/ast/AstVariable.hpp>
#include <script/compiler/ast/AstArgument.hpp>
#include <script/compiler/ast/AstAsExpression.hpp>
#include <script/compiler/ast/AstPrototypeSpecification.hpp>
#include <script/compiler/ast/AstTemplateInstantiation.hpp>
#include <script/compiler/ast/AstTypeRef.hpp>
#include <script/compiler/AstVisitor.hpp>
#include <script/compiler/Module.hpp>
#include <script/compiler/Configuration.hpp>

#include <script/compiler/type-system/BuiltinTypes.hpp>

#include <script/compiler/emit/BytecodeChunk.hpp>
#include <script/compiler/emit/BytecodeUtil.hpp>
#include <script/compiler/emit/StorageOperation.hpp>

#include <script/Instructions.hpp>
#include <system/Debug.hpp>

#include <core/lib/FlatSet.hpp>

#include <Types.hpp>

namespace hyperion::compiler {

AstArrayExpression::AstArrayExpression(
    const Array<RC<AstExpression>> &members,
    const SourceLocation &location
) : AstExpression(location, ACCESS_MODE_LOAD),
    m_members(members),
    m_held_type(BuiltinTypes::ANY)
{
}

void AstArrayExpression::Visit(AstVisitor *visitor, Module *mod)
{
    m_expr_type = BuiltinTypes::UNDEFINED;

    m_replaced_members.Reserve(m_members.Size());

    FlatSet<SymbolTypePtr_t> held_types;

    for (auto &member : m_members) {
        AssertThrow(member != nullptr);
        member->Visit(visitor, mod);

        if (member->GetExprType() != nullptr) {
            held_types.Insert(member->GetExprType());
        } else {
            held_types.Insert(BuiltinTypes::ANY);
        }

        m_replaced_members.PushBack(CloneAstNode(member));
    }

    for (const auto &it : held_types) {
        AssertThrow(it != nullptr);

        if (m_held_type->IsOrHasBase(*BuiltinTypes::UNDEFINED)) {
            // `Undefined` invalidates the array type
            break;
        }
        
        if (m_held_type->IsAnyType() || m_held_type->IsPlaceholderType()) {
            // take first item found that is not `Any`
            m_held_type = it;
        } else if (m_held_type->TypeCompatible(*it, false)) { // allow non-strict numbers because we can do a cast
            m_held_type = SymbolType::TypePromotion(m_held_type, it);
        } else {
            // more than one differing type, use Any.
            m_held_type = BuiltinTypes::ANY;
            break;
        }
    }

    for (SizeType index = 0; index < m_replaced_members.Size(); index++) {
        auto &replaced_member = m_replaced_members[index];
        AssertThrow(replaced_member != nullptr);

        auto &member = m_members[index];
        AssertThrow(member != nullptr);

        if (SymbolTypePtr_t expr_type = member->GetExprType()) {
            if (!expr_type->TypeEqual(*m_held_type)) {
                // replace with a cast to the held type
                replaced_member.Reset(new AstAsExpression(
                    replaced_member,
                    RC<AstPrototypeSpecification>(new AstPrototypeSpecification(
                        RC<AstTypeRef>(new AstTypeRef(
                            m_held_type,
                            member->GetLocation()
                        )),
                        member->GetLocation()
                    )),
                    member->GetLocation()
                ));
            }
        }

        replaced_member->Visit(visitor, mod);
    }

    m_array_type_expr.Reset(new AstPrototypeSpecification(
        RC<AstTemplateInstantiation>(new AstTemplateInstantiation(
            RC<AstVariable>(new AstVariable(
                "array",
                m_location
            )),
            {
                RC<AstArgument>(new AstArgument(
                    RC<AstTypeRef>(new AstTypeRef(
                        m_held_type,
                        m_location
                    )),
                    false,
                    false,
                    false,
                    false,
                    "T",
                    m_location
                ))
            },
            m_location
        )),
        m_location
    ));

    m_array_type_expr->Visit(visitor, mod);

    auto *array_type_expr_value_of = m_array_type_expr->GetDeepValueOf();
    AssertThrow(array_type_expr_value_of != nullptr);

    SymbolTypePtr_t array_type = array_type_expr_value_of->GetHeldType();
    
    if (array_type == nullptr) {
        // error already reported
        return;
    }

    array_type = array_type->GetUnaliased();

    // @TODO: Cache generic instance types
    m_expr_type = array_type;

    // m_type_object.Reset(new AstTypeObject(
    //     m_expr_type,
    //     BuiltinTypes::CLASS_TYPE,
    //     m_location
    // ));

    // m_expr_type->SetTypeObject(m_type_object);

    // visitor->GetCompilationUnit()->GetCurrentModule()->
    //     m_scopes.Root().GetIdentifierTable().AddSymbolType(m_expr_type);

    //m_type_object->Visit(visitor, mod);
}

std::unique_ptr<Buildable> AstArrayExpression::Build(AstVisitor *visitor, Module *mod)
{
    std::unique_ptr<BytecodeChunk> chunk = BytecodeUtil::Make<BytecodeChunk>();

    // if (m_type_object != nullptr) {
    //     chunk->Append(m_type_object->Build(visitor, mod));
    // }

    if (m_array_type_expr != nullptr) {
        chunk->Append(m_array_type_expr->Build(visitor, mod));
    }

    const Bool has_side_effects = MayHaveSideEffects();
    const UInt32 array_size = UInt32(m_members.Size());
    
    // get active register
    UInt8 rp = visitor->GetCompilationUnit()->GetInstructionStream().GetCurrentRegister();

    { // add NEW_ARRAY instruction
        auto instr_new_array = BytecodeUtil::Make<RawOperation<>>();
        instr_new_array->opcode = NEW_ARRAY;
        instr_new_array->Accept<UInt8>(rp);
        instr_new_array->Accept<UInt32>(array_size);
        chunk->Append(std::move(instr_new_array));
    }
    
    Int stack_size_before = 0;

    if (has_side_effects) {
        // move to stack temporarily
        { // store value of the right hand side on the stack
            auto instr_push = BytecodeUtil::Make<RawOperation<>>();
            instr_push->opcode = PUSH;
            instr_push->Accept<UInt8>(rp);
            chunk->Append(std::move(instr_push));
        }
        
        stack_size_before = visitor->GetCompilationUnit()->GetInstructionStream().GetStackSize();
        // increment stack size
        visitor->GetCompilationUnit()->GetInstructionStream().IncStackSize();
    } else {
        // claim register for array
        visitor->GetCompilationUnit()->GetInstructionStream().IncRegisterUsage();

        // get active register
        rp = visitor->GetCompilationUnit()->GetInstructionStream().GetCurrentRegister();
    }

    // assign all array items
    Int index = 0;

    for (auto &member : m_replaced_members) {
        chunk->Append(member->Build(visitor, mod));

        rp = visitor->GetCompilationUnit()->GetInstructionStream().GetCurrentRegister();

        if (has_side_effects) {
            // claim register for member
            visitor->GetCompilationUnit()->GetInstructionStream().IncRegisterUsage();
            // get active register
            rp = visitor->GetCompilationUnit()->GetInstructionStream().GetCurrentRegister();

            const Int stack_size_after = visitor->GetCompilationUnit()->GetInstructionStream().GetStackSize();
            const Int diff = stack_size_after - stack_size_before;
            AssertThrow(diff == 1);

            { // load array from stack back into register
                auto instr_load_offset = BytecodeUtil::Make<RawOperation<>>();
                instr_load_offset->opcode = LOAD_OFFSET;
                instr_load_offset->Accept<UInt8>(rp);
                instr_load_offset->Accept<UInt16>(UInt16(diff));
                chunk->Append(std::move(instr_load_offset));
            }

            { // send to the array
                auto instr_mov_array_idx = BytecodeUtil::Make<RawOperation<>>();
                instr_mov_array_idx->opcode = MOV_ARRAYIDX;
                instr_mov_array_idx->Accept<UInt8>(rp);
                instr_mov_array_idx->Accept<UInt32>(index);
                instr_mov_array_idx->Accept<UInt8>(rp - 1);
                chunk->Append(std::move(instr_mov_array_idx));
            }

            // unclaim register for member
            visitor->GetCompilationUnit()->GetInstructionStream().DecRegisterUsage();
            // get active register
            rp = visitor->GetCompilationUnit()->GetInstructionStream().GetCurrentRegister();
        } else {
            // send to the array
            auto instr_mov_array_idx = BytecodeUtil::Make<RawOperation<>>();
            instr_mov_array_idx->opcode = MOV_ARRAYIDX;
            instr_mov_array_idx->Accept<UInt8>(rp - 1);
            instr_mov_array_idx->Accept<UInt32>(index);
            instr_mov_array_idx->Accept<UInt8>(rp);
            chunk->Append(std::move(instr_mov_array_idx));
        }

        index++;
    }

    if (!has_side_effects) {
        // unclaim register for array
        visitor->GetCompilationUnit()->GetInstructionStream().DecRegisterUsage();
        // get active register
        rp = visitor->GetCompilationUnit()->GetInstructionStream().GetCurrentRegister();
    } else {
        // move from stack to register 0
    
        int stack_size_after = visitor->GetCompilationUnit()->GetInstructionStream().GetStackSize();
        int diff = stack_size_after - stack_size_before;
        AssertThrow(diff == 1);
        
        { // load array from stack back into register
            auto instr_load_offset = BytecodeUtil::Make<StorageOperation>();
            instr_load_offset->GetBuilder().Load(rp).Local().ByOffset(diff);
            chunk->Append(std::move(instr_load_offset));
        }

        // pop the array from the stack
        chunk->Append(BytecodeUtil::Make<PopLocal>(1));

        // decrement stack size
        visitor->GetCompilationUnit()->GetInstructionStream().DecStackSize();
    }

    return chunk;
}

void AstArrayExpression::Optimize(AstVisitor *visitor, Module *mod)
{
    if (m_array_type_expr != nullptr) {
        m_array_type_expr->Optimize(visitor, mod);
    }

    for (auto &member : m_replaced_members) {
        if (member != nullptr) {
            member->Optimize(visitor, mod);
        }
    }
}

RC<AstStatement> AstArrayExpression::Clone() const
{
    return CloneImpl();
}

Tribool AstArrayExpression::IsTrue() const
{
    return Tribool::True();
}

bool AstArrayExpression::MayHaveSideEffects() const
{
    bool side_effects = false;

    for (const auto &member : m_replaced_members) {
        AssertThrow(member != nullptr);
        
        if (member->MayHaveSideEffects()) {
            side_effects = true;
            break;
        }
    }

    return side_effects;
}

SymbolTypePtr_t AstArrayExpression::GetExprType() const
{
    if (m_expr_type == nullptr) {
        return BuiltinTypes::UNDEFINED;
    }

    return m_expr_type;
}

} // namespace hyperion::compiler
