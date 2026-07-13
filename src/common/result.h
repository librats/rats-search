#ifndef RATS_COMMON_RESULT_H
#define RATS_COMMON_RESULT_H

#include <QJsonValue>
#include <QString>
#include <functional>
#include <utility>

namespace rats {

// Unified result type used across the service and front-end layers.
//
// A Result is either a success carrying a JSON payload, or a failure carrying a
// human-readable message. This maps 1:1 onto the REST/WebSocket/P2P response
// envelope, so services, the API router and the peer API all speak the same
// shape and there is a single fail-vs-empty policy.
class Result {
public:
    Result() = default;

    static Result success(QJsonValue data = {})
    {
        Result r;
        r.ok_ = true;
        r.data_ = std::move(data);
        return r;
    }

    static Result failure(QString message)
    {
        Result r;
        r.ok_ = false;
        r.error_ = std::move(message);
        return r;
    }

    bool ok() const { return ok_; }
    const QJsonValue& data() const { return data_; }
    const QString& error() const { return error_; }

private:
    bool ok_ = false;
    QJsonValue data_;
    QString error_;
};

// Asynchronous result delivery. Every service method that cannot answer
// synchronously takes a ResultCallback; the callback is always invoked exactly
// once, on the main thread (see MainThread::post).
using ResultCallback = std::function<void(const Result&)>;

} // namespace rats

#endif // RATS_COMMON_RESULT_H
