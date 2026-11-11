#pragma once
// v0.8.4: API declaration/typing sketch only, NOT the actual UMAA SDK.
// Only intended to check C++11 surface syntax in EXAMPLE_COMPONENT_CXX11_SKETCH_v0.8.4.cpp.
#include <type_traits>
#include <vector>

namespace umaa {

enum class ServiceKind { Config, Control, Specs, StatusReport, Other };
enum class FlowKind { CommandResponse, RequestReplyNoQueryData };
enum class OperationRole { Command, CommandAck, CommandStatus, ExecutionStatus, Report };
enum class CommandDeliveryKind { New, Updated, Recovered };
enum class PublishMode { Upsert, ReplaceOwnedInstances };
enum class CommandReason { ValidationFailed, ServiceFailed };

template<class D, ServiceKind K>
struct Service { static constexpr ServiceKind kind = K; };

template<class S, class T, OperationRole R, FlowKind F>
struct Operation {
    typedef S ServiceType;
    typedef T Payload;
    static constexpr OperationRole role = R;
    static constexpr FlowKind flow = F;
};

struct CommandToken {};
struct CommandInfo {
    CommandDeliveryKind kind() const;
    CommandToken token() const;
};
struct CancelInfo {
    CommandToken token() const;
};
struct CommandDecision {
    static CommandDecision accepted();
    static CommandDecision resume();
    static CommandDecision rejected(CommandReason);
};
struct CancelDecision {
    static CancelDecision canceled();
    static CancelDecision deferred();
    static CancelDecision cannotCancel();
};
struct CommandResult { static CommandResult canceled(); };
struct ConsumerOptions {
    template<class CommandOp, class Handler>
    ConsumerOptions& onRecovered(Handler);
};
enum class RecoveryDecision { Continue, Cancel };
template<class T> struct RecoveredCommand {
    const T& payload() const;
};
struct CommandCall {};
struct Component {
protected:
    template<class CommandOp>
    CommandCall send(const typename CommandOp::Payload&);
    template<class ReportOp>
    void publish(const typename ReportOp::Payload&);
    template<class ExecutionOp>
    void publish(const CommandToken&, const typename ExecutionOp::Payload&);
    template<class ReportOp>
    void publish(const std::vector<typename ReportOp::Payload>&, PublishMode);
    void respond(const CommandToken&, const CommandResult&);
};

template<class S>
struct ProviderBinding {
    static_assert(std::is_base_of<Service<S, S::kind>, S>::value,
                  "Provider requires generated Service type");
    template<class Op, class Handler>
    ProviderBinding& command(Handler) {
        static_assert(std::is_same<typename Op::ServiceType,S>::value,
                      "Operation not in declared provider Service");
        static_assert(Op::role == OperationRole::Command,
                      "Only Command operation in provider.command");
        return *this;
    }
    template<class Op, class Handler>
    ProviderBinding& cancel(Handler) {
        static_assert(std::is_same<typename Op::ServiceType,S>::value,
                      "Operation not in declared provider Service");
        static_assert(Op::role == OperationRole::Command,
                      "Cancel must reference the original Command operation");
        return *this;
    }
};

template<class S>
struct ConsumerBinding {
    static_assert(std::is_base_of<Service<S, S::kind>, S>::value,
                  "Consumer requires generated Service type");
    template<class Op, class Handler>
    ConsumerBinding& report(Handler) {
        static_assert(std::is_same<typename Op::ServiceType,S>::value,
                      "Operation not in declared consumer Service");
        static_assert(Op::role != OperationRole::Command,
                      "Command operation cannot be subscribed as a Report");
        return *this;
    }
};

template<class ComponentT>
struct Registrar {
    template<class S> ProviderBinding<S> provider();
    template<class S> ConsumerBinding<S> consumer(const ConsumerOptions& = ConsumerOptions());
};

} // namespace umaa

#define UMAA_COMPONENT(Type, m) void register_##Type(umaa::Registrar<Type>& m)
