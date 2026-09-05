// Copyright (c) 2026 The Taler Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <rpc/server.h>
#include <rpc/protocol.h>
#include <sidecar.h>
#include <useragent.h>
#include <utilstrencodings.h>

#include <univalue.h>

static UniValue SidecarToUniValue(const sidecar::Registration& registration)
{
    UniValue obj(UniValue::VOBJ);
    obj.pushKV("registered", registration.registered);
    if (!registration.registered) return obj;
    obj.pushKV("name", registration.name);
    obj.pushKV("version", registration.version);
    obj.pushKV("registered_at", registration.registered_at);
    obj.pushKV("last_heartbeat", registration.last_heartbeat);
    obj.pushKV("timeout", registration.timeout);
    obj.pushKV("expires_at", registration.last_heartbeat + registration.timeout);
    return obj;
}

static UniValue sidecarregister(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() < 2 || request.params.size() > 4) {
        throw std::runtime_error(
            "sidecarregister \"name\" \"version\" ( timeout \"token\" )\n"
            "\nTells the node that an API sidecar is running alongside it, so the node can\n"
            "report it to the network in its version string as name:version.\n"
            "\nThe registration lapses on its own if sidecarheartbeat is not called within\n"
            "the timeout, so a sidecar that stops does not leave the node advertising an API\n"
            "nobody is serving. It is not remembered across restarts: a node that comes back\n"
            "up has no sidecar until one registers again.\n"
            "\nArguments:\n"
            "1. \"name\"     (string, required) Short identifier, e.g. \"api\". At most 16\n"
            "               characters: letters, digits, '.', '-' and '_' only.\n"
            "2. \"version\"  (string, required) The sidecar's version, e.g. \"1.2.0\". At most 32\n"
            "               characters, same restriction. These two are broadcast to peers, so\n"
            "               anything that could be mistaken for structure in the version string\n"
            "               is refused rather than quietly rewritten.\n"
            "3. timeout     (numeric, optional, default=90) Seconds without a heartbeat before\n"
            "               the registration lapses. Between 10 and 3600.\n"
            "4. \"token\"    (string, optional) The token from a previous registration. Supply it\n"
            "               to update the version of a sidecar already registered; omit it when\n"
            "               registering for the first time.\n"
            "\nResult:\n"
            "{\n"
            "  \"token\": \"...\",       (string) Needed by sidecarheartbeat and sidecarderegister.\n"
            "                          Keep it; it is shown only here.\n"
            "  \"subversion\": \"...\",  (string) The version string peers will now be told\n"
            "  \"timeout\": n,          (numeric)\n"
            "  \"expires_at\": n        (numeric) Unix time this lapses without a heartbeat\n"
            "}\n"
            "\nExamples:\n"
            + HelpExampleCli("sidecarregister", "\"api\" \"1.2.0\"")
            + HelpExampleRpc("sidecarregister", "\"api\", \"1.2.0\", 90")
        );
    }

    const std::string name = request.params[0].get_str();
    const std::string version = request.params[1].get_str();
    int64_t timeout = sidecar::DEFAULT_TIMEOUT;
    if (!request.params[2].isNull()) timeout = request.params[2].get_int64();
    std::string token;
    if (!request.params[3].isNull()) token = request.params[3].get_str();

    sidecar::Registration registration;
    std::string error;
    if (!sidecar::Register(name, version, timeout, token, registration, error)) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, error);
    }

    UniValue obj(UniValue::VOBJ);
    obj.pushKV("token", registration.token);
    obj.pushKV("subversion", useragent::Get());
    obj.pushKV("timeout", registration.timeout);
    obj.pushKV("expires_at", registration.last_heartbeat + registration.timeout);
    return obj;
}

