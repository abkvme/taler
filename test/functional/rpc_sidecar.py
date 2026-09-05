#!/usr/bin/env python3
# Copyright (c) 2026 The Taler Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""The API-sidecar registry and what it puts in the node's user agent.

A sidecar is a separate process alongside the node. It registers, the node
advertises it to peers as part of its BIP-14 user agent, and the registration
lapses on its own if the sidecar stops calling - so a sidecar that crashes does
not leave the node claiming an API nobody is serving.

The version a sidecar supplies is broadcast to the network, so it is untrusted
input on a path that reaches other people's machines. Several cases here are
about refusing it rather than about the happy path.
"""

import time

from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import assert_equal, assert_raises_rpc_error, assert_greater_than


class SidecarTest(BitcoinTestFramework):
    def set_test_params(self):
        self.setup_clean_chain = True
        self.num_nodes = 1
        self.extra_args = [['-stakegen=0']]

    def subversion(self):
        return self.nodes[0].getnetworkinfo()['subversion']

    def run_test(self):
        node = self.nodes[0]

        self.log.info("A daemon reports SERV and no sidecar")
        assert 'SERV' in self.subversion(), self.subversion()
        assert 'api:' not in self.subversion()
        assert_equal(node.getnetworkinfo()['runmode'], 'SERV')
        assert_equal(node.getnetworkinfo()['sidecar'], None)
        assert_equal(node.getsidecarinfo()['registered'], False)

        self.log.info("Registering puts api:<version> in the user agent")
        result = node.sidecarregister('api', '1.2.0')
        token = result['token']
        assert 'api:1.2.0' in result['subversion'], result['subversion']
        assert_equal(result['subversion'], self.subversion())
        assert_equal(node.getnetworkinfo()['sidecar'], 'api:1.2.0')
        assert_greater_than(result['expires_at'], int(time.time()))

        info = node.getsidecarinfo()
        assert_equal(info['registered'], True)
        assert_equal(info['name'], 'api')
        assert_equal(info['version'], '1.2.0')
        # The token is handed out once, at registration, and never echoed back.
        assert 'token' not in info

        self.log.info("Heartbeat pushes the expiry out")
        first = node.getsidecarinfo()['expires_at']
        time.sleep(1.2)
        beat = node.sidecarheartbeat(token)
        assert_greater_than(beat['expires_at'], first)

        self.log.info("Re-registering with the token is a version update, not a clash")
        updated = node.sidecarregister('api', '1.3.0', 90, token)
        assert 'api:1.3.0' in updated['subversion']
        # Same slot, so the same token keeps working.
        assert_equal(updated['token'], token)
        node.sidecarheartbeat(token)

        self.log.info("A second sidecar cannot displace the first")
        assert_raises_rpc_error(-8, "already registered", node.sidecarregister, 'api', '9.9.9')

        self.log.info("The token guards heartbeat and deregistration")
        assert_raises_rpc_error(-8, "token does not match", node.sidecarheartbeat, 'wrong')
        assert_raises_rpc_error(-8, "token does not match", node.sidecarderegister, 'wrong')
        assert 'api:1.3.0' in self.subversion()

        self.log.info("Deregistering removes it, and is idempotent")
        assert 'api:' not in node.sidecarderegister(token)['subversion']
        assert 'api:' not in self.subversion()
        node.sidecarderegister(token)
        assert_equal(node.getsidecarinfo()['registered'], False)

        self.log.info("Unusable registrations are refused")
        # Anything that could forge the shape of the user agent other nodes parse.
        # ';' separates comments and ' ' is legal inside one, so a version carrying
        # either could split into two comments on the peers that parse it. BIP-14's
        # own character set permits both, which is why this check is stricter.
        for bad in ['1.0)/evil', 'a;b', 'a/b', 'a(b', 'a b', 'a:b', '']:
            assert_raises_rpc_error(-8, "may contain only", node.sidecarregister, 'api', bad)
        assert_raises_rpc_error(-8, "longer than 32", node.sidecarregister, 'api', 'x' * 33)
        assert_raises_rpc_error(-8, "longer than 16", node.sidecarregister, 'y' * 17, '1.0')
        assert_raises_rpc_error(-8, "timeout must be", node.sidecarregister, 'api', '1.0', 5)
        assert_raises_rpc_error(-8, "timeout must be", node.sidecarregister, 'api', '1.0', 3601)
        assert_raises_rpc_error(-8, "no sidecar is registered", node.sidecarheartbeat, 'anything')
        # A refused registration must leave the node exactly as it was.
        assert 'api:' not in self.subversion()

        self.log.info("A silent sidecar is dropped on its own")
        node.sidecarregister('api', '2.0.0', 10)
        assert 'api:2.0.0' in self.subversion()
        # Minimum timeout is 10s and the sweep runs every 15s, so allow for both.
        deadline = time.time() + 60
        while time.time() < deadline and node.getsidecarinfo()['registered']:
            time.sleep(1)
        assert_equal(node.getsidecarinfo()['registered'], False)
        assert 'api:' not in self.subversion()
        self.log.info("...and the user agent went back to %s", self.subversion())


if __name__ == '__main__':
    SidecarTest().main()
