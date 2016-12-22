#ifndef SCRIPTING_H
#define SCRIPTING_H

#include "hexutil/basics/error.h"
#include "hexutil/basics/objmap.h"
#include "hexutil/basics/properties.h"

#include "hexutil/messaging/message.h"
#include "hexutil/messaging/builtin_messages.h"


class Compiler;
class Execution;

class Instruction {
public:
    virtual ~Instruction() { }

    virtual void execute(Execution *execution) = 0;
};

typedef std::vector<Instruction *> InstructionSequence;

class Script: public boost::enable_shared_from_this<Script> {
public:
    typedef boost::shared_ptr<Script> pointer;

    Script(const std::string& name, const InstructionSequence& instructions):
            name(name), instructions(instructions) { }

public:
    std::string name;
    InstructionSequence instructions;
};

class InstructionCompiler {
public:
    virtual ~InstructionCompiler() { }

    virtual Instruction *compile(Message *message, Compiler *compiler) = 0;
};

class BuiltinInstructionCompiler: public InstructionCompiler {
    virtual Instruction *compile(Message *message, Compiler *compiler);
};

class Compiler {
public:
    Compiler();

    void register_instruction_compiler(InstructionCompiler *instruction_compiler);

    Script::pointer compile(const std::string& name, MessageSequence& sequence);
    InstructionSequence compile_sequence(MessageSequence sequence);

private:
    Instruction *compile_instruction(Message *message);

    std::vector<InstructionCompiler *> instruction_compilers;
};

class Execution {
public:
    Execution(StrMap<Script> *scripts): scripts(scripts), return_active(false) { }
    virtual ~Execution() { }

    void add_properties(Properties *properties);

    void run(Script *script);
    void run(const std::string& script_name);

    void execute_script(Script *script);
    void execute_sequence(InstructionSequence& instructions);
    void execute_instruction(Instruction *instr);

    const Datum& get(const Atom& name) const {
        if (variables.contains(name))
            return variables.get(name);
        for (auto iter = properties_list.begin(); iter != properties_list.end(); iter++) {
            Properties& properties = **iter;
            if (properties.contains(name))
                return properties.get(name);
        }
        return variables.get(name);
    }

public:
    StrMap<Script> *scripts;
    Properties variables;
    std::vector<Properties *> properties_list;
    bool return_active;
};

class ScriptError: public BaseError<ScriptError> {
};


#endif