static UniValue sidecarheartbeat(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 1) {
        throw std::runtime_error(
            "sidecarheartbeat \"token\"\n"
            "\nKeeps the sidecar registration alive. Call it well inside the timeout - a third\n"
            "of it is a reasonable interval, so one lost call costs nothing.\n"
            "\nArguments:\n"
            "1. \"token\"  (string, required) The token from sidecarregister.\n"
            "\nResult:\n"
            "{\n"
            "  \"expires_at\": n,  (numeric) Unix time this now lapses\n"
            "  \"timeout\": n      (numeric)\n"
            "}\n"
            "\nExamples:\n"
            + HelpExampleCli("sidecarheartbeat", "\"6f1c...\"")
            + HelpExampleRpc("sidecarheartbeat", "\"6f1c...\"")
        );
    }

    sidecar::Registration registration;
    std::string error;
    if (!sidecar::Heartbeat(request.params[0].get_str(), registration, error)) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, error);
    }

    UniValue obj(UniValue::VOBJ);
    obj.pushKV("expires_at", registration.last_heartbeat + registration.timeout);
    obj.pushKV("timeout", registration.timeout);
    return obj;
}

static UniValue sidecarderegister(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 1) {
        throw std::runtime_error(
            "sidecarderegister \"token\"\n"
            "\nRemoves the sidecar registration, so the node stops reporting it.\n"
            "\nSucceeds when nothing is registered, so a shutdown path does not have to know\n"
            "whether the registration had already lapsed.\n"
            "\nArguments:\n"
            "1. \"token\"  (string, required) The token from sidecarregister.\n"
            "\nResult:\n"
            "{\n"
            "  \"subversion\": \"...\"  (string) The version string peers will now be told\n"
            "}\n"
            "\nExamples:\n"
            + HelpExampleCli("sidecarderegister", "\"6f1c...\"")
            + HelpExampleRpc("sidecarderegister", "\"6f1c...\"")
        );
    }

    std::string error;
    if (!sidecar::Deregister(request.params[0].get_str(), error)) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, error);
    }

    UniValue obj(UniValue::VOBJ);
    obj.pushKV("subversion", useragent::Get());
    return obj;
}

static UniValue getsidecarinfo(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 0) {
        throw std::runtime_error(
            "getsidecarinfo\n"
            "\nReports the API sidecar registered with this node, if any. Read-only, and needs\n"
            "no token.\n"
            "\nResult:\n"
            "{\n"
            "  \"registered\": true|false,\n"
            "  \"name\": \"...\",          (string) present when registered\n"
            "  \"version\": \"...\",       (string) present when registered\n"
            "  \"registered_at\": n,      (numeric) unix time\n"
            "  \"last_heartbeat\": n,     (numeric) unix time\n"
            "  \"timeout\": n,            (numeric) seconds\n"
            "  \"expires_at\": n,         (numeric) unix time\n"
            "  \"runmode\": \"GUI\"|\"SERV\", (string) how this node reports itself\n"
            "  \"subversion\": \"...\"      (string) the version string peers are told\n"
            "}\n"
            "\nExamples:\n"
            + HelpExampleCli("getsidecarinfo", "")
            + HelpExampleRpc("getsidecarinfo", "")
        );
    }

    UniValue obj = SidecarToUniValue(sidecar::Current());
    obj.pushKV("runmode", useragent::RunModeTag());
    obj.pushKV("subversion", useragent::Get());
    return obj;
}

// clang-format off
static const CRPCCommand commands[] =
{ //  category    name                  actor (function)     argNames
  //  ----------- --------------------- -------------------- ----------
    { "sidecar",  "sidecarregister",    &sidecarregister,    {"name","version","timeout","token"} },
    { "sidecar",  "sidecarheartbeat",   &sidecarheartbeat,   {"token"} },
    { "sidecar",  "sidecarderegister",  &sidecarderegister,  {"token"} },
    { "sidecar",  "getsidecarinfo",     &getsidecarinfo,     {} },
};
// clang-format on

void RegisterSidecarRPCCommands(CRPCTable &t)
{
    for (unsigned int vcidx = 0; vcidx < ARRAYLEN(commands); vcidx++)
        t.appendCommand(commands[vcidx].name, &commands[vcidx]);
}
