// Copyright (c) 2026 The Taler Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/asyncrpc.h>

#include <interfaces/node.h>

#include <QEventLoop>
#include <QProgressDialog>
#include <QTimer>

#include <atomic>
#include <thread>

bool RunNodeRpc(interfaces::Node& node, const QString& method, const UniValue& params,
                const std::string& uri, UniValue& result, QString& error,
                QWidget* parent, const QString& busy_message)
{
    std::atomic<bool> finished(false);
    bool ok = false;
    UniValue local_result;
    QString local_error;

    const std::string method_utf8 = method.toStdString();

    std::thread worker([&]() {
        try {
            local_result = node.executeRpc(method_utf8, params, uri);
            ok = true;
        } catch (const UniValue& e) {
            const UniValue message = find_value(e, "message");
            local_error = QString::fromStdString(message.isStr() ? message.get_str() : e.write());
        } catch (const std::exception& e) {
            local_error = QString::fromStdString(e.what());
        } catch (...) {
            local_error = QObject::tr("Unknown error");
        }
        finished = true;
    });

    // Application-modal and cancel-less: the operation cannot be interrupted safely
    // half way through, and nothing else should be started while it runs.
    QProgressDialog progress(busy_message, QString(), 0, 0, parent);
    progress.setWindowModality(Qt::ApplicationModal);
    progress.setCancelButton(nullptr);
    progress.setMinimumDuration(150);
    progress.setAutoClose(false);
    progress.setAutoReset(false);

    QEventLoop loop;
    // Poll rather than rely on a queued quit(): the worker can finish before the loop
    // starts, and a lost wake-up here would hang the application just as badly as the
    // deadlock this function exists to avoid.
    QTimer timer;
    timer.setInterval(25);
    QObject::connect(&timer, &QTimer::timeout, [&]() {
        if (finished) loop.quit();
    });
    timer.start();
    loop.exec();
    timer.stop();

    worker.join();
    progress.close();

    result = local_result;
    error = local_error;
    return ok;
}
