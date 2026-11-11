#pragma once
// UMAA Public API v0.8.5 (2026-11-12 Improvement A) C++11 DECLARATION SKETCH.
// For syntax/type demonstration ONLY. No actual DDS, registry or SDK implementation.
#include <type_traits>
#include <vector>

namespace umaa {

enum class ServiceKind { Control, Config, Specs, StatusReport, Other };
enum class FlowKind { CommandResponse, RequestReplyNoQueryData };
enum class OperationRole { Command, CommandAck, CommandStatus, ExecutionStatus, Report };
enum class CommandDelivery { New, Updated, Recovered };
enum class ReportKind { Data, Disposed };
enum class PublishMode { Upsert, ReplaceOwnedInstances };
enum class CommandReason { ValidationFailed, ServiceFailed };
enum class WaitStage { Ack, Commanded, Terminal };
enum class RecoveryDecision { Continue, Cancel, Unknown };

template<class Derived, ServiceKind Kind>
struct Service { static constexpr ServiceKind kind = Kind; };

template<class S, class P, OperationRole Role, FlowKind Flow, class KeyType = int>
struct Operation {
    typedef S ServiceType;
    typedef P Payload;
    typedef KeyType Key;
    static constexpr OperationRole role = Role;
    static constexpr FlowKind flow = Flow;
};

struct CommandToken { unsigned long session; };
struct CommandOptions {};
struct RequestOptions {};
struct StatusEvent { bool isTerminal() const { return false; } };
struct AckEvent {};
struct CommandInfo {
    CommandDelivery delivery() const { return CommandDelivery::New; }
    CommandToken token() const { return CommandToken(); }
};
struct CancelInfo {
    CommandToken token() const { return CommandToken(); }
};
struct CommandDecision {
    static CommandDecision accepted() { return CommandDecision(); }
    static CommandDecision resume() { return CommandDecision(); }
    static CommandDecision deferred() { return CommandDecision(); }
    static CommandDecision rejected(CommandReason) { return CommandDecision(); }
};
struct CancelDecision {
    static CancelDecision canceled() { return CancelDecision(); }
    static CancelDecision deferred() { return CancelDecision(); }
    static CancelDecision cannotCancel() { return CancelDecision(); }
};
struct CommandProgress {
    static CommandProgress executing() { return CommandProgress(); }
    static CommandProgress completed() { return CommandProgress(); }
    static CommandProgress canceled() { return CommandProgress(); }
};
struct CancelAttempt {};
struct PublishResult {};
template<class T> struct Optional {
    bool hasValue() const { return false; }
    const T& value() const;
};
template<class T>
struct ReportEvent {
    ReportKind kind() const { return ReportKind::Data; }
    const T& data() const;
};
template<class T> struct ExecutionStatusEvent {
    const T& data() const;
};
template<class T> struct RecoveredCommand {
    const T& payload() const;
};

template<class Op>
class CommandCall {
public:
    void update(const typename Op::Payload&); // preserve the same session
    StatusEvent get() const;
    StatusEvent wait(WaitStage, unsigned int timeoutMs) const;
};

class Subscription {
public:
    void close();
};

template<class S>
struct IsService : std::is_base_of<Service<S, S::kind>, S> {};

template<class S, class Op, OperationRole Expected>
struct CheckOperation {
    static_assert(IsService<S>::value, "S must be a generated Service");
    static_assert(std::is_same<typename Op::ServiceType, S>::value,
                  "Operation belongs to another Service");
    static_assert(Op::role == Expected, "Operation role mismatch");
};

template<class S>
class CommandProvider {
public:
    template<class Op, class Handler> void onCommand(Handler) {
        CheckOperation<S, Op, OperationRole::Command> check; (void)check;
    }
    template<class Op, class Handler> void onCancel(Handler) {
        CheckOperation<S, Op, OperationRole::Command> check; (void)check;
    }
    void respond(const CommandToken&, const CommandProgress&);
    template<class ExecOp> void publish(
        const CommandToken&, const typename ExecOp::Payload&) {
        CheckOperation<S, ExecOp, OperationRole::ExecutionStatus> check; (void)check;
    }
};

template<class S>
class CommandConsumer {
public:
    template<class Op>
    CommandCall<Op> command(const typename Op::Payload&,
                            const CommandOptions& = CommandOptions()) {
        CheckOperation<S, Op, OperationRole::Command> check; (void)check;
        return CommandCall<Op>();
    }
    template<class Op> CancelAttempt cancel(const CommandCall<Op>&) {
        CheckOperation<S, Op, OperationRole::Command> check; (void)check;
        return CancelAttempt();
    }
    template<class Op, class Handler> void onStatus(Handler) {
        CheckOperation<S, Op, OperationRole::Command> check; (void)check;
    }
    template<class Op, class Handler> void onAck(Handler) {
        CheckOperation<S, Op, OperationRole::Command> check; (void)check;
    }
    template<class Op, class Handler> void onExecutionStatus(Handler) {
        CheckOperation<S, Op, OperationRole::ExecutionStatus> check; (void)check;
    }
    template<class Op, class Handler> void onRecoveredCommand(Handler) {
        CheckOperation<S, Op, OperationRole::Command> check; (void)check;
    }
};

template<class S>
class ReportProvider {
public:
    template<class Op> void publish(const typename Op::Payload&) {
        CheckOperation<S, Op, OperationRole::Report> check; (void)check;
    }
    template<class Op> PublishResult publish(
        const std::vector<typename Op::Payload>&, PublishMode) {
        CheckOperation<S, Op, OperationRole::Report> check; (void)check;
        return PublishResult();
    }
    template<class Op> void dispose(const typename Op::Key&) {
        CheckOperation<S, Op, OperationRole::Report> check; (void)check;
    }
};

template<class S>
class ReportConsumer {
public:
    template<class Op, class Handler> void onReport(Handler) {
        CheckOperation<S, Op, OperationRole::Report> check; (void)check;
    }
    template<class Op, class Handler> Subscription request(
        Handler, const RequestOptions& = RequestOptions()) {
        CheckOperation<S, Op, OperationRole::Report> check; (void)check;
        return Subscription();
    }
    template<class Op> Optional<typename Op::Payload> getLatest(
        const typename Op::Key&) const {
        CheckOperation<S, Op, OperationRole::Report> check; (void)check;
        return Optional<typename Op::Payload>();
    }
};

template<class S>
class ServiceHandle {
public:
    CommandProvider<S>& commandProvider();
    CommandConsumer<S>& commandConsumer();
    ReportProvider<S>& reportProvider();
    ReportConsumer<S>& reportConsumer();
};

class Component {
public:
    virtual ~Component() {}
protected:
    virtual void onStart() {}
    virtual void onStop() {}
    template<class S> ServiceHandle<S>& service();
};

// Service-centric registration builder: available hooks are checked by
// actual generated Operation role/flow, NOT by ServiceKind.
template<class S>
class ProviderBinding {
public:
    static_assert(IsService<S>::value, "provide<S> requires a generated Service");
    template<class Op, class Handler>
    ProviderBinding& onCommand(Handler handler) {
        CheckOperation<S, Op, OperationRole::Command> check; (void)check;
        static_assert(Op::flow == FlowKind::CommandResponse, "Command flow required");
        (void)handler;
        return *this;
    }
    template<class Op, class Handler>
    ProviderBinding& onCancel(Handler handler) {
        CheckOperation<S, Op, OperationRole::Command> check; (void)check;
        static_assert(Op::flow == FlowKind::CommandResponse, "Command flow required");
        (void)handler;
        return *this;
    }
    // Report-only Provider requires no incoming Hook; declaration alone suffices.
};

template<class S>
class ConsumerBinding {
public:
    static_assert(IsService<S>::value, "consume<S> requires a generated Service");
    template<class Op, class Handler>
    ConsumerBinding& onStatus(Handler handler) {
        CheckOperation<S, Op, OperationRole::Command> check; (void)check;
        static_assert(Op::flow == FlowKind::CommandResponse, "Command flow required");
        (void)handler;
        return *this;
    }
    template<class Op, class Handler>
    ConsumerBinding& onAck(Handler handler) {
        CheckOperation<S, Op, OperationRole::Command> check; (void)check;
        static_assert(Op::flow == FlowKind::CommandResponse, "Command flow required");
        (void)handler;
        return *this;
    }
    template<class Op, class Handler>
    ConsumerBinding& onExecutionStatus(Handler handler) {
        CheckOperation<S, Op, OperationRole::ExecutionStatus> check; (void)check;
        static_assert(Op::flow == FlowKind::CommandResponse, "Command flow required");
        (void)handler;
        return *this;
    }
    template<class Op, class Handler>
    ConsumerBinding& onRecoveredCommand(Handler handler) {
        CheckOperation<S, Op, OperationRole::Command> check; (void)check;
        static_assert(Op::flow == FlowKind::CommandResponse, "Command flow required");
        (void)handler;
        return *this;
    }
    template<class Op, class Handler>
    ConsumerBinding& onReport(Handler handler) {
        CheckOperation<S, Op, OperationRole::Report> check; (void)check;
        static_assert(Op::flow == FlowKind::RequestReplyNoQueryData, "Queryless Report flow required");
        (void)handler;
        return *this;
    }
};

template<class ComponentType>
class Registrar {
public:
    template<class S> ProviderBinding<S> provide();
    template<class S> ConsumerBinding<S> consume();
};
} // namespace umaa

#define UMAA_COMPONENT(Type, m) void register_##Type(umaa::Registrar<Type>& m)
